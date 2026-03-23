<script lang="ts">
import Channel from "@components/Channel.vue";
import { defineComponent } from "vue";
import { sendCommand, onConnectHandler } from "@/msp/comm";
import { MspFn } from "@/msp/protocol";
import { delay, leBytesToInt } from "@utils/utils";
import RxMode from "@/components/RxMode.vue";
import ReceiverDevice from "@/components/ReceiverDevice.vue";
import { CrsfDevice } from "@/utils/types";

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
			exiting: false,
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
				{ name: 'DCDC enabled', min: -50, max: 50, channel: 11 },
			],
			crsfDevices: [] as CrsfDevice[],
			subbedTo: 0,
			getInterval: -1,
		};
	},
	components: {
		Channel,
		RxMode,
		ReceiverDevice,
	},
	mounted() {
		this.getRcContinuous()
		this.channelInterval = setInterval(() => {
			sendCommand(MspFn.GET_RX_STATUS)
				.then(c => {
					this.isReceiverUp = c.data[0] > 0;
					this.isLinkUp = c.data[1] > 0;
					this.uplinkRssi[0] = leBytesToInt(c.data, 2, 1, true);
					this.uplinkRssi[1] = leBytesToInt(c.data, 3, 1, true);
					this.uplinkLinkQuality = c.data[4];
					this.uplinkSnr = leBytesToInt(c.data, 5, 1, true);
					this.antennaSelection = c.data[6];
					this.packetRateIdx = c.data[7];
					this.txPower = leBytesToInt(c.data, 8, 2);
					this.targetPacketRate = leBytesToInt(c.data, 10, 2);
					this.actualPacketRate = leBytesToInt(c.data, 12, 2);
					this.rcMsgCount = leBytesToInt(c.data, 14, 4);
				})
				.catch(() => { });
		}, 1000);
		this.scanDevices()
		this.getInterval = setInterval(this.getDevices, 200)
		this.relaxGetter()
		this.getModes();
		onConnectHandler(this.getModes, this.scanDevices, this.unsubscribe);
	},
	unmounted() {
		clearInterval(this.channelInterval);
		clearInterval(this.getInterval);
		this.getInterval = -1
		this.exiting = true
	},
	methods: {
		subscribe(i: number) {
			this.subbedTo = 0
			const address = this.crsfDevices[i].address
			sendCommand(MspFn.CRSF_SUBSCRIBE, [address, 1, 0x2B]).then(c => {
				if (c.data[0] === address && c.data[1] === 1)
					this.subbedTo = address
			})
		},
		unsubscribe() {
			this.subbedTo = 0
			sendCommand(MspFn.CRSF_SUBSCRIBE, [0, 0])
		},
		relaxGetter() {
			setTimeout(() => {
				if (this.getInterval !== -1) {
					clearInterval(this.getInterval)
					this.getInterval = setInterval(this.getDevices, 5000)
				}
			}, 3500)
		},
		speedupGetter() {
			if (this.getInterval !== -1) {
				clearInterval(this.getInterval)
				this.getInterval = setInterval(this.getDevices, 200)
				this.relaxGetter()
			}
		},
		scanDevices() {
			sendCommand(MspFn.CRSF_SCAN_DEVICES)
				.then(this.speedupGetter)
				.catch(() => { })
		},
		getDevices() {
			sendCommand(MspFn.CRSF_GET_DEVICES)
				.then(c => {
					this.crsfDevices = []
					for (let i = 0; i < c.data.length / 47; i++) {
						const d = c.data.slice(i * 47, i * 47 + 47)
						const strEnd = d.indexOf(0)
						const name = String.fromCharCode(...d.slice(0, strEnd))
						const address = d[32]
						const paramCount = d[33]
						const paramVersion = d[34]
						const serialNo = d.slice(35, 39)
						const hardwareId = d.slice(39, 43)
						const firmwareId = d.slice(43, 47)
						this.crsfDevices.push({
							name,
							address,
							paramCount,
							paramVersion,
							serialNo,
							hardwareId,
							firmwareId,
						})
					}
				}).catch(() => { })
		},
		async getRcContinuous() {
			while (!this.exiting) {
				try {
					const c = await sendCommand(MspFn.RC)
					const channelCount = c.data.length / 2;
					if (channelCount !== this.channels.length) {
						this.channels = new Array(channelCount).fill(1500);
					}
					for (let i = 0; i < 16; i++) {
						this.channels[i] = leBytesToInt(c.data, i * 2, 2);
					}
				} catch (_) {
					await delay(10) // avoid hung program if sendCommand rejects immediately
				}
			}
		},
		getModes() {
			sendCommand(MspFn.GET_RX_MODES)
				.then(c => {
					const modeCount = c.data.length / 4;
					for (let i = 0; i < modeCount; i++) {
						const offset = i * 4;
						this.rxModes[i].channel = c.data[offset] > 127 ? c.data[offset] - 256 : c.data[offset];
						this.rxModes[i].min = c.data[offset + 1] > 127 ? c.data[offset + 1] - 256 : c.data[offset + 1];
						this.rxModes[i].max = c.data[offset + 2] > 127 ? c.data[offset + 2] - 256 : c.data[offset + 2];
					}
				})
				.catch(() => { });
		},
		saveSettings() {
			const data: number[] = []
			this.rxModes.forEach((mode, index) => {
				data[index * 4] = mode.channel < 0 ? mode.channel + 256 : mode.channel;
				data[index * 4 + 1] = mode.min < 0 ? mode.min + 256 : mode.min;
				data[index * 4 + 2] = mode.max < 0 ? mode.max + 256 : mode.max;
				data[index * 4 + 3] = 0; // Reserved byte
			});
			sendCommand(MspFn.SET_RX_MODES, data)
				.then(() => { return sendCommand(MspFn.SAVE_SETTINGS) })
				.then(this.getModes)
				.catch(() => { });
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
				<button @click="scanDevices">Rescan Devices</button>
				<div class="crsfDevices">
					<ReceiverDevice v-for="(c, i) in crsfDevices" :dev="c" @sub="subscribe(i)" @unsub="unsubscribe"
						:key="c.address" :subbed="subbedTo === c.address" />
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
	padding: 1rem 5% 0px 5%;
	width: 100%;
	height: 100%;
	display: flex;
	flex-direction: column;
	align-items: center;
	overflow: hidden;
	box-sizing: border-box;
}

.header {
	width: 100%;
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
	overflow: auto;
}

#rxFlex>* {
	box-sizing: border-box;
}

#rxChannels {
	min-width: 300px;
	flex-grow: 1;
	box-sizing: border-box;
}

#rxSettings {
	min-width: 350px;
	flex-grow: 2;
}

.crsfDevices {
	display: grid;
	grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
	grid-auto-rows: minmax(350px, auto);
	overflow: hidden;
	gap: 2px;
}

#rxModes {
	min-width: 350px;
	flex-grow: 1;
	height: 100%;
	overflow: auto;
}

button {
	color: black
}
</style>
