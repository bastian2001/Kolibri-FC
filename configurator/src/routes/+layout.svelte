<script lang="ts">
	import { invoke } from '@tauri-apps/api';
	import { onMount, onDestroy } from 'svelte';
	import { Serialport } from 'tauri-plugin-serialport-api';
	import { port } from '../stores';

	let devices: string[] = [];

	let interval: number;

	let device: {
		path: string;
		baudRate: number;
		connected: boolean;
		port: Serialport | undefined;
	};

	port.subscribe((p) => {
		device = p;
	});

	function listDevices() {
		Serialport.available_ports().then((ports) => {
			devices = ports;
			if (!device.connected) {
				const newDevices = [];
				for (let dev of devices) {
					if (!devices.includes(dev)) {
						newDevices.push(dev);
					}
				}
				if (newDevices.length == 1) {
					device.path = newDevices[0];
				} else if (newDevices.length > 1) {
					//choose the first non-COM1 device
					for (let dev of newDevices) {
						if (dev != 'COM1') {
							device.path = dev;
							break;
						}
					}
				} else if (newDevices.length == 0) {
					//if currently selected device is not available, select the first available device that is not COM1
					if (!devices.includes(device.path)) {
						for (let dev of devices) {
							if (dev != 'COM1') {
								device.path = dev;
								break;
							}
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
		device.port = new Serialport({
			baudRate: device.baudRate,
			path: device.path
		});
		device.port
			.open()
			.then(() => {
				device.connected = true;
			})
			.catch((err) => {
				console.error(err);
				disconnect();
			});
	}
	function disconnect() {
		device.port?.close().then(() => {
			device.connected = false;
			device.port = undefined;
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
	</div>

	<slot />
	<button on:click={listDevices}>Test</button>
</div>

<style>
	.main {
		display: grid;
		grid-template-rows: 0fr 1fr 0fr;
	}

	.header {
		display: flex;
		flex-direction: row;
		align-items: center;
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
</style>
