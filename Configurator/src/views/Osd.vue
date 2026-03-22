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
const dragText = ref('')
let grabbedChar = 0;
const previewImage = useTemplateRef('previewImage')

type OsdElement = {
	name: string,
	len: number,
	def: string,
	options?: { [key: string]: string },
}
const OSD_LIST: OsdElement[] = [
	{ name: 'Test 1', len: 10, def: 'option 1', options: { 'option 1': 'WHAT YOU\'RE', 'option 2': 'REFERRING TO AS', 'option 3': 'LINUX', 'option 4': 'IS IN FACT', 'option 5': 'GNU/LINUX' } },
	{ name: 'Lorem', len: 3, def: 'BYEWORLD' },
	{ name: 'Ipsum', len: 16, def: 'ITSAMI' },
	{ name: 'dolor', len: 8, def: 'MAAARIO' },
	{ name: 'sit amet.', len: 2, def: '22.6\u0006' },
	{ name: 'Hwllo.', len: 2, def: 'HELLOWORLD' },
	{ name: 'world', len: 2, def: 'HELLOWORLD' },
	{ name: 'what the fuck', len: 2, def: 'HELLOWORLD' },
	{ name: 'is going on', len: 2, def: 'I use Arch btw', options: { 'I use Arch btw': 'NEIN MANN', 'actually I don\'t': 'ICH WILL NOCH NICHT GEHEN', 'sudo rm -rf --no-preserve-root': 'ICH WILL NOCH EIN', 'Microslop sucks': 'BISSCHEN TANZEN' } },
	{ name: 'Hello', len: 2, def: 'HELLOWORLD' },
	{ name: 'it\'s me', len: 2, def: 'HELLOWORLD' },
	{ name: 'Never gonna', len: 2, def: 'HELLOWORLD' },
	{ name: 'give YOUUUU up <3', len: 2, def: 'HELLOWORLD' },
	{ name: 'Actually', len: 2, def: 'HELLOWORLD' },
	{ name: 'might insert', len: 2, def: 'HELLOWORLD' },
	{ name: 'some useful', len: 2, def: 'HELLOWORLD' },
	{ name: 'stuff here', len: 2, def: 'HELLOWORLD' },
	{ name: 'Like Battery Voltage', len: 2, def: 'HELLOWORLD' },
	{ name: 'You want Current? here!', len: 2, def: 'HELLOWORLD' },
	{ name: 'LQI? nah', len: 2, def: 'HELLOWORLD' },
	{ name: 'badumm tss.', len: 2, def: 'HELLOWORLD' },
]

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
		event.dataTransfer.effectAllowed = type;

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
		dragText.value = text
	}

	dragging.value = type
	grabbedChar = gChar
	draggingId.value = OSD_LIST.indexOf(el)
}
function dragEnd(event: DragEvent) {
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
		<div class="osdListWrapper">
			<div class="activeElements" v-if="activeElements.length">
				<h3>Enabled OSD Elements</h3>
				<div class="activeElement" v-for="el in activeElements">
					<div class="activeLeft">
						<p>{{ OSD_LIST[el.id].name }}</p>
						<div class="options" v-if="OSD_LIST[el.id].options">
							<select v-model="el.option">
								<option v-for="(_, i) in OSD_LIST[el.id].options" :value="i">{{ i }}</option>
							</select>
						</div>
					</div>
					<button class="defaultBtn red small"
						@click="() => { activeElements.splice(activeElements.indexOf(el), 1) }">
						<i class="fa-solid fa-trash"></i>
					</button>
				</div>
			</div>
			<div class="osdSearch">
				<input type="text" v-model="filter" placeholder="Filter">
			</div>
			<div class="osdList">
				<div class="listElem"
					v-for="el in filteredList.filter((_, i) => activeElements.findIndex(el => el.id === i) === -1)"
					draggable="true" @dragstart="(event) => { dragStart(el, event, 'copy') }" @dragend="dragEnd">
					<p><i class="fa-solid fa-arrow-pointer"></i> {{ el.name }}</p>
				</div>
			</div>
			<div class="trash" v-if="dragging === 'move'"></div>
		</div>
		<div class="line"></div>
		<div class="previewWrapper">
			<div class="preview">
				<div class="previewImage" @drop="dropped" @dragover="dragover" ref="previewImage">
					<img src="@assets/DJI_0124.JPG">
					<div class="grid" :style="`display: ${dragging === 'none' ? 'none' : 'block'}`">
						<div class="hline" v-for="i in (rows - 1)" :style="`top: ${100 * i / rows}%`"></div>
						<div class="vline" v-for="i in (cols - 1)" :style="`left: ${100 * i / cols}%`"></div>
					</div>
					<canvas class="draggerCanvas" ref="draggerCanvas"
						:style="`width: ${100 * dragCanvasCols / cols}%; height: ${100 / rows}%;`"></canvas>
					<TextCanvas v-for="el in activeElements"
						:opacity="(el.id === draggingId && dragging !== 'none') ? 0 : 1" :key="el.id"
						:text="OSD_LIST[el.id].options ? OSD_LIST[el.id].options![el.option] : OSD_LIST[el.id].def"
						:rows="rows" :cols="cols" :row="el.posY" :col="el.posX" :chars="charCanvases"
						@dragstart="(event, grabbedChar, text) => { dragStart(OSD_LIST[el.id], event, 'move', grabbedChar, text) }"
						:poiev="dragging !== 'none' ? 'none' : 'initial'" @dragend="dragEnd" />
					<TextCanvas v-if="dragging !== 'none'" :opacity="0.5" :rows="rows" :cols="cols"
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
	border: 3px solid;
	border-color: var(--border-color);
	flex-grow: 1;
}

.activeElements {
	padding: .6rem;
	border-radius: .6rem;
	box-shadow: 1px 1px 5px 0px black;
	background-color: var(--background-highlight);
}

.activeElements h3 {
	margin: .2rem 0px .4rem 0px;
}

.activeElement {
	display: flex;
	align-items: center;
	padding: .3rem .3rem .4rem .3rem;
}

.activeElement:not(:last-child) {
	border-bottom: 1px solid var(--border-color);
}

.activeLeft {
	flex-grow: 1;
}

.activeLeft p {
	margin: 0px;
}

.listElem {
	padding: .6rem;
	border: 2px solid var(--border-color);
	margin: .6rem;
	user-select: none;
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
	background-image: url('/src/assets/DJI_0124.JPG');
	background-position: center;
	background-repeat: no-repeat;
	background-size: cover;
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
