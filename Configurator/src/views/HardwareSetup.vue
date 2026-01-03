<script setup lang="ts">
import { sendCommand } from '@/msp/comm';
import { MspFn } from '@/msp/protocol';
import { onBeforeUnmount, onMounted, ref } from 'vue';

const AXES_NAMES = ['+X', '-X', '+Y', '-Y', '+Z', '-Z']
const imuAxes = ref([0, 2, 4])
let lastAccelCalibState = 0
const accelCalibState = ref(0)
let lastImuOrientationState = 0
const imuOrientationState = ref(0)
const imuOrientationTimer = ref(0)

const getImuSetupState = setInterval(() => {
	sendCommand(MspFn.GET_IMU_SETUP_STATE).then(c => {
		imuOrientationState.value = c.data[0]
		imuOrientationTimer.value = c.data[1] * 256
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

onMounted(() => {
})

onBeforeUnmount(() => {
	clearInterval(getImuSetupState)
})
</script>

<template>
	<div class="wrapper">
		<div class="calibration">
			<button @click="() => sendCommand(MspFn.ACC_CALIBRATION)">Calibrate Accelerometer</button>
			<p v-if="accelCalibState === 0">Use this to compensate small errors.</p>
			<p v-else-if="accelCalibState === 1">Rest the quad on a horizontal surface.</p>
			<p v-else-if="accelCalibState === 2">Hold still.</p>
			<p v-else-if="accelCalibState === 3">Calibration finished, test whether it is good.</p>
		</div>
		<div class="orientation">
			<button @click="() => sendCommand(MspFn.START_IMU_ORIENTATION)">Start Gyro Orientation</button>
			{{ imuOrientationTimer }}
			<p v-if="imuOrientationState === 0">Use this to reorient the gyro in 90° steps.</p>
			<p v-else-if="imuOrientationState === 1">Put your quad in its normal orientation.</p>
			<p v-else-if="imuOrientationState === 2">Pitch your quad forward so that the nose points down.</p>
			<p v-else-if="imuOrientationState === 3">Test the orientation.</p>
			<p>Forward: {{ AXES_NAMES[imuAxes[0]] }}</p>
			<p>Right: {{ AXES_NAMES[imuAxes[1]] }}</p>
			<p>Down: {{ AXES_NAMES[imuAxes[2]] }}</p>
		</div>
		<div>
			Serial Port Setup
		</div>
		<div>
			Sensor status<br>
			- Gyro/accel found, model<br>
			- Mag found, model<br>
			- baro found, model<br>
			- Large FS (SD/FCKAFD) found, size and available size, audio flashing helper<br>
			- GPS found, init state<br>
			- Other serial devices found such as RX/VTX<br>
			- DShot status<br>
			- ADC values
		</div>
	</div>
</template>

<style scoped>
.wrapper {
	display: flex;
	gap: 1rem;
}

button {
	color: black;
}
</style>
