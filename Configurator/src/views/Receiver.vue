<script lang="ts">
import Channel from "@components/Channel.vue";
import { defineComponent } from "vue";
import { addOnCommandHandler, sendCommand, removeOnCommandHandler } from "@/communication/serial";
import { MspFn } from "@utils/msp";
import { Command } from "@utils/types";
import { leBytesToInt } from "@utils/utils";

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
		};
	},
	components: {
		Channel,
	},
	mounted() {
		this.statusInterval = setInterval(() => {
			sendCommand('request', MspFn.RC).catch(() => { });
		}, 20);
		this.channelInterval = setInterval(() => {
			sendCommand('request', MspFn.GET_RX_STATUS).catch(() => { });
		}, 1000);
		addOnCommandHandler(this.onCommand);
	},
	unmounted() {
		clearInterval(this.statusInterval);
		clearInterval(this.channelInterval);
		removeOnCommandHandler(this.onCommand);
	},
	methods: {
		onCommand(command: Command) {
			if (command.cmdType === 'response') {
				switch (command.command) {
					case MspFn.RC: {
						const channelCount = command.data.length / 2;
						if (channelCount !== this.channels.length) {
							this.channels = new Array(channelCount).fill(1500);
						}
						for (let i = 0; i < 16; i++) {
							this.channels[i] = leBytesToInt(command.data.slice(i * 2, i * 2 + 2));
						}
					} break;
					case MspFn.GET_RX_STATUS: {
						this.isReceiverUp = command.data[0] > 0;
						this.isLinkUp = command.data[1] > 0;
						this.uplinkRssi[0] = leBytesToInt([command.data[2]], true);
						this.uplinkRssi[1] = leBytesToInt([command.data[3]], true);
						this.uplinkLinkQuality = command.data[4];
						this.uplinkSnr = leBytesToInt([command.data[5]], true);
						this.antennaSelection = command.data[6];
						this.packetRateIdx = command.data[7];
						this.txPower = leBytesToInt(command.data.slice(8, 10));
						this.targetPacketRate = leBytesToInt(command.data.slice(10, 12));
						this.actualPacketRate = leBytesToInt(command.data.slice(12, 14));
						this.rcMsgCount = leBytesToInt(command.data.slice(14, 18));
					} break;
				}
			}
		},
	}
})
</script>
<template>
	<div id="rxWrapper">
		<div id="rxChannels">
			<Channel v-for="ch in channels" :value="ch" />
		</div>
		<div id="rxSettings">
			<div id="rxStatus">
				{{ isReceiverUp ? 'Receiver found' + (isLinkUp ? ' and connected to TX' : ', but not connected to TX') :
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
	</div>
</template>

<style scoped>
#rxWrapper {
	margin: 1rem auto;
	width: 90vw;
	max-width: 200vh;
	display: flex;
	flex-wrap: wrap;
	flex-direction: row;
	justify-content: space-between;
	gap: 3rem;
}

#rxChannels {
	min-width: 700px;
	flex-grow: 1;
}

#rxSettings {
	min-width: 200px;
	flex-grow: 1;
}
</style>
