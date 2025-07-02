<script lang="ts">
import { BBLog, FlagProps, GenFlagProps, LogDataType, TraceInGraph, TraceInternalData } from '@utils/types';
import { defineComponent, PropType } from 'vue';

function getBinomialCoeff(n: number, k: number) {
	let result = 1;
	for (let i = 1; i <= n; i++) {
		result *= i;
	}
	for (let i = 1; i <= k; i++) {
		result /= i;
	}
	for (let i = 1; i <= n - k; i++) {
		result /= i;
	}
	return result;
}

export default defineComponent({
	name: 'TracePlacer',
	emits: ['backupfn', 'update', 'delete'],
	props: {
		flagProps: {
			type: Object as PropType<{ [key: string]: FlagProps }>,
			required: true,
		},
		genFlagProps: {
			type: Object as PropType<{ [key: string]: GenFlagProps }>,
			required: true,
		},
		loadedLog: {
			type: Object as PropType<BBLog>,
			required: true,
		},
		trace: {
			type: Object as PropType<TraceInGraph>,
			required: true,
		},
		setData: {
			type: Object,
			default: {},
		}
	},
	data() {
		return {
			autoRangeOn: true,
			currentModifierName: '',
			filteringOn: false,
			filterType: 'pt1' as 'pt1' | 'pt2' | 'pt3' | 'sma' | 'binomial',
			filterValue1: 0,
			filterValue2: false,
			flagName: '',
			minValue: 0,
			maxValue: 1,
			setupDone: false
		};
	},
	computed: {
		availableFlagNames(): string[] {
			return this.loadedLog.flags.filter(f => {
				return f.startsWith('LOG_');
			})
		},
		availableGenFlagNames(): string[] {
			return this.loadedLog.flags.filter(f => {
				return f.startsWith('GEN_');
			})
		},
		currentFlag(): FlagProps | GenFlagProps | undefined {
			if (this.flagName.startsWith('LOG_')) {
				return this.flagProps[this.flagName];
			} else if (this.flagName.startsWith('GEN_')) {
				return this.genFlagProps[this.flagName];
			}
			return undefined;
		},
		currentNormalizedFlag(): FlagProps | undefined {
			if (this.flagName.startsWith('LOG_')) {
				return this.flagProps[this.flagName];
			} else if (this.flagName.startsWith('GEN_')) {
				return this.flagProps[this.genFlagProps[this.flagName].replaces];
			}
			return undefined;
		},
		currentModifier() {
			return this.currentNormalizedFlag?.modifier?.find(m => m.path === this.currentModifierName)
		},
		autoRange(): { min: number, max: number } {
			let range = { min: 0, max: 1 };
			if (this.currentNormalizedFlag) {
				range.min = this.currentModifier?.min || this.currentNormalizedFlag.minValue || 0;
				range.max = this.currentModifier?.max || this.currentNormalizedFlag.maxValue || 1;
				if (this.currentModifier?.rangeFn) {
					range = this.currentModifier.rangeFn(this.loadedLog);
				} else if (this.currentNormalizedFlag.rangeFn) {
					range = this.currentNormalizedFlag.rangeFn(this.loadedLog);
				}
			}
			return range;
		},
		path(): string {
			if (this.currentNormalizedFlag) {
				if (this.currentModifierName) {
					return this.currentModifierName;
				}
				return this.currentNormalizedFlag.path;
			}
			return ''
		},
		watchForFilter() {
			return [
				this.filteringOn,
				this.filterType,
				this.filterValue1,
				this.filterValue2
			]
		},
		watchForRedraw() {
			return [
				this.trace.overrideData,
				this.trace.color,
				this.minValue,
				this.maxValue,
				this.path,
			]
		}
	},
	mounted() {
		this.$emit('backupfn', this.backupfn);
		if (!this.trace.hasSetData) {
			if (this.trace.color === 'transparent') {
				const h = Math.random() * 360;
				const s = Math.random() * 0.5 + 0.5;
				const l = Math.random() * 0.5 + 0.3; // 0.3 - 0.8
				this.trace.color = `hsl(${h}, ${s * 100}%, ${l * 100}%)`;
			}
			this.$nextTick(() => {
				this.setupDone = true;
			})
			return;
		}

		if (this.trace.hasSetData) {
			this.flagName = this.setData.flagName
			this.autoRangeOn = this.setData.autoRangeOn
			this.filteringOn = this.setData.filteringOn
			this.filterType = this.setData.filterType
			this.filterValue1 = this.setData.filterValue1
			this.filterValue2 = this.setData.filterValue2
			this.minValue = this.setData.minValue
			this.maxValue = this.setData.maxValue
			this.currentModifierName = this.setData.currentModifierName
		}
		this.$nextTick(() => {
			this.setupDone = true;
		})
	},
	methods: {
		applyFilter() {
			if (this.filteringOn && this.flagName) {
				// @ts-expect-error
				const array = this.loadedLog.logData[this.path] as LogDataType;
				switch (this.filterType) {
					case 'pt1':
						{
							this.trace.overrideData = [array[0]];
							let state = this.trace.overrideData[0] || 0;
							if (this.filterValue1 && this.filterValue1 < 0) this.filterValue1 = 0;
							const omega = (2 * Math.PI * (this.filterValue1 || 1)) / this.loadedLog.framesPerSecond;
							const alpha = omega / (1 + omega);
							for (let i = 1; i < array.length; i++) {
								state = state + alpha * (array[i] - state);
								this.trace.overrideData.push(state);
							}
						}
						break;
					case 'pt2':
						{
							this.trace.overrideData = [array[0]];
							let state1 = this.trace.overrideData[0] || 0;
							let state = this.trace.overrideData[0] || 0;
							if (this.filterValue1 && this.filterValue1 < 0) this.filterValue1 = 0;
							const omega = (1.554 * (2 * Math.PI * (this.filterValue1 || 1))) / this.loadedLog.framesPerSecond;
							const alpha = omega / (1 + omega);
							for (let i = 1; i < array.length; i++) {
								state1 = state1 + alpha * (array[i] - state1);
								state = state + alpha * (state1 - state);
								this.trace.overrideData.push(state);
							}
						}
						break;
					case 'pt3':
						{
							this.trace.overrideData = [array[0]];
							let state1 = this.trace.overrideData[0] || 0;
							let state2 = this.trace.overrideData[0] || 0;
							let state = this.trace.overrideData[0] || 0;
							if (this.filterValue1 && this.filterValue1 < 0) this.filterValue1 = 0;
							const omega = (1.961 * (2 * Math.PI * (this.filterValue1 || 1))) / this.loadedLog.framesPerSecond;
							const alpha = omega / (1 + omega);
							for (let i = 1; i < array.length; i++) {
								state1 = state1 + alpha * (array[i] - state1);
								state2 = state2 + alpha * (state1 - state2);
								state = state + alpha * (state2 - state);
								this.trace.overrideData.push(state);
							}
						}
						break;
					case 'sma':
						{
							this.trace.overrideData = [];
							this.filterValue1 = Math.round(this.filterValue1);
							this.filterValue1 = Math.min(this.filterValue1, 100);
							this.filterValue1 = Math.max(this.filterValue1, 1);
							const compFrames = this.filterValue2 ? this.filterValue1 / 2 : 0;
							for (let i = 0; i < array.length; i++) {
								let sum = 0;
								for (let j = 0; j < this.filterValue1; j++) {
									sum += array[Math.round(i - j + compFrames)] || 0; // || {} to prevent undefined => default value will be used
								}
								this.trace.overrideData.push(sum / this.filterValue1);
							}
						}
						break;
					case 'binomial':
						{
							this.trace.overrideData = [];
							const binomialCoeffs = [];
							this.filterValue1 = Math.round(this.filterValue1);
							this.filterValue1 = Math.min(this.filterValue1, 100);
							this.filterValue1 = Math.max(this.filterValue1, 1);
							let binomSum = 0;
							for (let i = 0; i < this.filterValue1; i++) {
								binomialCoeffs.push(getBinomialCoeff(this.filterValue1 - 1, i));
								binomSum += binomialCoeffs[i];
							}
							const compFrames = this.filterValue2 ? this.filterValue1 / 2 : 0;
							for (let i = 0; i < array.length; i++) {
								let sum = 0;
								for (let j = 0; j < this.filterValue1; j++) {
									sum += (array[Math.round(i - j + compFrames)] || 0) * binomialCoeffs[j];
								}
								this.trace.overrideData.push(sum / binomSum);
							}
						}
						break;
				}
			} else if (this.trace.overrideData) delete this.trace.overrideData;
		},
		selectColor() {
			// open color selector from browser
			const picker = document.createElement('input');
			picker.type = 'color';
			picker.value = this.trace.color;
			let timeout = -1
			let color = this.trace.color;
			picker.addEventListener('input', (event) => {
				color = (event.target as HTMLInputElement).value;
				if (timeout !== -1) clearTimeout(timeout);
				timeout = setTimeout(() => {
					this.trace.color = color;
				}, 1000);
			});
			picker.click();
		},
		backupfn() {
			const d: TraceInternalData = {
				autoRangeOn: this.autoRangeOn,
				currentModifierName: this.currentModifierName,
				filteringOn: this.filteringOn,
				filterType: this.filterType,
				filterValue1: this.filterValue1,
				filterValue2: this.filterValue2,
				flagName: this.flagName,
				minValue: this.minValue,
				maxValue: this.maxValue
			}
			return d
		}
	},
	watch: {
		autoRangeOn: {
			handler(newValue) {
				if (newValue && this.setupDone) {
					this.minValue = this.autoRange.min;
					this.maxValue = this.autoRange.max;
				}
			},
		},
		autoRange: {
			handler(newRange) {
				if (this.autoRangeOn && this.setupDone) {
					this.minValue = newRange.min;
					this.maxValue = newRange.max;
				}
			},
		},
		watchForFilter: {
			handler() {
				if (this.setupDone)
					this.applyFilter();
			},
			deep: true,
		},
		flagName: {
			handler() {
				if (this.setupDone) {
					this.currentModifierName = ''
					this.filteringOn = false;
				}
			}
		},
		watchForRedraw: {
			handler() {
				this.trace.path = this.path;
				this.trace.decimals = this.currentModifier?.decimals || this.currentNormalizedFlag?.decimals || 0;
				this.trace.unit = this.currentModifier?.unit || this.currentNormalizedFlag?.unit || '';
				this.trace.states = this.currentModifier?.states || this.currentNormalizedFlag?.states;
				this.trace.displayName = this.currentModifier?.displayName || '';
				this.trace.minValue = this.minValue;
				this.trace.maxValue = this.maxValue;
				if (this.trace.displayName)
					this.trace.displayName = this.currentNormalizedFlag?.name + ' ' + this.trace.displayName;
				else this.trace.displayName = this.currentNormalizedFlag?.name || '';
				this.$emit('update', this.trace);
			},
			deep: true,
		},
	}
})
</script>

