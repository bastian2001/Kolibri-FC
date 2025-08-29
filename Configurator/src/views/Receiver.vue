<script lang="ts">
import Channel from "@components/Channel.vue";
import { defineComponent } from "vue";
import { addOnCommandHandler, sendCommand, removeOnCommandHandler, addOnConnectHandler, removeOnConnectHandler } from "@/msp/comm";
import { MspFn } from "@/msp/protocol";
import { Command } from "@utils/types";
import { leBytesToInt } from "@utils/utils";
import RxMode from "@/components/RxMode.vue";

export default defineComponent({
	name: "Receiver",
	data() {
		return {
			isReceiverUp: false,
			isLinkUp: false,
			uplinkRssi: [0, 0],
			uplinkLinkQuality: 0,
			uplinkSnr: 0,
			antennaSelection: 0,
			packetRateIdx: 0,
			targetPacketRate: 0,
			actualPacketRate: 0,
			txPower: 0,
			rcMsgCount: 0,
			channels: new Array(16).fill(1500),
			channelInterval: -1,
			statusInterval: -1,
			rxModes: [
				{ name: "Armed", min: -50, max: 50, channel: 4 },
				{ name: "Angle Mode", min: -50, max: 50, channel: 5 },
				{ name: "Altitude Hold", min: -50, max: 50, channel: 6 },
				{ name: "GPS Position Hold", min: -50, max: 50, channel: 7 },
				{ name: "Waypoint Mode", min: -50, max: 50, channel: 8 },
				{ name: "Beeper", min: -50, max: 50, channel: 9 },
				{ name: "Blackbox Highlight", min: -50, max: 50, channel: 10 },
				{ name: "Tuning: Next Parameter", min: -50, max: 50, channel: 10 },
				{ name: "Tuning: Previous Parameter", min: -50, max: 50, channel: 10 },
				{ name: "Tuning: Increase Value", min: -50, max: 50, channel: 10 },
				{ name: "Tuning: Decrease Value", min: -50, max: 50, channel: 10 },
			]
		};
	},
	components: {
		Channel,
		RxMode,
	},
	mounted() {
		this.statusInterval = setInterval(() => {
			sendCommand(MspFn.RC).catch(() => { });
		}, 20);
		this.channelInterval = setInterval(() => {
			sendCommand(MspFn.GET_RX_STATUS).catch(() => { });
		}, 1000);
		this.getModes();
		addOnConnectHandler(this.getModes);
		addOnCommandHandler(this.onCommand);
	},
	unmounted() {
		clearInterval(this.statusInterval);
		clearInterval(this.channelInterval);
		removeOnConnectHandler(this.getModes);
		removeOnCommandHandler(this.onCommand);
	},
	methods: {
		getModes() {
			sendCommand(MspFn.GET_RX_MODES).catch(() => { });
		},
		saveSettings() {
			const data: number[] = []
			this.rxModes.forEach((mode, index) => {
				data[index * 4] = mode.channel < 0 ? mode.channel + 256 : mode.channel;
				data[index * 4 + 1] = mode.min < 0 ? mode.min + 256 : mode.min;
				data[index * 4 + 2] = mode.max < 0 ? mode.max + 256 : mode.max;
				data[index * 4 + 3] = 0; // Reserved byte
			});
			sendCommand(MspFn.SET_RX_MODES, data).catch(() => { });
		},
		onCommand(command: Command) {
			if (command.cmdType === 'response') {
				switch (command.command) {
					case MspFn.RC: {
						const channelCount = command.data.length / 2;
						if (channelCount !== this.channels.length) {
							this.channels = new Array(channelCount).fill(1500);
						}
						for (let i = 0; i < 16; i++) {
							this.channels[i] = leBytesToInt(command.data, i * 2, 2);
						}
					} break;
					case MspFn.GET_RX_STATUS: {
						this.isReceiverUp = command.data[0] > 0;
						this.isLinkUp = command.data[1] > 0;
						this.uplinkRssi[0] = leBytesToInt(command.data, 2, 1, true);
						this.uplinkRssi[1] = leBytesToInt(command.data, 3, 1, true);
						this.uplinkLinkQuality = command.data[4];
						this.uplinkSnr = leBytesToInt(command.data, 5, 1, true);
						this.antennaSelection = command.data[6];
						this.packetRateIdx = command.data[7];
						this.txPower = leBytesToInt(command.data, 8, 2);
						this.targetPacketRate = leBytesToInt(command.data, 10, 2);
						this.actualPacketRate = leBytesToInt(command.data, 12, 2);
						this.rcMsgCount = leBytesToInt(command.data, 14, 4);
					} break;
					case MspFn.GET_RX_MODES: {
						const modeCount = command.data.length / 4;
						for (let i = 0; i < modeCount; i++) {
							const offset = i * 4;
							this.rxModes[i].channel = command.data[offset] > 127 ? command.data[offset] - 256 : command.data[offset];
							this.rxModes[i].min = command.data[offset + 1] > 127 ? command.data[offset + 1] - 256 : command.data[offset + 1];
							this.rxModes[i].max = command.data[offset + 2] > 127 ? command.data[offset + 2] - 256 : command.data[offset + 2];
						}
					} break;
					case MspFn.SET_RX_MODES: {
						sendCommand(MspFn.SAVE_SETTINGS).catch(() => { });
						this.getModes();
					} break;
				}
			}
		},
	}
})
</script>
<template>
	<div id="rxWrapper">
		<div class="header">
			<button class="saveBtn" @click="() => { saveSettings() }">Save Settings</button>
		</div>
		<div id="rxFlex">
			<div id="rxChannels">
				<Channel v-for="ch in channels" :value="ch" />
			</div>
			<div id="rxSettings">
				<div id="rxStatus">
					{{ isReceiverUp ? 'Receiver found' + (isLinkUp ? ' and connected to TX' :
						', but not connected to TX') :
						'No Receiver found' }}<br />
					RSSI: {{ uplinkRssi[0] }}dBm, {{ uplinkRssi[1] }}dBm<br />
					LQI: {{ uplinkLinkQuality }}%<br />
					SNR: {{ uplinkSnr }}dB<br />
					Antenna {{ antennaSelection }} is used<br />
					Packet Rate index {{ packetRateIdx }} ({{ targetPacketRate }}Hz) => actual {{ actualPacketRate
					}}Hz<br />
					TX Power: {{ txPower }}mW<br />
					Total RC message count: {{ rcMsgCount }}
				</div>
			</div>
			<div id="rxModes">
				<RxMode v-for="(mode, index) in rxModes" :key="index" :min="mode.min" :max="mode.max" :rc="channels"
					:channel="mode.channel" :name="mode.name"
					@update:channel="(channel) => { rxModes[index].channel = channel; }"
					@update:range="(min, max) => { rxModes[index].min = min; rxModes[index].max = max; }" />
			</div>
		</div>
	</div>
</template>

<style scoped>
#rxWrapper {
	margin: 1rem auto;
	width: 90vw;
	max-width: 200vh;
}

.saveBtn {
	float: right;
	background-color: transparent;
	border: 1px solid var(--border-green);
	border-radius: 5px;
	padding: 0.5rem 1rem;
	font-size: 1rem;
	color: var(--text-color);
	transition: background-color 0.2s ease-out;
	margin-bottom: 1rem;
}

.saveBtn:hover {
	background-color: #fff1;
}

#rxFlex {
	width: 100%;
	display: flex;
	flex-wrap: wrap;
	flex-direction: row;
	justify-content: space-between;
	gap: 3rem;
}

#rxChannels {
	min-width: 500px;
	flex-grow: 1;
}

#rxSettings {
	min-width: 200px;
	flex-grow: 1;
}

#rxModes {
	min-width: 500px;
	flex-grow: 1;
}
</style>
