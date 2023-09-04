<script lang="ts">
	import { invoke } from '@tauri-apps/api';
	import type { t } from '@tauri-apps/api/event-41a9edf5';
	import { onMount, onDestroy } from 'svelte';
	import { page } from '$app/stores';

	let devices: string[] = [];

	let interval: number;

	let device = {
		path: '',
		baudRate: 115200,
		connected: false
	};

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
		}
	] as NavigatorElement[];

	let log = ['test log', 'test log 2'] as string[];

	// port.subscribe((p) => {
	// 	device = p;
	// });

	function listDevices() {
		invoke('serial_list').then((d: unknown) => {
			devices = d as string[];
			if (!device.connected) {
				const newDevices: string[] = [];
				for (const dev of devices) {
					if (!devices.includes(dev)) {
						newDevices.push(dev);
					}
				}
				if (newDevices.length == 1) {
					device.path = newDevices[0];
				} else if (newDevices.length > 1) {
					//choose the first non-COM1-device
					for (const dev of newDevices) {
						if (!dev.startsWith('COM1')) {
							device.path = dev;
							break;
						}
					}
				} else if (devices.length == 0) {
					//if current device is not available, select the first available device that is not COM1
					for (const dev of devices) {
						if (dev != 'COM1') {
							device.path = dev;
							break;
						}
					}
				}
			}
		});
	}
	onMount(() => {
		listDevices();
		interval = setInterval(() => {
			listDevices();
		}, 1000);
	});
	onDestroy(() => {
		clearInterval(interval);
	});
	function connect() {
		if (device.connected) return;
		invoke('serial_open', { path: device.path })
			.then(() => {
				device.connected = true;
				console.log('connected');
			})
			.catch((e) => {
				console.log(e);
				disconnect();
			});
	}
	function disconnect() {
		if (!device.connected) return;
		invoke('serial_close').then(() => {
			device.connected = false;
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
			<select bind:value={device.path}>
				{#each devices as d}
					<option value={d}>{d}</option>
				{/each}
			</select>&nbsp;&nbsp;
			{#if device.connected}
				<button on:click={() => disconnect()} class="connectButton"> Disconnect </button>
			{:else if devices.length > 0}
				<button on:click={() => connect()} class="connectButton">Connect</button>
			{:else}
				<span style="color: #888">No devices found</span>
			{/if}
		</div>
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
		display: grid;
		grid-template-rows: 0fr 1fr 0fr;
		height: 100%;
	}

	.header {
		display: flex;
		flex-direction: row;
		align-items: center;
		border-bottom: 3px solid var(--border-color);
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
		color: #888;
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
		border-top: 3px solid var(--border-color);
		color: var(--text-color);
		text-decoration: none;
		background-color: var(--background-color-light);
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
</style>