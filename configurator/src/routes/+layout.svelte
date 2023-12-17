<script lang="ts">
	import { invoke } from '@tauri-apps/api';
	import { onMount, onDestroy } from 'svelte';
	import { page } from '$app/stores';
	import { port, ConfigCmd } from '../stores';
	import type { Command } from '../stores';
	import { leBytesToInt } from '../utils';

	let devices: any = [];

	let listInterval: number;

	let battery = '';

	let device: any;
	let connected = false;

	type NavigatorElement = {
		name: string;
		path: string;
		img?: string;
	};

	const navElements = [
		{
			name: 'Home',
			path: '/'
		},
		{
			name: 'Blackbox',
			path: '/blackbox'
		},
		{
			name: 'Tasks',
			path: '/tasks'
		},
		{
			name: 'OSD',
			path: '/osd'
		},
		{
			name: 'Motors',
			path: '/motors'
		},
		{
			name: 'Tuning',
			path: '/tuning'
		}
	] as NavigatorElement[];

	let log = [] as string[];

	$: handleCommand($port);
	function handleCommand(command: Command) {
		switch (command.command) {
			case ConfigCmd.STATUS | 0x4000:
				battery = `${leBytesToInt(command.data.slice(0, 2)) / 100}V`;
				break;
			case ConfigCmd.IND_MESSAGE:
				log = [...log, command.dataStr];
			case ConfigCmd.PLAY_SOUND | 0x4000:
				console.log(command.data);
				break;
		}
	}

	function listDevices() {
		devices = port.getDevices();
	}
	onMount(() => {
		listInterval = setInterval(() => {
			listDevices();
		}, 1000);
	});
	onDestroy(() => {
		clearInterval(listInterval);
		disconnect();
	});
	function connect() {
		port
			.connect(device)
			.then(() => {
				connected = true;
			})
			.catch(() => {
				connected = false;
			});
	}
	function disconnect() {
		port
			.disconnect()
			.then(() => {
				connected = false;
			})
			.catch(() => {
				connected = false;
			});
	}
</script>

<div class="main">
	<div class="header">
		<img
			src="https://svelte.dev/svelte-logo-horizontal.svg"
			alt="Svelte logo"
			class="kolibriLogo"
		/>
		<div class="space" />
		<div class="connector">
			<select bind:value={device}>
				{#each devices as d}
					<option value={d}>{d}</option>
				{/each}
			</select>&nbsp;&nbsp;
			{#if connected}
				<button on:click={() => disconnect()} class="connectButton">Disconnect</button>
			{:else if devices.length > 0}
				<button on:click={() => connect()} class="connectButton">Connect</button>
			{:else}
				<span style="color: #888">No devices found</span>
			{/if}&nbsp;&nbsp;
		</div>
		{#if connected}
			<div class="battery">
				<p>Battery: {battery}</p>
			</div>
		{/if}
		<div class="log">
			{#each log as l}<p>{l}</p>{/each}
		</div>
	</div>

	<div class="pageContent">
		<slot />
	</div>
	<div class="navigator">
		<div class="navElement grow" />
		{#each navElements as el}
			<a href={el.path} class="navElement" class:currentPage={$page.route.id === el.path}>
				{#if el.img}
					<img src={el.img} alt={`${el.name} Icon`} class="pageIcon" />
				{/if}
				<p class="pageName">{el.name}</p>
			</a>
		{/each}
		<div class="navElement grow" />
	</div>
</div>

<style>
	.main {
		display: flex;
		flex-direction: column;
		height: 100vh;
		overflow: hidden;
	}

	.header {
		display: flex;
		flex-direction: row;
		align-items: center;
		border-bottom: 3px solid var(--border-blue);
	}
	.kolibriLogo {
		height: 3rem;
	}

	.space {
		flex-grow: 1;
	}

	.connector {
		display: flex;
		flex-direction: row;
		align-items: center;
	}

	.connector select {
		width: 8rem;
		background-color: transparent;
		border: 1px solid var(--border-color);
		border-radius: 4px;
		padding: 3px 6px;
		color: var(--text-color);
		outline: none;
	}
	.connector select option {
		color: #444;
	}

	.connectButton {
		background-color: transparent;
		border: 1px solid var(--border-color);
		border-radius: 4px;
		padding: 4px 8px;
		color: var(--text-color);
		outline: none;
	}
	.log {
		flex-grow: 1;
		min-width: 250px;
		margin-left: 1rem;
		background-color: var(--background-color-light);
		height: 100%;
		padding: 0 0.5rem;
		color: var(--text-color);
		font-size: 0.8rem;
	}
	.log p {
		margin: 3px 0;
		padding: 0;
	}

	.navigator {
		display: flex;
		flex-direction: row;
		align-items: center;
		justify-content: center;
	}
	.navElement {
		display: flex;
		flex-direction: column;
		align-items: center;
		justify-content: center;
		padding: 0.5rem;
		border-top: 3px solid var(--border-blue);
		color: var(--text-color);
		text-decoration: none;
		background-color: var(--background-blue);
	}
	.navElement.currentPage {
		border-top: 3px solid transparent;
		background-color: var(--background-color);
	}
	.pageName {
		margin: 0;
		padding: 0;
	}
	.navElement.grow {
		flex-grow: 1;
		height: 100%;
		box-sizing: border-box;
	}
	.pageContent {
		flex-shrink: 1;
		flex-grow: 1;
		overflow: hidden;
	}

	.battery p {
		margin: 0;
		padding: 0;
	}
</style>
