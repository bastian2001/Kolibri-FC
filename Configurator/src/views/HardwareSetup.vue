<script setup lang="ts">
import Drone3dPreview from '@/components/Drone3dPreview.vue';
import { sendCommand } from '@/msp/comm';
import { MspFn } from '@/msp/protocol';
import { delay, leBytesToInt } from '@/utils/utils';
import { onBeforeUnmount, onMounted, ref } from 'vue';

const AXES_NAMES = ['+X', '-X', '+Y', '-Y', '+Z', '-Z']
const imuAxes = ref([0, 2, 4])
let lastAccelCalibState = 0
const accelCalibState = ref(0)
let lastImuOrientationState = 0
const imuOrientationState = ref(0)
const imuOrientationTimer = ref(0)

let exiting = false
const attitude = ref({ roll: 0, pitch: 0, yaw: 0 })

const getImuSetupState = setInterval(() => {
	sendCommand(MspFn.GET_IMU_SETUP_STATE).then(c => {
		imuOrientationState.value = c.data[0]
		imuOrientationTimer.value = leBytesToInt(c.data, 1, 1, true)
		for (let i = 0; i < 3; i++) {
			imuAxes.value[i] = c.data[i + 2]
		}

		if (imuOrientationState.value === 0 && (lastImuOrientationState === 2 || lastImuOrientationState === 3)) {
			imuOrientationState.value = 3
			setTimeout(() => {
				if (imuOrientationState.value === 3) {
					imuOrientationState.value = 0
					lastImuOrientationState = 0
				}
			}, 5000)
		}
		if (lastImuOrientationState !== 3 || imuOrientationState.value === 0)
			lastImuOrientationState = imuOrientationState.value

		accelCalibState.value = c.data[5]
		if (accelCalibState.value === 0 && (lastAccelCalibState === 2 || lastAccelCalibState === 3)) {
			accelCalibState.value = 3
			setTimeout(() => {
				if (accelCalibState.value === 3) {
					accelCalibState.value = 0
					lastAccelCalibState = 0
				}
			}, 5000)
		}
		if (lastAccelCalibState !== 3 || accelCalibState.value !== 0)
			lastAccelCalibState = accelCalibState.value
	})
}, 100)


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
	clearInterval(getImuSetupState)
})
</script>

<template>
	<div class="wrapper">
		<div class="previews">
			<Drone3dPreview :roll="attitude.roll" :pitch="attitude.pitch" :yaw="attitude.yaw" :size="400" />
		</div>
		<div class="gridWrapper">
			<div class="grid">
				<div class="gyro">
					<div class="hardwareIcon green"></div>
					<h3>Gyro / Accelerometer</h3>
					<div class="imuAlignment inlineblock">
						<button @click="() => sendCommand(MspFn.START_IMU_ALIGNMENT)" :style="{
							background: `linear-gradient(to right, #fff3 ${Math.max(imuOrientationTimer, 0)}%, transparent ${Math.max(imuOrientationTimer, 0)}%)`,
						}">Start Gyro Alignment</button>
						<p v-if="imuOrientationState === 0">Use this to align the gyro/accelerometer in 90° steps.</p>
						<p v-else-if="imuOrientationState === 1">1. Put your quad in its normal orientation.</p>
						<p v-else-if="imuOrientationState === 2">2. Pitch your quad forward so that the nose points
							down.
						</p>
						<p v-else-if="imuOrientationState === 3">3. Test the alignment.</p>
						<p>Forward: {{ AXES_NAMES[imuAxes[0]] }}, Right: {{ AXES_NAMES[imuAxes[1]] }}, Down: {{
							AXES_NAMES[imuAxes[2]] }}</p>
					</div>
					<div class="accelCalib inlineblock">
						<button @click="() => sendCommand(MspFn.ACC_CALIBRATION)">Calibrate Accelerometer</button>
						<p v-if="accelCalibState === 0">Use this to compensate small errors.</p>
						<p v-else-if="accelCalibState === 1">1. Rest the quad on a horizontal surface.</p>
						<p v-else-if="accelCalibState === 2">2. Hold still.</p>
						<p v-else-if="accelCalibState === 3">3. Calibration finished, test whether it is good.</p>
					</div>
				</div>
				<div>
					Serial Port Setup
				</div>
				<div>
					Serial Port Setup
				</div>
				<div>
					Serial Port Setup
				</div>
				<div>
					Serial Port Setup
				</div>
				<!--
				Sensor status
				- Gyro/accel found, model
				- Mag found, model
				- baro found, model
				- Large FS (SD/FCKAFD) found, size and available size, audio flashing helper
				- GPS found, init state
				- Other serial devices found such as RX/VTX
				- DShot status
				- ADC values
			-->
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

.grid>div {
	box-sizing: border-box;
	background-color: var(--background-light);
	text-align: center;
	padding: 0.3rem;
	box-shadow: 0px 0px 0px 2px var(--border-color);
}

.hardwareIcon {
	height: 8rem;
	background-color: #0f0;
	-webkit-mask: url(@assets/gyroaccel_app.svg) no-repeat center;
	mask: url(@assets/gyroaccel_app.svg) no-repeat center;
	-webkit-mask-size: contain;
	mask-size: contain;
}

h3 {
	margin-top: 6px;
	margin-bottom: 0px;
}

p {
	font-size: .85rem;
	margin-top: 6px;
}

.inlineblock>* {
	margin-right: 10px;
	margin-left: 10px;
	display: inline-block;
}

.imuAlignment {
	margin-top: 8px;
}

button {
	color: var(--text-color);
	font-size: .8rem;
	background-color: transparent;
	border: 1px solid var(--border-color);
	border-radius: 6px;
	padding: 5px 12px;
}
</style>
