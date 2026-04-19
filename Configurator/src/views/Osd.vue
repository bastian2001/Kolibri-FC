<script setup lang="ts">
import { onCommandHandler, onConnectHandler, sendCommand, strToArray } from "@/msp/comm";
import { MspFn } from "@/msp/protocol";
import { computed, onBeforeUnmount, onMounted, ref, useTemplateRef, watch } from "vue";
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
const hoverIndex = ref(-1);

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
let previewScreen = new Uint8Array(rows.value * cols.value);
const previewSnippets = ref<{ text: string, x: number, y: number }[]>([])
let gotHeartbeat = false;
let heartbeatInterval = -1;
const showRealPreview = ref(false);

watch([rows, cols], ([newRows, newCols]) => {
	previewScreen = new Uint8Array(newRows * newCols);
})


type OsdElement = {
	name: string,
	def: string,
	group: string,
	previewFn?: (element: OsdPlacement) => string,
	options?: { name: string, preview?: string, id?: number }[][], // Option Byte 0
}
type OsdGroup = {
	name: string,
	short: string,
	expanded: boolean,
	hidden?: boolean
}
const groups = ref<OsdGroup[]>([
	{ name: 'Battery', short: 'bat', expanded: true },
	{ name: 'GPS', short: 'gps', expanded: true },
	{ name: 'Flight', short: 'flight', expanded: true },
	{ name: 'RC', short: 'rc', expanded: true },
	{ name: 'Sensors', short: 'sensor', expanded: true },
	{ name: 'Timers', short: 'timer', expanded: true },
	{ name: 'Other', short: 'other', expanded: true },
	{ name: 'Debug', short: 'debug', expanded: true, hidden: true }
]);
const OSD_LIST: OsdElement[] = []
OSD_LIST[0x0000] = { name: 'Battery Pack Voltage', def: '15.3\u0006', group: 'bat' };
OSD_LIST[0x0001] = { name: 'Battery Cell Voltage', def: '3.83\u0006', group: 'bat' };
OSD_LIST[0x0002] = { name: 'Battery Cell Count', def: '4S', group: 'bat' };
OSD_LIST[0x0003] = { name: 'Battery Current', def: '109\u009a', group: 'bat' };
OSD_LIST[0x0004] = { name: 'Battery mAh drawn', def: '1001\u0007', group: 'bat' };
OSD_LIST[0x0005] = { name: 'Battery Voltage Min', def: '13.3\u0006', group: 'bat' };

OSD_LIST[0x0020] = { name: 'GPS Longitude', def: '\u0098 13.413049', group: 'gps' };
OSD_LIST[0x0021] = { name: 'GPS Latitude', def: '\u0089 52.526477', group: 'gps' };
OSD_LIST[0x0022] = { name: 'GPS Pluscode', def: '9F4MGCG7+H6', group: 'gps' };
OSD_LIST[0x0023] = { name: 'Speed', def: '161\u009e', group: 'gps' };
OSD_LIST[0x0024] = { name: 'Altitude', def: '\u007f123\u000c', group: 'gps' };
OSD_LIST[0x0025] = { name: 'Home Distance', def: '\u0011234\u000c', group: 'gps' };
OSD_LIST[0x0026] = { name: 'Home Direction', def: '\u0062', group: 'gps' };
OSD_LIST[0x0027] = { name: 'GPS Satellite Count', def: '\u001e\u001f12', group: 'gps' };

OSD_LIST[0x0040] = { name: 'Flight Mode', def: 'ACRO', group: 'flight' };
OSD_LIST[0x0041] = { name: 'Rescue Status', def: 'CLIMB', group: 'flight' };

OSD_LIST[0x0060] = { name: 'RSSI Value', def: '\u0001-101', group: 'rc' };
OSD_LIST[0x0061] = { name: 'Link Quality', def: '\u007b100%', group: 'rc' };
OSD_LIST[0x0062] = {
	name: 'RC Channel Value', def: 'ROL:1312', previewFn: rcChannelTextPreview, options: [[
		{ name: "Roll" }, { name: "Pitch" }, { name: "Throttle" }, { name: "Yaw" }, { name: "Aux 1" }, { name: "Aux 2" }, { name: "Aux 3" }, { name: "Aux 4" }, { name: "Aux 5" }, { name: "Aux 6" }, { name: "Aux 7" }, { name: "Aux 8" }, { name: "Aux 9" }, { name: "Aux 10" }, { name: "Aux 11" }, { name: "Aux 12" }
	], [
		{ name: "Without Label" }, { name: "With Label" }
	]],
	group: 'rc'
};

