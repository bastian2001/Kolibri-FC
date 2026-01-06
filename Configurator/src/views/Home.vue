<script lang="ts">
import { defineComponent } from 'vue';
import { addOnCommandHandler, getPingTime, sendCommand, sendRaw, disconnect, removeOnCommandHandler, enableCommands } from '@/msp/comm';
import { useLogStore } from '@stores/logStore';
import { leBytesToInt, delay, intToLeBytes } from '@utils/utils';
import { MspFn } from '@/msp/protocol';
import { Command } from '@utils/types';
import { prefixZeros } from '@utils/utils';
import Drone3dPreview from '@/components/Drone3dPreview.vue';

const FLIGHT_MODES = ['Acro', 'Angle', 'Altitude Hold', 'GPS Hold', 'Waypoint'];
const ARMING_DISABLE_FLAGS = [
	'Arming switch',
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
	components: {
		Drone3dPreview
	}
	,
	mounted() {
		addOnCommandHandler(this.onCommand);
		this.getRotationContinuous();

		this.pingInterval = setInterval(() => {
			this.fcPing = getPingTime();
		}, 200);
		this.rtcInterval = setInterval(() => {
			sendCommand(MspFn.GET_RTC).then(c => {
				this.time.year = leBytesToInt(c.data, 0, 2);
				this.time.month = c.data[2];
				this.time.day = c.data[3];
				this.time.hour = c.data[4];
				this.time.minute = c.data[5];
				this.time.second = c.data[6];
			}).catch(() => { })
		}, 1000);
	},
	unmounted() {
		removeOnCommandHandler(this.onCommand);
		clearInterval(this.getRotationInterval);
		clearInterval(this.pingInterval);
		clearInterval(this.rtcInterval);
		this.exiting = true
	},
	data() {
		return {
			flightMode: 0,
			armingDisableFlags: 0,
			armed: false,
			fcPing: -1,
			getRotationInterval: -1,
			attitude: { roll: 0, pitch: 0, yaw: 0, heading: 0 },
			quat: { qw: 1, q0: 0, q1: 0, q2: 0 },
			showHeading: false,
			serialNum: 1,
			baudRate: 115200,
			time: { year: 0, month: 1, day: 1, hour: 0, minute: 0, second: 0, },
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
			exiting: false,
		};
	},
	methods: {
		async getRotationContinuous() {
			while (!this.exiting) {
				try {
					const c = await sendCommand(MspFn.GET_ROTATION)
					let roll = leBytesToInt(c.data, 0, 2, true)
					roll /= 8192.0
					roll *= 180.0 / Math.PI
					let pitch = leBytesToInt(c.data, 2, 2, true)
					pitch /= 8192.0
					pitch *= 180.0 / Math.PI
					let yaw = leBytesToInt(c.data, 4, 2, true)
					yaw /= 8192.0
					yaw *= 180.0 / Math.PI
					let heading = leBytesToInt(c.data, 6, 2, true)
					heading /= 8192.0
					heading *= 180.0 / Math.PI
					this.attitude = { roll, pitch, yaw, heading }
					let qw = leBytesToInt(c.data, 8, 2, true) / 20000
					let q0 = leBytesToInt(c.data, 10, 2, true) / 20000
					let q1 = leBytesToInt(c.data, 12, 2, true) / 20000
					let q2 = leBytesToInt(c.data, 14, 2, true) / 20000
					this.quat = { qw, q0, q1, q2 }
				} catch (_) {
					await delay(10) // avoid hung program if sendCommand rejects immediately
				}
			}
		},
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
						break;
					case MspFn.REBOOT:
						this.configuratorLog.push('Rebooting');
						disconnect();
						break;
				}
			}
		},
		ledOn() {
			sendCommand(MspFn.SET_DEBUG_LED, [1])
		},
		ledOff() {
			sendCommand(MspFn.SET_DEBUG_LED, [0])
		},
		playSound() {
			sendCommand(MspFn.PLAY_SOUND).then(c => console.log(c.data))
		},
		startPassthrough() {
			sendCommand(MspFn.SERIAL_PASSTHROUGH, [
				this.serialNum,
				...intToLeBytes(this.baudRate, 4),
			]).then(c => {
				const sPort = c.data[0];
				const baud = leBytesToInt(c.data, 1, 4);
				this.configuratorLog.push(
					`Serial passthrough started on Serial${sPort} with baud rate ${baud}`
				);
				disconnect();
			})
		},
		getCrashDump() {
			sendCommand(MspFn.GET_CRASH_DUMP).then(c => {
				this.configuratorLog.push('See console for crash dump');
				console.log(c.data);
			})
		},
		clearCrashDump() {
			sendCommand(MspFn.CLEAR_CRASH_DUMP).then(() => {
				this.configuratorLog.push('Crash dump cleared');
			})
		},
		reboot() {
			sendCommand(MspFn.REBOOT, [REBOOT_MODES.FIRMWARE])
		},
		rebootBootloader() {
			sendCommand(MspFn.REBOOT, [REBOOT_MODES.BOOTLOADER_FLASH])
		},
	},
})
</script>
<template>
	<div class="wrapper">
		<div>
			<button @click="ledOn">LED On</button>
			<button @click="ledOff">LED Off</button>
			<button @click="playSound">Play Sound</button>
			<input type="number" step="1" min="1" max="4" placeholder="Serial Number" v-model="serialNum" />
			<input type="number" step="1" min="9600" max="115200" placeholder="Baud Rate" v-model="baudRate" />
			<button @click="startPassthrough">Start Serial Passthrough</button>
			<button @click="() => {
				enableCommands(false);
				sendRaw([], '+++')
					.then(() => {
						return delay(1500)
					})
					.then(() => enableCommands(true));
			}">Stop Serial Passthrough</button>
			<button @click="getCrashDump">Get Crash Dump</button>
			<button @click="clearCrashDump">Clear Crash Dump</button>
			<button @click="reboot">Reboot</button>
			<button @click="rebootBootloader">Bootloader</button>
		</div>
		<div class="droneStatus">Flight Mode: {{ FLIGHT_MODES[flightMode] }}, {{ armed ? 'Armed' : 'Disarmed' }}<br />
			Arming Disabled Flags:<br />
			{{
				ARMING_DISABLE_FLAGS
					.map((flag, i) => {
						if (armingDisableFlags & (1 << i)) return flag
					}).filter(el => el)
					.join(', ')
			}}
		</div>
		<div>Ping: {{ fcPing === -1 ? 'Error' : fcPing + ' ms' }}</div>
		<div>
			Time: {{ prefixZeros(time.year, 4) }}-{{ prefixZeros(time.month, 2) }}-{{ prefixZeros(time.day, 2) }}
			{{ prefixZeros(time.hour, 2) }}:{{ prefixZeros(time.minute, 2) }}:{{ prefixZeros(time.second, 2) }}
		</div>
		<Drone3dPreview :roll="attitude.roll" :pitch="attitude.pitch"
			:yaw="showHeading ? attitude.heading : attitude.yaw" />
		<div class="attInfo">
			<div class="axisLabel axisRoll">Roll: {{ attitude.roll.toFixed(2) }}°</div>
			<div class="axisLabel axisPitch">Pitch: {{ attitude.pitch.toFixed(2) }}°</div>
			<div v-if="showHeading" class="axisLabel axisHeading">Heading: {{ attitude.heading.toFixed(2) }}°
			</div>
			<div v-else class="axisLabel axisYaw">Yaw: {{ attitude.yaw.toFixed(2) }}°</div>
			<br />
			w: {{ quat.qw }}<br>v0: {{ quat.q0 }}<br>v1: {{ quat.q1 }}<br>v2: {{ quat.q2 }}<br>
			<input type="checkbox" v-model="showHeading" id="headingCheckbox" />
			<label for="headingCheckbox">Show Heading instead of Yaw</label>
		</div>
	</div>
</template>

<style scoped>
.wrapper {
	padding: 1rem 0px;
	box-sizing: border-box;
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
