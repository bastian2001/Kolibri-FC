<script setup lang="ts">
import { onConnectHandler, sendCommand, strToArray } from "@/msp/comm";
import { MspFn } from "@/msp/protocol";
import { computed, onBeforeUnmount, onMounted, ref, useTemplateRef } from "vue";
import fonts from "@/utils/fonts";
import { useLogStore } from "@/stores/logStore";
import TextCanvas from "@/components/TextCanvas.vue";
import { delay, intToLeBytes, leBytesToInt } from "@/utils/utils";

const file = fonts.clarity;
const chars = ref([] as Uint8Array[]);
const log = useLogStore();
const charsDone = ref(0);
const dragging = ref('none' as 'none' | 'copy' | 'move');
const draggingIndex = ref(-1);
const draggingNew = ref(-1);
const filter = ref('');
const rows = ref(13);
const cols = ref(30);
const canvasSizeSrc = ref(0)
const showAlreadyPlaced = ref(false);
let exiting = false;

const charCanvases: HTMLCanvasElement[] = [];
const draggerCanvas = useTemplateRef('draggerCanvas');
const dragCanvasCols = ref(0);
const draggingRow = ref(0);
const draggingCol = ref(0);
const dragText = ref('');
let grabbedChar = 0;
const dragTrashHover = ref(false);
const dragHide = ref(true);
const previewImage = useTemplateRef('previewImage');

type OsdElement = {
	name: string,
	def: string,
	options?: { name: string, preview: string }[],
}
const OSD_LIST: OsdElement[] = []
OSD_LIST[0x0000] = { name: 'Battery Pack Voltage', def: '15.3\u0006' };
OSD_LIST[0x0001] = { name: 'Battery Cell Voltage', def: '3.83\u0006' };
OSD_LIST[0x0002] = { name: 'Battery Cell Count', def: '4S' };
OSD_LIST[0x0003] = { name: 'Battery Current', def: '109\u009a' };
OSD_LIST[0x0004] = { name: 'Battery mAh drawn', def: '1001\u0007' };
OSD_LIST[0x0005] = { name: 'Battery Voltage Min', def: '13.3\u0006' };

OSD_LIST[0x0020] = { name: 'GPS Longitude', def: '\u0098 13.413049' };
OSD_LIST[0x0021] = { name: 'GPS Latitude', def: '\u0089 52.526477' };
OSD_LIST[0x0022] = { name: 'GPS Pluscode', def: '9F4MGCG7+H6' };
OSD_LIST[0x0023] = { name: 'Speed', def: '161\u009e' };
OSD_LIST[0x0024] = { name: 'Altitude', def: '\u007f123\u000c' };
OSD_LIST[0x0025] = { name: 'Home Distance', def: '\u0011234\u000c' };
OSD_LIST[0x0026] = { name: 'Home Direction', def: '\u0062' };
OSD_LIST[0x0027] = { name: 'GPS Sattelite Count', def: '\u001e\u001f12' };

OSD_LIST[0x0040] = { name: 'Flight Mode', def: 'ACRO' };
OSD_LIST[0x0041] = { name: 'Rescue Status', def: 'CLIMB' };

OSD_LIST[0x0060] = { name: 'RSSI Value', def: '\u0001-101' };
OSD_LIST[0x0061] = { name: 'Link Quality', def: '\u007b100%' };

OSD_LIST[0x0070] = { name: 'Baro Altitude', def: '\u007f128\u000c' };
OSD_LIST[0x0071] = { name: 'ESC Temp 0', def: 'E\u007a69\u000e' };
OSD_LIST[0x0072] = { name: 'ESC Temp 1', def: 'E\u007a70\u000e' };
OSD_LIST[0x0073] = { name: 'ESC Temp 2', def: 'E\u007a71\u000e' };
OSD_LIST[0x0074] = { name: 'ESC Temp 3', def: 'E\u007a72\u000e' };
OSD_LIST[0x0075] = { name: 'ESC Temp Avg', def: 'E\u007a70\u000e' };
OSD_LIST[0x0076] = { name: 'IMU Acceleration', def: '\u00761.2G' };
OSD_LIST[0x0077] = { name: 'IMU Pitch', def: '\u0015-12.3D' };
OSD_LIST[0x0078] = { name: 'IMU Roll', def: '\u0014-23.4D' };
OSD_LIST[0x0079] = { name: 'IMU Yaw', def: '34.5D' };

