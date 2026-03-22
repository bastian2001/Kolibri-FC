<script setup lang="ts">
import { sendCommand } from "@/msp/comm";
import { MspFn } from "@/msp/protocol";
import { computed, onMounted, ref, useTemplateRef } from "vue";
import fonts from "@/utils/fonts";
import { useLogStore } from "@/stores/logStore";

const file = fonts.clarity;
const chars = ref([] as Uint8Array[]);
const log = useLogStore();
const charsDone = ref(0);
const dragging = ref('' as '' | 'new' | 'move');
const filter = ref('');
const rows = ref(13)
const cols = ref(30);

const charCanvases: HTMLCanvasElement[] = []
const osdCanvas = useTemplateRef('osdCanvas')
const draggerCanvas = useTemplateRef('draggerCanvas')

type OsdElement = {
	name: string,
	len: number,
	def: string,
	options?: string[],
}
const OSD_LIST: OsdElement[] = [
	{ name: 'Test 1', len: 10, def: 'HELLOWORLD' },
	{ name: 'Lorem', len: 3, def: 'BYEWORLD' },
	{ name: 'Ipsum', len: 16, def: 'HELLOWORLD' },
	{ name: 'dolor', len: 8, def: 'HELLOWORLD' },
	{ name: 'sit amet.', len: 2, def: 'HELLOWORLD' },
	{ name: 'Hwllo.', len: 2, def: 'HELLOWORLD' },
	{ name: 'world', len: 2, def: 'HELLOWORLD' },
	{ name: 'what the fuck', len: 2, def: 'HELLOWORLD' },
	{ name: 'is going on', len: 2, def: 'HELLOWORLD' },
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

function dragStart(el: OsdElement, event: DragEvent) {
	const canvas = draggerCanvas.value;
	if (!canvas) return;
	canvas.width = 48 * el.def.length;
	canvas.height = 72;
	const ctx = canvas.getContext('2d');
	if (!ctx) return;
	ctx.clearRect(0, 0, canvas.width, canvas.height);
	for (let i = 0; i < el.def.length; i++) {
		ctx.drawImage(charCanvases[el.def.charCodeAt(i)], 48 * i, 0);
	}
	if (!event.dataTransfer) return;
	event.dataTransfer.setData('text/plain', 'dummy');
	event.dataTransfer.setDragImage(canvas, 24, 36);
	event.dataTransfer.effectAllowed = 'copy'
}
function dragEnd(event: DragEvent) {
	event.preventDefault();
	const ctx = osdCanvas.value?.getContext('2d')
	if (!ctx || !draggerCanvas.value) return;
}
const dragover = (event: DragEvent) => {
	console.log('dragover')
	event.preventDefault();
	if (!event.dataTransfer) return;
	event.dataTransfer.dropEffect = 'copy'
	console.log(event.pageX, event.x, event.layerX, event.clientX, event.offsetX, event.screenX, event.movementX)
}
function dropped(event: DragEvent) {
	event.preventDefault();
	if (!osdCanvas.value || !draggerCanvas.value) return;
	const box = osdCanvas.value.getBoundingClientRect();
	const ctx = osdCanvas.value.getContext('2d');
	if (!ctx) return;
	ctx.drawImage(draggerCanvas.value, event.offsetX * 1600 / box.width, event.offsetY * 900 / box.height);
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
			<div class="osdSearch">
				<input type="text" v-model="filter" placeholder="Filter">
			</div>
			<div class="osdList" v-if="dragging !== 'move'">
				<div class="listElem" v-for="el in filteredList" draggable="true"
					@dragstart="(event) => { dragStart(el, event) }" @dragend="dragEnd">
					<p><i class="fa-solid fa-arrow-pointer"></i> {{ el.name }}</p>
				</div>
			</div>
			<div class="trash" v-else></div>
		</div>
		<div class="line"></div>
		<div class="previewWrapper">
			<div class="preview">
				<div class="previewImage" @drop="dropped" @dragover="dragover">
					<div class="grid" :style="`display: ${dragging === '' ? 'none' : 'block'}`">
						<div class="hline" v-for="i in (rows - 1)" :style="`top: ${100 * i / rows}%`"></div>
						<div class="vline" v-for="i in (cols - 1)" :style="`left: ${100 * i / cols}%`"></div>
					</div>
					<canvas class="draggerCanvas" ref="draggerCanvas"></canvas>
					<canvas ref="osdCanvas" class="osdCanvas" width="1600" height="900"></canvas>
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
input {
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
	min-width: 500px;
	max-height: 70vh;
	background-image: url('/src/assets/DJI_0124.JPG');
	background-position: center;
	background-repeat: no-repeat;
	background-size: cover;
	margin: 0px auto;
}

.grid {
	height: 100%;
	width: 100%;
	position: absolute;
}

.hline {
	height: 1px;
	width: 100%;
	background-color: grey;
	position: absolute;
}

.vline {
	width: 1px;
	height: 100%;
	background-color: grey;
	position: absolute;
}

.draggerCanvas {
	/* hide it away, but it needs to be in the DOM and technically on-screen to be used */
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
