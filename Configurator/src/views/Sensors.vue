<script lang="ts">
import { defineComponent } from "vue";
import { sendCommand, addOnCommandHandler, removeOnCommandHandler } from "@/communication/serial";
import { MspFn } from "@utils/msp";
import { Command } from "@utils/types";
import { leBytesToInt } from "@utils/utils";
import SensorGraph from "@/components/SensorGraph.vue";

export default defineComponent({
	name: "Sensors",
	data() {
		return {
			fetchInterval: -1,
			history: new Array(600).fill(0).map(() => new Array(9).fill(0)),
			total: 0,
			width: 300,
			frequency: 50,
			graphs: ['accel', 'gyro', 'mag'],
		};
	},
	components: {
		SensorGraph
	},
	mounted() {
		this.fetchInterval = setInterval(() => {
			sendCommand("request", MspFn.MSP_RAW_IMU)
		}, 20);
		addOnCommandHandler(this.onCommand);
	},
	unmounted() {
		clearInterval(this.fetchInterval);
		removeOnCommandHandler(this.onCommand);
	},
	methods: {
		onCommand(command: Command) {
			if (command.cmdType === 'response') {
				switch (command.command) {
					case MspFn.MSP_RAW_IMU: {
						const data = command.data;
						if (data.length < 18) break;
						this.total++;
						const values = new Array(9).fill(0)
						// accel comes in raw 16 bit signed
						for (let i = 0; i < 3; i++) {
							values[i] = leBytesToInt(data.slice(i * 2, i * 2 + 2), true) / 2048;
						}
						// gyro comes in as deg/s already
						for (let i = 3; i < 6; i++) {
							values[i] = leBytesToInt(data.slice(i * 2, i * 2 + 2), true);
						}
						// mag comes in raw 16 bit signed
						for (let i = 6; i < 9; i++) {
							values[i] = leBytesToInt(data.slice(i * 2, i * 2 + 2), true);
						}
						this.history.shift();
						this.history.push(values);
						this.history = [...this.history];
						break;
					}
					default:
						break;
				}
			}
		},
		addGraph() {
			if (this.graphs.length < 3) {
				this.graphs.push(this.graphs.length === 0 ? 'accel' : this.graphs.length === 1 ? 'gyro' : 'mag');
			} else {
				this.graphs.push(Math.random().toString())
			}
		},
	},
	watch: {
		frequency: {
			handler(newVal) {
				if (this.fetchInterval !== -1) {
					clearInterval(this.fetchInterval);
					this.fetchInterval = setInterval(() => {
						sendCommand("request", MspFn.MSP_RAW_IMU)
					}, 1000 / newVal);
				}
			},
			immediate: true,
		},
	},
})
</script>
<template>
	<div class="wrapper">
		<p>Zoom: <input type="range" v-model="width" min="100" max="600" step="25"> {{ width }} frames</p>
		<p>Frequency: <input type="range" v-model="frequency" min="10" max="60"> {{ frequency }} Hz</p>
		<div class="all">
			<SensorGraph v-for="(graph, index) in graphs" :key="graph" :gid="graph" :history :width :total @delete="(_e) => {
				graphs.splice(index, 1);
			}">
			</SensorGraph>
			<div class="buttonWrapper">
				<button @click="addGraph">Add</button>
			</div>
		</div>
	</div>
</template>

<style scoped>
.wrapper {
	width: 100%;
	padding: 2rem;
	box-sizing: border-box;
}

p {
	display: inline-block;
	margin: .5rem 1rem;
}

button {
	margin: 1rem;
	padding: 1rem 2rem;
	background-color: #007BFF;
	color: white;
	border: none;
	border-radius: 4px;
	cursor: pointer;
	display: inline;
}

.all {
	display: grid;
	grid-template-columns: 1fr 1fr;
	gap: 1.5rem;
}

.buttonWrapper {
	display: flex;
	justify-content: center;
	align-items: center;
	width: 100%;
	height: 100%;
}
</style>