<template>
	<div class="wrapper">
		<span class="colorMark" :style="`background-color: ${trace.color}`" @click="selectColor">&nbsp;</span>
		<select id="flagSelector" v-model="flagName">
			<option v-for="flag in availableFlagNames" :value="flag">{{ flagProps[flag].name }}</option>
			<option v-for="flag in availableGenFlagNames" :value="flag">{{ flagProps[genFlagProps[flag].replaces].name
			}} (Gen.)</option>
		</select>
		<select v-if="currentNormalizedFlag?.modifier" id="graphNum" v-model="currentModifierName" style="width: auto">
			<option v-for="m in currentNormalizedFlag.modifier || []" :value="m.path">{{ m.displayNameShort }}</option>
		</select>
		<button class="delete" aria-label="delete trace" @click="$emit('delete')">
			<i class="fa-solid fa-delete-left"></i>
		</button>
		<br />
		<label><input type="checkbox" v-model="autoRangeOn" />Auto</label>&nbsp;
		<input type="number" id="minValue" v-model="minValue" :disabled="autoRangeOn" />
		&nbsp;-
		<input type="number" id="maxValue" v-model="maxValue" :disabled="autoRangeOn" />
		&nbsp;
		<p class="unit">{{ currentNormalizedFlag?.unit || '' }}</p>
		<br />
		<label><input type="checkbox" v-model="filteringOn" /> Filter </label>
		<div v-if="filteringOn">
			<select v-model="filterType" style="width:4rem">
				<option value="pt1">PT1</option>
				<option value="pt2">PT2</option>
				<option value="pt3">PT3</option>
				<option value="sma">SMA</option>
				<option value="binomial">Binomial</option>
			</select>
			<input v-if="filterType === 'pt1'" type="number" id="pt1Cutoff" placeholder="cutoff"
				v-model="filterValue1" />
			<input v-else-if="filterType === 'pt2'" type="number" id="pt2Cutoff" placeholder="cutoff"
				v-model="filterValue1" />
			<input v-else-if="filterType === 'pt3'" type="number" id="pt3Cutoff" placeholder="cutoff"
				v-model="filterValue1" />
			<input v-else-if="filterType === 'sma'" type="number" id="smaN" placeholder="frames"
				v-model="filterValue1" />
			<input v-else-if="filterType === 'binomial'" type="number" id="binomialN" placeholder="frames"
				v-model="filterValue1" />
			<label>
				<input type="checkbox" id="delayCompCheckbox" v-model="filterValue2" />
				Delay Comp.
			</label>
		</div>
	</div>
