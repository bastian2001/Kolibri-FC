<script setup lang="ts">
import { sendCommand } from '@/msp/comm';
import { MspFn } from '@/msp/protocol';
import { leBytesToInt } from '@/utils/utils';
import { computed, onBeforeUnmount, ref } from 'vue';

// shows motor/dshot state and allows adjustments
// - remapping
// - (reversing)
// - output strength
// - bidir dshot state (=> show RPM)
// - EDT state (=> show temp)

const motors = ref([{
	us: 1500,
	name: 'RR',
	enabled: true,
	bidirEnabled: true,
	found: false,
	edt: false,
}, {
	us: 1500,
	name: 'FR',
	enabled: true,
	bidirEnabled: true,
	found: false,
	edt: false,
}, {
	us: 1500,
	name: 'RL',
	enabled: true,
	bidirEnabled: true,
	found: false,
	edt: false,
}, {
	us: 1500,
	name: 'FL',
	enabled: true,
	bidirEnabled: true,
	found: false,
	edt: false,
}])

const batState = ref(3)

const fetchInterval = setInterval(fetchState, 300)
fetchState()
function fetchState() {
	sendCommand(MspFn.GET_MOTOR).then(c => {
		for (let i = 0; i < 4; i++) {
			motors.value[i].us = leBytesToInt(c.data, i * 2, 2, false)
		}
		return sendCommand(MspFn.GET_MOTOR_STATE)
	}).then(c => {
		for (let i = 0; i < motors.value.length; i++) {
			const m = motors.value[i]
			m.enabled = (c.data[i] & 1) !== 0
			m.bidirEnabled = (c.data[i] & 2) !== 0
			m.found = (c.data[i] & 4) !== 0
			m.edt = (c.data[i] & 8) !== 0
		}
		return sendCommand(MspFn.MSP_BATTERY_STATE)
	}).then(c => {
		batState.value = c.data[8]
	})
}

const iconColor = computed(() => {
	let oneIsFound = false
	let oneHasEdt = false
	let worst = 0 // 0 = green, 1 = yellow, 2 = red

	for (const m of motors.value) {
		if (m.found) oneIsFound = true
		if (m.edt) oneHasEdt = true;
	}

	for (const m of motors.value) {
		let thisMotor = 0
		if (!m.enabled || !m.bidirEnabled) continue

		if (batState.value < 3 && !m.found) thisMotor = 2
		else if (batState.value >= 3 && !m.found) thisMotor = 1

		if (m.found !== oneIsFound || m.edt != oneHasEdt) thisMotor = 2


		if (thisMotor > worst) worst = thisMotor
	}

	return ['green', 'yellow', 'red'][worst]
})


onBeforeUnmount(() => {
	clearInterval(fetchInterval)
})
</script>

<template>
	<div>
		<div :class="`hardwareIcon ${iconColor}`">
		</div>
		<div class="motors">
			<div class="motor" v-for="m in motors">
				<p>{{ m.name }}</p>
				<div class="motorSpeed"
					:style="{ background: `linear-gradient(to right, var(--background-blue) ${(m.us - 1000) / 10}%, var(--background-dark-blue) ${(m.us - 1000) / 10}%)` }">
					<p>{{ m.us }}</p>
				</div>
				<div class="properties">
					<p class="bidir" v-if="!m.bidirEnabled">Found: ❔</p>
					<p class="bidir" v-else-if="m.found">Found: ✅</p>
					<p class="bidir" v-else>Found: ❌</p>
					<p class="edt" v-if="m.edt">EDT: ✅</p>
					<p class="bidir" v-else>EDT: ❌</p>
				</div>
			</div>
		</div>
	</div>
</template>

<style scoped>
.hardwareIcon {
	-webkit-mask: url(@assets/motor_app.svg) no-repeat center;
	mask: url(@assets/motor_app.svg) no-repeat center;
}

.motors {
	display: flex;
	gap: 1rem;
	padding: 1rem;
}

.motor {
	flex-grow: 1;
}

.motorSpeed {
	border: 2px solid var(--border-color);
	text-align: center;
	border-radius: .5rem;
}
</style>
