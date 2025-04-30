<script lang="ts">
import { MspFn } from "@/utils/msp";
import { defineComponent, PropType } from "vue";

export default defineComponent({
	name: "SensorGraph",
	props: {
		history: {
			type: Object as PropType<number[][]>,
			required: true,
		},
		gid: {
			type: String,
			required: true,
		},
		width: {
			type: Number,
			default: 150,
		},
		total: {
			type: Number,
			default: 0,
		},
	},
	data() {
		return {
			traces: [] as {
				color: string, sensorId: number, min: number, max: number, id: number, mspFn: number, unit?: string, rangeFn?: () => { min: number, max: number }
			}[],
			sensorOptions: [
				{
					name: 'Accel X',
					sensorId: 0,
					min: -2,
					max: 2,
					unit: 'g',
					mspFn: MspFn.MSP_RAW_IMU
				},
				{
					name: 'Accel Y',
					sensorId: 1,
					min: -2,
					max: 2,
					unit: 'g',
					mspFn: MspFn.MSP_RAW_IMU
				},
				{
					name: 'Accel Z',
					sensorId: 2,
					min: -2,
					max: 2,
					unit: 'g',
					mspFn: MspFn.MSP_RAW_IMU
				},
				{
					name: 'Gyro X',
					sensorId: 3,
					min: -800,
					max: 800,
					unit: '°/s',
					mspFn: MspFn.MSP_RAW_IMU
				},
				{
					name: 'Gyro Y',
					sensorId: 4,
					min: -800,
					max: 800,
					unit: '°/s',
					mspFn: MspFn.MSP_RAW_IMU
				},
				{
					name: 'Gyro Z',
					sensorId: 5,
					min: -800,
					max: 800,
					unit: '°/s',
					mspFn: MspFn.MSP_RAW_IMU
				},
				{
					name: 'Mag X',
					sensorId: 6,
					min: -600,
					max: 600,
					mspFn: MspFn.MSP_RAW_IMU
				},
				{
					name: 'Mag Y',
					sensorId: 7,
					min: -600,
					max: 600,
					mspFn: MspFn.MSP_RAW_IMU
				},
				{
					name: 'Mag Z',
					sensorId: 8,
					min: -600,
					max: 600,
					mspFn: MspFn.MSP_RAW_IMU
				},
				{
					name: 'Roll angle',
					sensorId: 9,
					min: -180,
					max: 180,
					mspFn: MspFn.GET_ROTATION,
					unit: '°'
				},
				{
					name: 'Pitch angle',
					sensorId: 10,
					min: -180,
					max: 180,
					mspFn: MspFn.GET_ROTATION,
					unit: '°'
				},
				{
					name: 'Yaw angle',
					sensorId: 11,
					min: -180,
					max: 180,
					mspFn: MspFn.GET_ROTATION,
					unit: '°'
				},
				{
					name: 'Heading',
					sensorId: 12,
					min: -180,
					max: 180,
					mspFn: MspFn.GET_ROTATION,
					unit: '°'
				},
				{
					name: 'Altitude',
					sensorId: 13,
					min: -100,
					max: 100,
					mspFn: MspFn.MSP_ALTITUDE,
					unit: 'm',
					rangeFn: this.altRange
				},
				{
					name: 'RC channel 1',
					sensorId: 14,
					min: 1000,
					max: 2000,
					mspFn: MspFn.RC,
					unit: 'µs'
				},
				{
					name: 'RC channel 2',
					sensorId: 15,
					min: 1000,
					max: 2000,
					mspFn: MspFn.RC,
					unit: 'µs'
				},
				{
					name: 'RC channel 3',
					sensorId: 16,
					min: 1000,
					max: 2000,
					mspFn: MspFn.RC,
					unit: 'µs'
				},
				{
					name: 'RC channel 4',
					sensorId: 17,
					min: 1000,
					max: 2000,
					mspFn: MspFn.RC,
					unit: 'µs'
				},
				{
					name: 'RC channel 5',
					sensorId: 18,
					min: 1000,
					max: 2000,
					mspFn: MspFn.RC,
					unit: 'µs'
				},
				{
					name: 'RC channel 6',
					sensorId: 19,
					min: 1000,
					max: 2000,
					mspFn: MspFn.RC,
					unit: 'µs'
				},
				{
					name: 'RC channel 7',
					sensorId: 20,
					min: 1000,
					max: 2000,
					mspFn: MspFn.RC,
					unit: 'µs'
				},
				{
					name: 'RC channel 8',
					sensorId: 21,
					min: 1000,
					max: 2000,
					mspFn: MspFn.RC,
					unit: 'µs'
				},
				{
					name: 'Debug 1',
					sensorId: 22,
					min: -100,
					max: 100,
					mspFn: MspFn.DEBUG_SENSORS,
				},
				{
					name: 'Debug 2',
					sensorId: 23,
					min: -1000,
					max: 1000,
					mspFn: MspFn.DEBUG_SENSORS,
				},
				{
					name: 'Debug 3',
					sensorId: 24,
					min: -10000,
					max: 10000,
					mspFn: MspFn.DEBUG_SENSORS,
				},
				{
					name: 'Debug 4',
					sensorId: 25,
					min: -256,
					max: 256,
					mspFn: MspFn.DEBUG_SENSORS,
				},
			] as {
				name: string, sensorId: number, min: number, max: number, mspFn: number, unit?: string
			}[],
			maximized: false,
			lowOpacity: false,
		};
	},
	methods: {
		onResize() {
			const canvas = this.$refs['sensorGraph' + this.gid] as HTMLCanvasElement;
			const wrapper = this.$refs['wrapper' + this.gid] as HTMLDivElement;
			if (!canvas || !wrapper) return;
			canvas.width = wrapper.clientWidth;
			canvas.height = wrapper.clientHeight;
		},
		selectColor(trace: { color: string, sensorId: number, min: number, max: number, unit?: string }) {
			// open color selector from browser
			const picker = document.createElement('input');
			picker.type = 'color';
			picker.value = trace.color;
			let timeout = -1
			let color = trace.color;
			picker.addEventListener('input', (event) => {
				color = (event.target as HTMLInputElement).value;
				if (timeout !== -1) clearTimeout(timeout);
				timeout = setTimeout(() => {
					trace.color = color;
				}, 100);
			});
			picker.click();
		},
		onChange(index: number) {
			const select = document.getElementById('selectSensor' + this.gid + '_' + index) as HTMLSelectElement;
			if (!select) return;
			const sensorId = parseInt(select.value);
			this.traces[index].sensorId = sensorId;
			this.traces[index].min = this.sensorOptions[sensorId].min;
			this.traces[index].max = this.sensorOptions[sensorId].max;
			this.traces[index].mspFn = this.sensorOptions[sensorId].mspFn;
			this.traces[index].unit = this.sensorOptions[sensorId].unit;
		},
		addTrace() {
			const h = Math.random() * 360;
			const s = Math.random() * 0.5 + 0.5;
			const l = Math.random() * 0.5 + 0.3; // 0.3 - 0.8
			let color = '';
			const colors = [
				'red',
				'green',
				'blue',
				'yellow',
				'purple',
				'orange',
				'pink',
				'cyan',
				'magenta',
			];
			color = colors[this.traces.length] || `hsl(${h}, ${s * 100}%, ${l * 100}%)`


			this.traces.push({
				color,
				sensorId: 0,
				min: this.sensorOptions[0].min,
				max: this.sensorOptions[0].max,
				unit: this.sensorOptions[0].unit,
				mspFn: this.sensorOptions[0].mspFn,
				id: Math.random()
			});
		},
		delTrace(index: number) {
			this.traces.splice(index, 1);
		},
		maximize() {
			this.maximized = !this.maximized;
			this.$nextTick().then(() => {
				this.onResize();
			});
		},
		delGraph() {
			this.$emit('delete', this.gid);
		},
		altRange(): { min: number, max: number } {
			const window: number[] = []
			for (let i = this.history.length - this.width; i < this.history.length; i++) {
				window.push(this.history[i][13]);
			}
			const min = Math.min(...window);
			const max = Math.max(...window);
			return {
				min: min,
				max: max,
			};
		}
	},
	watch: {
		history: {
			handler(newHistory) {
				const canvas = this.$refs['sensorGraph' + this.gid] as HTMLCanvasElement;
				const ctx = canvas.getContext('2d');
				if (!ctx || !this.traces.length) return;
				ctx.fillStyle = 'black';
				ctx.fillRect(0, 0, canvas.width, canvas.height);
				// draw grid, one line every 50 frames, using total%50 as the offset
				ctx.strokeStyle = 'rgba(255, 255, 255,0.8)';
				ctx.lineWidth = 1;
				for (let i = this.total % 50; i < this.width; i += 50) {
					ctx.beginPath();
					const x = Math.round(canvas.width * (this.width - i) / (this.width - 1));
					ctx.moveTo(x, 0);
					ctx.lineTo(x, canvas.height);
					ctx.stroke();
				}
				// horizontal center lines
				ctx.strokeStyle = 'rgba(255, 255, 255, 0.5)';
				for (let i = 1; i < 4; i++) {
					ctx.beginPath();
					const y = Math.round(canvas.height * i / 4);
					ctx.moveTo(0, y);
					ctx.lineTo(canvas.width, y);
					ctx.stroke();
				}
				// draw the traces
				for (const trace of this.traces) {
					ctx.beginPath();
					ctx.strokeStyle = trace.color;
					ctx.lineWidth = 2;
					const min = trace.rangeFn ? trace.rangeFn().min : trace.min;
					const max = trace.rangeFn ? trace.rangeFn().max : trace.max;
					trace.min = min;
					trace.max = max;
					for (let i = newHistory.length - this.width; i < newHistory.length; i++) {
						const x = canvas.width * (i - (newHistory.length - this.width)) / (this.width - 1);
						let value = newHistory[i][trace.sensorId];
						value = Math.max(min, Math.min(max, value));
						const y = canvas.height * (1 - (value - min) / (max - min));
						ctx.lineTo(x, y);
					}
					ctx.stroke();
				}
				// draw limits
				ctx.font = '14px sans-serif';
				ctx.textAlign = 'right';
				ctx.textBaseline = 'top';
				const topText = this.traces[0].max + ' ' + (this.traces[0].unit || '');
				let needsIndividualTexts = false;
				for (const trace of this.traces) {
					const text = trace.max + ' ' + (trace.unit || '');
					if (text !== topText) {
						needsIndividualTexts = true;
						break;
					}
				}
				if (needsIndividualTexts) {
					let right = 0;
					for (let i = this.traces.length - 1; i >= 0; i--) {
						const text = this.traces[i].max + ' ' + (this.traces[i].unit || '');
						ctx.textBaseline = 'top';
						ctx.fillStyle = this.traces[i].color;
						ctx.fillText(text, canvas.width - 5 - right, 5);
						const bottomText = this.traces[i].min + ' ' + (this.traces[i].unit || '');
						ctx.textBaseline = 'bottom';
						ctx.fillText(bottomText, canvas.width - 5 - right, canvas.height - 5);
						right += ctx.measureText(text).width + 15;
					}
				} else {
					ctx.fillStyle = 'white';
					ctx.fillText(topText, canvas.width - 5, 5);
					const bottomText = this.traces[0].min + ' ' + (this.traces[0].unit || '');
					ctx.textBaseline = 'bottom';
					ctx.fillText(bottomText, canvas.width - 5, canvas.height - 5);
				}
			},
		},
		traces: {
			handler(newTraces: {
				color: string, sensorId: number, min: number, max: number, id: number, mspFn: number, unit?: string
			}[]) {
				this.$emit('mspFn', newTraces.map(trace => trace.mspFn));
			},
			deep: true,
		}
	},
	mounted() {
		switch (this.gid) {
			case 'gyro':
				this.traces = [
					{ ...this.sensorOptions[3], color: 'red', id: Math.random() },
					{ ...this.sensorOptions[4], color: 'green', id: Math.random() },
					{ ...this.sensorOptions[5], color: 'blue', id: Math.random() }
				];
				break;
			case 'accel':
				this.traces = [
					{ ...this.sensorOptions[0], color: 'red', id: Math.random() },
					{ ...this.sensorOptions[1], color: 'green', id: Math.random() },
					{ ...this.sensorOptions[2], color: 'blue', id: Math.random() }
				];
				break;
			case 'mag':
				this.traces = [
					{ ...this.sensorOptions[6], color: 'red', id: Math.random() },
					{ ...this.sensorOptions[7], color: 'green', id: Math.random() },
					{ ...this.sensorOptions[8], color: 'blue', id: Math.random() }
				];
				break;
			case 'atti':
				this.traces = [
					{ ...this.sensorOptions[9], color: 'red', id: Math.random() },
					{ ...this.sensorOptions[10], color: 'green', id: Math.random() },
					{ ...this.sensorOptions[11], color: 'blue', id: Math.random() }
				];
				break;
			case 'baro':
				this.traces = [
					{ ...this.sensorOptions[13], color: 'red', id: Math.random() }
				];
				break;
			case 'rc':
				this.traces = [
					{ ...this.sensorOptions[14], color: 'red', id: Math.random() },
					{ ...this.sensorOptions[15], color: 'green', id: Math.random() },
					{ ...this.sensorOptions[16], color: 'blue', id: Math.random() },
					{ ...this.sensorOptions[17], color: 'yellow', id: Math.random() },
				];
				break;
			case 'debug':
				this.traces = [
					{ ...this.sensorOptions[22], color: 'red', id: Math.random() },
					{ ...this.sensorOptions[23], color: 'green', id: Math.random() },
					{ ...this.sensorOptions[24], color: 'blue', id: Math.random() },
					{ ...this.sensorOptions[25], color: 'yellow', id: Math.random() },
				];
				break;
		}
		window.addEventListener('resize', this.onResize);
		this.onResize();
	},
	beforeUnmount() {
		window.removeEventListener('resize', this.onResize);
	},
})
</script>
<template>
	<div class="graph" :class="{ lowOpacity, maximized }">
		<div :ref="'wrapper' + gid" class="wrapper">
			<canvas :ref="'sensorGraph' + gid" class="canvas"></canvas>
		</div>
		<div class="setup">
			<div class="trace" v-for="(trace, index) in traces" :key="trace.id">
				<div class="color" :style="{ backgroundColor: trace.color }" @click="selectColor(trace)"></div>
				<select :name="'selectSensor' + gid + '_' + index" :id="'selectSensor' + gid + '_' + index"
					v-model="trace.sensorId" class="select" @change="onChange(index)">
					<option v-for="(s, i) in sensorOptions" :key="i" :value="i">
						{{ s.name }}
					</option>
				</select>
				<button class="del" @click="delTrace(index)"><i class="fa-solid fa-trash"></i></button>
			</div>
			<div class="buttons">
				<button @click="addTrace" class="graphButton"><i class="fa-solid fa-plus"></i></button>
				<button @click="maximize" class="graphButton"><i class="fa-solid fa-arrows-left-right"></i></button>
				<button @click="delGraph" class="graphButton" @mouseenter="() => { lowOpacity = true }"
					@mouseleave="() => { lowOpacity = false }"><i class="fa-solid fa-trash"></i></button>
			</div>
		</div>
	</div>
