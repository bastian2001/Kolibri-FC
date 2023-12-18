<script lang="ts">
	import { port, ConfigCmd } from '../stores';
	import { onMount, onDestroy } from 'svelte';
	import type { Command } from '../stores';
	import { leBytesToInt } from '../utils';

	let getRotationInterval = 0;
	let xBox = null as any;
	let yBox = null as any;
	let zBox = null as any;

	$: handleCommand($port);
	function handleCommand(command: Command) {
		switch (command.command) {
			case ConfigCmd.GET_ROTATION | 0x4000:
				let pitch = leBytesToInt(command.data.slice(0, 2));
				if (pitch > 32768) pitch -= 65536;
				pitch /= 8192.0;
				pitch *= 180.0 / Math.PI;
				console.log('pitch: ', pitch);
				let roll = leBytesToInt(command.data.slice(2, 4));
				if (roll > 32768) roll -= 65536;
				roll /= 8192.0;
				roll *= 180.0 / Math.PI;
				console.log('roll: ', roll);
				let yaw = leBytesToInt(command.data.slice(4, 6));
				if (yaw > 32768) yaw -= 65536;
				yaw /= 8192.0;
				yaw *= 180.0 / Math.PI;
				console.log('yaw: ', yaw);
				yBox.style.transform = `rotateX(${-pitch}deg)`;
				xBox.style.transform = `rotateY(${roll}deg)`;
				zBox.style.transform = `rotateX(90deg) rotateZ(${
					yaw + 180
				}deg) translate3d(0px, 0px, -180px)`;
				break;
		}
	}

	function ledOn() {
		port.sendCommand(ConfigCmd.SET_DEBUG_LED, [1]);
	}
	function ledOff() {
		port.sendCommand(ConfigCmd.SET_DEBUG_LED, [0]);
	}
	function playSound() {
		port.sendCommand(ConfigCmd.PLAY_SOUND);
	}

	onMount(() => {
		getRotationInterval = setInterval(() => {
			port.sendCommand(ConfigCmd.GET_ROTATION).catch(() => {});
		}, 30);
	});
	onDestroy(() => {
		clearInterval(getRotationInterval);
	});
</script>

<button on:click={() => ledOn()}>LED On</button>
<button on:click={() => ledOff()}>LED Off</button>
<button on:click={() => playSound()}>Play Sound</button>

<div class="drone3DPreview">
	<div class="zBox" bind:this={zBox}>
		<div class="yBox" bind:this={yBox}>
			<div class="xBox" bind:this={xBox}>
				<div class="droneFrame">
					<div class="flrrBar" />
					<div class="rlfrBar" />
				</div>
				<div class="arrowForward" />
				<div class="props">
					<div class="dronePropellerRR" />
					<div class="dronePropellerFR" />
					<div class="dronePropellerRL" />
					<div class="dronePropellerFL" />
				</div>
			</div>
		</div>
	</div>
</div>

<style>
	.drone3DPreview {
		width: 500px;
		height: 500px;
		background-color: rgba(255, 255, 255, 0.4);
		perspective: 600px;
		position: relative;
	}
	.zBox {
		width: 100%;
		height: 100%;
		position: relative;
		top: -130px;
		transform-style: preserve-3d;
		transform: rotateX(90deg) rotateY(0deg) translate3d(0px, 0px, -180px);
	}
	/*Rotating the bounding box later via JS will rotate the whole drone with it*/
	.xBox,
	.yBox {
		width: 100%;
		height: 100%;
		position: relative;
		transform-style: preserve-3d;
		transform: rotateX(0deg) rotateY(0deg);
	}
	.flrrBar,
	.rlfrBar {
		width: 350px;
		height: 20px;
		background-color: #000;
		position: absolute;
		transform-style: preserve-3d;
		top: 50%;
		left: 50%;
	}
	.flrrBar {
		transform: translate(-50%, -50%) rotatez(-45deg);
	}
	.rlfrBar {
		transform: translate(-50%, -50%) rotatez(45deg);
	}
	.props {
		transform-style: preserve-3d;
	}
	.props div {
		width: 150px;
		transform-style: preserve-3d;
		height: 150px;
		background-color: #f00;
		position: absolute;
		transform-style: preserve-3d;
		border-radius: 100%;
	}
	.dronePropellerFL {
		transform: translate3d(60px, 60px, 15px);
	}
	.dronePropellerFR {
		transform: translate3d(290px, 60px, 15px);
	}
	.dronePropellerRL {
		transform: translate3d(60px, 290px, 15px);
	}
	.dronePropellerRR {
		transform: translate3d(290px, 290px, 15px);
	}
	.arrowForward {
		width: 100px;
		height: 50px;
		position: absolute;
		left: 50%;
		top: 150px;
		transform: translate3d(-50%, -50%, 10px);
		background-color: green;
		clip-path: polygon(0 100%, 50% 0, 100% 100%);
	}
</style>
