<script lang="ts">
	import { port, ConfigCmd } from '../../stores';
	import { onMount, onDestroy } from 'svelte';
	import type { Command } from '../../stores';
	import { leBytesToInt, roundToDecimal } from '../../utils';
	let canvasxy: HTMLCanvasElement;
	let canvasyz: HTMLCanvasElement;
	let canvaszx: HTMLCanvasElement;

	$: handleCommand($port);
	function handleCommand(command: Command) {
		switch (command.command) {
			case ConfigCmd.MAG_POINT | 0xc000:
				if (!canvasxy || !canvasyz || !canvaszx) return;
				const ctxxy = canvasxy.getContext('2d');
				const ctxyz = canvasyz.getContext('2d');
				const ctxzx = canvaszx.getContext('2d');
				if (!ctxxy || !ctxyz || !ctxzx) return;
				const data = command.data;
				const x = leBytesToInt(data.slice(0, 2), true);
				const y = leBytesToInt(data.slice(2, 4), true);
				const z = leBytesToInt(data.slice(4, 6), true);
				//each canvas plots values from -1000 to 1000
				const scale = 500 / 1000;
				const xpx = x * scale + 250;
				const ypx = y * scale + 250;
				const zpx = z * scale + 250;
				ctxxy.fillStyle = 'red';
				ctxxy.beginPath();
				ctxxy.arc(xpx, ypx, 2, 0, 2 * Math.PI);
				ctxxy.fill();
				ctxyz.fillStyle = 'green';
				ctxyz.beginPath();
				ctxyz.arc(ypx, zpx, 2, 0, 2 * Math.PI);
				ctxyz.fill();
				ctxzx.fillStyle = 'blue';
				ctxzx.beginPath();
				ctxzx.arc(zpx, xpx, 2, 0, 2 * Math.PI);
				ctxzx.fill();
		}
	}
</script>

<canvas
	width="500"
	height="500"
	bind:this={canvasxy}
	style="display:inline-block; border: 1px solid white;"
/>
<canvas
	width="500"
	height="500"
	bind:this={canvasyz}
	style="display:inline-block; border: 1px solid white;"
/>
<canvas
	width="500"
	height="500"
	bind:this={canvaszx}
	style="display:inline-block; border: 1px solid white;"
/>
