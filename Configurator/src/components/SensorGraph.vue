<script lang="ts">
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
				color: string, sensorId: number, min: number, max: number, id: number, unit?: string
			}[],
			sensorOptions: [
				{
					name: 'Accel X',
					sensorId: 0,
					min: -2,
					max: 2,
					unit: 'g',
				},
				{
					name: 'Accel Y',
					sensorId: 1,
					min: -2,
					max: 2,
					unit: 'g',
				},
				{
					name: 'Accel Z',
					sensorId: 2,
					min: -2,
					max: 2,
					unit: 'g',
				},
				{
					name: 'Gyro X',
					sensorId: 3,
					min: -800,
					max: 800,
					unit: '°/s',
				},
				{
					name: 'Gyro Y',
					sensorId: 4,
					min: -800,
					max: 800,
					unit: '°/s',
				},
				{
					name: 'Gyro Z',
					sensorId: 5,
					min: -800,
					max: 800,
					unit: '°/s',
				},
				{
					name: 'Mag X',
					sensorId: 6,
					min: -600,
					max: 600,
				},
				{
					name: 'Mag Y',
					sensorId: 7,
					min: -600,
					max: 600,
				},
				{
					name: 'Mag Z',
					sensorId: 8,
					min: -600,
					max: 600,
				},
			],
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
			console.log('selectColor', trace);
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
			this.traces[index].unit = this.sensorOptions[sensorId].unit;
		},
		addTrace() {
			const h = Math.random() * 360;
			const s = Math.random() * 0.5 + 0.5;
			const l = Math.random() * 0.5 + 0.3; // 0.3 - 0.8
			let color = '';
			switch (this.traces.length) {
				case 0:
					color = 'red'
					break;
				case 1:
					color = 'green'
					break;
				case 2:
					color = 'blue'
					break;
				default:
					color = `hsl(${h}, ${s * 100}%, ${l * 100}%)`;
			}

			this.traces.push({
				color,
				sensorId: 0,
				min: this.sensorOptions[0].min,
				max: this.sensorOptions[0].max,
				unit: this.sensorOptions[0].unit,
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
					for (let i = newHistory.length - this.width; i < newHistory.length; i++) {
						const x = canvas.width * (i - (newHistory.length - this.width)) / (this.width - 1);
						let value = newHistory[i][trace.sensorId];
						value = Math.max(trace.min, Math.min(trace.max, value));
						const y = canvas.height * (1 - (value - trace.min) / (trace.max - trace.min));
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