</template>

<style scoped>
.wrapper {
	line-height: 180%;
	margin-bottom: 0.5rem;
}

select {
	min-width: 8rem;
	appearance: none !important;
	background-color: transparent;
	background: transparent url('data:image/gif;base64,R0lGODlhBgAGAKEDAFVVVX9/f9TU1CgmNyH5BAEKAAMALAAAAAAGAAYAAAIODA4hCDKWxlhNvmCnGwUAOw==') right center no-repeat !important;
	background-position: calc(100% - 5px) center !important;
	border: 1px solid var(--border-color);
	border-radius: 4px;
	padding: 3px 15px 3px 6px;
	color: var(--text-color) !important;
	color: black;
	outline: none;
	text-transform: capitalize;
}

option {
	text-transform: capitalize;
	color: #444;
}

input[type='number'] {
	width: 3rem;
	background-color: transparent;
	border: 1px solid var(--border-color);
	border-radius: 4px;
	padding: 3px 6px;
	color: var(--text-color);
	outline: none;
	text-transform: capitalize;
}

input[disabled] {
	opacity: 0.5;
	cursor: not-allowed;
}

.colorMark {
	display: inline-block;
	width: 1.8rem;
	transform: scale(0.8);
	margin: 0 0.2rem;
}

.delete {
	background-color: transparent;
	border: 1px solid var(--border-color);
	border-radius: 4px;
	padding: 4px 8px;
	color: var(--text-color);
	outline: none;
	float: right;
	transform: translateY(2px);
}

.unit {
	display: inline-block;
	margin: 0;
}
</style>
