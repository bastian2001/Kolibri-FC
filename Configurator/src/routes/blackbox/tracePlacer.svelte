<script lang="ts">
	import { onMount, tick } from 'svelte';
	import {
		type BBLog,
		type TraceInGraph,
		getNestedProperty,
		type FlagProps,
		type GenFlagProps
	} from '../../utils';

	interface Props {
		flagProps: { [key: string]: FlagProps };
		genFlagProps: { [key: string]: GenFlagProps };
		log: BBLog;
		update: () => void;
		delete: () => void;
		trace: TraceInGraph;
	}

	let { flagProps, genFlagProps, log, update, delete: del, trace = $bindable() }: Props = $props();

	let autoRangeOn = $state(true);
	let currentModifierName = $state('');
	let filteringOn = $state(false);
	let filterType: 'pt1' | 'pt2' | 'pt3' | 'sma' | 'binomial' = $state('pt1');
	let filterValue1 = $state(0);
	let filterValue2 = $state(false);
	let flagName = $state('');

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

	let availableFlagNames = $derived(
		log.flags.filter(f => {
			return f.startsWith('LOG_');
		})
	);
	let availableGenFlagNames = $derived(
		log.flags.filter(f => {
			return f.startsWith('GEN_');
		})
	);

	let currentFlag = $derived.by(() => {
		if (flagName.startsWith('LOG_')) {
			return flagProps[flagName];
		} else if (flagName.startsWith('GEN_')) {
			return genFlagProps[flagName];
		}
		return undefined;
	});
	let currentNormalizedFlag = $derived.by(() => {
		if (flagName.startsWith('LOG_')) {
			return flagProps[flagName];
		} else if (flagName.startsWith('GEN_')) {
			return flagProps[genFlagProps[flagName].replaces];
		}
		return undefined;
	});
	let currentModifier = $derived(
		currentNormalizedFlag?.modifier?.find(m => m.path === currentModifierName)
	);

	let autoRange = $derived.by(() => {
		let range = { min: 0, max: 1 };
		if (currentNormalizedFlag) {
			range.min = currentModifier?.min || currentNormalizedFlag.minValue || 0;
			range.max = currentModifier?.max || currentNormalizedFlag.maxValue || 1;
			if (currentModifier?.rangeFn) {
				range = currentModifier.rangeFn(log);
			} else if (currentNormalizedFlag.rangeFn) {
				range = currentNormalizedFlag.rangeFn(log);
			}
		}
		return range;
	});
	let minValue = $derived(autoRange.min); // auto derived, but overridable by the user
	let maxValue = $derived(autoRange.max); // auto derived, but overridable by the user

	let lastMin = -1;
	let lastMax = -1;
	let path = '';
	$effect(() => {
		// update trace => draw canvas
		// any changes to any of the following will trigger a redraw
		// values are cached to prevent loops

		// update range
		if (lastMin !== minValue || lastMax !== maxValue) {
			lastMin = minValue;
			lastMax = maxValue;
			trace.minValue = minValue;
			trace.maxValue = maxValue;
		}

		// update path and other fixed properties
		let newPath = '';
		if (currentNormalizedFlag?.path) {
			if (currentModifierName) {
				newPath = currentNormalizedFlag.path + '.' + currentModifierName.toLowerCase();
			} else {
				newPath = currentNormalizedFlag.path;
			}
		} else {
			newPath = '';
		}
		if (path !== newPath) {
			// i.e. if modifier or flag changed, update unit etc.
			path = newPath;
			trace.path = newPath;
			trace.decimals = currentModifier?.decimals || currentNormalizedFlag?.decimals || 0;
			trace.unit = currentModifier?.unit || currentNormalizedFlag?.unit || '';
			trace.states = currentModifier?.states || currentNormalizedFlag?.states;
			trace.displayName = currentModifier?.displayName || '';
			if (trace.displayName)
				trace.displayName = currentNormalizedFlag?.name + ' ' + trace.displayName;
			else trace.displayName = currentNormalizedFlag?.name || '';
		}

		update();
	});

	function applyFilter() {
		if (filteringOn && flagName) {
			switch (filterType) {
				case 'pt1':
					{
						trace.overrideData = [getNestedProperty(log.frames[0], path)];
						let state = trace.overrideData[0] || 0;
						if (filterValue1 && filterValue1 < 0) filterValue1 = 0;
						const omega = (2 * Math.PI * (filterValue1 || 1)) / log.framesPerSecond;
						const alpha = omega / (1 + omega);
						for (let i = 1; i < log.frames.length; i++) {
							state =
								state +
								alpha * (getNestedProperty(log.frames[i], path, { defaultValue: 0 }) - state);
							trace.overrideData.push(state);
						}
					}
					break;
				case 'pt2':
					{
						trace.overrideData = [getNestedProperty(log.frames[0], path)];
						let state1 = trace.overrideData[0] || 0;
						let state = trace.overrideData[0] || 0;
						if (filterValue1 && filterValue1 < 0) filterValue1 = 0;
						const omega = (1.554 * (2 * Math.PI * (filterValue1 || 1))) / log.framesPerSecond;
						const alpha = omega / (1 + omega);
						for (let i = 1; i < log.frames.length; i++) {
							state1 =
								state1 +
								alpha * (getNestedProperty(log.frames[i], path, { defaultValue: 0 }) - state1);
							state = state + alpha * (state1 - state);
							trace.overrideData.push(state);
						}
					}
					break;
				case 'pt3':
					{
						trace.overrideData = [getNestedProperty(log.frames[0], path)];
						let state1 = trace.overrideData[0] || 0;
						let state2 = trace.overrideData[0] || 0;
						let state = trace.overrideData[0] || 0;
						if (filterValue1 && filterValue1 < 0) filterValue1 = 0;
						const omega = (1.961 * (2 * Math.PI * (filterValue1 || 1))) / log.framesPerSecond;
						const alpha = omega / (1 + omega);
						for (let i = 1; i < log.frames.length; i++) {
							state1 =
								state1 +
								alpha * (getNestedProperty(log.frames[i], path, { defaultValue: 0 }) - state1);
							state2 = state2 + alpha * (state1 - state2);
							state = state + alpha * (state2 - state);
							trace.overrideData.push(state);
						}
					}
					break;
				case 'sma':
					{
						trace.overrideData = [];
						filterValue1 = Math.round(filterValue1);
						filterValue1 = Math.min(filterValue1, 100);
						filterValue1 = Math.max(filterValue1, 1);
						const compFrames = filterValue2 ? filterValue1 / 2 : 0;
						for (let i = 0; i < log.frames.length; i++) {
							let sum = 0;
							for (let j = 0; j < filterValue1; j++) {
								sum += getNestedProperty(log.frames[Math.round(i - j + compFrames)] || {}, path, {
									defaultValue: 0
								}); // || {} to prevent undefined => default value will be used
							}
							trace.overrideData.push(sum / filterValue1);
						}
					}
					break;
				case 'binomial':
					{
						trace.overrideData = [];
						const binomialCoeffs = [];
						filterValue1 = Math.round(filterValue1);
						filterValue1 = Math.min(filterValue1, 100);
						filterValue1 = Math.max(filterValue1, 1);
						let binomSum = 0;
						for (let i = 0; i < filterValue1; i++) {
							binomialCoeffs.push(getBinomialCoeff(filterValue1 - 1, i));
							binomSum += binomialCoeffs[i];
						}
						const compFrames = filterValue2 ? filterValue1 / 2 : 0;
						for (let i = 0; i < log.frames.length; i++) {
							let sum = 0;
							for (let j = 0; j < filterValue1; j++) {
								sum +=
									getNestedProperty(log.frames[Math.round(i - j + compFrames)] || {}, path, {
										defaultValue: 0
									}) * binomialCoeffs[j];
							}
							trace.overrideData.push(sum / binomSum);
						}
					}
					break;
			}
		} else if (trace.overrideData) delete trace.overrideData;
		update();
	}

	$effect(() => {
		filteringOn;
		filterType;
		filterValue1;
		filterValue2;
		// tick to remove looping reactivity, the values above act as triggers
		tick().then(() => {
			applyFilter();
		});
	});
	$effect(() => {
		flagName;
		currentModifierName = '';
	});
	$effect(() => {
		currentModifierName;
		filteringOn = false;
	});

	onMount(() => {
		const h = Math.random() * 360;
		const s = Math.random() * 0.5 + 0.5;
		const l = Math.random() * 0.5 + 0.3; // 0.3 - 0.8
		trace.color = `hsl(${h}, ${s * 100}%, ${l * 100}%)`;
	});
