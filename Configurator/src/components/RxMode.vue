<script lang="ts">
import { defineComponent, PropType } from "vue";

export default defineComponent({
	name: "RxMode",
	emits: ["update:range", "update:channel"],
	props: {
		min: {
			type: Number, // range: -120 to 120
			required: true,
		},
		max: {
			type: Number, // range: -120 to 120
			required: true,
		},
		rc: {
			type: Object as PropType<number[]>,
			required: true,
		},
		channel: {
			type: Number,
			required: true,
		},
		name: {
			type: String,
			required: true,
		},
	},
	data() {
		return {
			currentlyGrabbing: 0,
			auto: false,
			isDragging: false,
			barRect: null as DOMRect | null,
			channelBackup: new Array(16).fill(1500),
			mouseX: 0, // closest -120...120 position from range start grab
			startMin: 0, // initial min value when range grab starts
		};
	},
	methods: {
		startGrab(thumb: number) {
			this.currentlyGrabbing = thumb;
			this.isDragging = true;
			this.barRect = (this.$refs.bar as HTMLElement).getBoundingClientRect();

			// Add global event listeners
			document.addEventListener('mousemove', this.onDrag);
			document.addEventListener('mouseup', this.stopGrab);
			document.addEventListener('touchmove', this.onDrag);
			document.addEventListener('touchend', this.stopGrab);
		},

		startRangeGrab(e: MouseEvent | TouchEvent) {
			this.isDragging = true;
			this.currentlyGrabbing = -1; // -1 indicates range grab
			this.barRect = (this.$refs.bar as HTMLElement).getBoundingClientRect();
			const clientX = e instanceof MouseEvent ? e.clientX : e.touches[0].clientX;
			const relativeX = clientX - this.barRect.left;
			const barHeight = this.barRect.height;
			const percentage = Math.max(0, Math.min(1, (relativeX - barHeight / 2) / (this.barRect.width - barHeight)));
			this.mouseX = Math.round((percentage * 240) - 120); // Convert percentage to range value (-120 to 120)
			this.startMin = this.min; // Store initial min value

			// Add global event listeners
			document.addEventListener('mousemove', this.onRangeDrag);
			document.addEventListener('mouseup', this.stopRangeGrab);
			document.addEventListener('touchmove', this.onRangeDrag);
			document.addEventListener('touchend', this.stopRangeGrab);
		},

		onDrag(e: MouseEvent | TouchEvent) {
			if (!this.isDragging || !this.barRect) return;

			const clientX = e instanceof MouseEvent ? e.clientX : e.touches[0].clientX;
			const relativeX = clientX - this.barRect.left;
			const barHeight = this.barRect.height;
			const percentage = Math.max(0, Math.min(1, (relativeX - barHeight / 2) / (this.barRect.width - barHeight)));

			// Convert percentage to range value (-120 to 120)
			let value = Math.round((percentage * 240) - 120);

			if (!e.shiftKey) {
				value = Math.round(value / 10) * 10;
			}

			if (this.currentlyGrabbing === 0) {
				// Update min value, ensure it doesn't exceed max
				if (value > this.max) {
					this.$emit('update:range', this.max, value);
					this.currentlyGrabbing = 1; // Switch to max grab
					return;
				}
				this.$emit('update:range', value, this.max);
			} else if (this.currentlyGrabbing === 1) {
				// Update max value, ensure it doesn't go below min
				if (value < this.min) {
					this.$emit('update:range', value, this.min);
					this.currentlyGrabbing = 0; // Switch to min grab
					return;
				}
				this.$emit('update:range', this.min, value);
			}
		},

		stopGrab() {
			this.isDragging = false;

			// Remove global event listeners
			document.removeEventListener('mousemove', this.onDrag);
			document.removeEventListener('mouseup', this.stopGrab);
			document.removeEventListener('touchmove', this.onDrag);
			document.removeEventListener('touchend', this.stopGrab);
		},

		onRangeDrag(e: MouseEvent | TouchEvent) {
			if (!this.isDragging || !this.barRect) return;

			const clientX = e instanceof MouseEvent ? e.clientX : e.touches[0].clientX;
			const relativeX = clientX - this.barRect.left;
			const barHeight = this.barRect.height;
			const percentage = Math.max(0, Math.min(1, (relativeX - barHeight / 2) / (this.barRect.width - barHeight)));
			const value = Math.round((percentage * 240) - 120); // Convert percentage to range value (-120 to 120)
			const rangeSize = this.max - this.min;
			let newMin = value - (this.mouseX - this.startMin);
			if (!e.shiftKey) {
				// snap min to increments of 10
				newMin = Math.round(newMin / 10) * 10;
			}
			if (newMin < -120) newMin = -120;
			if (newMin > 120 - rangeSize) newMin = 120 - rangeSize;
			let newMax = newMin + rangeSize;
			if (newMax > 120) newMax = 120;
			if (newMax < -120) newMax = -120;
			this.$emit('update:range', newMin, newMax);
		},

		stopRangeGrab() {
			this.isDragging = false;

			// Remove global event listeners
			document.removeEventListener('mousemove', this.onRangeDrag);
			document.removeEventListener('mouseup', this.stopRangeGrab);
			document.removeEventListener('touchmove', this.onRangeDrag);
			document.removeEventListener('touchend', this.stopRangeGrab);
		},

		updateChannel(e: HTMLSelectElement) {
			const newChannel = parseInt(e.value);
			this.$emit('update:channel', newChannel);
		},
		startAuto() {
			this.channelBackup = [...this.rc];
			this.auto = true;
		}
	},
	computed: {
		thumb0Position() {
			// Convert min value (-120 to 120) to percentage (0 to 1)
			return (this.min + 120) / 240;
		},

		thumb1Position() {
			// Convert max value (-120 to 120) to percentage (0 to 1)
			return (this.max + 120) / 240;
		},

		rangeStyle() {
			return {
				left: `calc(${this.thumb0Position} * (100% - 1rem) + 0.5rem)`,
				width: `calc(${this.thumb1Position - this.thumb0Position} * (100% - 1rem))`,
			};
		},
		minValue() {
			return this.min * 5 + 1500
		},
		maxValue() {
			return this.max * 5 + 1500
		},
		inRange() {
			const channelValue = this.rc[this.channel];
			if (channelValue === undefined) {
				return false; // If channel value is not defined, consider it out of range
			}
			return channelValue >= this.minValue && channelValue <= this.maxValue;
		}
	},
	watch: {
		rc: {
			handler: function (newValue) {
				if (this.auto) {
					for (let i = 4; i < newValue.length && i < 14; i++) { // chan 14 and 15 are RSSI, 0-3 are sticks
						if (Math.abs(newValue[i] - this.channelBackup[i]) > 100) {
							this.$emit('update:channel', i);
							let val = Math.round((newValue[i] - 1500) / 5);
							val = Math.round(val / 10) * 10; // snap to 10
							let min = val - 20;
							let max = val + 20;
							if (min < -120) min = -120;
							if (max > 120) max = 120;
							this.$emit('update:range', min, max);
							this.auto = false;
							break;
						}
					}
				}
			},
			deep: true,
		}
	}
});
</script>