OSD_LIST[0x00A0] = { name: 'RC Roll', def: '1310' };
OSD_LIST[0x00A1] = { name: 'RC Pitch', def: '1311' };
OSD_LIST[0x00A2] = { name: 'RC Throttle', def: '1312' };
OSD_LIST[0x00A3] = { name: 'RC Yaw', def: '1313' };

OSD_LIST[0x00B0] = { name: 'Battery Time', def: '\u009b0:00' };
OSD_LIST[0x00B1] = { name: 'Arm Time', def: '\u009c0:00' };

OSD_LIST[0x00C0] = { name: 'Warnings', def: '##LOW VOLTAGE##' };

OSD_LIST[0xFFF0] = { name: 'Debug 1', def: 'DBG 1' };
OSD_LIST[0xFFF1] = { name: 'Debug 2', def: 'DBG 2' };
OSD_LIST[0xFFF2] = { name: 'Debug 3', def: 'DBG 3' };
OSD_LIST[0xFFF3] = { name: 'Debug 4', def: 'DBG 4' };

type OsdPlacement = {
	id: number,
	col: number,
	row: number,
	option: number,
};
const activeElements = ref([
	{ id: 0x0000, col: 3, row: 1 },
	{ id: 0x00C0, col: 8, row: 1 },
	{ id: 0x0060, col: 23, row: 1 },
] as OsdPlacement[])

const filteredList = computed(() =>
	OSD_LIST.filter(
		s => s.name.toLocaleLowerCase().includes(filter.value.toLocaleLowerCase())
	));

const aspectStyle = computed(() => 'aspect-ratio: ' + (canvasSizeSrc.value ? '16/9;' : rows.value === 16 ? '4/3;' : '16/9;'));

function decode() {
	const cs = chars.value;
	cs.length = 0;
	const lines = file.split('\n');
	//each line has 8 0/1s, and those are one byte each. 64 (actually 54) make up a char
	for (let i = 0; i < lines.length - 1; i++) {
		const c = Math.floor(i / 64);
		const byte = i % 64
		const line = lines[i + 1];
		if (!cs[c]) cs[c] = new Uint8Array(64);
		cs[c][byte] = parseInt(line, 2);
	}
}

async function upload() {
	decode();
	const cs = chars.value;
	if (cs.length >= 128) {
		for (let i = 0; i < cs.length; i++) {
			try {
				await sendCommand(MspFn.WRITE_OSD_FONT_CHARACTER, {
					data: [i, ...cs[i].slice(0, 54)],
					verifyFn: (req, res) =>
						req.command === res.command &&
						res.length === 1 &&
						req.data[0] === res.data[0]
				})
				charsDone.value = i + 1;
				if (i + 1 >= cs.length) log.push('Successfully uploaded OSD font');
			} catch (er) {
				log.push(`There was an error uploading your font (${er}), please try again`);
				break;
			}
		}
	} else {
		log.push('Please provide a full file');
	}
}

function createCanvases() {
	decode();
	const FILL_STYLES = ['black', 'transparent', 'white', 'transparent'];
	for (let i = 0; i < chars.value.length; i++) {
		const can = document.createElement('canvas');
		const c = chars.value[i]
		can.width = 48;
		can.height = 72;
		const ctx = can.getContext('2d');
		if (!ctx) continue;
		c.forEach((b, i) => {
			const xStart = (i % 3) * 4;
			const y = Math.floor(i / 3);
			for (let xDiff = 3; xDiff >= 0; xDiff--) {
				ctx.fillStyle = FILL_STYLES[b & 0b11];
				ctx.fillRect((xStart + xDiff) * 4, y * 4, 4, 4);
				b = b >> 2;
			}
		})
		charCanvases[i] = can;
	}
}

