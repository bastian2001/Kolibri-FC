<script setup lang="ts">
import Drone3dPreview from '@/components/Drone3dPreview.vue';
import Imu from '@/components/hardwarePorts/imu.vue'
import { sendCommand } from '@/msp/comm';
import { MspFn } from '@/msp/protocol';
import { delay, leBytesToInt } from '@/utils/utils';
import { onBeforeUnmount, onMounted, ref } from 'vue';

type portType = 'analog-osd' | 'digital-osd' | 'analog-vtx' | 'adc' | 'dshot' | 'elrs' | 'gps' | 'baro' | 'mag' | 'large-fs' | 'ledstrip' | 'speaker' | 'imu'
type port = {
	type: portType,

	serialBaud: number,

	// analog OSD status
	detectedVideoType?: 'pal' | 'ntsc',

	// digital OSD status
	canvasWidth?: number,
	canvasHeight?: number,

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

	// ELRS status
	rxFound: boolean,
	txFound: boolean,
	lqi: number,
	packetRate: number,

	// GPS settings
	gpsProtocol: number,
	gpsFound: boolean,
	gpsInited: boolean,

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

	// IMU
	// imuAxes: number[]
	// imuLastAccelCalibState: number,
	// imuAccelCalibState: number,
	// imuLastOrientationState: number,
	// imuOrientationState: number,
	// imuOrientationTimer: number,
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
getRotationContinuous();

onMounted(() => {
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
	background-color: #0f0;
	-webkit-mask: url(@assets/gyroaccel_app.svg) no-repeat center;
	mask: url(@assets/gyroaccel_app.svg) no-repeat center;
	-webkit-mask-size: contain;
	mask-size: contain;
}
</style>
