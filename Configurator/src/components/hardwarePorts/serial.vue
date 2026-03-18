<script setup lang="ts">
import { sendCommand } from '@/msp/comm';
import { MspFn } from '@/msp/protocol';
import { SERIAL_TYPE_LUT, usePortStore } from '@/stores/portStore';
import { leBytesToInt } from '@/utils/utils';
import { computed, onBeforeUnmount, onMounted, ref } from 'vue';
import Tooltip from '../Tooltip.vue';

type SerialFunction = "crsf" | "msp" | "gps" | "4way" | "tramp" | "smartaudio" | "esc_telem" | "msp_dp"

const SERIAL_FUNCTIONS: SerialFunction[] = ["crsf", "msp", "gps", "4way", "tramp", "smartaudio", "esc_telem", "msp_dp"]

const SERIAL_FUNCTIONS_LUT: { [key in SerialFunction]: string } = {
	crsf: "CRSF",
	msp: 'MSP',
	gps: 'GPS (Ublox)',
	'4way': "4Way ESC Config",
	tramp: 'IRC Tramp',
	smartaudio: 'TBS SmartAudio',
	esc_telem: 'ESC Telemetry',
	msp_dp: 'MSP DisplayPort',
}

const functions = computed(() => {
	const funcs: SerialFunction[] = []
	SERIAL_FUNCTIONS.forEach((s, i) => {
		if (serial.functions & 1 << i) funcs.push(s)
	})
	return funcs
})
const initialFunctions = computed(() => {
	const funcs: SerialFunction[] = []
	SERIAL_FUNCTIONS.forEach((s, i) => {
		if (serial.initialFunctions & 1 << i) funcs.push(s)
	})
	return funcs
})

const props = defineProps<{
	num: number
}>()

const ports = usePortStore()
const serial = ports.serials[props.num]

// Digital OSD Status
// - canvas size
// - recognized/state

// ELRS Status
const rxFound = ref(false)
const txFound = ref(false)
const lqi = ref(0)
const packetRate = ref(0)


let updateInterval = -1
function update() {
	if (initialFunctions.value.includes('crsf')) {
		sendCommand(MspFn.GET_RX_STATUS).then(c => {
			const d = c.data
			rxFound.value = d[0] > 0
			txFound.value = d[1] > 0
			packetRate.value = leBytesToInt(d, 10, 2)
			lqi.value = d[4]
		})
	}
}

function del() {
	serial.modified = true
	serial.functions = 0
	serial.type = 'disabled'
	serial.exists = false
}

onMounted(() => {
	updateInterval = setInterval(update, 500)
})

onBeforeUnmount(() => {
	clearInterval(updateInterval)
})

const tooltipText = computed(() => {
	switch (serial.type) {
		case 'uart':
			return `Hardware UART\nUses pin ${serial.rxPin} for RX and pin ${serial.txPin} for TX.`
		case 'pio':
			return `PIO-based UART\nUses pin ${serial.rxPin} for RX and pin ${serial.txPin} for TX. Will run on pio${serial.hwParam >> 4} for RX and pio${serial.hwParam & 0xF} for TX.`
		case 'pio-hdx':
			return `Halfduplex UART (PIO-based)\nUses one pin for RX and TX: ${serial.txPin}. Will run on pio${serial.hwParam}.`
		default:
			return ''
	}
})


// // ELRS status
// rxFound: boolean,
// txFound: boolean,
// lqi: number,
// packetRate: number,

// // GPS settings
// gpsProtocol: number,
// gpsFound: boolean,
// gpsInited: boolean,

// // analog VTX status
// analogVtxOnline?: boolean,
// analogVtxFreq?: number,
</script>

<template>
	<div>
		<div class="hardwareIcons">
			<div class="spacer"></div>
			<div class="hardwareIcon crsf" v-if="functions.includes('crsf')"></div>
			<div class="hardwareIcon gps" v-if="functions.includes('gps')"></div>
			<div class="hardwareIcon analogVtx" v-if="functions.includes('tramp') || functions.includes('smartaudio')">
			</div>
			<div class="hardwareIcon escTelem" v-if="functions.includes('esc_telem')"></div>
			<div class="hardwareIcon mspDp" v-if="functions.includes('msp_dp')"></div>
			<div class="deleteAndInfo">
				<button class="deleteBtn" @click="del"><i class="fa-solid fa-trash"></i></button>
				<Tooltip position="bottom-left" :text="tooltipText" style="flex-grow: 1; height: auto;" width="s" />
			</div>
		</div>
		<h3>UART {{ serial.no + 1 }}{{ SERIAL_TYPE_LUT[serial.type] ? ` (${SERIAL_TYPE_LUT[serial.type]})` : '' }}</h3>
		{{functions.map(f => SERIAL_FUNCTIONS_LUT[f]).join(', ')}}
		<input type="number" v-model="serial.functions">
		<input type="number" v-model="serial.baudSet">
	</div>
</template>

<style scoped>
.hardwareIcons {
	display: flex;
	flex-direction: row;
	height: 8rem;
	justify-content: space-between;
}

.hardwareIcon {
	flex-grow: 3;
}

.spacer {
	width: 3rem;
	height: 100%;
}

.crsf {
	-webkit-mask: url(@assets/rc_app.svg) no-repeat center;
	mask: url(@assets/rc_app.svg) no-repeat center;
}

.gps {
	-webkit-mask: url(@assets/gps_app.svg) no-repeat center;
	mask: url(@assets/gps_app.svg) no-repeat center;
}

.analogVtx {
	-webkit-mask: url(@assets/vtx_app.svg) no-repeat center;
	mask: url(@assets/vtx_app.svg) no-repeat center;
}

.deleteAndInfo {
	margin: 0.5rem;
	width: 3rem;
	display: flex;
	flex-direction: column;
	gap: 1rem;
}

.deleteBtn {
	width: 100%;
	box-sizing: border-box;
	flex-grow: 2;
	padding: 0.5rem auto;
	background-color: var(--accent-blue);
	color: white;
	border: none;
	border-radius: 4px;
	cursor: pointer;
	display: inline-block;
	transition: all 0.2s ease-out;
}

.deleteBtn:hover {
	background-color: #5599ff;
}

.deleteBtn:active {
	background-color: #73abff;
}

h3 {
	margin-top: 6px;
	margin-bottom: 0px;
}

input {
	color: black
}
</style>