</template>

<style scoped>
.lowOpacity {
	opacity: 0.5;
}

.maximized {
	grid-column: span 2;
}

.graph {
	display: flex;
	flex-direction: row;
	justify-content: center;
	align-items: center;
	width: 100%;
	gap: 1rem;
	height: 12rem;
	transition: opacity 0.3s ease;
}

.canvas {
	height: 100%;
	width: 100%;
}

.wrapper {
	flex-grow: 1;
	width: 100%;
	height: 100%;
	position: relative;
	overflow: hidden;
}

.setup {
	height: 12rem;
	width: 12rem;
	overflow-y: auto;
}

.trace {
	display: flex;
	align-items: center;
	justify-content: center;
	width: 100%;
	height: 2rem;
}

.color {
	width: 20px;
	height: 20px;
	margin-right: 0.5rem;
}

.select,
option {
	color: black;
}

button.graphButton {
	padding: 0.5rem 0.5rem;
	margin: .5rem;
	background-color: var(--accent-blue);
	color: white;
	border: none;
	border-radius: 4px;
	cursor: pointer;
	display: inline-block;
}

button.del {
	margin-left: 0.5rem;
	padding: 0.3rem;
	background-color: var(--accent-blue);
	color: white;
	border: none;
	border-radius: 4px;
	cursor: pointer;
}
</style>