function dragStart(index: number, event: DragEvent, type: 'copy' | 'move', gChar = 0, text = '') {
	if (type === 'copy') {
		if (!event.dataTransfer) return;
		event.dataTransfer.setData('text/plain', 'https://www.youtube.com/watch?v=dQw4w9WgXcQ');
		event.dataTransfer.effectAllowed = 'all';

		const canvas = draggerCanvas.value;
		if (!canvas) return;
		const el = OSD_LIST[index]
		let t = text || el.def
		// TODO get option
		if (el.options && !text) t = el.options[0].preview
		canvas.width = 48 * t.length;
		dragCanvasCols.value = t.length;
		dragText.value = t
		canvas.height = 72;
		const ctx = canvas.getContext('2d');
		if (!ctx) return;
		ctx.clearRect(0, 0, canvas.width, canvas.height);
		for (let i = 0; i < t.length; i++) {
			ctx.drawImage(charCanvases[t.charCodeAt(i)], 48 * i, 0);
		}
		if (!previewImage.value) return;
		const box = previewImage.value.getBoundingClientRect();
		const cWidth = box.width / cols.value;
		const cHeight = box.height / rows.value;
		event.dataTransfer.setDragImage(canvas, cWidth * (gChar + 1 / 2), cHeight / 2);
		draggingNew.value = index
	} else {
		dragText.value = text;
		draggingIndex.value = index;
	}

	dragging.value = type;
	grabbedChar = gChar;
	dragHide.value = true;
	dragTrashHover.value = false;
}
function dragEnd(event: DragEvent) {
	console.log('end')
	event.preventDefault();
	dragging.value = 'none'
}
const dragover = (event: DragEvent) => {
	event.preventDefault();
	if (!event.dataTransfer) return;
	event.dataTransfer.dropEffect = dragging.value;

	if (!previewImage.value || (dragging.value !== 'copy' && dragging.value !== 'move')) return;
	const box = previewImage.value.getBoundingClientRect();
	draggingRow.value = Math.floor(event.offsetY / box.height * rows.value);
	draggingCol.value = Math.floor(event.offsetX / box.width * cols.value) - grabbedChar;
	dragHide.value = false;
}
function dropped(event: DragEvent) {
	event.preventDefault();
	if (!previewImage.value || (dragging.value !== 'copy' && dragging.value !== 'move')) return;
	const box = previewImage.value.getBoundingClientRect();
	const row = Math.floor(event.offsetY / box.height * rows.value);
	const col = Math.floor(event.offsetX / box.width * cols.value) - grabbedChar;
	if (dragging.value == 'move') {
		const el = activeElements.value[draggingIndex.value];
		el.col = col;
		el.row = row;
		draggingIndex.value = -1;
	} else {
		activeElements.value.push({ id: draggingNew.value, col, row, option: 0 });
		draggingNew.value = -1;
	}
	pushElements();
}
function dragoverTrash(event: DragEvent) {
	event.preventDefault();
	if (event.dataTransfer) event.dataTransfer.dropEffect = 'move';
	dragTrashHover.value = true;
}
function dropTrash(event: DragEvent) {
	event.preventDefault();
	console.log('drop')
	delete activeElements.value[draggingIndex.value];
	collapse();
	draggingIndex.value = -1;
	dragging.value = 'none';
	pushElements();
}

async function pushElements() {
	const data = [0]; // initial index
	for (let i = 0; i < 60; i++) {
		const el = activeElements.value[i];
		if (!el) continue;
		data.push(...intToLeBytes(el.id, 2));
		data.push(...intToLeBytes(el.col, 1), ...intToLeBytes(el.row, 1));
		data.push(...intToLeBytes(el.option, 4));
	}
	try {
		await sendCommand(MspFn.SET_OSD_ELEMENTS, data);
	} catch { }
}

