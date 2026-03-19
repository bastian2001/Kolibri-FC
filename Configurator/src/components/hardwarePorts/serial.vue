<script setup lang="ts">
import { onConnectHandler, onDisconnectHandler, sendCommand } from '@/msp/comm';
import { MspFn } from '@/msp/protocol';
import { SERIAL_TYPE_LUT, usePortStore } from '@/stores/portStore';
import { constrain, leBytesToInt } from '@/utils/utils';
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue';
import Tooltip from '../Tooltip.vue';
import Drone3dPreview from '../Drone3dPreview.vue';
import NumericInput from '../NumericInput.vue';
import Leaflet from '../Leaflet.vue';

type SerialFunction = "crsf" | "msp" | "gps" | "4way" | "tramp" | "smartaudio" | "esc_telem" | "msp_dp"

const SERIAL_FUNCTIONS: SerialFunction[] = ["crsf", "msp", "gps", "4way", "tramp", "smartaudio", "esc_telem", "msp_dp"]
const SELECTABLE_FUNCTIONS: SerialFunction[] = ['crsf', 'gps', 'msp_dp', 'tramp', 'smartaudio', 'esc_telem']

const SERIAL_FUNCTIONS_LUT: { [key in SerialFunction]: string } = {
	crsf: "CRSF",
	msp: 'MSP',
	gps: 'GPS (UBX)',
	'4way': "4Way ESC Config",
	tramp: 'IRC Tramp',
	smartaudio: 'TBS SmartAudio',
	esc_telem: 'ESC Telemetry',
	msp_dp: 'MSP DisplayPort',
}

const functionToAdd = ref(0)

const functions = computed(() => {
	const funcs: SerialFunction[] = []
	SERIAL_FUNCTIONS.forEach((s, i) => {
		if ((serial?.value?.functions || 0) & 1 << i) funcs.push(s)
	})
	return funcs
})
const initialFunctions = computed(() => {
	const funcs: SerialFunction[] = []
	SERIAL_FUNCTIONS.forEach((s, i) => {
		if ((serial?.value?.initialFunctions || 0) & 1 << i) funcs.push(s)
	})
	return funcs
})

const props = defineProps<{
	num: number
}>()

const $emit = defineEmits<{
	update: []
}>()

const ports = usePortStore()
const serial = computed(() => ports.serials[props.num])

// Digital OSD Status
// - canvas size
// - recognized/state

// ELRS Status
const rxFound = ref(false)
const txFound = ref(false)
const lqi = ref(0)
const packetRate = ref(0)
const attiQuat = ref([1, 0, 0, 0])
function quatMultiply(q1: number[], q2: number[]) {
	const [w1, x1, y1, z1] = q1
	const [w2, x2, y2, z2] = q2

	const w = w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2
	const x = w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2
	const y = w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2
	const z = w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2

	return [w, x, y, z]
}
const crsfColor = ref('white')
const rpy = computed(() => {
	const res = {
		roll: 0,
		pitch: 0,
		yaw: 0,
	}
	const q = {
		w: attiQuat.value[0],
		v: attiQuat.value.slice(1)
	}
	res.roll = Math.atan2(2 * (q.w * q.v[0] + q.v[1] * q.v[2]), 1 - 2 * (q.v[0] * q.v[0] + q.v[1] * q.v[1]));
	res.pitch = Math.asin(constrain(2 * (q.w * q.v[1] - q.v[2] * q.v[0]), -1, 1));
	res.yaw = Math.atan2(2 * (q.w * q.v[2] + q.v[0] * q.v[1]), 1 - 2 * (q.v[1] * q.v[1] + q.v[2] * q.v[2]));
	return res
})


const gpsColor = ref('white')
const gpsFound = ref(false)
const gpsFix = ref(0)
const gpsSats = ref(0)
const coords = ref([0, 0, 0])
const FIX_TYPES = ['None', 'Dead Reckoning', '2D', '3D', 'GPS Dead Reckoning', 'Time only']

