<script lang="ts">
import { defineComponent } from "vue";
import { sendCommand, addOnCommandHandler, removeOnCommandHandler } from "@/msp/comm";
import { MspFn } from "@/msp/protocol";
import { leBytesToInt } from "@utils/utils";
import { useLogStore } from "@stores/logStore";
import { Command } from "@utils/types";

export default defineComponent({
	name: "GpsMag",
	mounted() {
		addOnCommandHandler(this.onCommand);
		this.magPointInterval = setInterval(() => {
			sendCommand('request', MspFn.GET_MAG_DATA);
		}, 50);
		this.getGpsDataInterval = setInterval(() => {
			sendCommand('request', MspFn.GET_GPS_ACCURACY)
				.then(() => {
					return sendCommand('request', MspFn.GET_GPS_MOTION);
				})
				.catch(() => { });
		}, 200);
		this.gpsDataSlowInterval = setInterval(() => {
			sendCommand('request', MspFn.GET_GPS_STATUS)
				.then(() => {
					return sendCommand('request', MspFn.GET_GPS_TIME);
				})
				.catch(() => { });
		}, 1000);
		this.baroDataInterval = setInterval(() => {
			sendCommand('request', MspFn.GET_BARO_DATA);
		}, 100);
	},
	unmounted() {
		removeOnCommandHandler(this.onCommand);
		clearInterval(this.magPointInterval);
		clearInterval(this.getGpsDataInterval);
		clearInterval(this.gpsDataSlowInterval);
		clearInterval(this.baroDataInterval);
	},
	data() {
		return {
			configuratorLog: useLogStore(),
			magPointInterval: -1,
			magX: 0,
			magY: 0,
			magZ: 0,
			magRight: 0,
			magFront: 0,
			magHeading: 0,
			gpsAcc: { tAcc: 1000, hAcc: 1000, vAcc: 1000, headAcc: 1000, sAcc: 1000, pDop: 1000, },
			gpsStatus: { gpsInited: false, initStep: 0, fix: 0, timeValidityFlags: 0, flags: 0, flags2: 0, flags3: 0, satCount: 0 },
			gpsMotion: { velN: 0, velE: 0, velD: 0, gSpeed: 0, headMot: 0, lat: 0, lon: 0, alt: 0 },
			gpsTime: { year: 0, month: 0, day: 0, hour: 0, minute: 0, second: 0 },
			combinedAltitude: 0,
			baro: { altitude: 0, pressure: 0, temperature: 0, pressureRaw: 0 },
			verticalVelocity: 0,
			getGpsDataInterval: -1,
			gpsDataSlowInterval: -1,
			baroDataInterval: -1,
			MspFn,
			sendCommand,
		};
	},
	methods: {
		degToDegMinSec(deg: number) {
			const d = Math.floor(deg);
			const m = Math.floor((deg - d) * 60);
			const s = Math.round(((deg - d) * 60 - m) * 60);
			return `${d}° ${m}' ${s}"`;
		},
		onCommand(command: Command) {
			if (command.cmdType === 'response') {
				switch (command.command) {
					case MspFn.GET_MAG_DATA:
						const canvasxy = this.$refs.canvasxy as HTMLCanvasElement | null;
						const canvasyz = this.$refs.canvasyz as HTMLCanvasElement | null;
						const canvaszx = this.$refs.canvaszx as HTMLCanvasElement | null;
						if (!canvasxy || !canvasyz || !canvaszx) return;
						const ctxxy = canvasxy.getContext('2d');
						const ctxyz = canvasyz.getContext('2d');
						const ctxzx = canvaszx.getContext('2d');
						if (!ctxxy || !ctxyz || !ctxzx) return;
						const data = command.data;
						this.magX = leBytesToInt(data.slice(0, 2), true);
						this.magY = leBytesToInt(data.slice(2, 4), true);
						this.magZ = leBytesToInt(data.slice(4, 6), true);
						this.magRight = leBytesToInt(data.slice(6, 8), true);
						this.magFront = leBytesToInt(data.slice(8, 10), true);
						this.magHeading = leBytesToInt(data.slice(10, 12), true);
						//each canvas plots values from -500 to 500
						const scale = 500 / 1000;
						const xpx = this.magX * scale + 250;
						const ypx = this.magY * scale + 250;
						const zpx = this.magZ * scale + 250;
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
					case MspFn.GET_GPS_ACCURACY:
						this.gpsAcc = {
							tAcc: leBytesToInt(command.data.slice(0, 4)) * 1e-3,
							hAcc: leBytesToInt(command.data.slice(4, 8)) * 1e-3,
							vAcc: leBytesToInt(command.data.slice(8, 12)) * 1e-3,
							sAcc: leBytesToInt(command.data.slice(12, 16)) * 1e-3,
							headAcc: leBytesToInt(command.data.slice(16, 20)) * 1e-5,
							pDop: leBytesToInt(command.data.slice(20, 24)) * 0.01
						};
						break;
					case MspFn.GET_GPS_MOTION:
						this.gpsMotion = {
							lat: leBytesToInt(command.data.slice(0, 4), true) * 1e-7,
							lon: leBytesToInt(command.data.slice(4, 8), true) * 1e-7,
							alt: leBytesToInt(command.data.slice(8, 12), true) * 1e-3,
							velN: leBytesToInt(command.data.slice(12, 16), true) * 1e-3,
							velE: leBytesToInt(command.data.slice(16, 20), true) * 1e-3,
							velD: leBytesToInt(command.data.slice(20, 24), true) * 1e-3,
							gSpeed: leBytesToInt(command.data.slice(24, 28), true) * 1e-3,
							headMot: leBytesToInt(command.data.slice(28, 32), true) * 1e-5
						};
						this.combinedAltitude = leBytesToInt(command.data.slice(32, 36), true) / 65536;
						this.verticalVelocity = leBytesToInt(command.data.slice(36, 40), true) / 65536;
						break;
					case MspFn.GET_GPS_STATUS:
						this.gpsStatus = {
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
					case MspFn.GET_GPS_TIME:
						this.gpsTime = {
							year: leBytesToInt(command.data.slice(0, 2)),
							month: command.data[2],
							day: command.data[3],
							hour: command.data[4],
							minute: command.data[5],
							second: command.data[6]
						};
						break;
					case MspFn.MAG_CALIBRATION:
						if (command.data[0] === 1) {
							this.configuratorLog.push('Magnetometer calibrated');
						} else {
							this.configuratorLog.push('Magnetometer calibration started');
						}
						break;
					case MspFn.GET_BARO_DATA:
						this.baro = {
							altitude: leBytesToInt(command.data.slice(0, 4), true) * 1e-3,
							pressure: leBytesToInt(command.data.slice(4, 8), true) * 1e-3,
							temperature: leBytesToInt(command.data.slice(8, 12), true) * 1e-2,
							pressureRaw: leBytesToInt(command.data.slice(12, 16), true)
						};
						break;
				}
			}
		}
	}
})
</script>
<template>
	<div>
		<canvas width="500" height="500" ref="canvasxy" style="display:inline-block; border: 1px solid white;"></canvas>
		<canvas width="500" height="500" ref="canvasyz" style="display:inline-block; border: 1px solid white;"></canvas>
		<canvas width="500" height="500" ref="canvaszx" style="display:inline-block; border: 1px solid white;"></canvas>
		<br />
		<button @click="() => sendCommand('request', MspFn.MAG_CALIBRATION)">Calibrate Magnetometer</button>
		<br />
		<div class="infoDiv magStatus">
			Magnetic Heading: {{ magHeading }}°<br />
			Mag X: {{ magX }}, Mag Y: {{ magY }}, Mag Z: {{ magZ }}<br />
			Mag total: {{ Math.sqrt(magX ** 2 + magY ** 2 + magZ ** 2).toFixed(2) }}<br />
			Mag Right: {{ magRight }}, Mag Front: {{ magFront }}<br />
			Combined Altitude: {{ combinedAltitude.toFixed(2) }}m<br />
			Vertical Velocity: {{ verticalVelocity.toFixed(2) }}m/s<br />
		</div>
		<div class="gpsAcc infoDiv">
			Time Accuracy: {{ gpsAcc.tAcc.toFixed(2) }}µs<br />
			Horizontal Accuracy: {{ gpsAcc.hAcc.toFixed(2) }}m<br />
			Vertical Accuracy: {{ gpsAcc.vAcc.toFixed(2) }}m<br />
			Heading Accuracy: {{ gpsAcc.headAcc.toFixed(2) }}°<br />
			Speed Accuracy: {{ gpsAcc.sAcc.toFixed(2) }}m/s<br />
			PDOP: {{ gpsAcc.pDop.toFixed(2) }}
		</div>
		<div class="gpsMotion infoDiv">
			Latitude: {{ degToDegMinSec(gpsMotion.lat) }} - {{ gpsMotion.lat.toFixed(2) }}°<br />
			Longitude: {{ degToDegMinSec(gpsMotion.lon) }} - {{ gpsMotion.lon.toFixed(2) }}°<br />
			Altitude: {{ gpsMotion.alt.toFixed(2) }}m<br />
			North Velocity: {{ gpsMotion.velN.toFixed(2) }}m/s<br />
			East Velocity: {{ gpsMotion.velE.toFixed(2) }}m/s<br />
			Down Velocity: {{ gpsMotion.velD.toFixed(2) }}m/s<br />
			Ground Speed: {{ gpsMotion.gSpeed.toFixed(2) }}m/s<br />
			Heading of motion: {{ gpsMotion.headMot.toFixed(2) }}°
		</div>
		<div class="gpsStatus infoDiv">
			GPS Inited: {{ gpsStatus.gpsInited ? 'Yes' : 'No' }}<br />
			Init Step: {{ gpsStatus.initStep }}<br />
			Fix: {{ gpsStatus.fix }}<br />
			Time Validity Flags: {{ gpsStatus.timeValidityFlags }}<br />
			Flags: {{ gpsStatus.flags }}<br />
			Flags2: {{ gpsStatus.flags2 }}<br />
			Flags3: {{ gpsStatus.flags3 }}<br />
			Satellite Count: {{ gpsStatus.satCount }}
		</div>
		<div class="gpsTime infoDiv">
			Year: {{ gpsTime.year }}<br />
			Month: {{ gpsTime.month }}<br />
			Day: {{ gpsTime.day }}<br />
			Hour: {{ gpsTime.hour }}<br />
			Minute: {{ gpsTime.minute }}<br />
			Second: {{ gpsTime.second }}
		</div>
		<div class="baroData infoDiv">
			Baro Raw Altitude: {{ baro.altitude.toFixed(2) }}m<br />
			Baro Raw Pressure: {{ baro.pressure.toFixed(2) }}Pa<br />
			Baro Raw Temperature: {{ baro.temperature.toFixed(2) }}°C<br />
			Baro Raw Pressure: 0x{{ baro.pressureRaw.toString(16) }}<br />
		</div>
	</div>
</template>

<style scoped>
.infoDiv {
	display: inline-block;
	margin: 0 1rem;
	min-width: 200px;
}

button {
	color: black;
}
</style>
