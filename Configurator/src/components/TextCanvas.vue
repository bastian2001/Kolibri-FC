<script setup lang="ts">
import { delay } from '@/utils/utils';
import { computed, nextTick, onMounted, ref, useTemplateRef, watch } from 'vue';

const props = defineProps<{
	text: string,
	chars: HTMLCanvasElement[],
	rows: number,
	cols: number,
	row: number,
	col: number,
	opacity: number,
	poiev: 'none' | 'initial',
}>();
const emit = defineEmits(['dragstart', 'dragend'])

const width = ref(0);
const canvas = useTemplateRef('canvas');

function redraw() {
	const ctx = canvas.value?.getContext('2d');
	if (!ctx || !canvas.value) return;
	const can = canvas.value;
	const text = props.text;
	canvas.value.width = 48 * text.length;
	ctx.clearRect(0, 0, can.width, can.height);
	for (let i = 0; i < text.length; i++) {
		const c = props.chars[text.charCodeAt(i)];
		if (c) ctx.drawImage(c, 48 * i, 0);
	}
}

watch(() => props, redraw, { deep: true });

onMounted(() => {
	nextTick().then(redraw);
})

const style = computed(() => {
	let s = '';
	s += 'top: ' + 100 * props.row / props.rows + '%;\n';
	s += 'left: ' + 100 * props.col / props.cols + '%;\n';
	s += 'width: ' + 100 * props.text.length / props.cols + '%;\n';
	s += 'height: ' + 100 / props.rows + '%;\n';
	s += 'opacity: ' + props.opacity + ';\n';
	s += 'pointer-events: ' + props.poiev + ';\n';
	return s;
})

function dragStart(event: DragEvent) {
	if (!event.dataTransfer || !canvas.value?.parentElement) return;
	event.dataTransfer.setData('text/plain', 'https://www.youtube.com/watch?v=dQw4w9WgXcQ');
	event.dataTransfer.effectAllowed = 'all';
	const box = canvas.value.parentElement.getBoundingClientRect();
	if (!box.width || !box.height) return;
	const cWidth = box.width / props.cols;
	const cHeight = box.height / props.rows;
	const grabbedChar = Math.floor(event.offsetX / cWidth)
	event.dataTransfer.setDragImage(canvas.value, cWidth * (grabbedChar + 1 / 2), cHeight / 2);

	// let JS capture the full opacity canvas, then send event to dim the view
	delay(0).then(() => {
		emit('dragstart', event, grabbedChar, props.text);
	});
}

function dragEnd(event: DragEvent) {
	if (!event.dataTransfer || !canvas.value) return;
	event.preventDefault();
	delay(0).then(() => {
		emit('dragend', event);
	})
}
</script>

<template>
	<canvas ref="canvas" class="osdText" :width="width" height="72" :style="style" draggable="true"
		@dragstart="dragStart" @dragend="dragEnd"></canvas>
</template>

<style scoped>
.osdText {
	position: absolute;
}
</style>
