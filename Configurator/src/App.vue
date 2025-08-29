<script lang="ts">
import { defineComponent } from "vue";
import { MspFn } from "@/msp/protocol";
import { leBytesToInt, delay, intToLeBytes } from "@utils/utils";
import { routes } from "./router";
import { connect, disconnect, sendCommand, getSerialDevices, getWifiDevices, addOnCommandHandler, addOnConnectHandler, addOnDisconnectHandler, removeOnCommandHandler, removeOnConnectHandler, removeOnDisconnectHandler } from "./msp/comm";
import { useLogStore } from "@stores/logStore";
import { Command } from "./utils/types";

export default defineComponent({
	name: "App",
	mounted() {
		this.listInterval = setInterval(this.listDevices, 1000);
		delay(150).then(this.listDevices);
		disconnect();

		addOnConnectHandler(this.och);
		addOnDisconnectHandler(this.odh);
		addOnCommandHandler(this.onCommand)

		this.configuratorLog.$subscribe(() => {
			this.$nextTick().then(() => {
				//wait for DOM to update
				const logDiv = this.$refs.logDiv as HTMLDivElement;
				logDiv.scrollTop = logDiv.scrollHeight;
			})
		})
	},
	unmounted() {
		clearInterval(this.listInterval);
		this.disconnect();
		removeOnConnectHandler(this.och);
		removeOnDisconnectHandler(this.odh);
		removeOnCommandHandler(this.onCommand);
	},
	data() {
		return {
			configuratorLog: useLogStore(),
			serialDevices: [] as string[],
			wifiDevices: [] as string[][],
			manualDevice: false,
			listInterval: -1,
			battery: '',
			device: '',
			connected: false,
			routes
		};
	},
	methods: {
		onCommand(command: Command) {
			if (command.cmdType === 'request') {
				switch (command.command) {
					case MspFn.IND_MESSAGE:
						this.configuratorLog.push(command.dataStr);
						break;
				}
			} else if (command.cmdType === 'response') {
				switch (command.command) {
					case MspFn.API_VERSION:
						this.configuratorLog.push(
							`Protocol: ${command.data[0]}, API: ${command.data[1]}.${command.data[2]}`
						);
						break;
					case MspFn.FIRMWARE_VARIANT:
						this.configuratorLog.push(`Firmware: ${command.dataStr}`);
						if (command.dataStr !== 'KOLI') {
							this.configuratorLog.push(
								`This configurator is only compatible with Kolibri firmware, disconnecting...`
							);
							this.disconnect();
						}
						break;
					case MspFn.FIRMWARE_VERSION:
						this.configuratorLog.push(`Version: ${command.data[0]}.${command.data[1]}.${command.data[2]}`);
						break;
					case MspFn.BOARD_INFO:
						this.configuratorLog.push(
							`Board: ${command.dataStr.substring(0, 4)} => ${command.dataStr.substring(
								9,
								9 + command.data[8]
							)}`
						);
						break;
					case MspFn.BUILD_INFO:
						const date = command.dataStr.substring(0, 11);
						const time = command.dataStr.substring(11, 19);
						const githash = command.dataStr.substring(19);
						this.configuratorLog.push(`Firmware released: ${date} ${time} (Git: #${githash})`);
						break;
					case MspFn.GET_NAME:
						this.configuratorLog.push(`Name: ${command.dataStr}`);
						break;
					case MspFn.STATUS:
						this.battery = `${(leBytesToInt(command.data, 0, 2) / 100).toFixed(2)}V`;
						break;
					case MspFn.SET_RTC:
						this.configuratorLog.push('RTC updated');
						break;
					case MspFn.SET_TZ_OFFSET:
						this.configuratorLog.push('Timezone offset updated');
						break;
					case MspFn.PLAY_SOUND:
						console.log(command.data);
						break;
					case MspFn.SAVE_SETTINGS:
						this.configuratorLog.push('EEPROM saved');
				}
			}
		},
		connect() {
			console.log("Connecting to " + this.device);
			connect(this.device).catch(() => {
				this.connected = false;
			});
		},
		disconnect() {
			disconnect().catch(() => {
				this.connected = false;
			});
		},
		listDevices() {
			this.serialDevices = getSerialDevices();
			this.wifiDevices = getWifiDevices();
		},
		odh() {
			this.connected = false;
		},
		och() {
			this.connected = true;
			this.configuratorLog.clearEntries();
			sendCommand(MspFn.API_VERSION)
				.then(() => sendCommand(MspFn.FIRMWARE_VARIANT))
				.then(() => delay(5))
				.then(() => sendCommand(MspFn.FIRMWARE_VERSION))
				.then(() => delay(5))
				.then(() => sendCommand(MspFn.BOARD_INFO))
				.then(() => delay(5))
				.then(() => sendCommand(MspFn.BUILD_INFO))
				.then(() => delay(5))
				.then(() => sendCommand(MspFn.GET_NAME))
				.then(() => delay(5))
				.then(() => sendCommand(MspFn.STATUS))
				.then(() => {
					const now = Date.now();
					sendCommand(MspFn.SET_RTC, [
						...intToLeBytes(Math.floor(now / 1000), 4),
						...intToLeBytes(now % 1000, 2)
					]);
				})
				.then(() => delay(5))
				.then(() => {
					const offset = -new Date().getTimezoneOffset();
					sendCommand(MspFn.SET_TZ_OFFSET, [
						...intToLeBytes(offset, 2),
					]);
				});
		}
	},
});
</script>

