<script lang="ts">
	import { port, type Command, ConfigCmd } from '../../stores';
	import { onMount, onDestroy } from 'svelte';
	import Motor from './motor.svelte';
	import { leBytesToInt } from '../../utils';
	const motorMapping = [3, 1, 2, 0];
	let int = -1;
	let throttles = [0, 0, 0, 0];
	let throttlesU8 = [0, 0, 0, 0, 0, 0, 0, 0];
	$: throttlesU8 = [
		throttles[0] & 0xff,
		throttles[0] >> 8,
		throttles[1] & 0xff,
		throttles[1] >> 8,
		throttles[2] & 0xff,
		throttles[2] >> 8,
		throttles[3] & 0xff,
		throttles[3] >> 8
	];
	$: handleCommand($port);
	function handleCommand(command: Command) {
		if (command.command === (ConfigCmd.GET_MOTORS | 0x4000)) {
			const m = [] as number[];
			for (let i = 0; i < 4; i++) {
				m.push(leBytesToInt(command.data.slice(i * 2, i * 2 + 2)));
			}
			motors = m;
		}
	}
	let motors = [0, 0, 0, 0];
	let getMotorsInterval = 0;
	function startMotors() {
		clearInterval(int);
		int = setInterval(() => {
			port.sendCommand(11, throttlesU8);
		}, 100);
	}
	function stopMotors() {
		throttles = [0, 0, 0, 0];
		setTimeout(() => {
			clearInterval(int);
		}, 200);
	}
	function spinMotor(motor: number) {
		throttles = [0, 0, 0, 0];
		throttles[motor] = 150;
		throttles = [...throttles];
	}
	onMount(() => {
		getMotorsInterval = setInterval(() => {
			port.sendCommand(ConfigCmd.GET_MOTORS);
		}, 100);
	});
	onDestroy(() => {
		clearInterval(getMotorsInterval);
	});
</script>

<div>
	<button on:click={() => spinMotor(0)}>Spin RR</button>
	<button on:click={() => spinMotor(1)}>Spin FR</button>
	<button on:click={() => spinMotor(2)}>Spin RL</button>
	<button on:click={() => spinMotor(3)}>Spin FL</button>
	<button on:click={() => stopMotors()}>Stop</button>
	<button on:click={() => startMotors()}>Start</button>
	<div class="quadPreview">
		{#each motors.map((m, i) => motors[motorMapping[i]]) as motor}
			<Motor throttlePct={motor / 20} />
		{/each}
	</div>
</div>

<style>
	button {
		color: black;
	}

	.quadPreview {
		display: grid;
		grid-template-columns: 1fr 1fr;
		grid-template-rows: 1fr 1fr;
		grid-gap: 2rem;
		width: 30vw;
		height: 30vw;
	}
</style>
