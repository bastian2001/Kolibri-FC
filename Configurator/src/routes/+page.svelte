<script lang="ts">
	import { port, ConfigCmd } from '../portStore';
	import { onMount, onDestroy } from 'svelte';
	import type { Command } from '../portStore';
	import { leBytesToInt, roundToDecimal } from '../utils';

	const FLIGHT_MODES = ['ACRO', 'ANGLE', 'ALT_HOLD', 'GPS_VEL', 'GPS_POS'];
	const ARMING_DISABLE_FLAGS = [
		'Arming switch not armed',
		'Throttle up',
		'No GPS fix',
		'Configurator attached',
		'ELRS missing',
		'Not Acro or Angle',
		'Gyro not calibrated'
	];
	let flightMode = 0;
	let armingDisableFlags = 0;
	let armed = false;
	let configuratorConnected = false;
	let pingFromConfigurator = -1,
		pingFromFC = -1;

	let getRotationInterval = 0;
	let xBox: any = null;
	let yBox: any = null;
	let zBox: any = null;
	let attitude = {
		roll: 0,
		pitch: 0,
		yaw: 0,
		heading: 0
	};
	let showHeading = false;
	let serialNum = 1;
	let baudRate = 115200;

	$: handleCommand($port);
	function handleCommand(command: Command) {
		switch (command.command) {
			case ConfigCmd.STATUS | 0x4000:
				armed = command.data[2] === 1;
				flightMode = command.data[3];
				armingDisableFlags = leBytesToInt(command.data.slice(4, 8));
				configuratorConnected = command.data[8] === 1;
				break;
			case ConfigCmd.GET_ROTATION | 0x4000:
				let pitch = leBytesToInt(command.data.slice(0, 2), true);
				pitch /= 8192.0;
				pitch *= 180.0 / Math.PI;
				let roll = leBytesToInt(command.data.slice(2, 4), true);
				roll /= 8192.0;
				roll *= 180.0 / Math.PI;
				let yaw = leBytesToInt(command.data.slice(4, 6), true);
				yaw /= 8192.0;
				yaw *= 180.0 / Math.PI;
				let heading = leBytesToInt(command.data.slice(6, 8), true);
				heading /= 8192.0;
				heading *= 180.0 / Math.PI;
				zBox.style.transform = `rotateZ(${showHeading ? heading : yaw}deg) translateZ(10px)`;
				yBox.style.transform = `rotateX(${pitch}deg)`;
				xBox.style.transform = `rotateY(${-roll}deg)`;
				attitude = {
					roll,
					pitch,
					yaw,
					heading
				};
				break;
			case ConfigCmd.SERIAL_PASSTHROUGH | 0x4000:
				const sPort = command.data[0];
				const baud = leBytesToInt(command.data.slice(1, 5));
				console.log(`Serial passthrough started on Serial${sPort} with baud rate ${baud}`);
				port.disconnect();
			case ConfigCmd.GET_CRASH_DUMP | 0x4000:
				console.log(command.data);
				break;
			case ConfigCmd.CALIBRATE_ACCELEROMETER | 0x4000:
				console.log('Accelerometer calibrated');
				break;
		}
	}

	function ledOn() {
		port.sendCommand(ConfigCmd.SET_DEBUG_LED, [1]);
	}
	function ledOff() {
		port.sendCommand(ConfigCmd.SET_DEBUG_LED, [0]);
	}
	function calibrateAccel() {
		port.sendCommand(ConfigCmd.CALIBRATE_ACCELEROMETER);
	}
	function playSound() {
		port.sendCommand(ConfigCmd.PLAY_SOUND);
	}

	function delay(ms: number) {
		return new Promise((resolve) => {
			setTimeout(resolve, ms);
		});
	}

	let pingInterval = 0;
	onMount(() => {
		getRotationInterval = setInterval(() => {
			port.sendCommand(ConfigCmd.GET_ROTATION).catch(() => {});
		}, 20);
		pingInterval = setInterval(() => {
			pingFromConfigurator = port.getPingTime().fromConfigurator;
			pingFromFC = port.getPingTime().fromFC;
		}, 1000);
	});
	onDestroy(() => {
		clearInterval(getRotationInterval);
		clearInterval(pingInterval);
	});
</script>