<template>
	<div class="main">
		<div class="header">
			<img src="https://svelte.dev/svelte-logo-horizontal.svg" alt="Svelte logo" class="kolibriLogo" />
			<div class="space"></div>
			<div class="connector">
				<input type="checkbox" id="manualDeviceEnabled" v-model="manualDevice" />
				<label for="manualDeviceEnabled">Manual device</label>
				&nbsp;&nbsp;
				<select v-model="device" v-if="!manualDevice">
					<option v-for="d in wifiDevices" :value="d[0]">{{ d[0] }} ({{ d[1] }})</option>
					<option v-for="d in serialDevices" :value="d">{{ d }}</option>
				</select>
				<input v-else type="text" v-model="device" placeholder="Enter device name" class="manualDeviceInput" />
				&nbsp;&nbsp;
				<button v-if="connected" @click="disconnect" class="connectButton">Disconnect</button>
				<button v-else-if="serialDevices.length || wifiDevices.length || manualDevice" @click="connect"
					class="connectButton">Connect</button>
				<span v-else style="color: #888">No devices found</span>
				&nbsp;&nbsp;
			</div>
			<div v-if="connected" class="battery">
				<p>Battery: {{ battery }}</p>
			</div>
			<div class="log" ref="logDiv">
				<p v-for="l in configuratorLog.getEntries()">{{ l }}</p>
			</div>
		</div>

		<div class="pageContent">
			<RouterView />
		</div>
		<div class="navigator">
			<div class="navElement grow"></div>
			<RouterLink v-for="el in routes" :to="el.path" class="navElement"
				:class="{ currentPage: $route.path === el.path }">
				<img v-if="false" :src="/*el.img*/ 'x'" :alt="`${el.name} Icon`" class="pageIcon" />
				<p class="pageName">{{ el.name }}</p>
			</RouterLink>
			<div class="navElement grow"></div>
		</div>
	</div>
</template>

<style scoped>
.main {
	display: flex;
	flex-direction: column;
	height: 100vh;
	overflow: hidden;
}

.header {
	display: flex;
	flex-direction: row;
	align-items: center;
	border-bottom: 3px solid var(--border-blue);
}

.kolibriLogo {
	height: 3rem;
}

.space {
	flex-grow: 1;
}

.connector {
	display: flex;
	flex-direction: row;
	align-items: center;
}

.connector select {
	width: 12rem;
	background-color: transparent;
	border: 1px solid var(--border-color);
	appearance: none;
	border-radius: 4px;
	padding: 4px 15px 4px 8px;
	background: transparent url('data:image/gif;base64,R0lGODlhBgAGAKEDAFVVVX9/f9TU1CgmNyH5BAEKAAMALAAAAAAGAAYAAAIODA4hCDKWxlhNvmCnGwUAOw==') right center no-repeat !important;
	background-position: calc(100% - 5px) center !important;
	color: var(--text-color);
	outline: none;
}

.connector select option {
	color: #444;
}

.connectButton {
	background-color: transparent;
	border: 1px solid var(--border-color);
	border-radius: 4px;
	padding: 4px 8px;
	color: var(--text-color);
	outline: none;
}

.log {
	flex-grow: 1;
	min-width: 250px;
	margin-left: 1rem;
	background-color: var(--background-light);
	height: 3rem;
	padding: 0 0.5rem;
	color: var(--text-color);
	font-size: 0.7rem;
	line-height: 100%;
	overflow-y: scroll;
}

.log p {
	margin: 3px 0;
	padding: 0;
}

.navigator {
	display: flex;
	flex-direction: row;
	align-items: center;
	justify-content: center;
}

.navElement {
	display: flex;
	flex-direction: column;
	align-items: center;
	justify-content: center;
	padding: 0.5rem;
	border-top: 3px solid var(--border-blue);
	color: var(--text-color);
	text-decoration: none;
	background-color: var(--background-blue);
}

.navElement.currentPage {
	border-top: 3px solid transparent;
	background-color: var(--background-color);
}

.pageName {
	margin: 0;
	padding: 0;
}

.navElement.grow {
	flex-grow: 1;
	height: 100%;
	box-sizing: border-box;
}

.pageContent {
	flex-shrink: 1;
	flex-grow: 1;
	overflow: auto;
}

.battery p {
	margin: 0;
	padding: 0;
}

.manualDeviceInput {
	background-color: transparent;
	border: 1px solid var(--border-color);
	border-radius: 4px;
	padding: 4px 8px;
	color: var(--text-color);
	outline: none;
}
</style>