function save() {
	pushElements()
		.then(() => sendCommand(MspFn.SET_OSD_CONFIG, [canvasSizeSrc.value]))
		.then(() => sendCommand(MspFn.SAVE_SETTINGS))
		.then(() => sendCommand(MspFn.OSD_CONTROL, [3]))
		.then(getConfig)
}

function getConfig() {
	sendCommand(MspFn.GET_OSD_ELEMENTS).then(({ data }) => {
		const len = Math.floor((data.length - 2) / 8);
		activeElements.value.length = 0
		for (let i = 0; i < len; i++) {
			const d = data.slice(2 + i * 8, 10 + i * 8);
			const el: OsdPlacement = {
				id: leBytesToInt(d, 0, 2),
				col: d[2],
				row: d[3],
				option: leBytesToInt(d, 4, 4),
			};
			if (el.id !== 0xFFFF) activeElements.value[i] = el;
		}
		return sendCommand(MspFn.MSP_GET_OSD_CANVAS)
	}).then(({ data }) => {
		cols.value = data[0];
		rows.value = data[1];
		return sendCommand(MspFn.GET_OSD_CONFIG);
	}).then(({ data }) => {
		canvasSizeSrc.value = data[0];
		return sendCommand(MspFn.OSD_CONTROL, [0])
	}).then(() => sendCommand(MspFn.OSD_CONTROL, [2]))
		.catch(() => { })
}

function collapse() {
	let putting = 0;
	for (let scanning = 0; scanning < activeElements.value.length; scanning++) {
		const el = activeElements.value[scanning];
		if (el) {
			activeElements.value[putting++] = activeElements.value[scanning];
		}
	}
	activeElements.value.length = putting;
}

function leave() {
	sendCommand(MspFn.OSD_CONTROL, [1])
		.then(() => sendCommand(MspFn.OSD_CONTROL, [2]))
		.catch(() => { })
}

let dndText = '';
let dndWidth = 0;
let dndHeight = 0;
let dndCol = 0;
let dndRow = 0;
let cursorEnabled = false;
let cursorRow = 0;
let cursorCol = 0;
let updateCursor = true;
async function sendDragNDrop() {
	while (!exiting) {
		await delay(10);
		try {
			const newText = (dragging.value === 'none' || dragHide.value) ? '' : dragText.value;
			const newWidth = newText.length;
			const newHeight = 1;
			const newCol = draggingCol.value;
			const newRow = draggingRow.value;
			if (newText !== dndText || newWidth !== dndWidth || newHeight !== dndHeight || newCol !== dndCol || newRow !== dndRow) {
				dndText = newText;
				dndWidth = newWidth;
				dndHeight = newHeight;
				dndCol = newCol;
				dndRow = newRow;
				const data = [
					5,
					...intToLeBytes(dndCol, 1),
					...intToLeBytes(dndRow, 1),
					dndWidth,
					dndHeight,
					...strToArray(dndText)
				]
				await sendCommand(MspFn.OSD_CONTROL, data)
			}
			if (updateCursor) {
				await sendCommand(MspFn.OSD_CONTROL, [4, cursorEnabled ? cursorCol : 255, cursorEnabled ? cursorRow : 255])
				updateCursor = false;
			}
		} catch {
		}
	}
}

function setCursorPos(event: MouseEvent) {
	if (!previewImage.value) return;
	const box = previewImage.value.getBoundingClientRect();
	const newRow = Math.floor((event.clientY - box.top) / box.height * rows.value);
	const newCol = Math.floor((event.clientX - box.left) / box.width * cols.value);
	if (newRow !== cursorRow || newCol !== cursorCol) {
		cursorEnabled = true;
		updateCursor = true;
		cursorRow = newRow;
		cursorCol = newCol;
	}
}
function enableCursor() { cursorEnabled = true; updateCursor = true; }
function disableCursor() { cursorEnabled = false; updateCursor = true; }

onMounted(() => {
	getConfig();
	createCanvases()
	onConnectHandler(getConfig)
	sendDragNDrop();
})

onBeforeUnmount(leave)
onBeforeUnmount(() => exiting = true)



