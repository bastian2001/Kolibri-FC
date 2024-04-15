<script lang="ts">
	import { createEventDispatcher, onMount } from 'svelte';
	import {
		type BBLog,
		type TraceInGraph,
		getNestedProperty,
		type FlagProps,
		type GenFlagProps
	} from '../../utils';

	const dispatch = createEventDispatcher();
	export let flagProps: { [key: string]: FlagProps };
	export let genFlagProps: { [key: string]: GenFlagProps };
	export let log: BBLog;

	let autoRangeOn = true;
	let flagName = '';
	let minValue = 0;
	let maxValue = 1;
	let modifier = '';
	let color = '#000000';
	let autoMin = 0;
	let autoMax = 1;
	let filteringOn = false;
	let filterType: 'pt1' | 'pt2' | 'pt3' | 'sma' | 'binomial' = 'pt1';
	let filterValue1 = 0;
	let filterValue2 = false;

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

	let trace: TraceInGraph = {
		color: 'transparent',
		minValue: 0,
		maxValue: 10,
		flagName: '',
		modifier: '',
		strokeWidth: 1,
		id: 0,
		unit: '',
		states: [],
		displayName: '',
		decimals: 0,
		overrideData: []
	};
	$: {
		const flag = flagProps[flagName];
		const mod = flagProps[flagName]?.modifier?.find((m) => m.path === modifier);
		if (flag) {
			autoMin = mod?.min || flag.minValue || 0;
			autoMax = mod?.max || flag.maxValue || 1;
			if (mod?.rangeFn) {
				const range = mod.rangeFn(log);
				autoMin = range.min;
				autoMax = range.max;
			} else if (flag.rangeFn) {
				const range = flag.rangeFn(log);
				autoMin = range.min;
				autoMax = range.max;
			}
		}
	}
	$: {
		if (flagName) {
			trace.color = color;
			trace.minValue = autoRangeOn ? autoMin || 0 : minValue;
			trace.maxValue = autoRangeOn ? autoMax || 1 : maxValue;
			minValue = trace.minValue;
			maxValue = trace.maxValue;
			trace.flagName = flagName;
			trace.decimals =
				flagProps[flagName].modifier?.find((m) => m.path === modifier)?.decimals ||
				flagProps[flagName].decimals ||
				0;
			trace.unit =
				flagProps[flagName].modifier?.find((m) => m.path === modifier)?.unit ||
				flagProps[flagName].unit ||
				'';
			trace.states = flagProps[flagName].modifier?.find((m) => m.path === modifier)?.states;
			trace.displayName =
				flagProps[flagName].modifier?.find((m) => m.path === modifier)?.displayName || '';
			if (trace.displayName) trace.displayName = flagProps[flagName].name + ' ' + trace.displayName;
			else trace.displayName = flagProps[flagName].name;
			trace.modifier = modifier;
			dispatch('update', trace);
		}
	}

	function applyFilter() {
		if (filteringOn && flagName) {
			const path = flagProps[flagName].path + (modifier ? '.' + modifier : '');
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
		dispatch('update', trace);
	}

	$: filteringOn, flagName, filterType, filterValue1, filterValue2, applyFilter();
	$: flagName, (modifier = '');
	$: flagName, (filteringOn = false);
	$: modifier, (filteringOn = false);

	onMount(() => {
		const h = Math.random() * 360;
		const s = Math.random() * 0.5 + 0.5;
		const l = Math.random() * 0.5 + 0.3; // 0.3 - 0.8
		color = `hsl(${h}, ${s * 100}%, ${l * 100}%)`;
	});
</script>

<div class="wrapper">
	<span class="colorMark" style:background-color={color}>&nbsp;</span>
	<select name="flag" id="flagSelector" bind:value={flagName}>
		{#each log.flags.filter((f) => {
			return f.startsWith('LOG_');
		}) as flag}
			<option value={flag}>{flagProps[flag].name}</option>
		{/each}
		{#each log.flags.filter((f) => {
			return f.startsWith('GEN_');
		}) as flag}
			<option value={flag}>{genFlagProps[flag].name} (Gen.)</option>
		{/each}
	</select>
	{#if flagProps[flagName]?.modifier}
		<select name="graphNum" id="graphNum" bind:value={modifier} style="width: auto">
			{#each flagProps[flagName].modifier || [] as m}
				<option value={m.path}>{m.displayNameShort}</option>
			{/each}
		</select>
	{/if}
	<button
		class="delete"
		on:click={() => {
			dispatch('delete');
		}}><i class="fa-solid fa-delete-left"></i></button
	>
	<br />
	<label><input type="checkbox" bind:checked={autoRangeOn} /> Auto </label>&nbsp;
	<input
		type="number"
		name="minValue"
		id="minValue"
		bind:value={minValue}
		disabled={autoRangeOn}
	/>&nbsp;-
	<input type="number" name="maxValue" id="maxValue" bind:value={maxValue} disabled={autoRangeOn} />
	&nbsp;
	<p class="unit">{flagProps[flagName]?.unit || genFlagProps[flagName]?.unit || ''}</p>
	<br />
	<label><input type="checkbox" bind:checked={filteringOn} /> Filtering</label>
	{#if filteringOn}
		<select bind:value={filterType} style="width:5rem">
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
				class="val1Input"
			/>
		{:else if filterType === 'pt2'}
			<input
				type="number"
				name="pt2Cutoff"
				id="pt2Cutoff"
				placeholder="cutoff"
				bind:value={filterValue1}
				class="val1Input"
			/>
		{:else if filterType === 'pt3'}
			<input
				type="number"
				name="pt3Cutoff"
				id="pt3Cutoff"
				placeholder="cutoff"
				bind:value={filterValue1}
				class="val1Input"
			/>
		{:else if filterType === 'sma'}
			<input
				type="number"
				name="smaN"
				id="smaN"
				placeholder="frames"
				bind:value={filterValue1}
				class="val1Input"
			/>
		{:else if filterType === 'binomial'}
			<input
				type="number"
				name="binomialN"
				id="binomialN"
				placeholder="frames"
				bind:value={filterValue1}
				class="val1Input"
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

	.val1Input {
		width: 5rem;
	}
</style>
