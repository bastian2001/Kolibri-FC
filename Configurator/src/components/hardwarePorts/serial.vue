<script setup lang="ts">
import { SERIAL_TYPE_LUT, usePortStore } from '@/stores/portStore';
import { computed } from 'vue';

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

const props = defineProps<{
	num: number
}>()

const ports = usePortStore()
const serial = ports.serials[props.num]

// // digital OSD status
// canvasWidth?: number,
// canvasHeight?: number,


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
			<div class="hardwareIcon crsf" v-if="functions.includes('crsf')"></div>
			<div class="hardwareIcon gps" v-if="functions.includes('gps')"></div>
			<div class="hardwareIcon analogVtx" v-if="functions.includes('tramp') || functions.includes('smartaudio')">
			</div>
			<div class="hardwareIcon escTelem" v-if="functions.includes('esc_telem')"></div>
			<div class="hardwareIcon mspDp" v-if="functions.includes('msp_dp')"></div>
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
}

.hardwareIcon {
	flex-grow: 1;
}

.crsf {
	-webkit-mask: url(@assets/rc_app.svg) no-repeat center;
	mask: url(@assets/rc_app.svg) no-repeat center;
}

.gps {
	-webkit-mask: url(@assets/gps_app.svg) no-repeat center;
	mask: url(@assets/gps_app.svg) no-repeat center;
}

h3 {
	margin-top: 6px;
	margin-bottom: 0px;
}

input {
	color: black
}
</style>
