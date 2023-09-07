<script lang="ts">
	import { port, type Command, ConfigCmd } from '../../stores';
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
</script>

<div>
	<button on:click={() => spinMotor(0)}>Spin RR</button>
	<button on:click={() => spinMotor(1)}>Spin FR</button>
	<button on:click={() => spinMotor(2)}>Spin RL</button>
	<button on:click={() => spinMotor(3)}>Spin FL</button>
	<button on:click={() => stopMotors()}>Stop</button>
	<button on:click={() => startMotors()}>Start</button>
</div>

<style>
	button {
		color: black;
	}
</style>
