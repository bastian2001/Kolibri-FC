<script lang="ts">
	import { createEventDispatcher, onMount } from 'svelte';
	const dispatch = createEventDispatcher();
	type TraceInGraph = {
		flagName: string;
		color: string;
		strokeWidth: number;
		autoRange: boolean;
		minValue: number;
		maxValue: number;
		modifier: any;
	};
	export let flags: string[];
	export let autoRange: { min: number; max: number };
	export let flagProps: { [key: string]: { name: string; unit: string } };

	let autoRangeOn = true;
	let flagName = '';
	let minValue = 0;
	let maxValue = 1;
	let modifier = 0;
	let color = '#000000';
	let autoMin = 0;
	let autoMax = 1;

	$: {
		if (autoRange.min !== autoMin) autoMin = autoRange.min;
		if (autoRange.max !== autoMax) autoMax = autoRange.max;
	}

	let traceInGraph: TraceInGraph;
	$: {
		traceInGraph = {
			flagName,
			color,
			strokeWidth: 1,
			autoRange: autoRangeOn,
			minValue: autoRangeOn ? autoMin || 0 : minValue,
			maxValue: autoRangeOn ? autoMax || 1 : maxValue,
			modifier
		};
		minValue = traceInGraph.minValue;
		maxValue = traceInGraph.maxValue;
		dispatch('update', traceInGraph);
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
		{#each flags as flag}
			<option value={flag}>{flagProps[flag].name}</option>
		{/each}
	</select>
	{#if flagName === 'LOG_MOTOR_OUTPUTS'}
		<select name="graphNum" id="graphNum" bind:value={modifier} style="width: auto">
			{#each ['RR', 'FR', 'RL', 'FL'] as m}
				<option value={m}>{m}</option>
			{/each}
		</select>
	{/if}
	<button
		class="delete"
		on:click={() => {
			dispatch('delete');
		}}>Del</button
	>
	<br />
	<label><input type="checkbox" bind:checked={autoRangeOn} />Auto </label>&nbsp;
	<input
		type="number"
		name="minValue"
		id="minValue"
		bind:value={minValue}
		disabled={autoRangeOn}
	/>&nbsp;-
	<input type="number" name="maxValue" id="maxValue" bind:value={maxValue} disabled={autoRangeOn} />
	&nbsp;
	<p class="unit">{flagProps[flagName]?.unit || ''}</p>
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
