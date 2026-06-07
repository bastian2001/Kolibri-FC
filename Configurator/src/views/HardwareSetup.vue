<!--
 + Copyright (c) 2026 Kolibri-FC contributors
 + 
 + This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 + 
 + Kolibri-FC is free software: you can redistribute it and/or modify
 + it under the terms of the GNU General Public License as published by
 + the Free Software Foundation, either version 3 of the License, or
 + (at your option) any later version.
 + 
 + Kolibri-FC is distributed in the hope that it will be useful,
 + but WITHOUT ANY WARRANTY; without even the implied warranty of
 + MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 + GNU General Public License for more details.
 + 
 + You should have received a copy of the GNU General Public License
 + along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
-->

<script setup lang="ts">
import Drone3dPreview from '@components/Drone3dPreview.vue';
import Imu from '@components/hardwarePorts/imu.vue'
import Dshot from '@components/hardwarePorts/dshot.vue'
import AddPort from '@components/hardwarePorts/addPort.vue'
import Serial from '@components/hardwarePorts/serial.vue'
import { onConnectHandler, sendCommand } from '@/msp/comm';
import { MspFn } from '@/msp/protocol';
import { delay, intToLeBytes, leBytesToBigInt, leBytesToInt, runAsync } from '@/utils/utils';
import { onBeforeUnmount, onMounted, ref } from 'vue';
import { Command } from '@/utils/types';
import { SerialPort, SerialType, usePortStore } from '@/stores/portStore';
import { useLogStore } from '@/stores/logStore';

const ports = usePortStore()

const { serials, pioSetup, pins } = ports
const log = useLogStore();

// // analog OSD status
// detectedVideoType?: 'pal' | 'ntsc',

// // ADC values
// rawVoltage: number,
// batVoltage: number,
// rawCurrent: number,
// batCurrent: number,
// cpuTemp: number,

// // Baro settings
// baroModel: number,
// baroFound: boolean,
// baroRaw: number,
// baroAltitude: number,
// baroUpdateRate: number,

// // Magnetometer settings
// magMode: number,
// magFound: boolean,
// magRaw: number[],
// magAlignment: number[],
// magCalibration: number,
// magCalibrationState: number,
// magCalibrationTimer: number,

// // Large FS info
// largeFsType: number,
// largeFsTotalSpace: number,
// largeFsFreeSpace: number,

// // LED strip
// ledColors: number[][],

// // Speaker
// speakerSoundFile: string,
// speakerFrequency: number,


let exiting = false
const attitude = ref({ roll: 0, pitch: 0, yaw: 0 })

async function getRotationContinuous() {
	while (!exiting) {
		try {
			const c = await sendCommand(MspFn.GET_ROTATION)
			let roll = leBytesToInt(c.data, 0, 2, true)
			roll /= 8192.0
			roll *= 180.0 / Math.PI
			let pitch = leBytesToInt(c.data, 2, 2, true)
			pitch /= 8192.0
			pitch *= 180.0 / Math.PI
			let yaw = leBytesToInt(c.data, 4, 2, true)
			yaw /= 8192.0
			yaw *= 180.0 / Math.PI
			attitude.value = { roll, pitch, yaw }
			await delay(20) // don't need to stress serial link, this function is only secondary
		} catch (_) {
			await delay(20) // avoid hung program if sendCommand rejects immediately
		}
	}
}

function ioConstraintMatcher(req: Command, res: Command) {
	if (req.command !== res.command) return false
	if (req.data[0] !== res.data[0]) return false
	return true
}

