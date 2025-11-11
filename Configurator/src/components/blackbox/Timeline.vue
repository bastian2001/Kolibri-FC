<script lang="ts">
import { defineComponent, PropType } from "vue";
import { GenFlagProps, FlagProps, BBLog, TypedArray } from "@utils/types";
import { delay } from "@/utils/utils";
import { skipValues } from "@/utils/blackbox/other";

export default defineComponent({
	name: "BlackboxTimeline",
	props: {
		loadedLog: {
			type: Object as PropType<BBLog | undefined>,
		},
		genFlagProps: {
			type: Object as PropType<{ [key: string]: GenFlagProps }>,
			required: true
		},
		flagProps: {
			type: Object as PropType<{ [key: string]: FlagProps }>,
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
		canvas() {
			return this.$refs.timelineCanvas as HTMLCanvasElement;
		},
		wrapper() {
			return this.$refs.timelineWrapper as HTMLDivElement;
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
		fullDraw(allowShortening = true) {
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
			this.drawTrace(drawFlag, minMax.min, minMax.max, allowShortening);
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
		},
		drawTrace(traceName: string, min: number, max: number, allowShortening: boolean) {
			if (!this.loadedLog) return;
			const ctx = this.osCanvas.getContext('2d') as CanvasRenderingContext2D;
			ctx.clearRect(0, 0, this.osCanvas.width, this.osCanvas.height);
			let drawArray: number[] | TypedArray = [];
			if (traceName === 'LOG_MOTOR_OUTPUTS') {
				for (let i = 0; i < this.loadedLog.frameCount; i++) {
					let avg = this.loadedLog.logData.motorOutRR![i];
					avg += this.loadedLog.logData.motorOutRL![i];
					avg += this.loadedLog.logData.motorOutFR![i];
					avg += this.loadedLog.logData.motorOutFL![i];
					avg /= 4;
					drawArray.push(avg);
				}
				if (allowShortening) {
					const everyNth = Math.floor(this.loadedLog.frameCount / this.canvas.width);
					if (everyNth > 1) {
						drawArray = drawArray.filter((_, i) => i % everyNth === 0);
						delay(500).then(() => {
							this.fullDraw(false);
						});
					}
				}
			} else {
				if (allowShortening) {
					const everyNth = Math.floor(this.loadedLog.frameCount / this.canvas.width);
					const skipped = skipValues(this.loadedLog.logData, everyNth)
					// @ts-expect-error
					drawArray = skipped[this.flagProps[traceName].path] as TypedArray;
					delay(500).then(() => {
						this.fullDraw(false);
					});
				} else {
					// @ts-expect-error
					drawArray = this.loadedLog.logData[this.flagProps[traceName].path] as TypedArray;
				}
			}
			const scaleX = this.canvas.width / drawArray.length;
			const scaleY = this.canvas.height / (max - min - 1);
			ctx.beginPath();
			ctx.strokeStyle = '#61a0ff';
			ctx.lineWidth = 1;
			for (let i = 0; i < drawArray.length; i++) {
				ctx.lineTo(i * scaleX, this.osCanvas.height - (drawArray[i] - min) * scaleY);
			}
			ctx.stroke();
		}
	},
	mounted() {
		this.onResize();
		window.addEventListener('resize', this.onResize);
	},
	beforeUnmount() {
		window.removeEventListener('resize', this.onResize);
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
	<div class="wrapper" id="bbTimelineWrapper" @mousemove="mouseMove" role="banner" ref="timelineWrapper">
		<canvas height="32" id="bbTimeline" @mousedown="mouseDown" @mouseup="mouseUp" ref="timelineCanvas"></canvas>
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
