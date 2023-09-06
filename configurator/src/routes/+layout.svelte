<script lang="ts">
	import { invoke } from '@tauri-apps/api';
	import { onMount, onDestroy } from 'svelte';
	import { page } from '$app/stores';
	import { port } from '../stores';
	import type { Command } from '../stores';

	let devices: string[] = [];

	let listInterval: number;

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

	let readInterval = -1;

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

	let log = [] as string[];

	function listDevices() {
		invoke('serial_list').then((d: unknown) => {
			if (!device.connected) {
				const newDevices: string[] = [];
				const scanned = d as string[];
				for (const dev of scanned) {
					if (!devices.includes(dev)) {
						newDevices.push(dev);
					}
				}
				if (newDevices.length == 1) {
					device.path = newDevices[0];
				} else if (newDevices.length > 1) {
					//choose the first non-COM1-device
					for (const dev of newDevices) {
						if (dev !== 'COM1') {
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

			devices = d as string[];
		});
	}
	onMount(() => {
		listDevices();
		listInterval = setInterval(() => {
			listDevices();
		}, 1000);
	});
	onDestroy(() => {
		clearInterval(listInterval);
		disconnect();
	});
	const rxBuf = [] as number[];
	function serialRead() {
		invoke('serial_read')
			.then((d: unknown) => {
				// console.log(d);
				rxBuf.push(...(d as number[]));
			})
			.catch((e) => {})
			.finally(() => {
				if (rxBuf.length < 7) return;
				if (rxBuf[0] != '_'.charCodeAt(0) || rxBuf[1] != 'K'.charCodeAt(0)) {
					rxBuf.splice(0, rxBuf.length);
					return;
				}
				const len = rxBuf[2] + rxBuf[3] * 256;
				if (rxBuf.length < len + 7) return;
				let checksum = 0;
				for (let i = 2; i < len + 7; i++) checksum ^= rxBuf[i];
				if (checksum !== 0) {
					rxBuf.splice(0, rxBuf.length);
					return;
				}
				const data = rxBuf.slice(6, len + 6);
				const command = rxBuf[4] + rxBuf[5] * 256;
				rxBuf.splice(0, len + 7);
				let dataStr = '';
				for (let i = 0; i < data.length; i++) {
					dataStr += String.fromCharCode(data[i]);
				}
				let cmdType: 'request' | 'info' | 'response' | 'error' = 'info';
				if (command < 0x4000) cmdType = 'request';
				else if (command < 0x8000) cmdType = 'response';
				else if (command < 0xc000) cmdType = 'error';
				const c: Command = {
					command,
					data,
					dataStr,
					cmdType,
					length: len
				};
				port.set(c);
			});
	}
	function connect() {
		invoke('serial_open', { path: device.path })
			.then(() => {
				device.connected = true;
				readInterval = setInterval(serialRead, 20);
			})
			.catch((e) => {
				console.log(e);
				disconnect();
			});
	}
	function disconnect() {
		invoke('serial_close').then(() => {
			device.connected = false;
		});
		clearInterval(readInterval);
		readInterval = -1;
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
</style>
