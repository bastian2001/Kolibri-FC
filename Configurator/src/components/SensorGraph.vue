<script lang="ts">
import { MspFn } from "@/msp/protocol";
import { defineComponent, PropType } from "vue";

type SensorTrace = {
	color: string;
	sensorId: number;
	id: number;
	mspFn: number;
};

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
			traces: [] as SensorTrace[],
			sensorOptions: [
				{ name: 'Accel X', sensorId: 0, mspFn: MspFn.MSP_RAW_IMU },
				{ name: 'Accel Y', sensorId: 1, mspFn: MspFn.MSP_RAW_IMU },
				{ name: 'Accel Z', sensorId: 2, mspFn: MspFn.MSP_RAW_IMU },
				{ name: 'Gyro X', sensorId: 3, mspFn: MspFn.MSP_RAW_IMU },
				{ name: 'Gyro Y', sensorId: 4, mspFn: MspFn.MSP_RAW_IMU },
				{ name: 'Gyro Z', sensorId: 5, mspFn: MspFn.MSP_RAW_IMU },
				{ name: 'Mag X', sensorId: 6, mspFn: MspFn.MSP_RAW_IMU },
				{ name: 'Mag Y', sensorId: 7, mspFn: MspFn.MSP_RAW_IMU },
				{ name: 'Mag Z', sensorId: 8, mspFn: MspFn.MSP_RAW_IMU },
				{ name: 'Roll angle', sensorId: 9, mspFn: MspFn.GET_ROTATION, },
				{ name: 'Pitch angle', sensorId: 10, mspFn: MspFn.GET_ROTATION, },
				{ name: 'Yaw angle', sensorId: 11, mspFn: MspFn.GET_ROTATION, },
				{ name: 'Heading', sensorId: 12, mspFn: MspFn.GET_ROTATION, },
				{ name: 'Altitude', sensorId: 13, mspFn: MspFn.MSP_ALTITUDE, },
				{ name: 'RC channel 1', sensorId: 14, mspFn: MspFn.RC, },
				{ name: 'RC channel 2', sensorId: 15, mspFn: MspFn.RC, },
				{ name: 'RC channel 3', sensorId: 16, mspFn: MspFn.RC, },
				{ name: 'RC channel 4', sensorId: 17, mspFn: MspFn.RC, },
				{ name: 'RC channel 5', sensorId: 18, mspFn: MspFn.RC, },
				{ name: 'RC channel 6', sensorId: 19, mspFn: MspFn.RC, },
				{ name: 'RC channel 7', sensorId: 20, mspFn: MspFn.RC, },
				{ name: 'RC channel 8', sensorId: 21, mspFn: MspFn.RC, },
				{ name: 'Debug 1', sensorId: 22, mspFn: MspFn.DEBUG_SENSORS, },
				{ name: 'Debug 2', sensorId: 23, mspFn: MspFn.DEBUG_SENSORS, },
				{ name: 'Debug 3', sensorId: 24, mspFn: MspFn.DEBUG_SENSORS, },
				{ name: 'Debug 4', sensorId: 25, mspFn: MspFn.DEBUG_SENSORS, },
			],
			maximized: false,
			lowOpacity: false,
			canvasWorker: new Worker(new URL('@/utils/worker/sensorCanvas.ts', import.meta.url), {
				type: 'classic',
			}),
		};
	},
	methods: {
		onResize() {
			const wrapper = this.$refs['wrapper' + this.gid] as HTMLDivElement;
			if (!wrapper) return;
			this.canvasWorker.postMessage({
				type: 'resize',
				width: wrapper.clientWidth,
				height: wrapper.clientHeight,
			});
		},
		selectColor(trace: SensorTrace, index: number) {
			// open color selector from browser
			const picker = document.createElement('input');
			picker.type = 'color';
			picker.value = trace.color;
			let timeout = -1
			let color = trace.color;
			picker.addEventListener('input', (event) => {
				color = (event.target as HTMLInputElement).value;
				if (timeout !== -1) clearTimeout(timeout);
				trace.color = color;
				this.canvasWorker.postMessage({
					type: 'color',
					index,
					color,
				});
			});
			picker.click();
		},
		traceChanged(index: number) {
			const select = document.getElementById('selectSensor' + this.gid + '_' + index) as HTMLSelectElement;
			if (!select) return;
			const sensorId = parseInt(select.value);
			this.traces[index].sensorId = sensorId;
			this.traces[index].mspFn = this.sensorOptions[sensorId].mspFn;
			this.canvasWorker.postMessage({
				type: 'sensorId',
				index,
				sensorId,
			});
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
				mspFn: this.sensorOptions[0].mspFn,
				id: Math.random()
			});
			this.canvasWorker.postMessage({
				type: 'new',
				color,
				sensorId: 0,
			});
		},
		delTrace(index: number) {
			this.traces.splice(index, 1);
			this.canvasWorker.postMessage({
				type: 'delete',
				index,
			});
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
	},
	watch: {
		history: {
			handler(newHistory) {
				if (!this.canvasWorker) return;
				const data = JSON.parse(JSON.stringify(newHistory[newHistory.length - 1]))

				this.canvasWorker.postMessage({
					type: 'data',
					data,
					offset: this.total,
				});
			},
		},
		traces: {
			handler(newTraces: SensorTrace[]) {
				this.$emit('mspFn', newTraces.map(trace => trace.mspFn));
			},
			deep: true,
		},
		width: {
			handler(newWidth) {
				this.canvasWorker.postMessage({
					type: 'window',
					window: newWidth,
				});
			},
		},
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

		const offscreen = (this.$refs['sensorGraph' + this.gid] as HTMLCanvasElement).transferControlToOffscreen(); const wrapper = this.$refs['wrapper' + this.gid] as HTMLDivElement;
		if (!wrapper) return;
		const width = wrapper.clientWidth;
		const height = wrapper.clientHeight;
		this.canvasWorker.postMessage({
			type: 'init',
			canvas: offscreen,
			width,
			height,
			traces: JSON.parse(JSON.stringify(this.traces)),
			window: this.width,
		}, [offscreen]);

		window.addEventListener('resize', this.onResize);
		this.onResize();
	},
	beforeUnmount() {
		window.removeEventListener('resize', this.onResize);
		this.canvasWorker.terminate();
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
				<div class="color" :style="{ backgroundColor: trace.color }" @click="selectColor(trace, index)"></div>
				<select :id="'selectSensor' + gid + '_' + index" v-model="trace.sensorId" class="select"
					@change="traceChanged(index)">
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
