<script lang="ts">
import { mathEval, roundToDecimal } from "@/utils/utils";
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
		precision: {
			type: Number,
			default: 6
		},
		disabled: {
			type: Boolean,
			default: false
		}
	},
	data() {
		return {
			val: 0,
			editText: '0',
			movement: 0,
			entered: false,
			moved: false,
			roundToDecimal,
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
		displayDecimals() {
			// derive from step
			if (this.step === 0) return 0;
			const stepStr = this.step.toString();
			const decimalIndex = stepStr.indexOf('.');
			if (decimalIndex === -1) return 0; // No decimal point, no
			return stepStr.length - decimalIndex - 1;
		},
		displayNumber() {
			const n = new Intl.NumberFormat(undefined, { minimumFractionDigits: this.displayDecimals })
			return n.format(this.val)
		},
		isNumber() {
			return !isNaN(Number(this.editText)) && !isNaN(parseFloat(this.editText))
		}
	},
	mounted() {
		const w = this.$refs.wrapper as HTMLDivElement;
		w.onmousedown = (e: MouseEvent) => {
			if (e.button !== 0) return;
			if (this.entered) return;
			if (this.disabled) return;
			e.preventDefault();
			w.requestPointerLock().then(() => {
				w.onmousemove = this.onMouseMove;
				this.movement = 0;
			})
		};
		w.onmouseup = (e) => {
			if (e.button !== 0) return;
			if (this.entered) return;
			if (this.disabled) return;
			document.exitPointerLock();
			w.onmousemove = null;
			if (!this.moved) {
				this.entered = true;
				this.editText = this.val.toString();
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
			if (this.disabled) return;
			this.entered = true;
			this.editText = this.val.toString();
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
			if (this.isNumber) {
				let val = parseFloat(this.editText)
				if (e.deltaY > 0) {
					val -= this.step;
				} else {
					val += this.step;
				}
				if (val < this.min) val = this.min;
				if (val > this.max) val = this.max;
				val = roundToDecimal(val, this.precision);
				this.editText = val.toString()
			}
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
			this.val = roundToDecimal(this.val, this.precision);
		},
		onKeyDown(e: KeyboardEvent) {
			if (e.key === 'Enter') {
				this.saveAndExit();
			} else if (e.key === 'Escape') {
				this.editText = this.val.toString();
				this.entered = false;
			} else if (e.key === 'ArrowUp') {
				e.preventDefault();
				if (this.isNumber) {
					let val = parseFloat(this.editText)
					val += this.step;
					if (val < this.min) val = this.min;
					if (val > this.max) val = this.max;
					val = roundToDecimal(val, this.precision);
					this.editText = val.toString()
				}
			} else if (e.key === 'ArrowDown') {
				e.preventDefault();
				let val = parseFloat(this.editText)
				val -= this.step;
				if (val < this.min) val = this.min;
				if (val > this.max) val = this.max;
				val = roundToDecimal(val, this.precision);
				this.editText = val.toString()
			}
		},
		saveAndExit() {
			this.entered = false;
			let num
			try {
				num = mathEval(this.editText)
			} catch (e) {
				num = this.val
			}
			this.val = num
			this.val = roundToDecimal(this.val, this.precision)
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
				this.editText = newVal.toString();
			},
		},
		val: {
			immediate: true,
			handler(newVal) {
				this.editText = newVal.toString();
				this.$emit('update:modelValue', newVal);
			},
		},
		disabled: {
			immediate: true,
			handler(newDis) {
				if (newDis) {
					this.editText = this.modelValue.toString()
					this.val = this.modelValue
					document.exitPointerLock();
					if (this.$refs.wrapper) (this.$refs.wrapper as HTMLDivElement).onmousemove = null;
					this.moved = false;
					this.entered = false
				}
			}
		}
	},
});
</script>

<template>
	<div class="numericInputWrapper" :class="{ entered, disabled }" ref="wrapper" :tabindex="entered ? -1 : 0">
		<div class="numericInputDisplay" v-if="!entered">
			{{ displayNumber }}&thinsp;{{ unit }}
		</div>
		<div v-else class="numericInputEdit" @wheel="onWheel">
			<input type="text" class="numericInputInput" ref="input" v-model="editText" @keydown="onKeyDown"
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

.numericInputWrapper.disabled .numericInputDisplay {
	background-color: var(--background-highlight);
	cursor: default;
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
