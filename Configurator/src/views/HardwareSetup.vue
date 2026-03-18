<script setup lang="ts">
import Drone3dPreview from '@components/Drone3dPreview.vue';
import Imu from '@components/hardwarePorts/imu.vue'
import Dshot from '@components/hardwarePorts/dshot.vue'
import AddPort from '@components/hardwarePorts/addPort.vue'
import Serial from '@components/hardwarePorts/serial.vue'
import { onConnectHandler, sendCommand } from '@/msp/comm';
import { MspFn } from '@/msp/protocol';
import { delay, leBytesToBigInt, leBytesToInt } from '@/utils/utils';
import { onBeforeUnmount, onMounted, ref } from 'vue';
import { Command } from '@/utils/types';
import { useLogStore } from '@/stores/logStore';
import { usePortStore } from '@/stores/portStore';

const ports = usePortStore()

const serials = ports.serials

type portType = 'analog-osd' | 'digital-osd' | 'analog-vtx' | 'adc' | 'dshot' | 'elrs' | 'gps' | 'baro' | 'mag' | 'large-fs' | 'ledstrip' | 'speaker' | 'imu'
type port = {
	type: portType,

	serialBaud: number,

	// analog OSD status
	detectedVideoType?: 'pal' | 'ntsc',

	// analog VTX status
	analogVtxOnline?: boolean,
	analogVtxFreq?: number,

	// ADC values
	rawVoltage: number,
	batVoltage: number,
	rawCurrent: number,
	batCurrent: number,
	cpuTemp: number,

	// DShot settings
	dshotSpeed: number,
	bidirStatus: boolean[],
	edtStatus: boolean[],


	// Baro settings
	baroModel: number,
	baroFound: boolean,
	baroRaw: number,
	baroAltitude: number,
	baroUpdateRate: number,

	// Magnetometer settings
	magMode: number,
	magFound: boolean,
	magRaw: number[],
	magAlignment: number[],
	magCalibration: number,
	magCalibrationState: number,
	magCalibrationTimer: number,

	// Large FS info
	largeFsType: number,
	largeFsTotalSpace: number,
	largeFsFreeSpace: number,

	// LED strip
	ledColors: number[][],

	// Speaker
	speakerSoundFile: string,
	speakerFrequency: number,
}


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
	serials.value.length = 0
	sendCommand(MspFn.GET_IO_CONSTRAINTS, {
		data: [0],
		verifyFn: ioConstraintMatcher,
	})
		.then(c => {
			const d = c.data.slice(1)
			if (d.length % 6 !== 0) throw ''
			for (let i = 0; i < d.length / 6; i++) {
				const funcs = leBytesToBigInt(d, i * 6, 6)
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
					pads: []
				}
				if (funcs & 1n << 0n) pin.hstx = true
				if (funcs & 1n << 1n) pin.spi = true
				for (let j = 2n; j < 10n; j++) {
					const isTx = j % 2n === 0n
					const serial = Number(j - 2n) / 2
					if (isTx && funcs & 1n << j) pin.uartTx[serial] = true
					if (!isTx && funcs & 1n << j) pin.uartRx[serial] = true
				}
				for (let j = 10n; j < 16n; j++) {
					const isSda = j % 2n === 0n
					const i2c = Number(j - 10n) / 2
					if (isSda && funcs & 1n << j) pin.sda[i2c] = true
					if (!isSda && funcs & 1n << j) pin.scl[i2c] = true
				}
				for (let j = 16n; j < 20; j++) {
					const pio = Number(j) - 16
					if (funcs & 1n << j) pin.pio[pio] = true
				}
				if (funcs & 1n << 46n) pin.recommended = true
				if (funcs & 1n << 47n) pin.allowed = true
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
			hwSerials = d[0]
			maxSerials = d[1]

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
		.then(c => {
			const d = c.data
			if (d.length % 17 !== 0) throw ''
			for (let i = 0; i < d.length / 17; i++) {
				const bin = d.slice(i * 17, i * 17 + 17)
				const s = {
					exists: false,
					type: 'disabled' as 'usb' | 'uart' | 'pio' | 'pio-hdx' | 'disabled' | 'invalid',
					running: false,
					baud: 0,
					baudSet: 0,
					txPin: 255,
					rxPin: 255,
					functions: 0,
					no: i,
					hwParam: 0
				}
				serials.value.push(s)
				if (bin[0]) s.exists = true
				else continue
				if (bin[1] & 1 << 7) s.running = true
				const lut = ['usb', 'uart', 'pio', 'pio-hdx'] as ('usb' | 'uart' | 'pio' | 'pio-hdx' | 'disabled')[]
				lut[127] = 'disabled'
				s.type = lut[bin[1] & ~(1 << 7)] || 'invalid'
				s.baud = leBytesToInt(bin, 2, 4)
				s.baudSet = leBytesToInt(bin, 6, 4)
				s.txPin = bin[10]
				s.rxPin = bin[11]
				s.functions = leBytesToInt(bin, 12, 4)
				s.hwParam = bin[16]
			}
			// TODO fetch pad positions, labels
		})
		.catch(e => {
			useLogStore().push('Error requesting serial setup')
			console.error(e)
		})
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
			<Drone3dPreview :roll="attitude.roll" :pitch="attitude.pitch" :yaw="attitude.yaw" :size="400" />
		</div>
		<div class="gridWrapper">
			<div class="grid">
				<Imu />
				<Dshot />
				<template v-for="s in serials">
					<Serial v-if="s.exists" />
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
	width: 400px;
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
	padding: 0.3rem;
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
</style>
