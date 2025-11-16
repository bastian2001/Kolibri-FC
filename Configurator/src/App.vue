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

		addOnConnectHandler(this.onConnect);
		addOnDisconnectHandler(this.onDisconnect);
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
		removeOnConnectHandler(this.onConnect);
		removeOnDisconnectHandler(this.onDisconnect);
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
			routes,
			sendCommand,
			MspFn,
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
		onDisconnect() {
			this.connected = false;
		},
		onConnect() {
			this.connected = true;
			this.configuratorLog.clearEntries();
			sendCommand(MspFn.API_VERSION)
				.then(c => {
					this.configuratorLog.push(
						`Protocol: ${c.data[0]}, API: ${c.data[1]}.${c.data[2]}`
					);
					return sendCommand(MspFn.FIRMWARE_VARIANT)
				})
				.then(c => {
					this.configuratorLog.push(`Firmware: ${c.dataStr}`);
					if (c.dataStr !== 'KOLI') {
						this.disconnect();
						throw 'This configurator is only compatible with Kolibri firmware.'
					}
					return sendCommand(MspFn.FIRMWARE_VERSION)
				})
				.then(c => {
					this.configuratorLog.push(`Version: ${c.data[0]}.${c.data[1]}.${c.data[2]}`);
					return sendCommand(MspFn.BOARD_INFO)
				})
				.then(c => {
					this.configuratorLog.push(
						`Board: ${c.dataStr.substring(0, 4)} => ${c.dataStr.substring(
							9,
							9 + c.data[8]
						)}`
					);
					return sendCommand(MspFn.BUILD_INFO)
				})
				.then(c => {
					const date = c.dataStr.substring(0, 11);
					const time = c.dataStr.substring(11, 19);
					const githash = c.dataStr.substring(19);
					this.configuratorLog.push(`Firmware released: ${date} ${time} (Git: #${githash})`);
					return sendCommand(MspFn.GET_NAME)
				})
				.then(c => {
					this.configuratorLog.push(`Name: ${c.dataStr}`);
					return sendCommand(MspFn.STATUS)
				})
				.then(c => {
					this.battery = `${(leBytesToInt(c.data, 0, 2) / 100).toFixed(2)}V`;

					const now = Date.now();
					return sendCommand(MspFn.SET_RTC, [
						...intToLeBytes(Math.floor(now / 1000), 4),
						...intToLeBytes(now % 1000, 2)
					]);
				})
				.then(() => {
					this.configuratorLog.push('RTC updated');

					const offset = -new Date().getTimezoneOffset();
					return sendCommand(MspFn.SET_TZ_OFFSET, [
						...intToLeBytes(offset, 2),
					]);
				})
				.then(() => {
					this.configuratorLog.push('Timezone offset updated');
				})
				.catch(er => {
					this.configuratorLog.push('Could not connect: ' + er)
				})
		},
		confScroll(e: WheelEvent) {
			(this.$refs.logDiv as HTMLDivElement).scrollBy({ top: e.deltaY > 0 ? 15 : -15 })
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
				<button v-if="connected" @click="disconnect"
					@click.right.prevent="() => { sendCommand(MspFn.REBOOT, [4]).then(disconnect) }"
					class="connectButton">Disconnect</button>
				<button v-else-if="serialDevices.length || wifiDevices.length || manualDevice" @click="connect"
					class="connectButton">Connect</button>
				<span v-else style="color: #888">No devices found</span>
				&nbsp;&nbsp;
			</div>
			<div v-if="connected" class="battery">
				<p>Battery: {{ battery }}</p>
			</div>
			<div class="log" ref="logDiv" @wheel.prevent="confScroll">
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
	background-color: var(--background-light);
}

.kolibriLogo {
	height: 3rem;
	flex-grow: 0;
	background-color: black;
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
	background-color: var(--background-blue);
	height: 5rem;
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
	height: 100%;
	box-sizing: border-box;
}

.navElement.currentPage {
	border-top: 3px solid transparent;
	background-color: var(--background-color);
}

.pageName {
	margin: 0;
	padding: 0;
}

.grow {
	flex-grow: 1;
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
