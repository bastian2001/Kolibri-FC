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
			pids: [[0, 0, 0, 0, 0], [0, 0, 0, 0, 0], [0, 0, 0, 0, 0]],
			rateFactors: [[0, 0, 0, 0, 0], [0, 0, 0, 0, 0], [0, 0, 0, 0, 0]],
			saveTimeout: -1,
			rateCanvas: document.createElement('canvas'),
			setpointCanvas: document.createElement('canvas'),
			rc: [0, 0, 0, 0],
			getRcInterval: -1,
			scale: 500,
		};
	},
	mounted() {
		addOnCommandHandler(this.onCommand);
		addOnConnectHandler(this.getSettings);
		window.addEventListener('resize', this.onResize);
		this.onResize();
		this.getSettings();
		this.getRcInterval = setInterval(() => {
			sendCommand('request', MspFn.RC);
		}, 1000 / 60);
	},
	unmounted() {
		removeOnCommandHandler(this.onCommand);
		removeOnConnectHandler(this.getSettings);
		window.removeEventListener('resize', this.onResize);
		clearTimeout(this.saveTimeout);
		clearInterval(this.getRcInterval);
	},
	methods: {
		getSettings() {
			sendCommand('request', MspFn.GET_PIDS).then(() => {
				sendCommand('request', MspFn.GET_RATES);
			});
		},
		scrollInputPID(e: WheelEvent, i: number, j: number) {
			if (e.deltaY > 0) this.pids[i][j]--;
			else this.pids[i][j]++;
			if (this.pids[i][j] < 0) this.pids[i][j] = 0;
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
			for (let ax = 0; ax < 3; ax++)
				for (let i = 0; i < 5; i++)
					data.push(this.pids[ax][i] & 0xff, (this.pids[ax][i] >> 8) & 0xff);

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
						if (command.length < 3 * 2 * 5) break;
						for (let ax = 0; ax < 3; ax++) {
							for (let i = 0; i < 5; i++)
								this.pids[ax][i] = leBytesToInt(
									command.data.slice(ax * 10 + i * 2, ax * 10 + i * 2 + 2),
									false
								);
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
					case MspFn.RC:
						if (command.length < 8) break;
						for (let i = 0; i < 4; i++) {
							this.rc[i] = (leBytesToInt(command.data.slice(i * 2, i * 2 + 2), false) - 1500) / 512;
						}
						this.drawSetpointCanvas();
						break;
				}
			}
		},
		onResize() {
			const domCanvas = this.$refs.rateCanvas as HTMLCanvasElement;
			const wrapper = domCanvas.parentElement as HTMLDivElement;
			if (!domCanvas || !wrapper) return;
			const rateCanvas = this.rateCanvas
			const setpointCanvas = this.setpointCanvas
			const height = Math.min(wrapper.clientWidth * 3 / 4, 500);
			const width = wrapper.clientWidth;
			domCanvas.width = width;
			domCanvas.height = height;
			rateCanvas.width = width;
			rateCanvas.height = height;
			setpointCanvas.width = width;
			setpointCanvas.height = height;
			this.drawAll();
		},
		drawAll() {
			this.drawRateCanvas();
			this.drawSetpointCanvas();
		},
		getSetpoint(s: number, ax: number): number {
			if (s > 1) s = 1;
			if (s < -1) s = -1;
			const sPos = s > 0 ? s : -s;
			let setpoint = 0;
			for (let i = 0; i < 5; i++) {
				const v = this.rateFactors[ax][i] * Math.pow(sPos, i + 1);
				if (s < 0) setpoint -= v;
				else setpoint += v;
			}
			return setpoint
		},
		drawRateCanvas() {
			const canvas = this.rateCanvas
			const ctx = canvas.getContext('2d');
			if (!ctx) return;
			ctx.fillStyle = '#333';
			ctx.fillRect(0, 0, canvas.width, canvas.height);
			ctx.strokeStyle = 'white';
			ctx.lineWidth = 2;
			ctx.beginPath();
			ctx.moveTo(0, canvas.height / 2);
			ctx.lineTo(canvas.width, canvas.height / 2);
			ctx.stroke()
			ctx.beginPath();
			ctx.moveTo(canvas.width / 2, 0);
			ctx.lineTo(canvas.width / 2, canvas.height);
			ctx.stroke()
			ctx.lineWidth = .5;
			ctx.beginPath();
			ctx.moveTo(canvas.width / 4, 0);
			ctx.lineTo(canvas.width / 4, canvas.height);
			ctx.stroke()
			ctx.beginPath();
			ctx.moveTo(canvas.width * 3 / 4, 0);
			ctx.lineTo(canvas.width * 3 / 4, canvas.height);
			ctx.stroke()

			// set scale based on max rate
			let max = 0;
			for (let i = 0; i < 3; i++) {
				let ax = 0;
				for (let j = 0; j < 5; j++) {
					ax += this.rateFactors[i][j]
				}
				if (ax > max) max = ax;
			}
			if (max > 500) this.scale = 750;
			if (max > 750) this.scale = 1000;
			if (max > 1000) this.scale = 1500;
			if (max > 1500) this.scale = 2000;

			// draw horizontal lines and text
			let drawLines = 0;
			switch (this.scale) {
				case 750:
				case 1500:
					drawLines = 6
					break;
				case 500:
				case 1000:
					drawLines = 4;
					break;
				case 2000:
					drawLines = 8;
					break;
			}
			ctx.fillStyle = 'white';
			ctx.font = '14px sans-serif';
			ctx.textBaseline = 'middle'
			for (let i = 1; i < drawLines; i++) {
				if (i === drawLines / 2) continue;
				ctx.beginPath();
				ctx.moveTo(0, canvas.height * i / drawLines)
				ctx.lineTo(canvas.width, canvas.height * i / drawLines);
				ctx.stroke()
				const rate = this.scale * (drawLines / 2 - i) / (drawLines / 2);
				ctx.textAlign = i < drawLines / 2 ? 'right' : 'left'
				ctx.fillText(rate.toString() + ' °/s', canvas.width / 2 + (i < drawLines / 2 ? -5 : 5), canvas.height * i / drawLines);
			}
			// draw max rate texts
			ctx.textBaseline = 'top'
			ctx.textAlign = 'right'
			ctx.fillText(this.scale.toString() + ' °/s', canvas.width / 2 - 5, 5);
			ctx.textBaseline = 'bottom'
			ctx.textAlign = 'left'
			ctx.fillText((-this.scale).toString() + ' °/s', canvas.width / 2 + 5, canvas.height - 5);
			ctx.lineWidth = 2;
			const colors = ['#ff0000', '#00ff00', '#0000ff'];

			// draw all 3 rate curves
			for (let i = 0; i < 3; i++) {
				ctx.strokeStyle = colors[i];
				ctx.beginPath();
				ctx.moveTo(0, canvas.height / 2);
				for (let j = -50; j <= 50; j++) {
					const val = this.getSetpoint(j / 50, i);
					const x = canvas.width * (j + 50) / 100;
					const y = canvas.height / 2 - canvas.height * val / (this.scale * 2);
					if (j === -50)
						ctx.moveTo(x, y);
					else
						ctx.lineTo(x, y);
				}
				ctx.stroke()
			}
		},
		drawSetpointCanvas() {
			const canvas = this.setpointCanvas
			const ctx = canvas.getContext('2d');
			if (!ctx) return;
			ctx.clearRect(0, 0, canvas.width, canvas.height);
			//draw all 3 setpoints as dots
			const colors = ['#ff5555', '#55ff55', '#5555ff'];
			const rcMap = [0, 1, 3]
			for (let i = 0; i < 3; i++) {
				ctx.fillStyle = colors[i];
				const setpoint = this.getSetpoint(this.rc[rcMap[i]], i);
				// console.log(setpoint, this.rc[rcMap[i]], i);

				const x = canvas.width * (this.rc[rcMap[i]] + 1) / 2;
				const y = canvas.height / 2 - canvas.height * setpoint / this.scale / 2;
				ctx.beginPath();
				ctx.arc(x, y, 7, 0, Math.PI * 2);
				ctx.fill();
			}

			(this.$refs.rateCanvas as HTMLCanvasElement).getContext('2d')?.drawImage(this.rateCanvas, 0, 0);
			(this.$refs.rateCanvas as HTMLCanvasElement).getContext('2d')?.drawImage(canvas, 0, 0);
		}
	},
	watch: {
		rateFactors: {
			handler() {
				this.drawAll();
			},
			deep: true
		},
	}
})
</script>
<template>

	<div class="wrapper">
		<div class="header">
			<button class="saveBtn" @click="() => { saveSettings() }">Save Settings</button>
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
						<th>Max rate</th>
					</tr>
				</thead>
				<tbody>
					<tr v-for="(ax, i) in rateFactors">
						<td style="text-align:left">{{ ['Roll', 'Pitch', 'Yaw'][i] }}</td>
						<td v-for="(_val, j) in ax">
							<input type="number" v-model="rateFactors[i][j]" @wheel="e => {
								scrollInputRate(e, i, j);
							}" />
						</td>
						<td> {{ ax[0] + ax[1] + ax[2] + ax[3] + ax[4]  + ' °/s'}}</td>
					</tr>
				</tbody>
			</table>
			<div class="rateCanvasWrapper">
				<canvas id="rateCanvas" width="500" height="500" ref="rateCanvas"></canvas>
			</div>
		</div>
		<div class="filters">

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

table {
	table-layout: fixed;
	width: 100%;
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

.rateCanvasWrapper {
	margin-top: 1rem;
}

#rateCanvas {
	width: 100%;
}
</style>
