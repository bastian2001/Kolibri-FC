<script setup lang="ts">
import { onBeforeUnmount, ref } from 'vue'
import { sendCommand } from '@/msp/comm'
import { MspFn } from '@/msp/protocol';
import { leBytesToInt } from '@/utils/utils';

const trampStatus = ref(0)
const trampUpdatedFields = ref(0)
const trampMinFreq = ref(0)
const trampMaxFreq = ref(0)
const trampMaxPwr = ref(0)
const trampCurBand = ref(0)
const trampCurChan = ref(0)
const trampCurFreq = ref(0)
const trampCurPitmode = ref(0)
const trampCurPower = ref(0)
const trampCurConfigPower = ref(0)
const trampCurTemp = ref(0)
const noResponseCounter = ref(0)

const STATUS_NAMES = ['Offline', 'Initializing', 'Online', 'Online', 'Online', 'Set Frequency', 'Set Frequency', 'Set Power', 'Set Power', 'Set Pitmode', 'Set Pitmode']
const BAND_NAMES = ['Boscam A', 'Boscam B', 'Boscam E', 'Fatshark', 'Raceband']

const fetchInterval = setInterval(() => {
	sendCommand(MspFn.GET_VTX_CURRENT_STATE).then(c => {
		const data = c.data
		trampUpdatedFields.value = leBytesToInt(data, 0, 4)
		trampStatus.value = data[4]
		trampMinFreq.value = leBytesToInt(data, 5, 2)
		trampMaxFreq.value = leBytesToInt(data, 7, 2)
		trampMaxPwr.value = leBytesToInt(data, 9, 2)
		trampCurBand.value = data[11]
		trampCurChan.value = data[12]
		trampCurFreq.value = leBytesToInt(data, 13, 2)
		trampCurPitmode.value = data[15]
		trampCurPower.value = leBytesToInt(data, 16, 2)
		trampCurConfigPower.value = leBytesToInt(data, 18, 2)
		trampCurTemp.value = leBytesToInt(data, 20, 2, true) / 100
		noResponseCounter.value = leBytesToInt(data, 22, 2)
	}).catch(_ => {
		trampUpdatedFields.value = 0
	})
}, 200)

onBeforeUnmount(() => {
	clearInterval(fetchInterval)
})
</script>

<template>
	<div class="wrapper">
		<div class="main"></div>
		<div class="status">
			<h2>Status</h2>
			<div class="statusItem">
				<div class="updateDot" :class="{ show: trampUpdatedFields & (1 << 3) }"></div>
				<div class="updateText hover">{{ STATUS_NAMES[trampStatus] }} <span style="display:none;">({{
					trampStatus }})</span></div>
			</div>
			<div class="statusItem">
				<div class="updateDot" :class="{ show: trampUpdatedFields & (1 << 0) }"></div>
				<div class="updateText">Frequency range: {{ trampMinFreq }} - {{ trampMaxFreq }} MHz</div>
			</div>
			<div class="statusItem">
				<div class="updateDot" :class="{ show: trampUpdatedFields & (1 << 0) }"></div>
				<div class="updateText">Maximum Power: {{ trampMaxPwr }} mW</div>
			</div>
			<div class="statusItem">
				<div class="updateDot" :class="{ show: trampUpdatedFields & (1 << 1) }"></div>
				<div class="updateText">Current Frequency: {{ trampCurFreq }} MHz<span v-if="trampCurBand < 5"> ({{
					BAND_NAMES[trampCurBand] }} {{ trampCurChan + 1 }})</span></div>
			</div>
			<div class="statusItem">
				<div class="updateDot" :class="{ show: trampUpdatedFields & (1 << 1) }"></div>
				<div class="updateText">Current Power: {{ trampCurPower }} mW
					<span v-if="trampCurPower !== trampCurConfigPower">(trying {{ trampCurConfigPower }} mW)</span>
				</div>
			</div>
			<div class="statusItem">
				<div class="updateDot" :class="{ show: trampUpdatedFields & (1 << 1) }"></div>
				<div class="updateText">Pit mode: {{ trampCurPitmode }}</div>
			</div>
			<div class="statusItem">
				<div class="updateDot" :class="{ show: trampUpdatedFields & (1 << 2) }"></div>
				<div class="updateText">Temperature: {{ trampCurTemp }} °C</div>
			</div>
		</div>
	</div>
</template>

<style scoped>
.wrapper {
	display: flex;
	gap: 1rem;
}

.main {
	flex-grow: 5;
}

.status {
	flex-grow: 1;
	padding: 1rem;
}

.statusItem {
	display: flex;
	flex-direction: row;
	align-items: center;
}

.updateDot {
	margin: .3rem .6rem .3rem 0rem;
	background-color: var(--accent-blue);
	width: .6rem;
	height: .6rem;
	border-radius: 50%;
	visibility: hidden;
}

.hover:hover span {
	display: inline !important;
}

.show {
	visibility: visible;
}
</style>
