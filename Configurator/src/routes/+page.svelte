<script lang="ts">
	import { port, MspFn, MspVersion } from '../portStore';
	import { onMount, onDestroy } from 'svelte';
	import { leBytesToInt, roundToDecimal, delay, prefixZeros } from '../utils';
	import { configuratorLog } from '../logStore';

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
	const REBOOT_MODES = {
		FIRMWARE: 0,
		BOOTLOADER_ROM: 1,
		MSC: 2,
		MSC_UTC: 3,
		BOOTLOADER_FLASH: 4
	};
	let flightMode = 0;
	let armingDisableFlags = 0;
	let armed = false;
	let configuratorConnected = false;
	let fcPing = -1;

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
	let time = {
		year: 0,
		month: 1,
		day: 1,
		hour: 0,
		minute: 0,
		second: 0
	};
	$: if (xBox && yBox && zBox) {
		zBox.style.transform = `rotateZ(${showHeading ? attitude.heading : attitude.yaw}deg) translateZ(10px)`;
		yBox.style.transform = `rotateX(${attitude.pitch}deg)`;
		xBox.style.transform = `rotateY(${-attitude.roll}deg)`;
	}

	const unsubscribe = port.subscribe(command => {
		if (command.cmdType === 'response') {
			switch (command.command) {
				case MspFn.MSP_ATTITUDE:
					attitude = {
						roll: -leBytesToInt(command.data.slice(0, 2), true) / 10,
						pitch: leBytesToInt(command.data.slice(2, 4), true) / 10,
						yaw: leBytesToInt(command.data.slice(4, 6), true),
						heading: leBytesToInt(command.data.slice(4, 6), true)
					};
					break;
				case MspFn.STATUS:
					armed = command.data[2] === 1;
					flightMode = command.data[3];
					armingDisableFlags = leBytesToInt(command.data.slice(4, 8));
					configuratorConnected = command.data[8] === 1;
					break;
				case MspFn.GET_ROTATION:
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
					attitude = {
						roll,
						pitch,
						yaw,
						heading
					};
					break;
				case MspFn.SERIAL_PASSTHROUGH:
					const sPort = command.data[0];
					const baud = leBytesToInt(command.data.slice(1, 5));
					configuratorLog.push(
						`Serial passthrough started on Serial${sPort} with baud rate ${baud}`
					);
					port.disconnect();
				case MspFn.GET_CRASH_DUMP:
					console.log(command.data);
					break;
				case MspFn.ACC_CALIBRATION:
					if (command.data[0] === 1) configuratorLog.push('Accelerometer calibrated');
					else configuratorLog.push('Accelerometer calibration started');
					break;
				case MspFn.REBOOT:
					configuratorLog.push('Rebooting');
					port.disconnect();
					break;
				case MspFn.GET_CRASH_DUMP:
					configuratorLog.push('See console for crash dump');
					break;
				case MspFn.CLEAR_CRASH_DUMP:
					configuratorLog.push('Crash dump cleared');
					break;
				case MspFn.GET_RTC:
					time.year = leBytesToInt(command.data.slice(0, 2));
					time.month = command.data[2];
					time.day = command.data[3];
					time.hour = command.data[4];
					time.minute = command.data[5];
					time.second = command.data[6];
					break;
			}
		}
	});

	function ledOn() {
		port.sendCommand('request', MspFn.SET_DEBUG_LED, MspVersion.V2, [1]);
	}
	function ledOff() {
		port.sendCommand('request', MspFn.SET_DEBUG_LED, MspVersion.V2, [0]);
	}
	function calibrateAccel() {
		port.sendCommand('request', MspFn.ACC_CALIBRATION);
	}
	function playSound() {
		port.sendCommand('request', MspFn.PLAY_SOUND);
	}

	let pingInterval = 0;
	let rtcInterval = 0;
	onMount(() => {
		getRotationInterval = setInterval(() => {
			port.sendCommand('request', MspFn.GET_ROTATION).catch(() => {});
		}, 20);
		pingInterval = setInterval(() => {
			fcPing = port.getPingTime();
		}, 1000);
		rtcInterval = setInterval(() => {
			port.sendCommand('request', MspFn.GET_RTC);
		}, 1000);
	});
	onDestroy(() => {
		clearInterval(getRotationInterval);
		clearInterval(pingInterval);
		clearInterval(rtcInterval);
		unsubscribe();
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
			port.sendCommand('request', MspFn.SERIAL_PASSTHROUGH, MspVersion.V2, [
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
	<button on:click={() => port.sendCommand('request', MspFn.GET_CRASH_DUMP)}>Get Crash Dump</button>
	<button on:click={() => port.sendCommand('request', MspFn.CLEAR_CRASH_DUMP)}
		>Clear Crash Dump</button
	>
	<button
		on:click={() =>
			port.sendCommand('request', MspFn.REBOOT, MspVersion.V2, [REBOOT_MODES.FIRMWARE])}
		>Reboot</button
	>
	<button
		on:click={() =>
			port.sendCommand('request', MspFn.REBOOT, MspVersion.V2, [REBOOT_MODES.BOOTLOADER_FLASH])}
		>Bootloader</button
	>
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
	Ping: {fcPing} ms
</div>
<div>
	Time: {prefixZeros(time.year, 4)}-{prefixZeros(time.month, 2)}-{prefixZeros(time.day, 2)}
	{prefixZeros(time.hour, 2)}:{prefixZeros(time.minute, 2)}:{prefixZeros(time.second, 2)}
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