OSD_LIST[0x0080] = { name: 'Baro Altitude', def: '\u007f128\u000c', group: 'sensor' };
OSD_LIST[0x0081] = {
	name: 'ESC Temperature', def: 'E\u007a69\u000e', options: [[
		{ name: "Maximum + Index", preview: 'E\u007a72\u000e@4' },
		{ name: "Maximum", preview: 'E\u007a72\u000e' },
		{ name: "Average", preview: 'E\u007a71\u000e' },
		{ name: "ESC 1", preview: 'E\u007a69\u000e' },
		{ name: "ESC 2", preview: 'E\u007a70\u000e' },
		{ name: "ESC 3", preview: 'E\u007a71\u000e' },
		{ name: "ESC 4", preview: 'E\u007a72\u000e' }
	]], group: 'sensor'
};
OSD_LIST[0x0082] = { name: 'IMU Acceleration', def: '1.2G', group: 'sensor' };
OSD_LIST[0x0083] = { name: 'IMU Pitch', def: '\u0015-12.3\u0008', group: 'sensor' };
OSD_LIST[0x0084] = { name: 'IMU Roll', def: '\u0014-23.4\u0008', group: 'sensor' };
OSD_LIST[0x0085] = { name: 'IMU Yaw', def: '34.5\u0008', group: 'sensor' };

OSD_LIST[0x00B0] = { name: 'Battery Time', def: '\u009b0:00', group: 'timer' };
OSD_LIST[0x00B1] = { name: 'Arm Time', def: '\u009c0:00', group: 'timer' };

OSD_LIST[0x00C0] = { name: 'Warnings', def: '##LOW VOLTAGE##', group: 'other' };
OSD_LIST[0x00C1] = { name: 'Custom Text', def: 'ABCD', group: 'other', previewFn: customTextPreview };

OSD_LIST[0xFFF0] = { name: 'Debug 1', def: 'DBG 1', group: 'debug' };
OSD_LIST[0xFFF1] = { name: 'Debug 2', def: 'DBG 2', group: 'debug' };
OSD_LIST[0xFFF2] = { name: 'Debug 3', def: 'DBG 3', group: 'debug' };
OSD_LIST[0xFFF3] = { name: 'Debug 4', def: 'DBG 4', group: 'debug' };

function rcChannelTextPreview(el: OsdPlacement) {
	console.log(el)
	console.trace()
	const channel = el.option[0] || 0;
	const withLabel = (el.option[1] || 0) === 1;
	const name = ['ROL', 'PIT', 'THR', 'YAW', 'A 1', 'A 2', 'A 3', 'A 4', 'A 5', 'A 6', 'A 7', 'A 8', 'A 9', 'A10', 'A11', 'A12'][channel] || `CH${channel + 1}`
	return (withLabel ? (name + ':') : '') + ([1312, 1313, 1314, 1315][channel] || 1000);
}
function customTextPreview(el: OsdPlacement) {
	return String.fromCharCode(...el.option);
}

function setDefaultOptions(el: OsdPlacement) {
	switch (el.id) {
		case 0x00C1:
			el.option[0] = 0x41;
			el.option[1] = 0x42;
			el.option[2] = 0x43;
			el.option[3] = 0x44;
			break;
	}
}

function getPreviewText(el: OsdPlacement) {
	const osdElement = OSD_LIST[el.id];
	if (!osdElement) return 'Unknown Element';
	const def = OSD_LIST[el.id].def;
	if (osdElement.previewFn) return osdElement.previewFn(el);
	for (let i = 3; i >= 0; i--) {
		const options = osdElement.options?.[i];
		if (options && options[el.option[i]]) {
			return options[el.option[i]].preview || def;
		}
	}
	return def;
}