</script>

<div class="wrapper">
	<span class="colorMark" style:background-color={trace.color}>&nbsp;</span>
	<select name="flag" id="flagSelector" bind:value={flagName}>
		{#each availableFlagNames as flag}
			<option value={flag}>{flagProps[flag].name}</option>
		{/each}
		{#each availableGenFlagNames as flag}
			<option value={flag}>{genFlagProps[flag].name} (Gen.)</option>
		{/each}
	</select>
	{#if currentNormalizedFlag?.modifier}
		<select name="graphNum" id="graphNum" bind:value={currentModifierName} style="width: auto">
			{#each currentNormalizedFlag.modifier || [] as m}
				<option value={m.path}>{m.displayNameShort}</option>
			{/each}
		</select>
	{/if}
	<button class="delete" aria-label="delete trace" onclick={del}>
		<i class="fa-solid fa-delete-left"></i>
	</button>
	<br />
	<label><input type="checkbox" bind:checked={autoRangeOn} /> Auto </label>&nbsp;
	<input type="number" name="minValue" id="minValue" bind:value={minValue} disabled={autoRangeOn} />
	&nbsp;-
	<input type="number" name="maxValue" id="maxValue" bind:value={maxValue} disabled={autoRangeOn} />
	&nbsp;
	<p class="unit">{currentNormalizedFlag?.unit || ''}</p>
	<br />
	<label><input type="checkbox" bind:checked={filteringOn} /> Filter </label>
	{#if filteringOn}
		<select bind:value={filterType} style="width:4rem">
			<option value="pt1">PT1</option>
			<option value="pt2">PT2</option>
			<option value="pt3">PT3</option>
			<option value="sma">SMA</option>
			<option value="binomial">Binomial</option>
		</select>
		{#if filterType === 'pt1'}
			<input
				type="number"
				name="pt1Cutoff"
				id="pt1Cutoff"
				placeholder="cutoff"
				bind:value={filterValue1}
			/>
		{:else if filterType === 'pt2'}
			<input
				type="number"
				name="pt2Cutoff"
				id="pt2Cutoff"
				placeholder="cutoff"
				bind:value={filterValue1}
			/>
		{:else if filterType === 'pt3'}
			<input
				type="number"
				name="pt3Cutoff"
				id="pt3Cutoff"
				placeholder="cutoff"
				bind:value={filterValue1}
			/>
		{:else if filterType === 'sma'}
			<input type="number" name="smaN" id="smaN" placeholder="frames" bind:value={filterValue1} />
		{:else if filterType === 'binomial'}
			<input
				type="number"
				name="binomialN"
				id="binomialN"
				placeholder="frames"
				bind:value={filterValue1}
			/>
		{/if}
		<label>
			<input
				type="checkbox"
				name="delayComp"
				id="delayCompCheckbox"
				bind:checked={filterValue2}
			/>Delay Comp.</label
		>
	{/if}
</div>

<style>
	.wrapper {
		line-height: 180%;
		margin-bottom: 0.5rem;
	}
	select {
		width: 8rem;
		background-color: transparent;
		border: 1px solid var(--border-color);
		border-radius: 4px;
		padding: 3px 6px;
		color: var(--text-color);
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
