<script lang="ts">
import { defineComponent } from "vue";
import { type GenFlagProps, type FlagProps, type BBLog } from "@utils/types";
import { getNestedProperty } from "@utils/utils";

export default defineComponent({
	name: "BlackboxTimeline",
	props: {
		ll: { // loadedLog
			type: Object,
		},
		gfp: { //genFlagProps
			type: Object,
			required: true
		},
		fp: { // flagProps
			type: Object,
			required: true
		},
		startFrame: {
			type: Number,
			required: true,
		},
		endFrame: {
			type: Number,
			required: true,
		},
	},
	data() {
		return {
			osCanvas: document.createElement("canvas"),
			selCanvas: document.createElement("canvas"),
			currentlyTracking: undefined as 'start' | 'end' | 'move' | undefined,
			downAtFrame: 0,
			startFrameOnDown: 0,
			endFrameOnDown: 0,
		};
	},
	emits: ["update"],
	computed: {
		flagProps(): { [key: string]: FlagProps } {
			return this.fp
		},
		genFlagProps(): { [key: string]: GenFlagProps } {
			return this.gfp
		},
		loadedLog(): BBLog | undefined {
			return this.ll as BBLog | undefined;
		},
		canvas() {
			return this.$refs.canvas as HTMLCanvasElement;
		},
		wrapper() {
			return this.$refs.wrapper as HTMLDivElement;
		},
	},
	methods: {
		mouseMove(e: MouseEvent) {
			if (!this.loadedLog || !this.currentlyTracking) return;
			if (e.buttons !== 1) {
				this.mouseUp();
				return;
			}
			const frame = Math.round((e.offsetX / this.canvas.width) * this.loadedLog.frameCount);
			let sf = this.startFrame;
			let ef = this.endFrame;
			if (this.currentlyTracking === 'start') {
				sf = frame;
			} else if (this.currentlyTracking === 'end') {
				ef = frame;
			} else if (this.currentlyTracking === 'move') {
				let diff = frame - this.downAtFrame;
				if (this.startFrameOnDown + diff < 0) diff = -this.startFrameOnDown;
				if (this.endFrameOnDown + diff > this.loadedLog.frameCount)
					diff = this.loadedLog.frameCount - this.endFrameOnDown;
				sf = this.startFrameOnDown + diff;
				ef = this.endFrameOnDown + diff;
			}
			this.$emit('update', sf, ef);
			this.drawSelection();
		},
		mouseDown(e: MouseEvent) {
			if (e.button !== 0) return;
			if (!this.loadedLog) return;
			const startFrameX = (this.startFrame / this.loadedLog.frameCount) * this.canvas.width;
			const endFrameX = (this.endFrame / this.loadedLog.frameCount) * this.canvas.width;
			const selectionWidth = Math.abs(endFrameX - startFrameX);
			const maxInside = Math.min(selectionWidth / 3, 10);
			if (e.offsetX - startFrameX < maxInside && startFrameX - e.offsetX < 10) {
				this.currentlyTracking = 'start';
			} else if (endFrameX - e.offsetX < maxInside && e.offsetX - endFrameX < 10) {
				this.currentlyTracking = 'end';
			} else if (e.offsetX > startFrameX && e.offsetX < endFrameX) {
				this.currentlyTracking = 'move';
				this.downAtFrame = Math.round((e.offsetX / this.canvas.width) * this.loadedLog.frameCount);
				this.startFrameOnDown = this.startFrame;
				this.endFrameOnDown = this.endFrame;
			} else {
				this.currentlyTracking = undefined;
			}
		},
		mouseUp() {
			this.currentlyTracking = undefined;
			let sf = this.startFrame;
			let ef = this.endFrame;
			const pStartFrame = sf;
			sf = Math.min(sf, ef);
			ef = Math.max(pStartFrame, ef);
			this.$emit('update', sf, ef);
			this.drawSelection();
		},
		fullDraw() {
			if (!this.loadedLog) return;
			let drawFlag = '';
			if (
				this.loadedLog.flags.includes('LOG_THROTTLE_SETPOINT') ||
				this.loadedLog.flags.includes('GEN_THROTTLE_SETPOINT')
			) {
				drawFlag = 'LOG_THROTTLE_SETPOINT';
			} else if (
				this.loadedLog.flags.includes('LOG_MOTOR_OUTPUTS') ||
				this.loadedLog.flags.includes('GEN_MOTOR_OUTPUTS')
			) {
				drawFlag = 'LOG_MOTOR_OUTPUTS';
			} else {
				drawFlag = this.loadedLog.flags[0];
			}
			const minMax = this.flagProps[drawFlag].rangeFn
				? this.flagProps[drawFlag].rangeFn!(this.loadedLog)
				: { min: this.flagProps[drawFlag].minValue!, max: this.flagProps[drawFlag].maxValue! };
			this.drawTrace(drawFlag, minMax.min, minMax.max);
			this.drawSelection();
		},
		onResize() {
			this.canvas.width = this.wrapper.clientWidth;
			this.canvas.height = this.wrapper.clientHeight;
			this.osCanvas.width = this.canvas.width;
			this.osCanvas.height = this.canvas.height;
			this.selCanvas.width = this.canvas.width;
			this.selCanvas.height = this.canvas.height;
			this.fullDraw();
		},
		drawSelection() {
			if (!this.loadedLog) return;
			const ctx = this.selCanvas.getContext('2d') as CanvasRenderingContext2D;
			ctx.clearRect(0, 0, this.selCanvas.width, this.selCanvas.height);
			const scaleX = this.selCanvas.width / (this.loadedLog!.frameCount - 1);
			ctx.fillStyle = 'rgba(0,0,0,0.5)';
			ctx.fillRect(0, 0, this.selCanvas.width, this.selCanvas.height);
			ctx.clearRect(this.startFrame * scaleX, 0, (this.endFrame - this.startFrame) * scaleX, this.selCanvas.height);
			ctx.strokeStyle = '#fff';
			ctx.lineWidth = 2;
			ctx.beginPath();
			ctx.moveTo(this.startFrame * scaleX, 0);
			ctx.lineTo(this.startFrame * scaleX, this.selCanvas.height);
			ctx.moveTo(this.endFrame * scaleX, 0);
			ctx.lineTo(this.endFrame * scaleX, this.selCanvas.height);
			ctx.stroke();

			const ctx2 = this.canvas.getContext('2d') as CanvasRenderingContext2D;
			ctx2.clearRect(0, 0, this.canvas.width, this.canvas.height);
			ctx2.drawImage(this.osCanvas, 0, 0);
			ctx2.drawImage(this.selCanvas, 0, 0);
			// this.$emit('update', startFrame.value, endFrame.value);
		},
		drawTrace(traceName: string, min: number, max: number) {
			if (!this.loadedLog) return;
			const ctx = this.osCanvas.getContext('2d') as CanvasRenderingContext2D;
			ctx.clearRect(0, 0, this.osCanvas.width, this.osCanvas.height);
			const drawArray = [];
			if (traceName === 'LOG_MOTOR_OUTPUTS') {
				for (let i = 0; i < this.loadedLog!.frameCount; i++) {
					let avg = this.loadedLog!.frames[i].motors.out.rr!;
					avg += this.loadedLog!.frames[i].motors.out.rl!;
					avg += this.loadedLog!.frames[i].motors.out.fr!;
					avg += this.loadedLog!.frames[i].motors.out.fl!;
					avg /= 4;
					drawArray.push(avg);
				}
			} else {
				for (let i = 0; i < this.loadedLog!.frameCount; i++) {
					drawArray.push(
						getNestedProperty(this.loadedLog!.frames[i], this.flagProps[traceName].path, { min: min, max: max })
					);
				}
			}
			const scaleX = this.canvas.width / this.loadedLog!.frameCount;
			const scaleY = this.canvas.height / (max - min - 1);
			ctx.beginPath();
			ctx.strokeStyle = '#61a0ff';
			ctx.lineWidth = 1;
			for (let i = 0; i < this.loadedLog!.frameCount; i++) {
				ctx.lineTo(i * scaleX, this.osCanvas.height - (drawArray[i] - min) * scaleY);
			}
			ctx.stroke();
		}
	},
	mounted() {
		this.onResize();
		window.addEventListener('resize', this.onResize);
	},
	watch: {
		loadedLog: {
			handler() {
				this.fullDraw();
			},
		},
		startFrame: {
			handler() {
				this.drawSelection();
			},
		},
		endFrame: {
			handler() {
				this.drawSelection();
			},
		},
	},
});
</script>

<template>
	<div class="wrapper" id="bbTimelineWrapper" @mousemove="mouseMove" role="banner" ref="wrapper">
		<canvas height="32" id="bbTimeline" @mousedown="mouseDown" @mouseup="mouseUp" ref="canvas"></canvas>
		<div v-if="currentlyTracking" class="selector"></div>
	</div>
</template>

<style scoped>
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