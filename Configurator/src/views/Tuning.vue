<script lang="ts">
import { defineComponent } from "vue";
import { MspFn, MspVersion } from "@utils/msp";
import { Command } from "@utils/types";
import { delay, intToLeBytes, leBytesToInt } from "@utils/utils";
import { sendCommand, addOnCommandHandler, removeOnCommandHandler, addOnConnectHandler, removeOnConnectHandler } from "@/communication/serial";
import NumericInput from "@/components/NumericInput.vue";

export default defineComponent({
	name: "Tuning",
	components: {
		NumericInput,
	},
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
			gyroCutoff: 0,
			accelCutoff: 0,
			dCutoff: 0,
			setpointDiffCutoff: 0,
			magCutoff: 0,
			gpsVelocityCutoff: 0,
			altholdFfCutoff: 0,
			altholdDCutoff: 0,
			posholdFfCutoff: 0,
			posholdIrelaxCutoff: 0,
			posholdPushCutoff: 0,
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
			sendCommand('request', MspFn.GET_PIDS)
				.then(() => delay(5))
				.then(() => sendCommand('request', MspFn.GET_RATES))
				.then(() => delay(5))
				.then(() => sendCommand('request', MspFn.GET_FILTER_CONFIG))
		},
		saveSettings() {
			const data = [];
			for (let ax = 0; ax < 3; ax++)
				for (let i = 0; i < 5; i++)
					data.push(...intToLeBytes(this.pids[ax][i], 2));

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
								data.push(...intToLeBytes(this.rateFactors[ax][i], 2));
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
					case MspFn.GET_FILTER_CONFIG:
						if (command.length < 22) break;
						this.gyroCutoff = leBytesToInt(command.data.slice(0, 2), false);
						this.accelCutoff = leBytesToInt(command.data.slice(2, 4), false);
						this.dCutoff = leBytesToInt(command.data.slice(4, 6), false);
						this.setpointDiffCutoff = leBytesToInt(command.data.slice(6, 8), false) / 10;
						this.magCutoff = leBytesToInt(command.data.slice(8, 10), false) / 100;
						this.altholdFfCutoff = leBytesToInt(command.data.slice(10, 12), false) / 100;
						this.altholdDCutoff = leBytesToInt(command.data.slice(12, 14), false) / 10;
						this.posholdFfCutoff = leBytesToInt(command.data.slice(14, 16), false) / 100;
						this.posholdIrelaxCutoff = leBytesToInt(command.data.slice(16, 18), false) / 100;
						this.posholdPushCutoff = leBytesToInt(command.data.slice(18, 20), false) / 10;
						this.gpsVelocityCutoff = leBytesToInt(command.data.slice(20, 22), false) / 100;
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
			this.scale = 500;
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
			const colors = ['#f77', '#7f7', '#77f'];

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
			const colors = ['#f99', '#9f9', '#99f'];
			const rcMap = [0, 1, 3]
			for (let i = 0; i < 3; i++) {
				ctx.fillStyle = colors[i];
				const setpoint = this.getSetpoint(this.rc[rcMap[i]], i);
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
			<h2>PID Gains</h2>
			<table cellspacing="0">
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
							<NumericInput v-model="pids[i][j]" :min="0" :max="2000" :step="1" unit=""
								style="width: 100%" />
						</td>
					</tr>
				</tbody>
			</table>
		</div>
		<div class="rates">
			<h2>Rate Factors</h2>
			<table cellspacing="0">
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
							<NumericInput v-model="rateFactors[i][j]" :min="0" :max="1000" :step="5" unit=""
								style="width: 100%" />
						</td>
						<td style="text-align: center;"> {{ ax[0] + ax[1] + ax[2] + ax[3] + ax[4] + ' °/s' }}</td>
					</tr>
				</tbody>
			</table>
			<div class="rateCanvasWrapper">
				<canvas id="rateCanvas" width="500" height="500" ref="rateCanvas"></canvas>
			</div>
		</div>
		<div class="filters">
			<h2>Filters</h2>
			<h3>Flight performance</h3>
			<div class="inputDiv">
				<p class="inputDescription" for="gyroCutoff">Gyro filter cutoff frequency</p>
				<NumericInput v-model="gyroCutoff" :min="50" :max="300" :step="5" unit="Hz" />
			</div>
			<div class="inputDiv">
				<p class="inputDescription" for="accelCutoff">Accelerometer filter cutoff frequency</p>
				<NumericInput v-model="accelCutoff" :min="50" :max="300" :step="5" unit="Hz" />
			</div>
			<div class="inputDiv">
				<p class="inputDescription" for="dCutoff">D-term filter cutoff frequency</p>
				<NumericInput v-model="dCutoff" :min="20" :max="300" :step="5" unit="Hz" />
			</div>
			<div class="inputDiv">
				<p class="inputDescription" for="setpointDiffCutoff">FF + I-term-relax filter cutoff frequency</p>
				<NumericInput v-model="setpointDiffCutoff" :min="5" :max="30" :step=".5" unit="Hz" />
			</div>
			<h3>Advanced flight modes</h3>
			<div class="inputDiv">
				<p class="inputDescription" for="magCutoff">Magnetometer filter cutoff frequency</p>
				<NumericInput v-model="magCutoff" :min="0.05" :max="1" :step=".01" unit="Hz" />
			</div>
			<div class="inputDiv">
				<p class="inputDescription" for="altholdFfCutoff">Altitude FF cutoff frequency</p>
				<NumericInput v-model="altholdFfCutoff" :min="0.5" :max="10" :step=".1" unit="Hz" />
			</div>
			<div class="inputDiv">
				<p class="inputDescription" for="altholdDCutoff">Altitude D cutoff frequency</p>
				<NumericInput v-model="altholdDCutoff" :min="5" :max="50" :step=".5" unit="Hz" />
			</div>
			<div class="inputDiv">
				<p class="inputDescription" for="gpsVelocityCutoff">GPS velocity filter cutoff frequency</p>
				<NumericInput v-model="gpsVelocityCutoff" :min="0.05" :max="1" :step=".01" unit="Hz" />
			</div>
			<div class="inputDiv">
				<p class="inputDescription" for="posholdFfCutoff">Position Hold FF cutoff frequency</p>
				<NumericInput v-model="posholdFfCutoff" :min="0.5" :max="10" :step=".1" unit="Hz" />
			</div>
			<div class="inputDiv">
				<p class="inputDescription" for="posholdIrelaxCutoff">Position Hold I-term-relax cutoff frequency</p>
				<NumericInput v-model="posholdIrelaxCutoff" :min="0.1" :max="5" :step=".1" unit="Hz" />
			</div>
			<div class="inputDiv">
				<p class="inputDescription" for="posholdPushCutoff">Position Hold pushback cutoff frequency</p>
				<NumericInput v-model="posholdPushCutoff" :min="1" :max="15" :step=".25" unit="Hz" />
			</div>
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
	padding: 0px;
	border-spacing: 0px;
	border-collapse: collapse;
}

td {
	border-bottom: 1px solid var(--border-color);
	border-right: 1px solid var(--border-color);
	margin: 0;
}

.inlineInput {
	width: 100px;
}

.inputDescription {
	display: inline-block;
	margin: 0
}

.inputDiv {
	display: flex;
	justify-content: space-between;
	align-items: center;
	border-bottom: 1px solid var(--border-color);
}

.rateCanvasWrapper {
	margin-top: 1rem;
}

#rateCanvas {
	width: 100%;
}
</style>