function fetchSetup() {
	ports.reset()
	sendCommand(MspFn.GET_IO_CONSTRAINTS, {
		data: [0],
		verifyFn: ioConstraintMatcher,
	})
		.then(c => {
			const d = c.data.slice(1)
			if (d.length % 11 !== 0) throw ''
			for (let i = 0; i < d.length / 11; i++) {
				const funcs = leBytesToBigInt(d, i * 11, 6)
				const pin = {
					hstx: false,
					spi: false,
					uartTx: Array(4).fill(false),
					uartRx: Array(4).fill(false),
					sda: Array(3).fill(false),
					scl: Array(3).fill(false),
					pio: Array(4).fill(false),
					recommended: false,
					allowed: false,
					label: '',
					pads: [],
				}
				if (funcs & 1n << 0n) pin.hstx = true
				if (funcs & 1n << 1n) pin.spi = true
				for (let j = 2n; j < 10n; j++) {
					const isTx = j % 2n === 0n
					const serial = Number((j - 2n) / 2n)
					if (funcs & (1n << j)) {
						if (isTx) pin.uartTx[serial] = true
						if (!isTx) pin.uartRx[serial] = true
					}
				}
				for (let j = 10n; j < 16n; j++) {
					const isSda = j % 2n === 0n
					const i2c = Number((j - 10n) / 2n)
					if (funcs & (1n << j)) {
						if (isSda) pin.sda[i2c] = true
						if (!isSda) pin.scl[i2c] = true
					}
				}
				for (let j = 16n; j < 20; j++) {
					const pio = Number(j) - 16
					if (funcs & 1n << j) pin.pio[pio] = true
				}
				if (funcs & 1n << 46n) pin.recommended = true
				if (funcs & 1n << 47n) pin.allowed = true
				const str = d.slice(i * 11 + 6, i * 11 + 11);
				let len = str.indexOf(0);
				if (len === -1) len = 5;
				pin.label = String.fromCharCode(...str.slice(0, len));
				pins.push(pin)
			}

			return sendCommand(MspFn.GET_IO_CONSTRAINTS, {
				data: [10],
				verifyFn: ioConstraintMatcher,
			})
		})
		.then(c => {
			const d = c.data.slice(1)
			if (d.length < 2) throw ''
			ports.hwSerials = d[0]
			ports.maxSerials = d[1]

			return sendCommand(MspFn.GET_IO_CONSTRAINTS, {
				data: [11],
				verifyFn: ioConstraintMatcher,
			})
		})
		.then(c => {
			let d = c.data.slice(1)
			if (d.length < 15 || (d.length - 15) % 5 !== 0) throw ''
			pioSetup.version = d[0]
			pioSetup.numPios = d[1]
			pioSetup.hdxLength = d[2]
			pioSetup.txLength = d[3]
			pioSetup.rxLength = d[4]
			d = d.slice(15)
			for (let i = 0; i < d.length / 5; i++) {
				pioSetup.freeSms[i] = d[i * 5 + 0]
				pioSetup.freeInstructions[i] = leBytesToBigInt(d, i * 5 + 1, 4)
			}

			return sendCommand(MspFn.GET_SERIAL_SETUP)
		})
		.then(onGetSerialSetup)
		.catch(e => {
			console.error(e)
		})
}

function update() {
	const data: number[] = []
	const lut: { [key in SerialType]: number } = {
		usb: 0,
		uart: 1,
		pio: 2,
		"pio-hdx": 3,
		disabled: 255,
		invalid: 255,
	}
	serials.forEach(ser => {
		data.push(lut[ser.type])
		data.push(ser.hwParam)
		data.push(...intToLeBytes(ser.baudSet, 4))
		data.push(...intToLeBytes(ser.functions, 4))
		data.push(ser.txPin)
		data.push(ser.rxPin)
		data.push(ser.mspDp.type | (ser.mspDp.resolution << 4))
		data.push(...Array(9).fill(0)) // reserved
	})
	sendCommand(MspFn.SET_SERIAL_SETUP, data)
		.then((c): Promise<any> => {
			if (c.data[0]) {
				console.log('success')
				return sendCommand(MspFn.SAVE_SETTINGS)
			}
			else {
				log.push('Unable to set the serials')
				console.log('F')
				return runAsync()
			}
		})
		.then(() => sendCommand(MspFn.GET_SERIAL_SETUP))
		.then(onGetSerialSetup)
}

