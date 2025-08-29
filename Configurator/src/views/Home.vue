<script lang="ts">
import { defineComponent } from 'vue';
import { addOnCommandHandler, getPingTime, sendCommand, sendRaw, disconnect, removeOnCommandHandler, enableCommands } from '@/msp/comm';
import { useLogStore } from '@stores/logStore';
import { leBytesToInt, delay, intToLeBytes } from '@utils/utils';
import { MspFn } from '@/msp/protocol';
import { Command } from '@utils/types';
import { prefixZeros } from '@utils/utils';

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
const REBOOT_MODES = { FIRMWARE: 0, BOOTLOADER_ROM: 1, MSC: 2, MSC_UTC: 3, BOOTLOADER_FLASH: 4 };

export default defineComponent({
	name: 'Home',
	mounted() {
		addOnCommandHandler(this.onCommand);


		this.getRotationInterval = setInterval(() => {
			sendCommand(MspFn.GET_ROTATION).catch(() => { });
		}, 20);
		this.pingInterval = setInterval(() => {
			this.fcPing = getPingTime();
		}, 1000);
		this.rtcInterval = setInterval(() => {
			sendCommand(MspFn.GET_RTC);
		}, 1000);
	},
	unmounted() {
		removeOnCommandHandler(this.onCommand);
		clearInterval(this.getRotationInterval);
		clearInterval(this.pingInterval);
		clearInterval(this.rtcInterval);
	},
	data() {
		return {
			flightMode: 0,
			armingDisableFlags: 0,
			armed: false,
			configuratorConnected: false,
			fcPing: -1,
			getRotationInterval: 0,
			attitude: { roll: 0, pitch: 0, yaw: 0, heading: 0 },
			showHeading: false,
			serialNum: 1,
			baudRate: 115200,
			time: { year: 0, month: 1, day: 1, hour: 0, minute: 0, second: 0, },
			sendCommand,
			sendRaw,
			enableCommands,
			configuratorLog: useLogStore(),
			pingInterval: -1,
			rtcInterval: -1,
			MspFn,
			delay,
			REBOOT_MODES,
			FLIGHT_MODES,
			ARMING_DISABLE_FLAGS,
			prefixZeros,
			intToLeBytes,
		};
	},
	methods: {
		onCommand(command: Command) {
			if (command.cmdType === 'response') {
				switch (command.command) {
					case MspFn.MSP_ATTITUDE:
						this.attitude = {
							roll: -leBytesToInt(command.data, 0, 2, true) / 10,
							pitch: leBytesToInt(command.data, 2, 2, true) / 10,
							yaw: leBytesToInt(command.data, 4, 2, true),
							heading: leBytesToInt(command.data, 4, 2, true)
						};
						break;
					case MspFn.STATUS:
						this.armed = command.data[2] === 1;
						this.flightMode = command.data[3];
						this.armingDisableFlags = leBytesToInt(command.data, 4, 4);
						this.configuratorConnected = command.data[8] === 1;
						break;
					case MspFn.GET_ROTATION:
						let pitch = leBytesToInt(command.data, 0, 2, true);
						pitch /= 8192.0;
						pitch *= 180.0 / Math.PI;
						let roll = leBytesToInt(command.data, 2, 2, true);
						roll /= 8192.0;
						roll *= 180.0 / Math.PI;
						let yaw = leBytesToInt(command.data, 4, 2, true);
						yaw /= 8192.0;
						yaw *= 180.0 / Math.PI;
						let heading = leBytesToInt(command.data, 6, 2, true);
						heading /= 8192.0;
						heading *= 180.0 / Math.PI;
						this.attitude = { roll, pitch, yaw, heading };
						break;
					case MspFn.SERIAL_PASSTHROUGH:
						const sPort = command.data[0];
						const baud = leBytesToInt(command.data, 1, 4);
						this.configuratorLog.push(
							`Serial passthrough started on Serial${sPort} with baud rate ${baud}`
						);
						disconnect();
						break;
					case MspFn.GET_CRASH_DUMP:
						console.log(command.data);
						break;
					case MspFn.ACC_CALIBRATION:
						if (command.data[0] === 1) this.configuratorLog.push('Accelerometer calibrated');
						else this.configuratorLog.push('Accelerometer calibration started');
						break;
					case MspFn.REBOOT:
						this.configuratorLog.push('Rebooting');
						disconnect();
						break;
					case MspFn.GET_CRASH_DUMP:
						this.configuratorLog.push('See console for crash dump');
						break;
					case MspFn.CLEAR_CRASH_DUMP:
						this.configuratorLog.push('Crash dump cleared');
						break;
					case MspFn.GET_RTC:
						this.time.year = leBytesToInt(command.data, 0, 2);
						this.time.month = command.data[2];
						this.time.day = command.data[3];
						this.time.hour = command.data[4];
						this.time.minute = command.data[5];
						this.time.second = command.data[6];
						break;
				}
			}
		},
		redrawDrone() {
			(this.$refs.zBox as HTMLDivElement).style.transform = `rotateZ(${this.showHeading ? this.attitude.heading : this.attitude.yaw}deg) translateZ(10px)`;
			(this.$refs.yBox as HTMLDivElement).style.transform = `rotateX(${this.attitude.pitch}deg)`;
			(this.$refs.xBox as HTMLDivElement).style.transform = `rotateY(${-this.attitude.roll}deg)`
		},
		ledOn() {
			sendCommand(MspFn.SET_DEBUG_LED, [1]);
		},
		ledOff() {
			sendCommand(MspFn.SET_DEBUG_LED, [0]);
		},
		calibrateAccel() {
			sendCommand(MspFn.ACC_CALIBRATION);
		},
		playSound() {
			sendCommand(MspFn.PLAY_SOUND);
		}
	},
	watch: {
		attitude: {
			handler() {
				this.redrawDrone();
			},
			deep: true,
		},
		showHeading: {
			handler() {
				this.redrawDrone();
			}
		}
	},
})
</script>
<template>
	<div>
		<div>
			<button @click="ledOn">LED On</button>
			<button @click="ledOff">LED Off</button>
			<button @click="calibrateAccel">Calibrate Accelerometer</button>
			<button @click="playSound">Play Sound</button>
			<input type="number" step="1" min="1" max="2" placeholder="Serial Number" v-model="serialNum" />
			<input type="number" step="1" min="9600" max="115200" placeholder="Baud Rate" v-model="baudRate" />
			<button @click="() => {
				sendCommand(MspFn.SERIAL_PASSTHROUGH, [
					serialNum,
					...intToLeBytes(baudRate, 4),
				]);
			}">Start Serial Passthrough</button>
			<button @click="() => {
				enableCommands(false);
				sendRaw([], '+++')
					.then(() => {
						return delay(1500)
					})
					.then(() => enableCommands(true));
			}">Stop Serial Passthrough</button>
			<button @click="() => sendCommand(MspFn.GET_CRASH_DUMP)">Get Crash Dump</button>
			<button @click="() => sendCommand(MspFn.CLEAR_CRASH_DUMP)">Clear Crash Dump</button>
			<button @click="() =>
				sendCommand(MspFn.REBOOT, [REBOOT_MODES.FIRMWARE])">Reboot</button>
			<button @click="() =>
				sendCommand(MspFn.REBOOT, [REBOOT_MODES.BOOTLOADER_FLASH])">Bootloader</button>
		</div>
		<div class="droneStatus">Flight Mode: {{ FLIGHT_MODES[flightMode] }}, Armed: {{ armed ? 'Yes' : 'No' }},
			Configurator Connected: {{ configuratorConnected ? 'Yes' : 'No' }}<br />
			Arming Disabled Flags:<br />
			{{
				ARMING_DISABLE_FLAGS
					.map((flag, i) => {
						if (armingDisableFlags & (1 << i)) return flag
					}).filter(el => el)
					.join(', ')
			}}
		</div>
		<div>Ping: {{ fcPing }} ms</div>
		<div>
			Time: {{ prefixZeros(time.year, 4) }}-{{ prefixZeros(time.month, 2) }}-{{ prefixZeros(time.day, 2) }}
			{{ prefixZeros(time.hour, 2) }}:{{ prefixZeros(time.minute, 2) }}:{{ prefixZeros(time.second, 2) }}
		</div>
		<div class="drone3DPreview">
			<div class="droneBase droneAxes">
				<div class="zBox droneAxes" ref="zBox">
					<div class="yBox droneAxes" ref="yBox">
						<div class="xBox droneAxes" ref="xBox">
							<div class="droneFrame">
								<div class="flrrBar"></div>
								<div class="rlfrBar"></div>
							</div>
							<div class="arrowForward"></div>
							<div class="props">
								<div class="dronePropellerRR"></div>
								<div class="dronePropellerFR"></div>
								<div class="dronePropellerRL"></div>
								<div class="dronePropellerFL"></div>
							</div>
						</div>
					</div>
				</div>
			</div>
		</div>
		<div class="attInfo">
			<div class="axisLabel axisRoll">Roll: {{ attitude.roll.toFixed(2) }}°</div>
			<div class="axisLabel axisPitch">Pitch: {{ attitude.pitch.toFixed(2) }}°</div>
			<div v-if="showHeading" class="axisLabel axisHeading">Heading: {{ attitude.heading.toFixed(2) }}°
			</div>
			<div v-else class="axisLabel axisYaw">Yaw: {{ attitude.yaw.toFixed(2) }}°</div>
			<br />
			<input type="checkbox" v-model="showHeading" id="headingCheckbox" />
			<label for="headingCheckbox">Show Heading instead of Yaw</label>
		</div>
	</div>
</template>

<style scoped>
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
	background-image: url("/src/assets/grid.png");
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