type OsdPlacement = {
	id: number,
	col: number,
	row: number,
	option: number[],
};
const activeElements = ref<OsdPlacement[]>([
	{ id: 0x0000, col: 3, row: 1, option: [0, 0, 0, 0] },
	{ id: 0x00C0, col: 8, row: 1, option: [0, 0, 0, 0] },
	{ id: 0x0060, col: 23, row: 1, option: [0, 0, 0, 0] },
])

const filteredList = computed(() =>
	OSD_LIST.filter(
		s => s.name.toLocaleLowerCase().includes(filter.value.toLocaleLowerCase())
	));

const aspectStyle = computed(() => {
	let aspect = '';
	if (canvasSizeSrc.value === 3 || canvasSizeSrc.value === 2) aspect = '16/9';
	else if (canvasSizeSrc.value === 1) aspect = '4/3';
	else {
		if (rows.value === 16) aspect = '4/3';
		else aspect = '16/9';
	}
	return 'aspect-ratio: ' + aspect + ';';
});

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
		const newElement = { id: index, col: 0, row: 0, option: [0, 0, 0, 0] };
		setDefaultOptions(newElement);
		let t = text || getPreviewText(newElement);
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
		const newElement = { id: draggingNew.value, col, row, option: [0, 0, 0, 0] };
		setDefaultOptions(newElement);
		activeElements.value.push(newElement);
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
		data.push(intToLeBytes(el.col, 1)[0], intToLeBytes(el.row, 1)[0]);
		data.push(...el.option.slice(0, 4).map(o => intToLeBytes(o, 1)[0] || 0));
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

const detectedPal = ref(false);
const detectedNtsc = ref(false);
const detectedMax = ref(false);
const detectedVideo = computed(() => {
	if (canvasSizeSrc.value === 3) return '';
	if (!detectedMax.value) return 'OSD chip not detected';
	const signals = [];
	if (detectedPal.value) signals.push('PAL');
	if (detectedNtsc.value) signals.push('NTSC');
	return `Detected ${signals.join(', ') || 'no analog'} video signal`
})

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
				option: [...d.slice(4, 8).map(b => leBytesToInt([b], 0, 1))]
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
		return sendCommand(MspFn.GET_OSD_STATUS);
	}).then(({ data }) => {
		detectedMax.value = data[0] & (1 << 0) ? true : false;
		detectedPal.value = data[0] & (1 << 1) ? true : false;
		detectedNtsc.value = data[0] & (1 << 2) ? true : false;
		return sendCommand(MspFn.OSD_CONTROL, [2])
	}).catch(() => { })
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

function enableRealPreview() {
	showRealPreview.value = true;
	sendCommand(MspFn.OSD_CONTROL, [0]).catch(() => { });
}
function disableRealPreview() {
	showRealPreview.value = false;
	sendCommand(MspFn.OSD_CONTROL, [1]).catch(() => { });
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
	createCanvases();
	onConnectHandler(getConfig);
	onCommandHandler(command => {
		if (command.cmdType !== 'response') return;
		if (command.command === MspFn.MSP_DISPLAYPORT) {
			const d = command.data
			switch (d[0]) {
				case 0: // heartbeat
					gotHeartbeat = true;
					break;
				case 2: // clear
					previewScreen.fill(0);
					break;
				case 3: // write string
					let row = d[1];
					let col = d[2];
					const text = d.slice(4);
					for (let i = 0; i < text.length; i++) {
						const c = text[i];
						if (col >= cols.value) {
							col = 0;
							row++;
							if (row >= rows.value) break;
						}
						previewScreen[row * cols.value + col] = c;
						col++;
					}
					break;
				case 4: // flush to snippets
					previewSnippets.value.length = 0;
					let writingToSnippet = false;
					let currentSnippet = { text: '', x: 0, y: 0 };
					for (let i = 0; i < previewScreen.length; i++) {
						const c = previewScreen[i];
						if (c) {
							if (!writingToSnippet) {
								writingToSnippet = true;
								currentSnippet.x = i % cols.value;
								currentSnippet.y = Math.floor(i / cols.value);
							}
							currentSnippet.text += String.fromCharCode(c);
						} else {
							if (writingToSnippet) {
								writingToSnippet = false;
								previewSnippets.value.push(currentSnippet);
								currentSnippet = { text: '', x: 0, y: 0 };
							}
						}
					}
					break;
			}
		}
	})
	heartbeatInterval = setInterval(() => {
		if (!gotHeartbeat && showRealPreview.value) {
			sendCommand(MspFn.OSD_CONTROL, { data: [0], retries: 0 }).catch(() => { });
		}
		gotHeartbeat = false;
	}, 1000)
	sendDragNDrop();
})

