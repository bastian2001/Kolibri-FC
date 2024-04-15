<script lang="ts">
	import { createEventDispatcher, onMount } from 'svelte';
	import { type BBLog, type TraceInGraph, getNestedProperty } from '../../utils';

	const dispatch = createEventDispatcher();
	export let flags: string[];
	export let autoRange: { min: number; max: number };
	export let flagProps: { [key: string]: { name: string; unit: string; path: string } };
	export let genFlagProps: { [key: string]: { name: string; unit: string } };
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
	const gpsModifier = [
		{
			displayNameShort: 'Year',
			displayName: 'Year',
			min: 2020,
			max: 2030,
			path: 'year'
		},
		{
			displayNameShort: 'Month',
			displayName: 'Month',
			min: 1,
			max: 12,
			path: 'month',
			states: [
				'',
				'January',
				'February',
				'March',
				'April',
				'May',
				'June',
				'July',
				'August',
				'September',
				'October',
				'November',
				'December'
			]
		},
		{
			displayNameShort: 'Day',
			displayName: 'Day',
			min: 1,
			max: 31,
			path: 'day'
		},
		{
			displayNameShort: 'Hour',
			displayName: 'Hour',
			min: 0,
			max: 23,
			path: 'hour'
		},
		{
			displayNameShort: 'Minute',
			displayName: 'Minute',
			min: 0,
			max: 59,
			path: 'minute'
		},
		{
			displayNameShort: 'Second',
			displayName: 'Second',
			min: 0,
			max: 59,
			path: 'second'
		},
		{
			displayNameShort: 'Valid',
			displayName: 'Validity Flags',
			min: 0,
			max: 255,
			path: 'time_validity_flags'
		},
		{
			displayNameShort: 'T Acc',
			displayName: 'Time Accuracy',
			min: 0,
			max: 100,
			path: 't_acc',
			unit: 'ns'
		},
		{
			displayNameShort: 'Nanosec',
			displayName: 'Nanoseconds',
			min: 0,
			max: 1e9,
			path: 'ns',
			unit: 'ns'
		},
		{
			displayNameShort: 'Fix',
			displayName: 'Fix Type',
			min: 0,
			max: 5,
			path: 'fix_type',
			states: [
				'No Fix',
				'Dead reckoning only',
				'2D Fix',
				'3D Fix',
				'GPS + Dead reckoning',
				'Time only fix'
			]
		},
		{
			displayNameShort: 'Flags',
			displayName: 'Flags',
			min: 0,
			max: 255,
			path: 'flags'
		},
		{
			displayNameShort: 'Flags2',
			displayName: 'Flags2',
			min: 0,
			max: 255,
			path: 'flags2'
		},
		{
			displayNameShort: 'Sats',
			displayName: 'Satellite Count',
			min: 0,
			max: 30,
			path: 'sat_count'
		},
		{
			displayNameShort: 'Lon',
			displayName: 'Longitude',
			min: -180,
			max: 180,
			path: 'lon',
			unit: '°',
			decimals: 7
		},
		{
			displayNameShort: 'Lat',
			displayName: 'Latitude',
			min: -90,
			max: 90,
			path: 'lat',
			unit: '°',
			decimals: 7
		},
		{
			displayNameShort: 'Alt',
			displayName: 'Altitude',
			min: -0,
			max: 1000,
			path: 'alt',
			unit: 'm',
			decimals: 2
		},
		{
			displayNameShort: 'Hor Acc',
			displayName: 'Horizontal Accuracy',
			min: 0,
			max: 20,
			path: 'h_acc',
			unit: 'm',
			decimals: 2
		},
		{
			displayNameShort: 'Ver Acc',
			displayName: 'Vertical Accuracy',
			min: 0,
			max: 20,
			path: 'v_acc',
			unit: 'm',
			decimals: 2
		},
		{
			displayNameShort: 'Vel N',
			displayName: 'Velocity North',
			min: -50,
			max: 50,
			path: 'vel_n',
			unit: 'm/s',
			decimals: 2
		},
		{
			displayNameShort: 'Vel E',
			displayName: 'Velocity East',
			min: -50,
			max: 50,
			path: 'vel_e',
			unit: 'm/s',
			decimals: 2
		},
		{
			displayNameShort: 'Vel D',
			displayName: 'Velocity Down',
			min: -50,
			max: 50,
			path: 'vel_d',
			unit: 'm/s',
			decimals: 2
		},
		{
			displayNameShort: 'G Speed',
			displayName: 'Ground Speed',
			min: 0,
			max: 50,
			path: 'g_speed',
			unit: 'm/s',
			decimals: 2
		},
		{
			displayNameShort: 'Head Mot',
			displayName: 'Heading of Motion',
			min: -180,
			max: 180,
			path: 'head_mot',
			unit: '°'
		},
		{
			displayNameShort: 'S Acc',
			displayName: 'Speed Accuracy',
			min: 0,
			max: 10,
			path: 's_acc',
			unit: 'm/s',
			decimals: 2
		},
		{
			displayNameShort: 'Head Acc',
			displayName: 'Heading Accuracy',
			min: 0,
			max: 20,
			path: 'head_acc',
			unit: '°',
			decimals: 2
		},
		{
			displayNameShort: 'pDop',
			displayName: 'pDop',
			min: 0,
			max: 100,
			path: 'p_dop',
			decimals: 2
		},
		{
			displayNameShort: 'Flags3',
			displayName: 'Flags3',
			min: 0,
			max: 31,
			path: 'flags3'
		}
	];

	$: {
		if (autoRange.min !== autoMin) autoMin = autoRange.min;
		if (autoRange.max !== autoMax) autoMax = autoRange.max;
	}

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

	let traceInGraph: TraceInGraph = {} as TraceInGraph;
	$: {
		traceInGraph.flagName = flagName;
		traceInGraph.color = color;
		traceInGraph.minValue = autoRangeOn ? autoMin || 0 : minValue;
		traceInGraph.maxValue = autoRangeOn ? autoMax || 1 : maxValue;
		minValue = traceInGraph.minValue;
		maxValue = traceInGraph.maxValue;
		traceInGraph.decimals = gpsModifier.find((m) => m.path === modifier)?.decimals;
		traceInGraph.unit = gpsModifier.find((m) => m.path === modifier)?.unit;
		traceInGraph.states = gpsModifier.find((m) => m.path === modifier)?.states;
		traceInGraph.displayName = gpsModifier.find((m) => m.path === modifier)?.displayName;
		if (traceInGraph.displayName)
			traceInGraph.displayName = flagProps[flagName].name + ' ' + traceInGraph.displayName;
		traceInGraph.modifier = modifier;
		if (filteringOn) {
			const path = flagProps[flagName].path + (modifier ? '.' + modifier : '');
			switch (filterType) {
				case 'pt1':
					{
						traceInGraph.overrideData = [getNestedProperty(log.frames[0], path)];
						let state = traceInGraph.overrideData[0] || 0;
						if (filterValue1 && filterValue1 < 0) filterValue1 = 0;
						const omega = (2 * Math.PI * (filterValue1 || 0.1)) / log.framesPerSecond;
						const alpha = omega / (1 + omega);
						for (let i = 1; i < log.frames.length; i++) {
							state =
								state +
								alpha * (getNestedProperty(log.frames[i], path, { defaultValue: 0 }) - state);
							traceInGraph.overrideData.push(state);
						}
					}
					break;
				case 'pt2':
					{
						traceInGraph.overrideData = [getNestedProperty(log.frames[0], path)];
						let state1 = traceInGraph.overrideData[0] || 0;
						let state = traceInGraph.overrideData[0] || 0;
						if (filterValue1 && filterValue1 < 0) filterValue1 = 0;
						const omega = (1.554 * (2 * Math.PI * (filterValue1 || 0.1))) / log.framesPerSecond;
						const alpha = omega / (1 + omega);
						for (let i = 1; i < log.frames.length; i++) {
							state1 =
								state1 +
								alpha * (getNestedProperty(log.frames[i], path, { defaultValue: 0 }) - state1);
							state = state + alpha * (state1 - state);
							traceInGraph.overrideData.push(state);
						}
					}
					break;
				case 'pt3':
					{
						traceInGraph.overrideData = [getNestedProperty(log.frames[0], path)];
						let state1 = traceInGraph.overrideData[0] || 0;
						let state2 = traceInGraph.overrideData[0] || 0;
						let state = traceInGraph.overrideData[0] || 0;
						if (filterValue1 && filterValue1 < 0) filterValue1 = 0;
						const omega = (1.961 * (2 * Math.PI * (filterValue1 || 0.1))) / log.framesPerSecond;
						const alpha = omega / (1 + omega);
						for (let i = 1; i < log.frames.length; i++) {
							state1 =
								state1 +
								alpha * (getNestedProperty(log.frames[i], path, { defaultValue: 0 }) - state1);
							state2 = state2 + alpha * (state1 - state2);
							state = state + alpha * (state2 - state);
							traceInGraph.overrideData.push(state);
						}
					}
					break;
				case 'sma':
					{
						traceInGraph.overrideData = [];
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
							traceInGraph.overrideData.push(sum / filterValue1);
						}
					}
					break;
				case 'binomial':
					{
						traceInGraph.overrideData = [];
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
							traceInGraph.overrideData.push(sum / binomSum);
						}
					}
					break;
			}
		} else if (traceInGraph.overrideData) delete traceInGraph.overrideData;
		dispatch('update', traceInGraph);
	}

	$: flagName, (modifier = '');
	$: {
		const auto = {
			min: gpsModifier.find((m) => m.path === modifier)?.min,
			max: gpsModifier.find((m) => m.path === modifier)?.max
		};
		if (auto.min !== undefined && auto.max !== undefined) dispatch('overrideAuto', auto);
		else dispatch('overrideAuto', undefined);
	}

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
		{#each flags.filter((f) => {
			return f.startsWith('LOG_');
		}) as flag}
			<option value={flag}>{flagProps[flag].name}</option>
		{/each}
		{#each flags.filter((f) => {
			return f.startsWith('GEN_');
		}) as flag}
			<option value={flag}>{genFlagProps[flag].name} (Gen.)</option>
		{/each}
	</select>
	{#if flagName === 'LOG_MOTOR_OUTPUTS' || flagName === 'GEN_MOTOR_OUTPUTS' || flagName === 'LOG_MOTOR_RPM'}
		<select name="graphNum" id="graphNum" bind:value={modifier} style="width: auto">
			{#each ['RR', 'FR', 'RL', 'FL'] as m}
				<option value={m}>{m}</option>
			{/each}
		</select>
	{/if}
	{#if flagName === 'LOG_GPS'}
		<select name="gpsModifier" id="gpsModifier" bind:value={modifier} style="width: 4.5rem">
			{#each gpsModifier as m}
				<option value={m.path}>{m.displayNameShort}</option>
			{/each}
		</select>
	{/if}
	{#if flagName === 'LOG_ACCEL_RAW' || flagName === 'LOG_ACCEL_FILTERED'}
		<select name="accelModifier" id="accelModifier" bind:value={modifier} style="width: 4.5rem">
			{#each ['X', 'Y', 'Z'] as m}
				<option value={m}>{m}</option>
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
