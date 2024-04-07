<script lang="ts">
	import { port, ConfigCmd } from '../../stores';
	import { onMount, onDestroy } from 'svelte';
	import type { Command } from '../../stores';
	import { leBytesToInt, roundToDecimal } from '../../utils';
	let canvasxy: HTMLCanvasElement;
	let canvasyz: HTMLCanvasElement;
	let canvaszx: HTMLCanvasElement;
	let magPointInterval: number;
	let magX = 0,
		magY = 0,
		magZ = 0,
		magRight = 0,
		magRear = 0,
		magHeading = 0;
	let gpsAcc: {
		tAcc: number;
		hAcc: number;
		vAcc: number;
		headAcc: number;
		sAcc: number;
		pDop: number;
	} = {
		tAcc: 1000,
		hAcc: 1000,
		vAcc: 1000,
		headAcc: 1000,
		sAcc: 1000,
		pDop: 1000
	};
	let gpsStatus: {
		gpsInited: boolean;
		initStep: number;
		fix: number;
		timeValidityFlags: number;
		flags: number;
		flags2: number;
		flags3: number;
		satCount: number;
	} = {
		gpsInited: false,
		initStep: 0,
		fix: 0,
		timeValidityFlags: 0,
		flags: 0,
		flags2: 0,
		flags3: 0,
		satCount: 0
	};
	let gpsMotion: {
		lat: number;
		lon: number;
		alt: number;
		velN: number;
		velE: number;
		velD: number;
		gSpeed: number;
		headMot: number;
	} = {
		velN: 0,
		velE: 0,
		velD: 0,
		gSpeed: 0,
		headMot: 0,
		lat: 0,
		lon: 0,
		alt: 0
	};
	let gpsTime: {
		year: number;
		month: number;
		day: number;
		hour: number;
		minute: number;
		second: number;
	} = {
		year: 0,
		month: 0,
		day: 0,
		hour: 0,
		minute: 0,
		second: 0
	};
	let combinedAltitude = 0,
		verticalVelocity = 0;

	let getGpsData = 0,
		gpsDataSlow = 0;

	$: handleCommand($port);
	let lastMagData = 0;
	function handleCommand(command: Command) {
		switch (command.command) {
			case ConfigCmd.GET_MAG_DATA | 0x4000:
				if (!canvasxy || !canvasyz || !canvaszx) return;
				const ctxxy = canvasxy.getContext('2d');
				const ctxyz = canvasyz.getContext('2d');
				const ctxzx = canvaszx.getContext('2d');
				if (!ctxxy || !ctxyz || !ctxzx) return;
				const data = command.data;
				magX = leBytesToInt(command.data.slice(0, 2), true);
				magY = leBytesToInt(command.data.slice(2, 4), true);
				magZ = leBytesToInt(command.data.slice(4, 6), true);
				magRight = leBytesToInt(command.data.slice(6, 8), true);
				magRear = leBytesToInt(command.data.slice(8, 10), true);
				magHeading = leBytesToInt(command.data.slice(10, 12), true);
				//each canvas plots values from -1000 to 1000
				const scale = 500 / 1000;
				const xpx = magX * scale + 250;
				const ypx = magY * scale + 250;
				const zpx = magZ * scale + 250;
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
				break;
			case ConfigCmd.GET_GPS_ACCURACY | 0x4000:
				gpsAcc = {
					tAcc: leBytesToInt(command.data.slice(0, 4)) * 1e-3,
					hAcc: leBytesToInt(command.data.slice(4, 8)) * 1e-3,
					vAcc: leBytesToInt(command.data.slice(8, 12)) * 1e-3,
					sAcc: leBytesToInt(command.data.slice(12, 16)) * 1e-3,
					headAcc: leBytesToInt(command.data.slice(16, 20)) * 1e-5,
					pDop: leBytesToInt(command.data.slice(20, 24)) * 0.01
				};
				break;
			case ConfigCmd.GET_GPS_MOTION | 0x4000:
				gpsMotion = {
					lat: leBytesToInt(command.data.slice(0, 4), true) * 1e-7,
					lon: leBytesToInt(command.data.slice(4, 8), true) * 1e-7,
					alt: leBytesToInt(command.data.slice(8, 12), true) * 1e-3,
					velN: leBytesToInt(command.data.slice(12, 16), true) * 1e-3,
					velE: leBytesToInt(command.data.slice(16, 20), true) * 1e-3,
					velD: leBytesToInt(command.data.slice(20, 24), true) * 1e-3,
					gSpeed: leBytesToInt(command.data.slice(24, 28), true) * 1e-3,
					headMot: leBytesToInt(command.data.slice(28, 32), true) * 1e-5
				};
				combinedAltitude = leBytesToInt(command.data.slice(32, 36), true) / 65536;
				verticalVelocity = leBytesToInt(command.data.slice(36, 40), true) / 65536;
				break;
			case ConfigCmd.GET_GPS_STATUS | 0x4000:
				gpsStatus = {
					gpsInited: command.data[0] === 1,
					initStep: command.data[1],
					fix: command.data[2],
					timeValidityFlags: command.data[3],
					flags: command.data[4],
					flags2: command.data[5],
					flags3: leBytesToInt(command.data.slice(6, 8)),
					satCount: command.data[8]
				};
				break;
			case ConfigCmd.GET_GPS_TIME | 0x4000:
				gpsTime = {
					year: leBytesToInt(command.data.slice(0, 2)),
					month: command.data[2],
					day: command.data[3],
					hour: command.data[4],
					minute: command.data[5],
					second: command.data[6]
				};
				break;
		}
	}

	onMount(() => {
		magPointInterval = setInterval(() => {
			port.sendCommand(ConfigCmd.GET_MAG_DATA);
		}, 50);
		getGpsData = setInterval(() => {
			port
				.sendCommand(ConfigCmd.GET_GPS_ACCURACY)
				.then(() => {
					return port.sendCommand(ConfigCmd.GET_GPS_MOTION);
				})
				.catch(() => {});
		}, 200);
		gpsDataSlow = setInterval(() => {
			port
				.sendCommand(ConfigCmd.GET_GPS_STATUS)
				.then(() => {
					return port.sendCommand(ConfigCmd.GET_GPS_TIME);
				})
				.catch(() => {});
		}, 1000);
	});

	onDestroy(() => {
		clearInterval(magPointInterval);
		clearInterval(getGpsData);
		clearInterval(gpsDataSlow);
	});

	function degToDegMinSec(deg: number) {
		const d = Math.floor(deg);
		const m = Math.floor((deg - d) * 60);
		const s = Math.round(((deg - d) * 60 - m) * 60);
		return `${d}° ${m}' ${s}"`;
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
/><br />
<button on:click={() => port.sendCommand(ConfigCmd.MAG_CALIBRATE)}>Calibrate Magnetometer</button
><br />
<div class="gpsInfo magStatus">
	Magnetic Heading: {roundToDecimal(magHeading, 2)}°<br />
	Mag X: {magX}, Mag Y: {magY}, Mag Z: {magZ}<br />
	Mag Right: {magRight}, Mag Rear: {magRear}<br />
	Combined Altitude: {roundToDecimal(combinedAltitude, 2)}m<br />
	Vertical Velocity: {roundToDecimal(verticalVelocity, 2)}m/s<br />
</div>
<div class="gpsAcc gpsInfo">
	Time Accuracy: {roundToDecimal(gpsAcc.tAcc, 2)}µs<br />
	Horizontal Accuracy: {roundToDecimal(gpsAcc.hAcc, 2)}m<br />
	Vertical Accuracy: {roundToDecimal(gpsAcc.vAcc, 2)}m<br />
	Heading Accuracy: {roundToDecimal(gpsAcc.headAcc, 2)}°<br />
	Speed Accuracy: {roundToDecimal(gpsAcc.sAcc, 2)}m/s<br />
	PDOP: {roundToDecimal(gpsAcc.pDop, 2)}
</div>
<div class="gpsMotion gpsInfo">
	Latitude: {degToDegMinSec(gpsMotion.lat)} - {roundToDecimal(gpsMotion.lat, 7)}°<br />
	Longitude: {degToDegMinSec(gpsMotion.lon)} - {roundToDecimal(gpsMotion.lon, 7)}°<br />
	Altitude: {roundToDecimal(gpsMotion.alt, 2)}m<br />
	North Velocity: {roundToDecimal(gpsMotion.velN, 2)}m/s<br />
	East Velocity: {roundToDecimal(gpsMotion.velE, 2)}m/s<br />
	Down Velocity: {roundToDecimal(gpsMotion.velD, 2)}m/s<br />
	Ground Speed: {roundToDecimal(gpsMotion.gSpeed, 2)}m/s<br />
	Heading: {roundToDecimal(gpsMotion.headMot, 2)}°
</div>
<div class="gpsStatus gpsInfo">
	GPS Inited: {gpsStatus.gpsInited ? 'Yes' : 'No'}<br />
	Init Step: {gpsStatus.initStep}<br />
	Fix: {gpsStatus.fix}<br />
	Time Validity Flags: {gpsStatus.timeValidityFlags}<br />
	Flags: {gpsStatus.flags}<br />
	Flags2: {gpsStatus.flags2}<br />
	Flags3: {gpsStatus.flags3}<br />
	Satellite Count: {gpsStatus.satCount}
</div>
<div class="gpsTime gpsInfo">
	Year: {gpsTime.year}<br />
	Month: {gpsTime.month}<br />
	Day: {gpsTime.day}<br />
	Hour: {gpsTime.hour}<br />
	Minute: {gpsTime.minute}<br />
	Second: {gpsTime.second}
</div>

<style>
	.gpsInfo {
		display: inline-block;
		margin: 0 1rem;
		min-width: 200px;
	}
	button {
		color: black;
	}
</style>
