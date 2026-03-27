<script setup lang="ts">
import { sendCommand } from "@/msp/comm";
import { MspFn } from "@/msp/protocol";
import { computed, onMounted, ref, useTemplateRef } from "vue";
import fonts from "@/utils/fonts";
import { useLogStore } from "@/stores/logStore";
import TextCanvas from "@/components/TextCanvas.vue";

const file = fonts.clarity;
const chars = ref([] as Uint8Array[]);
const log = useLogStore();
const charsDone = ref(0);
const dragging = ref('none' as 'none' | 'copy' | 'move');
const draggingId = ref(0);
const filter = ref('');
const rows = ref(13);
const cols = ref(30);

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
	len: number,
	def: string,
	options?: { [key: string]: string },
}
const OSD_LIST: OsdElement[] = []
OSD_LIST[0x0000] = { name: 'Battery Pack Voltage', len: 5, def: '15.3\u0006' };
OSD_LIST[0x0001] = { name: 'Battery Cell Voltage', len: 5, def: '3.83\u0006' };
OSD_LIST[0x0002] = { name: 'Battery Cell Count', len: 2, def: '4S' };
// OSD_LIST[0x0003] = { name: 'Battery Current', }

type OsdPlacement = {
	id: number,
	posX: number,
	posY: number,
	option: string,
};
const activeElements = ref([
	{ id: 1, posX: 10, posY: 3 },
	{ id: 2, posX: 11, posY: 4 },
	{ id: 3, posX: 3, posY: 5 },
	{ id: 4, posX: 0, posY: 3 },
	{ id: 5, posX: 10, posY: 2 },
] as OsdPlacement[])

const filteredList = computed(() => {
	return OSD_LIST.filter(s => s.name.toLocaleLowerCase().includes(filter.value.toLocaleLowerCase()));
})

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

function dragStart(el: OsdElement, event: DragEvent, type: 'copy' | 'move', gChar = 0, text = '') {
	if (type === 'copy') {
		if (!event.dataTransfer) return;
		event.dataTransfer.setData('text/plain', 'https://www.youtube.com/watch?v=dQw4w9WgXcQ');
		event.dataTransfer.effectAllowed = 'all';

		const canvas = draggerCanvas.value;
		if (!canvas) return;
		let t = text || el.def
		if (el.options && !text) t = el.options[el.def]
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
	} else {
		dragText.value = text;
	}

	dragging.value = type;
	grabbedChar = gChar;
	dragHide.value = true;
	dragTrashHover.value = false;
	draggingId.value = OSD_LIST.indexOf(el);
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

	if (!previewImage.value || draggingId.value === -1 || (dragging.value !== 'copy' && dragging.value !== 'move')) return;
	const box = previewImage.value.getBoundingClientRect();
	draggingRow.value = Math.floor(event.offsetY / box.height * rows.value);
	draggingCol.value = Math.floor(event.offsetX / box.width * cols.value) - grabbedChar;
	dragHide.value = false;
}
function dropped(event: DragEvent) {
	event.preventDefault();
	if (!previewImage.value || draggingId.value === -1 || (dragging.value !== 'copy' && dragging.value !== 'move')) return;
	const box = previewImage.value.getBoundingClientRect();
	const row = Math.floor(event.offsetY / box.height * rows.value);
	const col = Math.floor(event.offsetX / box.width * cols.value) - grabbedChar;
	const el = activeElements.value.find(ae => ae.id === draggingId.value);
	const osdEl = OSD_LIST[draggingId.value]
	if (el) {
		el.posX = col;
		el.posY = row;
	} else {
		activeElements.value.push({ id: draggingId.value, posX: col, posY: row, option: osdEl.def });
	}
}
function dragoverTrash(event: DragEvent) {
	event.preventDefault();
	if (event.dataTransfer) event.dataTransfer.dropEffect = 'move';
	dragTrashHover.value = true;
}
function dropTrash(event: DragEvent) {
	event.preventDefault();
	const el = activeElements.value.findIndex(ae => ae.id === draggingId.value);
	activeElements.value.splice(el, 1);
	dragging.value = 'none'
}

onMounted(() => {
	createCanvases()
})



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
			<div class="activeElements" v-if="activeElements.length">
				<h3>Enabled OSD Elements</h3>
				<div class="activeElement" v-for="el in activeElements">
					<p>{{ OSD_LIST[el.id].name }}</p>
					<select v-model="el.option" v-if="OSD_LIST[el.id].options">
						<option v-for="(_, i) in OSD_LIST[el.id].options" :value="i">{{ i }}</option>
					</select>
					<button class="defaultBtn red small"
						@click="() => { activeElements.splice(activeElements.indexOf(el), 1) }">
						<i class="fa-solid fa-trash"></i>
					</button>
				</div>
			</div>
			<div class="osdList">
				<h3>Available Elements (Drag and Drop)</h3>
				<div class="osdSearch">
					<input type="text" v-model="filter" placeholder="Filter">
				</div>
				<div class="listElem"
					v-for="el in filteredList.filter((_, i) => activeElements.findIndex(el => el.id === i) === -1)"
					draggable="true" @dragstart="(event) => { dragStart(el, event, 'copy') }" @dragend="dragEnd">
					<p><i class="fa-solid fa-arrow-pointer"></i>{{ el.name }}</p>
				</div>
			</div>
		</div>
		<div class="line"></div>
		<div class="previewWrapper">
			<div class="preview">
				<div class="previewImage" @drop="dropped" @dragover="dragover" @dragleave="() => dragHide = true"
					ref="previewImage">
					<img src="@assets/DJI_0124.JPG">
					<div class="grid" :style="`display: ${dragging === 'none' ? 'none' : 'block'}`">
						<div class="hline" v-for="i in (rows - 1)" :style="`top: ${100 * i / rows}%`"></div>
						<div class="vline" v-for="i in (cols - 1)" :style="`left: ${100 * i / cols}%`"></div>
					</div>
					<canvas class="draggerCanvas" ref="draggerCanvas"
						:style="`width: ${100 * dragCanvasCols / cols}%; height: ${100 / rows}%;`"></canvas>
					<TextCanvas v-for="el in activeElements" :key="el.id"
						:opacity="(el.id === draggingId && dragging !== 'none') ? 0 : 1"
						:text="OSD_LIST[el.id].options ? OSD_LIST[el.id].options![el.option] : OSD_LIST[el.id].def"
						:rows="rows" :cols="cols" :row="el.posY" :col="el.posX" :chars="charCanvases"
						@dragstart="(ev, gc, txt) => { dragStart(OSD_LIST[el.id], ev, 'move', gc, txt) }"
						:poiev="dragging !== 'none' ? 'none' : 'initial'" @dragend="dragEnd" />
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
	aspect-ratio: 16/9;
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
