<script setup lang="ts">
import { delay } from '@/utils/utils';
import { computed, nextTick, onMounted, useTemplateRef, watch } from 'vue';

const props = defineProps<{
	text: string | string[],
	chars: HTMLCanvasElement[],
	rows: number,
	cols: number,
	row: number,
	col: number,
	opacity: number,
	poiev: 'none' | 'initial',
}>();
const emit = defineEmits(['dragstart', 'dragend', 'mouseenter', 'mouseleave'])

const canvas = useTemplateRef('canvas');

function redraw() {
	const ctx = canvas.value?.getContext('2d');
	if (!ctx || !canvas.value) return;
	const can = canvas.value;
	const text = typeof props.text === 'string' ? [props.text] : props.text;
	can.width = 48 * text.reduce((a, b) => Math.max(a, b.length), 0);
	can.height = 72 * text.length;
	ctx.clearRect(0, 0, can.width, can.height);
	for (let l = 0; l < text.length; l++) {
		const line = text[l];
		for (let i = 0; i < line.length; i++) {
			const c = props.chars[line.charCodeAt(i)];
			if (c) ctx.drawImage(c, 48 * i, 72 * l);
		}
	}
}

watch(() => props.text, redraw, { deep: true });

onMounted(() => {
	nextTick().then(redraw);
})

const style = computed(() => {
	const text = typeof props.text === 'string' ? [props.text] : props.text;
	let s = '';
	s += 'top: ' + 100 * props.row / props.rows + '%;\n';
	s += 'left: ' + 100 * props.col / props.cols + '%;\n';
	s += 'width: ' + 100 * text.reduce((a, b) => Math.max(a, b.length), 0) / props.cols + '%;\n';
	s += 'height: ' + 100 * text.length / props.rows + '%;\n';
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
	const grabbedChar = Math.floor(event.offsetX / cWidth);
	const grabbedRow = Math.floor(event.offsetY / cHeight);
	event.dataTransfer.setDragImage(canvas.value, cWidth * (grabbedChar + 1 / 2), cHeight * (grabbedRow + 1 / 2));

	// let JS capture the full opacity canvas, then send event to dim the view
	delay(0).then(() => {
		emit('dragstart', event, [grabbedChar, grabbedRow], props.text);
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
	<canvas ref="canvas" class="osdText" width="48" height="72" :style="style" draggable="true" @dragstart="dragStart"
		@dragend="dragEnd" @mouseenter="() => emit('mouseenter')" @mouseleave="() => emit('mouseleave')"></canvas>
</template>

<style scoped>
.osdText {
	position: absolute;
}
</style>