/**
 * Ideas:
 * - List all available OSD elements on the left (maybe grouped)
 * - Search through them
 * - Drag and drop onto canvas (center/right (no scroll))
 * - (live updates in goggles as you are dragging?)
 * - general settings at the top
 * - detailed settings can be below
 */
</script>
<template>
	<div class="wrapper">
		<div class="osdListWrapper" :class="{ lowOpacity: dragging === 'copy' }">
			<div class="dragTrash" :class="{ red: dragTrashHover }" v-if="dragging === 'move'"
				@dragenter="() => dragTrashHover = true" @dragover="dragoverTrash"
				@dragleave="() => dragTrashHover = false" @drop="dropTrash">
				<p class="trashText">
					<i class="fa-solid fa-trash"></i>
				</p>
			</div>
			<div class="activeElements" v-if="activeElements.filter(el => el).length">
				<h3>Enabled OSD Elements</h3>
				<template v-for="(el, index) in activeElements">
					<div class="activeElement" v-if="OSD_LIST[el.id]">
						<p>{{ OSD_LIST[el.id].name }}</p>
						<select v-model="el.option" v-if="OSD_LIST[el.id].options">
							<option v-for="(_, i) in OSD_LIST[el.id].options" :value="i">{{ i }}</option>
						</select>
						<button class="defaultBtn red small"
							@click="() => { delete activeElements[index]; collapse(); pushElements(); }">
							<i class="fa-solid fa-trash"></i>
						</button>
					</div>
				</template>
			</div>
			<div class="osdList">
				<h3>Available Elements (Drag and Drop)</h3>
				<div class="osdSearch">
					<input type="text" v-model="filter" placeholder="Filter" style="margin-right: 1rem;">
					<input type="checkbox" id="showAlreadyPlaced" v-model="showAlreadyPlaced">&nbsp;
					<label for="showAlreadyPlaced">Show already placed</label>
				</div>
				<div class="listElem"
					v-for="el in filteredList.filter(osdEl => showAlreadyPlaced || activeElements.findIndex(actEl => OSD_LIST[actEl.id] === osdEl) === -1)"
					draggable="true" @dragstart="(event) => { dragStart(OSD_LIST.indexOf(el), event, 'copy') }"
					@dragend="dragEnd">
					<p><i class="fa-solid fa-arrow-pointer"></i>{{ el.name }}</p>
				</div>
			</div>
		</div>
		<div class="line"></div>
		<div class="previewWrapper">
			<div class="preview">
				<div>
					<button @click="save">Save</button>
					<select v-model="canvasSizeSrc">
						<option :value="0">Analog</option>
						<option :value="1">MSP Displayport</option>
					</select>
				</div>
				<div class="previewImage" :style="aspectStyle" @drop="dropped" @dragover="dragover"
					@dragleave="() => dragHide = true" ref="previewImage" @mouseenter="enableCursor"
					@mouseleave="disableCursor" @mousemove="setCursorPos">
					<img src="@assets/DJI_0124.JPG">
					<div class="grid" :style="`display: ${dragging === 'none' ? 'none' : 'block'}`">
						<div class="hline" v-for="i in (rows - 1)" :style="`top: ${100 * i / rows}%`"></div>
						<div class="vline" v-for="i in (cols - 1)" :style="`left: ${100 * i / cols}%`"></div>
					</div>
					<canvas class="draggerCanvas" ref="draggerCanvas"
						:style="`width: ${100 * dragCanvasCols / cols}%; height: ${100 / rows}%;`"></canvas>
					<template v-for="(el, index) in activeElements">
						<TextCanvas :key="index" v-if="OSD_LIST[el.id]"
							:opacity="(index === draggingIndex && dragging !== 'none') ? 0 : 1"
							:text="OSD_LIST[el.id].options ? OSD_LIST[el.id].options![el.option].preview : OSD_LIST[el.id].def"
							:rows="rows" :cols="cols" :row="el.row" :col="el.col" :chars="charCanvases"
							@dragstart="(ev, gc, txt) => { dragStart(index, ev, 'move', gc, txt) }"
							:poiev="dragging !== 'none' ? 'none' : 'initial'" @dragend="dragEnd" />
					</template>
					<TextCanvas v-if="dragging !== 'none' && !dragHide" :opacity="0.5" :rows="rows" :cols="cols"
						:chars="charCanvases" :text="dragText" :row="draggingRow" :col="draggingCol" :poiev="'none'" />
				</div>
			</div>
		</div>
	</div>
	<p>Paste the .mcm file here, using an ASCII character table.</p>
	<textarea id="fontInput" cols="30" rows="10" v-model="file"></textarea>
	<button @click="upload">Upload</button>
	<p>Done: {{ charsDone }} / {{ chars.length }}</p>
