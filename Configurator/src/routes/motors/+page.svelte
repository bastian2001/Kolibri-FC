<script lang="ts">
	import { port, MspFn, MspVersion } from '../../portStore';
	import { onMount, onDestroy } from 'svelte';
	import Motor from './motor.svelte';
	import { leBytesToInt } from '../../utils';
	import { configuratorLog } from '../../logStore';
	const motorMapping = [3, 1, 2, 0];
	let int = -1;
	let throttles = $state([0, 0, 0, 0]);
	let throttlesU8 = $derived([
		throttles[0] & 0xff,
		throttles[0] >> 8,
		throttles[1] & 0xff,
		throttles[1] >> 8,
		throttles[2] & 0xff,
		throttles[2] >> 8,
		throttles[3] & 0xff,
		throttles[3] >> 8
	]);
	const unsubscribe = port.subscribe(command => {
		if (command.cmdType === 'response') {
			switch (command.command) {
				case MspFn.GET_MOTOR:
					{
						const m: number[] = [];
						for (let i = 0; i < 4; i++) {
							m.push(leBytesToInt(command.data.slice(i * 2, i * 2 + 2)));
						}
						motors = m;
					}
					break;
			}
		}
	});
	let motors = $state([0, 0, 0, 0]);
	let getMotorsInterval = 0;
	function startMotors() {
		clearInterval(int);
		int = setInterval(() => {
			port.sendCommand('request', MspFn.SET_MOTOR, MspVersion.V2, throttlesU8);
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
		throttles[motor] = 1075;
		throttles = [...throttles];
	}
	onMount(() => {
		getMotorsInterval = setInterval(() => {
			port.sendCommand('request', MspFn.GET_MOTOR);
		}, 100);
		port.addOnDisconnectHandler(stopMotors);
	});
	onDestroy(() => {
		clearInterval(getMotorsInterval);
		stopMotors();
		port.removeOnDisconnectHandler(stopMotors);
		unsubscribe();
	});
</script>

<div>
	<button onclick={() => spinMotor(0)}>Spin RR</button>
	<button onclick={() => spinMotor(1)}>Spin FR</button>
	<button onclick={() => spinMotor(2)}>Spin RL</button>
	<button onclick={() => spinMotor(3)}>Spin FL</button>
	<button onclick={() => stopMotors()}>Stop</button>
	<button onclick={() => startMotors()}>Start</button>
	<div class="quadPreview">
		{#each motors.map((m, i) => motors[motorMapping[i]]) as motor}
			<Motor throttlePct={(motor - 1000) / 10} />
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