<template>
	<div :class="{ rxMode: true, inRange: inRange }">
		<span class="rxModeName">{{ name }}</span>&nbsp;&nbsp;&nbsp;
		<select class="rxChannelSelect" @input="updateChannel($event.target as HTMLSelectElement)" :value="channel">
			<option value="-1">Disabled</option>
			<option v-for="i in 12" :key="i + 3" :value="i + 3">Aux {{ i }}</option>
			<option value="0">Roll</option>
			<option value="1">Pitch</option>
			<option value="2">Throttle</option>
			<option value="3">Yaw</option>
		</select>&nbsp;&nbsp;&nbsp;
		<button @click="startAuto()" class="autoBtn">Auto</button>&nbsp;&nbsp;&nbsp;
		<span v-if="auto">Move a switch</span>
		<div class="rangeTexts" style="margin-bottom: .5rem">
			<span style="float:right">{{ maxValue }}</span>
			<span>{{ minValue }}</span>
		</div>
		<div class="rxModeBarWrapper">
			<div class="rxModeBar" ref="bar">
				<div class="rxModeRange" ref="range" :style="rangeStyle" @mousedown="startRangeGrab($event)"
					@touchstart="startRangeGrab($event)"></div>
				<div class="thumb0 thumb" ref="thumb0"
					:style="{ left: `calc(${thumb0Position} * (100% - 1.2rem) + 0.6rem)` }" @mousedown="startGrab(0)"
					@touchstart="startGrab(0)"></div>
				<div class="thumb1 thumb" ref="thumb1"
					:style="{ left: `calc(${thumb1Position} * (100% - 1.2rem) + 0.6rem)` }" @mousedown="startGrab(1)"
					@touchstart="startGrab(1)"></div>
			</div>
		</div>
	</div>
</template>

<style scoped>
.rxMode {
	border: 3px solid var(--border-color);
	border-radius: 4px;
	padding: 0.7rem;
	margin-bottom: 1rem;
	background-color: var(--background-light);
}

.inRange {
	border-color: var(--border-green);
}

.rxModeName {
	font-weight: bold;
	margin-bottom: 1rem;
	display: inline-block;
}

.rxModeBarWrapper {
	height: .8rem;
	position: relative;
}

.rxModeBar {
	height: 100%;
	background: var(--background-highlight);
	border-radius: .4rem;
	position: relative;
}

.rxModeRange {
	position: absolute;
	top: 0;
	left: 0;
	right: 0;
	bottom: 0;
	background-color: var(--accent-blue);
	height: .8rem;
	cursor: grab;
}

.rxModeRange:active {
	cursor: grabbing;
}

.thumb {
	width: 1.2rem;
	height: 1.2rem;
	background: #ddd;
	border-radius: 50%;
	border: 1px solid #000;
	position: absolute;
	top: 50%;
	transform: translate(-50%, -50%);
	cursor: grab;
}

.thumb:active {
	cursor: grabbing;
}

.rxChannelSelect,
option {
	color: black;
}

.autoBtn {
	background-color: transparent;
	border: 1px solid var(--border-color);
	border-radius: 4px;
	padding: 4px 8px;
	color: var(--text-color);
	outline: none;
}

.autoBtn:hover {
	background-color: #fff1;
}

.autoBtn:active {
	background-color: #fff3;
}
</style>