</template>

<style scoped>
input,
textarea,
button:not(.defaultBtn),
select {
	color: black;
}

.wrapper {
	display: flex;
	flex-direction: row;
	gap: .8rem;
}

.osdListWrapper {
	margin: 1rem;
	padding: 1rem;
	border-radius: 1rem;
	background-color: var(--background-light);
	border: 3px solid var(--border-color);
	flex-grow: 1;
	position: relative;
	transition: opacity 0.3s ease-out;
}

.lowOpacity {
	opacity: 0.5;
}

.dragTrash {
	position: absolute;
	top: calc(-1rem - 3px);
	left: calc(-1rem - 3px);
	width: calc(100% + 6px);
	height: calc(100% + 6px);
	background-color: var(--background-light);
	margin: 1rem;
	padding: 1rem;
	border-radius: 1rem;
	box-sizing: border-box;
	border: 3px solid var(--border-color);
	transition: all .2s ease-out;
}

.dragTrash.red {
	background-color: rgb(168, 47, 47);
	border-color: #faa;
	/* FAA sucks, but I want this color */
}

.trashText {
	font-size: 4rem;
	text-align: center;
	position: sticky;
	top: 20vh;
	transition: color .2s ease-out;
}

.red .trashText {
	color: #fdd;
}

.activeElements,
.osdList {
	padding: .6rem;
	border-radius: .6rem;
	box-shadow: 1px 1px 5px 0px black;
	background-color: var(--background-highlight);
}

.activeElements {
	margin-bottom: 1rem;
}

.osdSearch {
	margin-bottom: .6rem;
}

.activeElements h3,
.osdList h3 {
	margin: .2rem 0px .4rem 0px;
}

.activeElement,
.listElem {
	display: flex;
	align-items: center;
	gap: 1rem;
	padding: .3rem .3rem .4rem .3rem;
	user-select: none;
}

.listElem {
	padding: .5rem .6rem;
}

.activeElement:not(:last-child),
.listElem:not(:last-child) {
	border-bottom: 1px solid var(--border-color);
}

.activeElement p,
.listElem p {
	flex-grow: 1;
	margin: 0px;
}

.listElem i {
	margin-right: .8rem;
}

.line {
	width: 2px;
	flex-shrink: 0;
	margin: 1rem 0px;
	background-color: var(--border-color);
}

.previewWrapper {
	flex-grow: 6;
}

.preview {
	position: sticky;
	top: 0;
}

.previewImage {
	position: relative;
	overflow: hidden;
	min-width: 500px;
	max-height: 70vh;
	margin: 0px auto;
}

.previewImage img {
	width: 100%;
	height: auto;
	position: absolute;
	top: 50%;
	transform: translateY(-50%);
}

.grid {
	height: 100%;
	width: 100%;
	position: absolute;
}

.hline {
	height: 1px;
	width: 100%;
	background-color: lightgrey;
	position: absolute;
	pointer-events: none;
}

.vline {
	width: 1px;
	height: 100%;
	background-color: lightgrey;
	position: absolute;
	pointer-events: none;
}

.draggerCanvas {
	/* hide it away, but it needs to be in the DOM and on-screen to be used */
	position: absolute;
	bottom: 0px;
	z-index: -1;
	pointer-events: none !important;
}

.osdCanvas {
	width: 100%;
	z-index: 1;
	height: 100%;
}
</style>
