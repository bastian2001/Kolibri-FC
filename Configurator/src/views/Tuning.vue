<script lang="ts">
import { defineComponent } from "vue";
import { MspFn, MspVersion } from "@utils/msp";
import { Command } from "@utils/types";
import { leBytesToInt } from "@utils/utils";
import { sendCommand, addOnCommandHandler, removeOnCommandHandler, addOnConnectHandler, removeOnConnectHandler } from "@/communication/serial";

export default defineComponent({
	name: "Tuning",
	data() {
		return {
			pids: [[], [], []] as number[][],
			rateFactors: [[], [], []] as number[][],
			saveTimeout: -1
		};
	},
	mounted() {
		addOnCommandHandler(this.onCommand);
		addOnConnectHandler(this.getSettings);
		this.getSettings();
	},
	unmounted() {
		removeOnCommandHandler(this.onCommand);
		removeOnConnectHandler(this.getSettings);
		clearTimeout(this.saveTimeout);
	},
	methods: {
		getSettings() {
			sendCommand('request', MspFn.GET_PIDS).then(() => {
				sendCommand('request', MspFn.GET_RATES);
			});
		},
		scrollInputPID(e: WheelEvent, i: number, j: number) {
			let val = 1;
			if (j === 5) val = 0.0001;
			if (e.deltaY > 0) this.pids[i][j] -= val;
			else this.pids[i][j] += val;
			this.pids[i][j] = Math.round(this.pids[i][j] * 10000) / 10000;
			if (this.pids[i][j] < 0) this.pids[i][j] = 0;
			if (j === 5 && this.pids[i][j] > 1) this.pids[i][j] = 1;
		},
		scrollInputRate(e: WheelEvent, i: number, j: number) {
			let val = 5;
			if (e.deltaY > 0) this.rateFactors[i][j] -= val;
			else this.rateFactors[i][j] += val;
			if (this.rateFactors[i][j] < 0) this.rateFactors[i][j] = 0;
			if (j === 5 && this.rateFactors[i][j] > 1) this.rateFactors[i][j] = 1;
		},
		saveSettings() {
			const data = [];
			for (let ax = 0; ax < 3; ax++) {
				for (let i = 0; i < 5; i++) data.push(this.pids[ax][i] & 0xff, (this.pids[ax][i] >> 8) & 0xff);
				data.push(
					Math.round(this.pids[ax][5] * 65536) & 0xff,
					(Math.round(this.pids[ax][5] * 65536) >> 8) & 0xff
				);
				data.push(0, 0);
			}
			sendCommand('request', MspFn.SET_PIDS, MspVersion.V2, data).then(() => {
				this.saveTimeout = setTimeout(() => {
					console.error('Save timeout');
				}, 500);
			});
		},
		onCommand(command: Command) {
			if (command.cmdType === 'response') {
				switch (command.command) {
					case MspFn.GET_PIDS:
						if (command.length !== 3 * 2 * 7) break;
						for (let ax = 0; ax < 3; ax++) {
							for (let i = 0; i < 6; i++)
								this.pids[ax][i] = leBytesToInt(
									command.data.slice(ax * 14 + i * 2, ax * 14 + i * 2 + 2),
									false
								);
							this.pids[ax][5] /= 65536;
							this.pids[ax][5] = Math.round(this.pids[ax][5] * 10000) / 10000;
						}
						break;
					case MspFn.GET_RATES:
						if (command.length !== 5 * 2 * 3) break;
						for (let ax = 0; ax < 3; ax++) {
							for (let i = 0; i < 5; i++)
								this.rateFactors[ax][i] = leBytesToInt(
									command.data.slice(ax * 10 + i * 2, ax * 10 + i * 2 + 2)
								);
						}
						break;
					case MspFn.SET_PIDS:
						const data = [];
						for (let ax = 0; ax < 3; ax++)
							for (let i = 0; i < 5; i++)
								data.push(this.rateFactors[ax][i] & 0xff, (this.rateFactors[ax][i] >> 8) & 0xff);
						sendCommand('request', MspFn.SET_RATES, MspVersion.V2, data);
						break;
					case MspFn.SET_RATES:
						sendCommand('request', MspFn.SAVE_SETTINGS);
						break;
					case MspFn.SAVE_SETTINGS:
						clearTimeout(this.saveTimeout);
						break;
				}
			}
		}
	}
})
</script>
<template>

	<div class="wrapper">
		<div class="header">
			<button class="saveBtn" @click="saveSettings">Save Settings</button>
		</div>
		<div class="pids">
			<h3>PID Gains</h3>
			<table>
				<thead>
					<tr>
						<th>&nbsp;</th>
						<th>P</th>
						<th>I</th>
						<th>D</th>
						<th>FF</th>
						<th>S</th>
						<th>iFall</th>
					</tr>
				</thead>
				<tbody>
					<tr v-for="(ax, i) in pids">
						<td>{{ ['Roll', 'Pitch', 'Yaw'][i] }}</td>
						<td v-for="(_val, j) in ax">
							<input type="number" v-model="pids[i][j]" @wheel="e => {
								scrollInputPID(e, i, j);
							}" />
						</td>
					</tr>
				</tbody>
			</table>
		</div>
		<div class="rates">
			<h3>Rate Factors</h3>
			<table>
				<thead>
					<tr>
						<th>&nbsp;</th>
						<th>x^1</th>
						<th>x^2</th>
						<th>x^3</th>
						<th>x^4</th>
						<th>x^5</th>
					</tr>
				</thead>
				<tbody>
					<tr v-for="(ax, i) in rateFactors">
						<td>{{ ['Roll', 'Pitch', 'Yaw'][i] }}</td>
						<td v-for="(_val, j) in ax">
							<input type="number" v-model="rateFactors[i][j]" @wheel="e => {
								scrollInputRate(e, i, j);
							}" />
						</td>
					</tr>
				</tbody>
			</table>
		</div>
	</div>
</template>

<style scoped>
.wrapper {
	display: grid;
	grid-template-columns: repeat(auto-fit, minmax(500px, 1fr));
	margin: 1.5rem;
	gap: 0rem 3rem;
}

.header {
	grid-column: 1 / -1;
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
}

.saveBtn:hover {
	background-color: #fff1;
}

th,
td {
	text-align: right;
	padding: 3px 8px;
}

input {
	width: 100%;
	background-color: transparent;
	border: none;
	border-bottom: 1px solid var(--border-color);
	text-align: right;
	padding: 3px;
	outline: none;
}

input::-webkit-outer-spin-button,
input::-webkit-inner-spin-button {
	-webkit-appearance: none;
	margin: 0;
}
</style>
