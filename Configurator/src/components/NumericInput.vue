<script lang="ts">
import { roundToDecimal } from "@/utils/utils";
import { defineComponent, PropType } from "vue";

const sensitivityMap = {
	low: 60,
	medium: 35,
	high: 20,
};

export default defineComponent({
	name: "NumericInput",
	props: {
		modelValue: {
			type: Number,
			required: true,
		},
		sensitivity: {
			type: String as PropType<'low' | 'medium' | 'high'>,
			default: 'medium',
		},
		step: {
			type: Number,
			default: 1,
		},
		min: {
			type: Number,
			default: Number.MIN_SAFE_INTEGER,
		},
		max: {
			type: Number,
			default: Number.MAX_SAFE_INTEGER,
		},
		unit: {
			type: String,
			default: '',
		},
		allowOnlyInts: {
			type: Boolean,
		},
		fixToSteps: {
			type: Boolean,
			default: false,
		},
		decimals: {
			type: Number,
			default: 6
		},
	},
	data() {
		return {
			val: 0,
			editVal: 0,
			movement: 0,
			entered: false,
			moved: false,
			lastWheel: 0,
		};
	},
	computed: {
		justInts() {
			if (this.allowOnlyInts !== undefined) {
				return this.allowOnlyInts;
			}
			return this.step === Math.floor(this.step);
		},
	},
	mounted() {
		const w = this.$refs.wrapper as HTMLDivElement;
		w.onmousedown = (e: MouseEvent) => {
			if (e.button !== 0) return;
			if (this.entered) return
			e.preventDefault();
			w.requestPointerLock().then(() => {
				w.onmousemove = this.onMouseMove;
				this.movement = 0;
			})
		};
		w.onmouseup = (e) => {
			if (e.button !== 0) return;
			if (this.entered) return
			document.exitPointerLock();
			w.onmousemove = null;
			if (!this.moved) {
				this.entered = true;
				this.editVal = this.val;
				this.$nextTick(() => {
					const input = this.$refs.input as HTMLInputElement;
					input.focus();
					input.select();
				});
			} else {
				this.moved = false;
			}
		};
		w.onfocus = () => {
			if (this.entered) return;
			this.entered = true;
			this.editVal = this.val;
			this.$nextTick(() => {
				const input = this.$refs.input as HTMLInputElement;
				input.focus();
				input.select();
			});
		};
	},
	methods: {
		onWheel(e: WheelEvent) {
			e.preventDefault();
			if (e.timeStamp - this.lastWheel < 3) return; // Prevents double wheel events
			this.lastWheel = e.timeStamp
			if (e.deltaY > 0) {
				this.editVal -= this.step;
			} else {
				this.editVal += this.step;
			}
			if (this.editVal < this.min) this.editVal = this.min;
			if (this.editVal > this.max) this.editVal = this.max;
			this.editVal = roundToDecimal(this.editVal, this.decimals);
		},
		onMouseMove(e: MouseEvent) {
			const sens = sensitivityMap[this.sensitivity];
			this.movement += e.movementX;
			while (this.movement > sens) {
				this.val += this.step;
				this.movement -= sens;
				this.moved = true;
				if (this.val > this.max) {
					this.val = this.max;
					this.movement = 0;
				}
			}
			while (this.movement < -sens) {
				this.val -= this.step;
				this.movement += sens;
				this.moved = true;
				if (this.val < this.min) {
					this.val = this.min;
					this.movement = 0;
				}
			}
			this.val = roundToDecimal(this.val, this.decimals);
		},
		onKeyDown(e: KeyboardEvent) {
			if (e.key === 'Enter') {
				this.saveAndExit();
			} else if (e.key === 'Escape') {
				this.editVal = this.val;
				this.entered = false;
			} else if (e.key === 'ArrowUp') {
				e.preventDefault();
				this.editVal += this.step;
				this.editVal = roundToDecimal(this.editVal, this.decimals);
			} else if (e.key === 'ArrowDown') {
				e.preventDefault();
				this.val -= this.step;
				this.editVal = roundToDecimal(this.editVal, this.decimals);
			}
		},
		saveAndExit() {
			this.entered = false;
			this.val = this.editVal;
			if (this.fixToSteps) {
				if (this.min !== Number.MIN_SAFE_INTEGER) {
					this.val = Math.round((this.val - this.min) / this.step) * this.step + this.min;
				}
			}
			if (this.justInts) {
				this.val = Math.round(this.val);
			}
			if (this.val < this.min) this.val = this.min;
			if (this.val > this.max) this.val = this.max;
		},
	},
	watch: {
		modelValue: {
			immediate: true,
			handler(newVal) {
				this.val = newVal;
				this.editVal = newVal;
			},
		},
		val: {
			immediate: true,
			handler(newVal) {
				this.editVal = newVal;
				this.$emit('update:modelValue', newVal);
			},
		},
	},
});
</script>

<template>
	<div class="numericInputWrapper" :class="{ entered }" ref="wrapper" :tabindex="entered ? -1 : 0">
		<div class="numericInputDisplay" v-if="!entered">
			{{ val.toLocaleString() }} {{ unit }}
		</div>
		<div v-else class="numericInputEdit" @wheel="onWheel">
			<input type="number" class="numericInputInput" ref="input" v-model="editVal" @keydown="onKeyDown"
				@focusout="saveAndExit">
		</div>
	</div>
</template>

<style scoped>
.numericInputWrapper {
	width: 7rem;
}

.numericInputDisplay {
	background-color: var(--background-blue);
	height: 100%;
	color: white;
	text-align: center;
	font-size: 1rem;
	cursor: ew-resize;
	padding: 6px 8px 5px 8px;
	box-sizing: border-box;
}

.numericInputDisplay:hover {
	background-color: var(--background-highlight);
}

.numericInputEdit {
	background-color: var(--background-blue);
	height: 100%;
	color: white;
	text-align: center;
	padding: 4px 16px 3px 16px;
	box-sizing: border-box;
	background-color: var(--background-highlight);
}

.numericInputInput {
	appearance: none;
	background-color: transparent;
	color: white;
	border: none;
	outline: none;
	text-align: center;
	width: 100%;
	padding: 2px 0px 1px 0px;
	font-size: 1rem;
	border-bottom: 1px solid white;
}

.numericInputInput::selection {
	background-color: var(--accent-blue);
	color: white;
}

.numericInputInput::-webkit-outer-spin-button,
.numericInputInput::-webkit-inner-spin-button {
	-webkit-appearance: none;
	margin: 0;
}
</style>
