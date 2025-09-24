<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { addOnConnectHandler, removeOnConnectHandler, sendCommand } from '@/msp/comm'
import { MspFn } from '@/msp/protocol';
import { delay, intToLeBytes, leBytesToInt } from '@/utils/utils';
import NumericInput from '@/components/NumericInput.vue';

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

const trampUseFreq = ref(false)
const trampConfPower = ref(25)
const trampConfFreq = ref(5658)
const trampConfBand = ref(4)
const trampConfChan = ref(0)

const STATUS_NAMES = ['Offline', 'Initializing', 'Online', 'Online', 'Online', 'Set Frequency', 'Set Frequency', 'Set Power', 'Set Power', 'Set Pitmode', 'Set Pitmode']
const BAND_NAMES = ['Boscam A', 'Boscam B', 'Boscam E', 'Fatshark', 'Raceband']
const VTX58_FREQ_TABLE = [
	[5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725], // Boscam A
	[5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866], // Boscam B
	[5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945], // Boscam E
	[5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880], // FatShark
	[5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917], // RaceBand
]

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

const onConnect = () => {
	sendCommand(MspFn.GET_VTX_CONFIG).then(c => {
		const data = c.data
		trampUseFreq.value = data[0] > 0
		trampConfFreq.value = leBytesToInt(data, 1, 2)
		trampConfPower.value = leBytesToInt(data, 3, 2)
		trampConfBand.value = data[5]
		trampConfChan.value = data[6]
	})
}

onMounted(() => {
	onConnect()
	addOnConnectHandler(onConnect)
})

onBeforeUnmount(() => {
	removeOnConnectHandler(onConnect)
	clearInterval(fetchInterval)
})

const setCmd = computed<number[]>(() => {
	return [
		trampUseFreq.value ? 1 : 0,
		...intToLeBytes(trampConfFreq.value, 2),
		...intToLeBytes(trampConfPower.value, 2),
		trampConfBand.value,
		trampConfChan.value
	]
})

let sendingConfig = false // as mutex to slow down requests
watch(setCmd, async () => {
	if (sendingConfig) return
	sendingConfig = true
	await delay(100)
	sendingConfig = false
	sendCommand(MspFn.SET_VTX_CONFIG, setCmd.value).catch(() => { })
})
</script>

<template>
	<div class="wrapper">
		<div class="main">
			<h2 class="center">Frequency</h2>
			<div class="freqSettings">
				<div class="setBandChan" :class="{ dim: trampUseFreq }" @click="() => { trampUseFreq = false }">
					<h4>Set Band/Channel</h4>
					<table>
						<tr>
							<th></th>
							<th v-for="i in 8">{{ i }}</th>
						</tr>
						<tr v-for="(band, i) of VTX58_FREQ_TABLE">
							<td>{{ BAND_NAMES[i] }}</td>
							<td v-for="(chan, j) of band" class="clickable"
								@click="() => { trampConfBand = i, trampConfChan = j }"
								:class="{ highlight: i === trampConfBand && j === trampConfChan }">{{ chan }}
							</td>
						</tr>
					</table>
				</div>
				<div class="setFreqDirect" :class="{ dim: !trampUseFreq }" @click="() => { trampUseFreq = true }">
					<h4>Set Frequency directly</h4>
					<NumericInput v-model="trampConfFreq" :min="5000" :max="6000" />
				</div>
			</div>
			<div class="powerSettings"></div>
		</div>
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
h2,
h4 {
	margin-top: 0px;
}

.wrapper {
	display: flex;
	gap: 1rem;
}

.main {
	flex-grow: 5;
	padding: 1rem;
}

.center {
	text-align: center;
}

.freqSettings {
	display: flex;
	flex-direction: row;
	justify-content: center;
	gap: 2rem;
}

.freqSettings>div {
	min-height: 100%;
	background-color: var(--background-light);
	padding: 1rem;
	border-radius: 1rem;
	border: 2px solid var(--border-green);
	transition: all 0.2s ease-out;
	transform: translate(-1px, -1px);
	box-shadow: 3px 3px 7px 3px #0008;
}

.dim {
	opacity: 0.5;
	transform: translate(0px, 0px) !important;
	border-color: var(--border-color) !important;
	box-shadow: 0px 0px 7px 3px #0000 !important;
}

th,
td {
	padding: 3px 8px;
	border-radius: 4px;
}

.clickable {
	cursor: pointer;
}

.highlight {
	background-color: var(--accent-blue);
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
