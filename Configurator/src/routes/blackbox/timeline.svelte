<script lang="ts">
	import { type BBLog, getNestedProperty } from '../../utils';
	import { onMount, createEventDispatcher } from 'svelte';
	const dispatch = createEventDispatcher();

	export let loadedLog: BBLog | undefined;
	export let startFrame: number;
	export let endFrame: number;
	export let genFlagProps: {
		[key: string]: {
			name: string;
			replaces: string;
			requires: (string | string[])[]; // if its a string, that has to be in there. If its an array, one of the mentioned ones has to be in there
			unit: string;
			exact: boolean;
		};
	};
	export let flagProps: {
		[key: string]: {
			name: string;
			path: string;
			minValue?: number;
			maxValue?: number;
			rangeFn?: (file: BBLog | undefined) => { max: number; min: number };
			unit: string;
			usesModifier?: boolean;
		};
	};
	let canvas: HTMLCanvasElement;
	let wrapper: HTMLDivElement;
	const osCanvas = document.createElement('canvas');
	const selCanvas = document.createElement('canvas');

	$: loadedLog, fullDraw();
	$: startFrame, drawSelection();
	$: endFrame, drawSelection();

	function onResize() {
		canvas.width = wrapper.clientWidth;
		canvas.height = wrapper.clientHeight;
		osCanvas.width = canvas.width;
		osCanvas.height = canvas.height;
		selCanvas.width = canvas.width;
		selCanvas.height = canvas.height;
		fullDraw();
	}

	function drawSelection() {
		if (!loadedLog) return;
		const ctx = selCanvas.getContext('2d') as CanvasRenderingContext2D;
		ctx.clearRect(0, 0, selCanvas.width, selCanvas.height);
		const scaleX = selCanvas.width / (loadedLog!.frameCount - 1);
		ctx.fillStyle = 'rgba(0,0,0,0.5)';
		ctx.fillRect(0, 0, selCanvas.width, selCanvas.height);
		ctx.clearRect(startFrame * scaleX, 0, (endFrame - startFrame) * scaleX, selCanvas.height);
		ctx.strokeStyle = '#fff';
		ctx.lineWidth = 2;
		ctx.beginPath();
		ctx.moveTo(startFrame * scaleX, 0);
		ctx.lineTo(startFrame * scaleX, selCanvas.height);
		ctx.moveTo(endFrame * scaleX, 0);
		ctx.lineTo(endFrame * scaleX, selCanvas.height);
		ctx.stroke();

		const ctx2 = canvas.getContext('2d') as CanvasRenderingContext2D;
		ctx2.clearRect(0, 0, canvas.width, canvas.height);
		ctx2.drawImage(osCanvas, 0, 0);
		ctx2.drawImage(selCanvas, 0, 0);
		dispatch('update', { startFrame, endFrame });
	}
	function drawTrace(traceName: string, min: number, max: number) {
		if (!loadedLog) return;
		const ctx = osCanvas.getContext('2d') as CanvasRenderingContext2D;
		ctx.clearRect(0, 0, osCanvas.width, osCanvas.height);
		const drawArray = [];
		const newTraceName = flagProps[traceName]
			? flagProps[traceName]
			: flagProps[genFlagProps[traceName].replaces];
		if (traceName === 'LOG_MOTOR_OUTPUTS') {
			for (let i = 0; i < loadedLog!.frameCount; i++) {
				let avg = loadedLog!.frames[i].motors.out.rr!;
				avg += loadedLog!.frames[i].motors.out.rl!;
				avg += loadedLog!.frames[i].motors.out.fr!;
				avg += loadedLog!.frames[i].motors.out.fl!;
				avg /= 4;
				drawArray.push(avg);
			}
		} else {
			for (let i = 0; i < loadedLog!.frameCount; i++) {
				drawArray.push(
					getNestedProperty(loadedLog!.frames[i], flagProps[traceName].path, { min: min, max: max })
				);
			}
		}
		const scaleX = canvas.width / loadedLog!.frameCount;
		const scaleY = canvas.height / (max - min - 1);
		ctx.beginPath();
		ctx.strokeStyle = '#61a0ff';
		ctx.lineWidth = 1;
		for (let i = 0; i < loadedLog!.frameCount; i++) {
			ctx.lineTo(i * scaleX, osCanvas.height - (drawArray[i] - min) * scaleY);
		}
		ctx.stroke();
	}
	function fullDraw() {
		if (!loadedLog) return;
		let drawFlag = '';
		if (
			loadedLog.flags.includes('LOG_THROTTLE_SETPOINT') ||
			loadedLog.flags.includes('GEN_THROTTLE_SETPOINT')
		) {
			drawFlag = 'LOG_THROTTLE_SETPOINT';
		} else if (
			loadedLog.flags.includes('LOG_MOTOR_OUTPUTS') ||
			loadedLog.flags.includes('GEN_MOTOR_OUTPUTS')
		) {
			drawFlag = 'LOG_MOTOR_OUTPUTS';
		} else {
			drawFlag = loadedLog.flags[0];
		}
		const minMax = flagProps[drawFlag].rangeFn
			? flagProps[drawFlag].rangeFn!(loadedLog)
			: { min: flagProps[drawFlag].minValue!, max: flagProps[drawFlag].maxValue! };
		drawTrace(drawFlag, minMax.min, minMax.max);
		drawSelection();
	}

	let currentlyTracking: 'start' | 'end' | 'move' | undefined = undefined;
	let downAtFrame = 0;
	let startFrameOnDown = 0;
	let endFrameOnDown = 0;
	function mouseDown(e: MouseEvent) {
		if (e.button !== 0) return;
		if (!loadedLog) return;
		const startFrameX = (startFrame / loadedLog.frameCount) * canvas.width;
		const endFrameX = (endFrame / loadedLog.frameCount) * canvas.width;
		const selectionWidth = Math.abs(endFrameX - startFrameX);
		const maxInside = Math.min(selectionWidth / 3, 10);
		if (e.offsetX - startFrameX < maxInside && startFrameX - e.offsetX < 10) {
			currentlyTracking = 'start';
		} else if (endFrameX - e.offsetX < maxInside && e.offsetX - endFrameX < 10) {
			currentlyTracking = 'end';
		} else if (e.offsetX > startFrameX && e.offsetX < endFrameX) {
			currentlyTracking = 'move';
			downAtFrame = Math.round((e.offsetX / canvas.width) * loadedLog.frameCount);
			startFrameOnDown = startFrame;
			endFrameOnDown = endFrame;
		} else {
			currentlyTracking = undefined;
		}
	}
	function mouseUp() {
		currentlyTracking = undefined;
		const pStartFrame = startFrame;
		startFrame = Math.min(startFrame, endFrame);
		endFrame = Math.max(pStartFrame, endFrame);
		drawSelection();
	}
	function mouseMove(e: MouseEvent) {
		if (!loadedLog || !currentlyTracking) return;
		if (e.buttons !== 1) {
			mouseUp();
			return;
		}
		const frame = Math.round((e.offsetX / canvas.width) * loadedLog.frameCount);
		if (currentlyTracking === 'start') {
			startFrame = frame;
		} else if (currentlyTracking === 'end') {
			endFrame = frame;
		} else if (currentlyTracking === 'move') {
			let diff = frame - downAtFrame;
			if (startFrameOnDown + diff < 0) diff = -startFrameOnDown;
			if (endFrameOnDown + diff > loadedLog.frameCount)
				diff = loadedLog.frameCount - endFrameOnDown;
			startFrame = startFrameOnDown + diff;
			endFrame = endFrameOnDown + diff;
		}
		drawSelection();
	}

	onMount(() => {
		canvas = document.getElementById('bbTimeline') as HTMLCanvasElement;
		wrapper = document.getElementById('bbTimelineWrapper') as HTMLDivElement;
		onResize();
		window.addEventListener('resize', onResize);
	});
</script>

<div class="wrapper" id="bbTimelineWrapper" on:mousemove={mouseMove} role="banner">
	<canvas height="32" id="bbTimeline" on:mousedown={mouseDown} on:mouseup={mouseUp} />
	{#if currentlyTracking}
		<div class="selector" />
	{/if}
</div>

<style>
	.wrapper {
		height: 2rem;
	}
	canvas {
		width: 100%;
		height: 100%;
	}
	.selector {
		opacity: 0;
		position: absolute;
		top: 0;
		left: 0;
		width: 100%;
		height: 100%;
	}
</style>