const groupedElements = computed(() => {
	const groupList: { [key: string]: { ids: number[], group: OsdGroup } } = {};
	const actEls = activeElements.value;
	const hideAlreadyPlaced = !showAlreadyPlaced.value;
	const gr = groups.value.filter(g => !g.hidden || !hideAlreadyPlaced);
	OSD_LIST.forEach((el, i) => {
		if (!el) return;
		if (hideAlreadyPlaced && actEls.findIndex(actEl => actEl.id === i) !== -1) return;
		const g = gr.find(g => g.short === el.group);
		if (!g) return;
		let group = groupList[g.short];
		if (!group) {
			group = groupList[g.short] = { ids: [], group: g };
		}
		group.ids.push(i);
	})
	return groupList;
})

onBeforeUnmount(leave)
onBeforeUnmount(() => exiting = true)
onBeforeUnmount(() => clearInterval(heartbeatInterval))
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
						<p :style="`font-weight: ${index === hoverIndex ? 'bold' : 'normal'};`"
							:text="OSD_LIST[el.id].name">
							{{ OSD_LIST[el.id].name }}
						</p>
						<template v-for="i in 4">
							<select v-model="el.option[4 - i]" v-if="OSD_LIST[el.id].options?.[4 - i]"
								@change="pushElements">
								<option v-for="(o, j) in OSD_LIST[el.id].options?.[4 - i]"
									:value="o.id === undefined ? j : o.id">{{
										o.name }}</option>
							</select>
						</template>
						<!-- SPECIAL ELEMENT OPTIONS FOR SOME ELEMENTS -->
						<template v-for="(o, i) in el.option" v-if="el.id === 0x00C1">
							<input @change="event => {
								const text = (event.target as HTMLInputElement).value;
								if (text.length === 1) { el.option[i] = text.charCodeAt(0); }
								else { el.option[i] = parseInt(text) || 0; } pushElements();
							}" style="width: 35px;" placeholder="A, 0x41..." :value="'0x' + o.toString(16).padStart(2, '0')">
						</template>
						<!-- SPECIAL ELEMENT OPTIONS END-->
						<button class="defaultBtn red small"
							@click="() => { delete activeElements[index]; collapse(); pushElements(); }">
							<i class="fa-solid fa-trash"></i>
						</button>
					</div>
					<div class="activeElement" v-else>
						<p style="font-style: italic; color: grey;">Unknown Element {{ el.id }}</p>
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
					<input type="text" v-model="filter" placeholder="Search" style="margin-right: 1rem;">
					<input type="checkbox" id="showAlreadyPlaced" v-model="showAlreadyPlaced">&nbsp;
					<label for="showAlreadyPlaced">Show already placed</label>
				</div>
				<template v-if="filter">
					<div class="listElem" v-for="el in filteredList" draggable="true"
						@dragstart="(event) => { dragStart(OSD_LIST.indexOf(el), event, 'copy') }" @dragend="dragEnd">
						<p><i class="fa-solid fa-arrow-pointer"></i>{{ el.name }}</p>
					</div>
				</template>
				<template v-else>
					<div class="listGroup" v-for="g in groupedElements">
						<div class="groupHeader" :class="{ expanded: g.group.expanded }"
							@click="() => g.group.expanded = !g.group.expanded">
							<p>
								<i class="fa-solid fa-angle-right"
									:style="`transform:rotate(${g.group.expanded ? 90 : 0}deg);`"></i>
								{{ g.group.name }}
							</p>
						</div>
						<template v-if="g.group.expanded">
							<div class="listElem" v-for="(id, i) in g.ids" draggable="true"
								@dragstart="(event) => { dragStart(id, event, 'copy') }" :key="i" @dragend="dragEnd">
								<p><i class="fa-solid fa-arrow-pointer"></i>{{ OSD_LIST[id].name }}</p>
							</div>
						</template>
					</div>
				</template>
			</div>
		</div>
		<div class="line"></div>
		<div class="previewWrapper">
			<div class="preview"
				:style="`grid-template-columns: ${showRealPreview ? 'repeat(auto-fit, minmax(min(100%, max(500px, calc(50% - 1rem))), 1fr))' : '1fr 0fr'};`">
				<div style="grid-column: 1 / -1;">
					<button @click="save">Save</button>
					<p>
						Canvas Size:
						<select v-model="canvasSizeSrc">
							<option :value="0">Analog Auto</option>
							<option :value="1">Analog PAL</option>
							<option :value="2">Analog NTSC</option>
							<option :value="3">Digital HD</option>
						</select>
						{{ detectedVideo }}
					</p>
				</div>
				<div class="previewImageWrapper">
					<div class="previewImage" :class="{ splitview: showRealPreview }" :style="aspectStyle"
						@drop="dropped" @dragover="dragover" @dragleave="() => dragHide = true" ref="previewImage"
						@mouseenter="enableCursor" @mouseleave="disableCursor" @mousemove="setCursorPos">
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
								:text="getPreviewText(el)" :rows="rows" :cols="cols" :row="el.row" :col="el.col"
								:chars="charCanvases"
								@dragstart="(ev, gc, txt) => { dragStart(index, ev, 'move', gc, txt) }"
								:poiev="dragging !== 'none' ? 'none' : 'initial'" @dragend="dragEnd"
								@mouseenter="() => hoverIndex = index"
								@mouseleave="() => hoverIndex = hoverIndex === index ? -1 : hoverIndex" />
						</template>
						<TextCanvas v-if="dragging !== 'none' && !dragHide" :opacity="0.5" :rows="rows" :cols="cols"
							:chars="charCanvases" :text="dragText" :row="draggingRow" :col="draggingCol"
							:poiev="'none'" />
					</div>
				</div>
				<div class="previewImageWrapper" :class="{ lowOpacity: dragging !== 'none' }" v-if="showRealPreview">
					<div class="previewImage splitview" :style="aspectStyle">
						<img src="@assets/DJI_0124.JPG">
						<button style="position: absolute; right: 0px; font-size: 1.5rem;"
							@click="disableRealPreview"><i class="fa-solid fa-close"></i></button>
						<template v-for="s in previewSnippets" :key="s.x + '-' + s.y">
							<TextCanvas :opacity="1" :rows="rows" :cols="cols" :row="s.y" :col="s.x" poiev="none"
								:chars="charCanvases" :text="s.text" />
						</template>
					</div>
				</div>
				<div class="addRealPreview" v-else>
					<button @click="enableRealPreview" class="defaultBtn">
						<i class="fa-solid fa-eye"></i>
					</button>
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
	margin: 1rem;
}

