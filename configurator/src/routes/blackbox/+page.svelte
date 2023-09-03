<script lang="ts">
	import { invoke } from '@tauri-apps/api';
	import { onMount, onDestroy } from 'svelte';

	let logNums = [] as number[];

	function strToArray(str: string) {
		const data = [];
		for (let i = 0; i < str.length; i++) {
			data[i] = str.charCodeAt(i);
		}
		return data;
	}
	let int = 0;
	const rxBuf = [] as number[];
	onMount(() => {
		invoke('serial_write', { data: [...strToArray('_K'), 0x00, 0x00, 0x05, 0x00, 0x5] }).then(
			() => {
				console.log('sent');
			}
		);
		int = setInterval(() => {
			invoke('serial_read')
				.then((d: unknown) => {
					//uint8array to string
					const da = d as number[];
					let str = '';
					for (let i = 0; i < da.length; i++) {
						str += String.fromCharCode(da[i]);
					}
					console.log(str);
					rxBuf.push(...(d as number[]));
					if (rxBuf.length < 7) return;
					if (rxBuf[0] != '_'.charCodeAt(0)) return;
					if (rxBuf[1] != 'K'.charCodeAt(0)) return;
					const len = rxBuf[2] + rxBuf[3] * 256;
					if (rxBuf.length < len + 7) return;
					let checksum = 0;
					for (let i = 2; i < len + 7; i++) checksum ^= rxBuf[i];
					if (checksum !== 0) return;
					const data = rxBuf.slice(6, len + 6);
					const command = rxBuf[4] + rxBuf[5] * 256;
					rxBuf.splice(0, len + 7);
					switch (command) {
						case 0x4005: //log file list
							logNums = [...data];
							console.log(logNums);
							break;
					}
				})
				.catch((e) => {
					// console.log(e);
				});
		}, 100);
	});
	onDestroy(() => {
		clearInterval(int);
	});
</script>

<div class="blackbox">
	<div class="selector">
		<select>
			{#each logNums as log}
				<option>{log}</option>
			{/each}
		</select>
	</div>
	<div class="dataViewer">Data Viewer</div>
</div>

<style>
	.blackbox {
		width: 100%;
		height: 100%;
	}

	.selector {
		padding: 0.5rem;
	}

	.selector {
		display: flex;
		flex-direction: row;
		align-items: center;
	}

	.selector select {
		width: 8rem;
		background-color: transparent;
		border: 1px solid var(--border-color);
		border-radius: 4px;
		padding: 3px 6px;
		color: var(--text-color);
		outline: none;
	}
	.selector select option {
		color: #888;
	}
</style>