let updateInterval = -1
function update() {
	if (initialFunctions.value.includes('crsf')) {
		sendCommand(MspFn.GET_RX_STATUS).then(c => {
			const d = c.data
			rxFound.value = d[0] > 0
			txFound.value = d[1] > 0
			if (!rxFound.value) crsfColor.value = 'red'
			else if (txFound.value) crsfColor.value = 'green'
			else crsfColor.value = 'yellow'
			packetRate.value = leBytesToInt(d, 10, 2)
			lqi.value = d[4]
			return sendCommand(MspFn.RC)
		}).then(c => {
			const d = c.data
			if (!txFound.value) return
			const diffQuat = [
				1,
				Math.pow((leBytesToInt(d, 0, 2) - 1500) / 512, 3) / 30,
				-Math.pow((leBytesToInt(d, 2, 2) - 1500) / 512, 3) / 30,
				Math.pow((leBytesToInt(d, 6, 2) - 1500) / 512, 3) / 30,
			]
			const div = Math.sqrt(diffQuat.reduce((prev, curr) => prev + curr * curr, 0))
			diffQuat.forEach((_, i) => { diffQuat[i] /= div })
			for (let i = 0; i < 10; i++) {
				setTimeout(() => {
					attiQuat.value = quatMultiply(attiQuat.value, diffQuat)
				}, i * 25)
			}
		}).catch(() => {
			crsfColor.value = 'red'
		})
	}
	if (initialFunctions.value.includes('gps')) {
		sendCommand(MspFn.GET_GPS_STATUS).then(c => {
			const d = c.data
			gpsFound.value = d[1] >= 2
			gpsFix.value = d[2]
			gpsSats.value = d[8]
			if (!gpsFound.value) gpsColor.value = 'red'
			else if (gpsFix.value >= 3) gpsColor.value = 'green'
			else gpsColor.value = 'yellow'
			return sendCommand(MspFn.GET_GPS_MOTION)
		}).then(c => {
			const d = c.data
			coords.value[0] = leBytesToInt(d, 0, 4, true) * 1e-7
			coords.value[1] = leBytesToInt(d, 4, 4, true) * 1e-7
			coords.value[2] = leBytesToInt(c.data, 8, 4, true) * 1e-3
		}).catch(() => {
			gpsColor.value = 'red'
		})
	}
}

function del() {
	serial.value.modified = true
	serial.value.functions = 0
	serial.value.initialFunctions = 0
	serial.value.type = 'disabled'
	serial.value.exists = false
}

onMounted(() => {
	updateInterval = setInterval(update, 250)
	onDisconnectHandler(() => { clearInterval(updateInterval) })
	onConnectHandler(() => {
		clearInterval(updateInterval)
		updateInterval = setInterval(update, 250)
		update()
	})
	update()
})

onBeforeUnmount(() => {
	clearInterval(updateInterval)
})

const tooltipText = computed(() => {
	switch (serial.value.type) {
		case 'uart':
			return `Hardware UART\nUses pin ${serial.value.rxPin} for RX and pin ${serial.value.txPin} for TX. Currently on baud rate ${serial.value.baud}.`
		case 'pio':
			return `PIO-based UART\nUses pin ${serial.value.rxPin} for RX and pin ${serial.value.txPin} for TX. Will run on pio${serial.value.hwParam >> 4} for RX and pio${serial.value.hwParam & 0xF} for TX. Currently on baud rate ${serial.value.baud}.`
		case 'pio-hdx':
			return `Halfduplex UART (PIO-based)\nUses one pin for RX and TX: ${serial.value.txPin}. Will run on pio${serial.value.hwParam}. Currently on baud rate ${serial.value.baud}.`
		default:
			return ''
	}
})

const BAUD_OPTIONS = [4800, 9600, 19200, 38400, 57600, 115200, 230400, 250000, 420000, 460800, 500000, 921600, 1000000]
const customBaud = ref(0)
const baudSelection = ref(0)
watch(serial, () => {
	customBaud.value = serial.value.baud
	const newBaudSel = [...BAUD_OPTIONS, 0].includes(serial.value.baudSet) ? serial.value.baudSet : -1
	if (baudSelection.value !== newBaudSel) baudSelection.value = newBaudSel
	crsfColor.value = 'white'
}, {
	deep: false
})
watch([baudSelection, customBaud], () => {
	if (baudSelection.value < 0) serial.value.baudSet = customBaud.value
	else serial.value.baudSet = baudSelection.value
})

function addFunction() {
	serial.value.functions |= (1 << functionToAdd.value) >> 1
}

// // analog VTX status
// analogVtxOnline?: boolean,
// analogVtxFreq?: number,
</script>

