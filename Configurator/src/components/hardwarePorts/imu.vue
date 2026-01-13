<script setup lang="ts">
import { sendCommand } from '@/msp/comm'
import { MspFn } from '@/msp/protocol'
import { leBytesToInt } from '@/utils/utils'
import { onBeforeUnmount, ref } from 'vue'


const AXES_NAMES = ['+X', '-X', '+Y', '-Y', '+Z', '-Z']
const IMU_NAMES = ['unknown', 'BMI270', 'ICM-42688-P']
const imuAxes = ref([0, 2, 4])
const imuModel = ref('unknown')
const readyFlags = ref(0b1111)

let lastAccelCalibState = 0
const accelCalibState = ref(0)
let lastImuOrientationState = 0
const imuOrientationState = ref(0)
const imuOrientationTimer = ref(0)



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

		imuModel.value = IMU_NAMES[c.data[6]]
		readyFlags.value = c.data[7]
	})
}, 100)

onBeforeUnmount(() => {
	clearInterval(getImuSetupState)
})
</script>

<template>

	<div class="gyro">
		<div
			:class="{ hardwareIcon: true, green: (readyFlags === 0), red: (readyFlags & 1), yellow: (readyFlags & 0b1110) }">
		</div>
		<h3>Gyro / Accelerometer</h3>
		<div class="imuAlignment inlineblock">
			<p>Model: {{ imuModel }}</p><br>
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
</template>

<style scoped>
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
