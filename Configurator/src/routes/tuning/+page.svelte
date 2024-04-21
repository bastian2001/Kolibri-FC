<script lang="ts">
	import { port, type Command, ConfigCmd } from '../../portStore';
	import { onDestroy, onMount } from 'svelte';
	import { leBytesToInt } from '../../utils';
	import { error } from '@sveltejs/kit';

	let pids = [[], [], []] as number[][];
	let rateFactors = [[], [], []] as number[][];

	let saveTimeout = 0;

	const unsubscribe = port.subscribe(command => {
		if (command.cmdType === 'response') {
			switch (command.command) {
				case ConfigCmd.GET_PIDS:
					if (command.length !== 3 * 2 * 7) break;
					for (let ax = 0; ax < 3; ax++) {
						for (let i = 0; i < 6; i++)
							pids[ax][i] = leBytesToInt(
								command.data.slice(ax * 14 + i * 2, ax * 14 + i * 2 + 2),
								false
							);
						pids[ax][5] /= 65536;
						pids[ax][5] = Math.round(pids[ax][5] * 10000) / 10000;
					}
					break;
				case ConfigCmd.GET_RATES:
					if (command.length !== 5 * 2 * 3) break;
					for (let ax = 0; ax < 3; ax++) {
						for (let i = 0; i < 5; i++)
							rateFactors[ax][i] = leBytesToInt(
								command.data.slice(ax * 10 + i * 2, ax * 10 + i * 2 + 2)
							);
					}
					break;
				case ConfigCmd.SET_PIDS:
					const data = [];
					for (let ax = 0; ax < 3; ax++)
						for (let i = 0; i < 5; i++)
							data.push(rateFactors[ax][i] & 0xff, (rateFactors[ax][i] >> 8) & 0xff);
					port.sendCommand('request', ConfigCmd.SET_RATES, data);
					break;
				case ConfigCmd.SET_RATES:
					port.sendCommand('request', ConfigCmd.SAVE_SETTINGS);
					break;
				case ConfigCmd.SAVE_SETTINGS:
					clearTimeout(saveTimeout);
					break;
			}
		}
	});
	function getSettings() {
		port.sendCommand('request', ConfigCmd.GET_PIDS).then(() => {
			port.sendCommand('request', ConfigCmd.GET_RATES);
		});
	}
	onMount(() => {
		getSettings();
		port.addOnConnectHandler(getSettings);
	});
	onDestroy(() => {
		port.removeOnConnectHandler(getSettings);
		unsubscribe();
	});

	function scrollInputPID(e: WheelEvent, i: number, j: number) {
		let val = 1;
		if (j === 5) val = 0.0001;
		if (e.deltaY > 0) pids[i][j] -= val;
		else pids[i][j] += val;
		pids[i][j] = Math.round(pids[i][j] * 10000) / 10000;
		if (pids[i][j] < 0) pids[i][j] = 0;
		if (j === 5 && pids[i][j] > 1) pids[i][j] = 1;
	}

	function scrollInputRate(e: WheelEvent, i: number, j: number) {
		let val = 5;
		if (e.deltaY > 0) rateFactors[i][j] -= val;
		else rateFactors[i][j] += val;
		if (rateFactors[i][j] < 0) rateFactors[i][j] = 0;
		if (j === 5 && rateFactors[i][j] > 1) rateFactors[i][j] = 1;
	}
	function saveSettings() {
		const data = [];
		for (let ax = 0; ax < 3; ax++) {
			for (let i = 0; i < 5; i++) data.push(pids[ax][i] & 0xff, (pids[ax][i] >> 8) & 0xff);
			data.push(
				Math.round(pids[ax][5] * 65536) & 0xff,
				(Math.round(pids[ax][5] * 65536) >> 8) & 0xff
			);
			data.push(0, 0);
		}
		port.sendCommand('request', ConfigCmd.SET_PIDS, data).then(() => {
			saveTimeout = setTimeout(() => {
				console.error('Save timeout');
			}, 500);
		});
	}
</script>

<div class="wrapper">
	<div class="header">
		<button class="saveBtn" on:click={saveSettings}>Save Settings</button>
	</div>
	<div class="pids">
		<h3>PID Gains</h3>
		<table>
			<tr>
				<th>&nbsp;</th>
				<th>P</th>
				<th>I</th>
				<th>D</th>
				<th>FF</th>
				<th>S</th>
				<th>iFall</th>
			</tr>
			{#each pids as ax, i}
				<tr>
					<td>{['Roll', 'Pitch', 'Yaw'][i]}</td>
					{#each ax as val, j}
						<td
							><input
								type="number"
								bind:value={pids[i][j]}
								on:wheel={e => {
									scrollInputPID(e, i, j);
								}}
							/></td
						>
					{/each}
				</tr>
			{/each}
		</table>
	</div>
	<div class="rates">
		<h3>Rate Factors</h3>
		<table>
			<tr>
				<th>&nbsp;</th>
				<th>x^1</th>
				<th>x^2</th>
				<th>x^3</th>
				<th>x^4</th>
				<th>x^5</th>
			</tr>
			{#each rateFactors as ax, i}
				<tr>
					<td>{['Roll', 'Pitch', 'Yaw'][i]}</td>
					{#each ax as val, j}
						<td
							><input
								type="number"
								bind:value={rateFactors[i][j]}
								on:wheel={e => {
									scrollInputRate(e, i, j);
								}}
							/></td
						>
					{/each}
				</tr>
			{/each}
		</table>
	</div>
</div>

<style>
	.wrapper {
		display: grid;
		grid-template-columns: repeat(auto-fit, minmax(500px, 1fr));
		margin: 1.5rem;
		gap: 0rem 3rem;
	}
	.header {
		grid-column: 1 / -1;
	}
	.saveBtn {
		float: right;
		background-color: transparent;
		border: 1px solid var(--border-green);
		border-radius: 5px;
		padding: 0.5rem 1rem;
		font-size: 1rem;
		color: var(--text-color);
		transition: background-color 0.2s ease-out;
	}
	.saveBtn:hover {
		background-color: #fff1;
	}
	th,
	td {
		text-align: right;
		padding: 3px 8px;
	}
	input {
		width: 100%;
		background-color: transparent;
		border: none;
		border-bottom: 1px solid var(--border-color);
		text-align: right;
		padding: 3px;
		outline: none;
	}
	input::-webkit-outer-spin-button,
	input::-webkit-inner-spin-button {
		-webkit-appearance: none;
		margin: 0;
	}
</style>
