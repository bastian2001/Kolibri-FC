<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import RxChannel from './channel.svelte';
	import { port, MspFn } from '../../portStore';
	import { leBytesToInt } from '../../utils';

	let channels = new Array(16).fill(1500);

	let isReceiverUp = false;
	let isLinkUp = false;
	let uplinkRssi = [0, 0];
	let uplinkLinkQuality = 0;
	let uplinkSnr = 0;
	let antennaSelection = 0;
	let packetRateIdx = 0;
	let targetPacketRate = 0;
	let actualPacketRate = 0;
	let txPower = 0;
	let rcMsgCount = 0;

	const channelInterval = setInterval(() => {
		port.sendCommand('request', MspFn.RC).catch(() => {});
	}, 20);

	const statusInterval = setInterval(() => {
		port.sendCommand('request', MspFn.GET_RX_STATUS).catch(() => {});
	}, 1000);

	const unsubscribe = port.subscribe(command => {
		if (command.cmdType === 'response') {
			switch (command.command) {
				case MspFn.RC:
					{
						const channelCount = command.data.length / 2;
						if (channelCount !== channels.length) {
							channels = new Array(channelCount).fill(1500);
						}
						for (let i = 0; i < 16; i++) {
							channels[i] = command.data[i * 2] + (command.data[i * 2 + 1] << 8);
						}
					}
					break;
				case MspFn.GET_RX_STATUS:
					{
						isReceiverUp = command.data[0] > 0;
						isLinkUp = command.data[1] > 0;
						uplinkRssi[0] = leBytesToInt([command.data[2]], true);
						uplinkRssi[1] = leBytesToInt([command.data[3]], true);
						uplinkLinkQuality = command.data[4];
						uplinkSnr = leBytesToInt([command.data[5]], true);
						antennaSelection = command.data[6];
						packetRateIdx = command.data[7];
						txPower = leBytesToInt(command.data.slice(8, 10));
						targetPacketRate = leBytesToInt(command.data.slice(10, 12));
						actualPacketRate = leBytesToInt(command.data.slice(12, 14));
						rcMsgCount = leBytesToInt(command.data.slice(14, 18));
					}
					break;
			}
		}
	});

	onMount(() => {
		port.sendCommand('request', MspFn.SCAN_CRSF_DEVICES).catch(() => {});
	});

	onDestroy(() => {
		clearInterval(channelInterval);
		clearInterval(statusInterval);
		unsubscribe();
	});
</script>

<div id="rxWrapper">
	<div id="rxChannels">
		{#each channels as ch}
			<RxChannel value={ch} />
		{/each}
	</div>
	<div id="rxSettings">
		<div id="rxStatus">
			{isReceiverUp
				? 'Receiver found' + (isLinkUp ? ' and connected to TX' : ', but not connected to TX')
				: 'No Receiver found'}<br />
			RSSI: {uplinkRssi[0]}dBm, {uplinkRssi[1]}dBm<br />
			LQI: {uplinkLinkQuality}%<br />
			SNR: {uplinkSnr}dB<br />
			Antenna {antennaSelection} is used<br />
			Packet Rate index {packetRateIdx} ({targetPacketRate}Hz) => actual {actualPacketRate}Hz<br />
			TX Power: {txPower}mW<br />
			Total RC message count: {rcMsgCount}
		</div>
	</div>
</div>

<style>
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