<template>
	<div class="serialWrapper">
		<div class="header">
			<div class="hardwareIcons">
				<div class="spacer"></div>
				<div class="hardwareIcon crsf" :class="crsfColor" v-if="functions.includes('crsf')"></div>
				<div class="hardwareIcon gps" :class="gpsColor" v-if="functions.includes('gps')"></div>
				<div class="hardwareIcon analogVtx"
					v-if="functions.includes('tramp') || functions.includes('smartaudio')">
				</div>
				<div class="hardwareIcon escTelem" v-if="functions.includes('esc_telem')"></div>
				<div class="hardwareIcon mspDp" v-if="functions.includes('msp_dp')"></div>
				<div class="deleteAndInfo">
					<button class="deleteBtn" @click="del"><i class="fa-solid fa-trash"></i></button>
					<Tooltip position="bottom-left" :text="tooltipText" style="flex-grow: 1; height: auto;" width="s" />
				</div>
			</div>
			<h3>UART {{ serial.no + 1 }}{{ SERIAL_TYPE_LUT[serial.type] ? ` (${SERIAL_TYPE_LUT[serial.type]})` : '' }}
				<span class="baud">– Baud:
					<select name="baudSelect" v-model="baudSelection">
						<option value="-1">Custom</option>
						<option value="0">Auto</option>
						<option v-for="b in BAUD_OPTIONS" :value="b">{{ b }}</option>
					</select>
					<NumericInput v-if="baudSelection == -1" type="number" v-model="customBaud"
						style="display:inline-block; width: 7rem;" :disable-drag-scroll-arrow="true" />
				</span>
			</h3>
		</div>
		<div class="line"></div>
		<div class="settings" v-if="initialFunctions.length !== 0">
			<div class="crsfOptions" v-if="functions.includes('crsf') && initialFunctions.includes('crsf')">
				<p>{{ rxFound ? ('RX found' + (txFound ? ' and connected to TX' : ', but not connected to TX'))
					: 'No RX found' }}</p>
				<Drone3dPreview :hide-base="true" :roll="rpy.roll * 180 / Math.PI" :pitch="rpy.pitch * 180 / Math.PI"
					:yaw="rpy.yaw * 180 / Math.PI" :size="250" @click="attiQuat = [1, 0, 0, 0]" />
			</div>
			<div class="gpsOptions" v-if="functions.includes('gps') && initialFunctions.includes('gps')">
				<p>GPS module{{ gpsFound ? ' ' : ' not ' }}found</p>
				<p>Fix: {{ FIX_TYPES[gpsFix] }}
					<template v-if="gpsFix >= 3">✅</template>
					<template v-else>❌</template> – {{ gpsSats }} Sats<br>
					Lat: {{ coords[0]?.toFixed(5) }}° – Lon: {{ coords[1]?.toFixed(5) }}° – Alt: {{
						coords[2]?.toFixed(1) }}m
				</p>
				<div class="map">
					<Leaflet :lat="coords[0]" :lng="coords[1]" :zoom="17" />
				</div>
			</div>
		</div>
		<div v-else-if="functions.length === 0">
			<select v-model="functionToAdd">
				<option :value="0" disabled="true">Select</option>
				<option v-for="s in SELECTABLE_FUNCTIONS" :value="SERIAL_FUNCTIONS.indexOf(s) + 1">{{
					SERIAL_FUNCTIONS_LUT[s] }}
				</option>
			</select>
			<button :disabled="functionToAdd === 0" @click="addFunction">
				Set Function
			</button>
		</div>
		<div v-else>
			<h3>{{functions.map(f => SERIAL_FUNCTIONS_LUT[f]).join(', ')}}</h3><br>
			<button class="updateBtn" @click="() => { $emit('update') }">Update</button>
		</div>
	</div>
</template>

<style scoped>
select,
button {
	color: black;
}

.serialWrapper {
	padding: 0px !important;
	display: flex;
	flex-direction: column;
	gap: .8rem;
}

.header {
	padding: 0.6rem 0.3rem 0 0.3rem;
}

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

.line {
	margin: 0px 2rem;
	height: 2px;
	background-color: #dcc;
}

.settings {
	flex-grow: 1;
	padding: 0px 0.6rem 0.6rem 0.6rem;
	display: flex;
	flex-direction: column;
}

.settings p {
	margin: 0;
}

.baud {
	font-size: 1rem;
	font-weight: normal;
}

input {
	color: black
}

.gpsOptions {
	flex-grow: 1;
	display: flex;
	flex-direction: column;
	gap: 0.5rem;
}

.map {
	height: 150px;
	flex-grow: 1;
}

.updateBtn {
	background-color: transparent;
	border: 1px solid var(--border-green);
	border-radius: 5px;
	padding: 0.5rem 1rem;
	font-size: 1rem;
	color: var(--text-color);
	transition: background-color 0.2s ease-out;
}

.updateBtn:hover {
	background-color: #fff1;
}
</style>