const onGetSerialSetup = (c: Command) => {
	const d = c.data
	if (d.length % 27 !== 0) throw ''
	if (d.length / 27 !== ports.maxSerials) throw ''
	for (let i = 0; i < d.length / 27; i++) {
		const bin = d.slice(i * 27, i * 27 + 27)
		const s: SerialPort = {
			exists: false,
			type: 'disabled' as 'usb' | 'uart' | 'pio' | 'pio-hdx' | 'disabled' | 'invalid',
			baud: 0,
			baudSet: 0,
			txPin: 255,
			rxPin: 255,
			functions: 0,
			no: i,
			hwParam: 0,
			modified: false,
			initialFunctions: 0,
			mspDp: { type: 0, resolution: 0 },
		}
		if (bin[0]) s.exists = true
		else {
			serials[i] = s
			continue
		}
		const lut = ['usb', 'uart', 'pio', 'pio-hdx'] as ('usb' | 'uart' | 'pio' | 'pio-hdx' | 'disabled')[]
		lut[255] = 'disabled'
		s.type = lut[bin[1]] || 'invalid'
		s.baud = leBytesToInt(bin, 2, 4)
		s.baudSet = leBytesToInt(bin, 6, 4)
		s.txPin = bin[10]
		s.rxPin = bin[11]
		s.functions = leBytesToInt(bin, 12, 4)
		s.initialFunctions = s.functions
		s.hwParam = bin[16]
		s.mspDp.type = bin[17] & 0xF
		s.mspDp.resolution = bin[17] >> 4
		serials[i] = s
	}
	// TODO fetch pad positions, labels
}

onMounted(() => {
	getRotationContinuous();
	fetchSetup()
	onConnectHandler(fetchSetup)
})

onBeforeUnmount(() => {
	exiting = true
})

</script>

<template>
	<div class="wrapper">
		<div class="previews">
			<Drone3dPreview :roll="attitude.roll" :pitch="attitude.pitch" :yaw="attitude.yaw" :size="300" />
		</div>
		<div class="gridWrapper">
			<div class="header">
				<div class="spacer"></div>
				<button class="updateBtn" @click="update">Save</button>
			</div>
			<div class="grid">
				<Imu />
				<Dshot />
				<template v-for="s in serials">
					<Serial v-if="s.exists" :key="s.no" :num="s.no" @update="update" />
				</template>
				<AddPort />
			</div>
		</div>
	</div>
</template>

<style scoped>
.wrapper {
	display: flex;
	flex-direction: row;
	height: 100%;
	flex-grow: 1;
}

.previews {
	width: 300px;
	flex-grow: 0;
}

.gridWrapper {
	height: 100%;
	overflow: auto;
	flex-grow: 1;
}

.grid {
	display: grid;
	grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
	grid-auto-rows: minmax(530px, auto);
	overflow: hidden;
	gap: 2px;
}

.grid>:deep(div) {
	box-sizing: border-box;
	background-color: var(--background-light);
	text-align: center;
	padding: .6rem 0.3rem;
	box-shadow: 0px 0px 0px 2px var(--border-color);
}

:deep(.hardwareIcon) {
	height: 8rem;
	background-color: white;
	-webkit-mask-size: contain;
	mask-size: contain;
}

:deep(.hardwareIcon.green) {
	background-color: #0f0;
}

:deep(.hardwareIcon.yellow) {
	background-color: #ff0;
}

:deep(.hardwareIcon.orange) {
	background-color: #f80;
}

:deep(.hardwareIcon.red) {
	background-color: #f00;
}

.header {
	display: flex;
	background-color: #223;
	padding: .6rem 2rem;
}

.spacer {
	flex-grow: 1;
}

.updateBtn {
	background-color: transparent;
	border: 1px solid var(--border-green);
	border-radius: 5px;
	padding: 0.5rem 1rem;
	font-size: 1rem;
	color: var(--text-color);
	transition: background-color 0.2s ease-out;
}

.updateBtn:hover {
	background-color: #fff1;
}
</style>