.osdListWrapper {
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
	z-index: 1;
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

.groupHeader {
	display: flex;
	align-items: center;
	gap: 1rem;
	padding: .5rem .6rem;
	user-select: none;
}

.groupHeader.expanded {
	border-bottom: 1px solid var(--border-color);
}

.listGroup+.listGroup .groupHeader {
	border-top: 3px solid var(--border-color);
}

.groupHeader p {
	font-weight: bold;
	margin: 0px;
}

.groupHeader i {
	transition: transform 0.2s ease-out;
	margin-right: 1rem;
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

.listGroup .listElem {
	padding-left: 2rem;
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

.activeElement p::before {
	content: attr(text);
	visibility: hidden;
	font-weight: bold;
	height: 0px;
	display: block;
}

.listElem i {
	margin-right: .8rem;
}

.line {
	width: 2px;
	flex-shrink: 0;
	background-color: var(--border-color);
}

.previewWrapper {
	flex-grow: 6;
}

.preview {
	position: sticky;
	top: 0;
	display: grid;
	gap: 1rem;
}

.previewImageWrapper {
	width: 100%;
	max-height: 70vh;
	transition: opacity 0.3s ease-out;
}

.previewImage {
	position: relative;
	overflow: hidden;
	margin: 0px auto;
	max-height: 70vh;
}

.splitview {
	display: inline-block;
	height: auto;
	width: 100%;
	max-width: calc(70vh * (16/9));
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