<div>
	<button on:click={() => ledOn()}>LED On</button>
	<button on:click={() => ledOff()}>LED Off</button>
	<button on:click={() => calibrateAccel()}>Calibrate Accelerometer</button>
	<button on:click={() => playSound()}>Play Sound</button>
	<input
		type="number"
		name="serial"
		step="1"
		min="1"
		max="2"
		placeholder="Serial Number"
		bind:value={serialNum}
	/>
	<input
		type="number"
		name="baud"
		step="1"
		min="9600"
		max="115200"
		placeholder="Baud Rate"
		bind:value={baudRate}
	/>
	<button
		on:click={() => {
			port.sendCommand(ConfigCmd.SERIAL_PASSTHROUGH, [
				serialNum,
				baudRate & 0xff,
				(baudRate >> 8) & 0xff,
				(baudRate >> 16) & 0xff,
				baudRate >> 24
			]);
		}}>Start Serial Passthrough</button
	>
	<button
		on:click={() => {
			port.enableCommands(false);
			port
				.sendRaw([], '+++')
				.then(() => {
					return delay(1500);
				})
				.then(() => port.enableCommands(true));
		}}>Stop Serial Passthrough</button
	>
	<button on:click={() => port.sendCommand(ConfigCmd.GET_CRASH_DUMP)}>Get Crash Dump</button>
	<button on:click={() => port.sendCommand(ConfigCmd.CLEAR_CRASH_DUMP)}>Clear Crash Dump</button>
	<button on:click={() => port.sendCommand(ConfigCmd.REBOOT)}>Reboot</button>
	<button on:click={() => port.sendCommand(ConfigCmd.REBOOT_TO_BOOTLOADER)}>Bootloader</button>
</div>
<div class="droneStatus">
	Flight Mode: {FLIGHT_MODES[flightMode]}, Armed: {armed ? 'Yes' : 'No'}, Configurator Connected: {configuratorConnected
		? 'Yes'
		: 'No'}<br />
	Arming Disabled Flags:<br />
	{ARMING_DISABLE_FLAGS.map((flag, i) => {
		if (armingDisableFlags & (1 << i)) return flag + ', ';
	}).join('')}
</div>
<div>
	Ping from Configurator: {pingFromConfigurator} - Ping from FC: {pingFromFC}
</div>
<div class="drone3DPreview">
	<div class="droneBase droneAxes">
		<div class="zBox droneAxes" bind:this={zBox}>
			<div class="yBox droneAxes" bind:this={yBox}>
				<div class="xBox droneAxes" bind:this={xBox}>
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
</div>
<div class="attInfo">
	<div class="axisLabel axisRoll">Roll: {roundToDecimal(attitude.roll, 2)}°</div>
	<div class="axisLabel axisPitch">Pitch: {roundToDecimal(attitude.pitch, 2)}°</div>
	{#if showHeading}
		<div class="axisLabel axisHeading">Heading: {roundToDecimal(attitude.heading, 2)}°</div>
	{:else}
		<div class="axisLabel axisYaw">Yaw: {roundToDecimal(attitude.yaw, 2)}°</div>
	{/if}
	<br />
	<input type="checkbox" bind:checked={showHeading} id="headingCheckbox" /><label
		for="headingCheckbox">Show Heading instead of Yaw</label
	>
</div>

<style>
	.drone3DPreview {
		width: 500px;
		height: 500px;
		background-color: rgba(255, 255, 255, 0.4);
		perspective: 600px;
		position: relative;
		display: inline-block;
	}
	.droneBase {
		transform: rotateX(60deg) translate3d(0px, 0px, -180px);
		top: -170px;
		border-radius: 50%;
		border: 5px solid #000;
		background-image: url($lib/images/grid.png);
		background-size: 20px 20px;
		image-rendering: crisp-edges;
	}
	.zBox {
		transform: rotateZ(0deg) translateZ(10px);
	}
	/*Rotating the bounding box later via JS will rotate the whole drone with it*/
	.xBox,
	.yBox {
		transform: rotateX(0deg) rotateY(0deg);
	}
	.droneAxes {
		width: 100%;
		height: 100%;
		position: relative;
		transform-style: preserve-3d;
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
		position: absolute;
		border-radius: 100%;
	}
	.dronePropellerFL {
		transform: translate3d(60px, 60px, 15px);
		background-color: #282;
	}
	.dronePropellerFR {
		transform: translate3d(290px, 60px, 15px);
		background-color: #282;
	}
	.dronePropellerRL {
		transform: translate3d(60px, 290px, 15px);
		background-color: #d00;
	}
	.dronePropellerRR {
		transform: translate3d(290px, 290px, 15px);
		background-color: #d00;
	}
	.arrowForward {
		width: 100px;
		height: 50px;
		position: absolute;
		left: 50%;
		top: 150px;
		transform: translate3d(-50%, -50%, 10px);
		background-color: #000;
		clip-path: polygon(0 100%, 50% 0, 100% 100%);
	}
	.attInfo {
		display: inline-block;
		margin: 0 1rem;
		min-width: 200px;
	}
	button,
	input {
		color: black;
	}
</style>
