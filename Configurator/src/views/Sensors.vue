<script lang="ts">
import { defineComponent } from "vue";
import { sendCommand, addOnCommandHandler, removeOnCommandHandler } from "@/msp/comm";
import { MspFn } from "@/msp/protocol";
import { Command } from "@utils/types";
import { leBytesToInt } from "@utils/utils";
import SensorGraph from "@/components/SensorGraph.vue";

export default defineComponent({
	name: "Sensors",
	data() {
		return {
			fetchInterval: -1,
			history: new Array(600).fill(0).map(() => new Array(26).fill(0)) as number[][],
			values: new Array(26).fill(0) as number[],
			total: 0,
			width: '300',
			frequency: 50,
			graphs: [
				{
					name: 'accel',
					mspFn: [MspFn.MSP_RAW_IMU],
				},
				{
					name: 'gyro',
					mspFn: [MspFn.MSP_RAW_IMU],
				},
			] as {
				name: string;
				mspFn: number[];
				updateFn?: (history: number[][], total: number) => void;
			}[],
			gotTypes: [] as number[],

		};
	},
	components: {
		SensorGraph
	},
	computed: {
		reqSensorMsgs(): number[] {
			const reqSensorMsgs: number[] = [];
			this.graphs.forEach((graph) => {
				graph.mspFn.forEach((mspFn) => {
					if (!reqSensorMsgs.includes(mspFn)) {
						reqSensorMsgs.push(mspFn);
					}
				});
			});
			return reqSensorMsgs;
		},
	},
	mounted() {
		this.fetchInterval = setInterval(this.getData, 20);
		addOnCommandHandler(this.onCommand);
	},
	unmounted() {
		clearInterval(this.fetchInterval);
		removeOnCommandHandler(this.onCommand);
	},
	methods: {
		getData() {
			this.reqSensorMsgs.forEach((mspFn, index) => {
				setTimeout(() => {
					sendCommand(mspFn);
				}, index);
			});
		},
		onCommand(command: Command) {
			if (command.cmdType === 'response') {
				switch (command.command) {
					case MspFn.MSP_RAW_IMU: {
						if (this.gotTypes.includes(MspFn.MSP_RAW_IMU)) this.pushHistory();

						const data = command.data;
						if (data.length < 18) break;
						// accel comes in raw 16 bit signed
						for (let i = 0; i < 3; i++) {
							this.values[i] = leBytesToInt(data, i * 2, 2, true) / 2048;
						}
						// gyro comes in as deg/s already
						for (let i = 3; i < 6; i++) {
							this.values[i] = leBytesToInt(data, i * 2, 2, true);
						}
						// mag comes in raw 16 bit signed
						for (let i = 6; i < 9; i++) {
							this.values[i] = leBytesToInt(data, i * 2, 2, true);
						}

						this.gotTypes.push(MspFn.MSP_RAW_IMU);
						this.checkHistory();
						break;
					}
					case MspFn.GET_ROTATION: {
						if (this.gotTypes.includes(MspFn.GET_ROTATION)) this.pushHistory();

						const data = command.data;
						if (data.length < 8) break;
						let pitch = leBytesToInt(data, 0, 2, true) / 8192 * 180 / Math.PI;
						let roll = leBytesToInt(data, 2, 2, true) / 8192 * 180 / Math.PI;
						let yaw = leBytesToInt(data, 4, 2, true) / 8192 * 180 / Math.PI;
						let heading = leBytesToInt(data, 6, 2, true) / 8192 * 180 / Math.PI;
						this.values[9] = roll;
						this.values[10] = pitch;
						this.values[11] = yaw;
						this.values[12] = heading;

						this.gotTypes.push(MspFn.GET_ROTATION);
						this.checkHistory();
						break;
					}
					case MspFn.MSP_ALTITUDE: {
						if (this.gotTypes.includes(MspFn.MSP_ALTITUDE)) this.pushHistory();

						const data = command.data;
						if (data.length < 4) break;
						this.values[13] = leBytesToInt(data, 0, 4, true) / 100;

						this.gotTypes.push(MspFn.MSP_ALTITUDE);
						this.checkHistory();
						break;
					}
					case MspFn.RC: {
						if (this.gotTypes.includes(MspFn.RC)) this.pushHistory();

						const data = command.data;
						for (let i = 0; i < data.length / 2; i++) {
							this.values[14 + i] = leBytesToInt(data, i * 2, 2, true);
						}

						this.gotTypes.push(MspFn.RC);
						this.checkHistory();
						break;
					}
					case MspFn.DEBUG_SENSORS: {
						if (this.gotTypes.includes(MspFn.DEBUG_SENSORS)) this.pushHistory();

						const data = command.data;
						if (data.length < 8) break;
						this.values[22] = leBytesToInt(data, 0, 2, true);
						this.values[23] = leBytesToInt(data, 2, 2, true);
						this.values[24] = leBytesToInt(data, 4, 2, true);
						this.values[25] = leBytesToInt(data, 6, 2, true);

						this.gotTypes.push(MspFn.DEBUG_SENSORS);
						this.checkHistory();
						break;
					}
				}
			}
		},
		pushHistory() {
			this.history.shift();
			this.history.push(this.values);
			this.values = [...this.values]
			this.gotTypes = [];
			this.total++;
			this.graphs.forEach((graph) => {
				if (graph.updateFn) {
					graph.updateFn(this.history, this.total);
				}
			});
		},
		checkHistory() {
			// check if all reqSensorMsgs are in the gotTypes array, then push the history
			let allGot = true;
			this.reqSensorMsgs.forEach((mspFn) => {
				if (!this.gotTypes.includes(mspFn)) {
					allGot = false;
				}
			});
			if (allGot) {
				this.pushHistory();
			}
		},
		addGraph() {
			if (!this.graphs.map(g => g.name).includes('accel'))
				this.graphs.push({ name: 'accel', mspFn: [] });
			else if (!this.graphs.map(g => g.name).includes('gyro'))
				this.graphs.push({ name: 'gyro', mspFn: [] });
			else if (!this.graphs.map(g => g.name).includes('mag'))
				this.graphs.push({ name: 'mag', mspFn: [] });
			else if (!this.graphs.map(g => g.name).includes('atti'))
				this.graphs.push({ name: 'atti', mspFn: [] });
			else if (!this.graphs.map(g => g.name).includes('baro'))
				this.graphs.push({ name: 'baro', mspFn: [] });
			else if (!this.graphs.map(g => g.name).includes('rc'))
				this.graphs.push({ name: 'rc', mspFn: [] });
			else if (!this.graphs.map(g => g.name).includes('debug'))
				this.graphs.push({ name: 'debug', mspFn: [] });
			else
				this.graphs.push({ name: Math.random().toString(), mspFn: [] });
		},
	},
	watch: {
		frequency: {
			handler(newVal) {
				if (this.fetchInterval !== -1) {
					clearInterval(this.fetchInterval);
					this.fetchInterval = setInterval(this.getData, 1000 / newVal);
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
			<SensorGraph v-for="(graph, index) in graphs" :key="graph.name" :gid="graph.name" :width="parseInt(width)"
				@delete="() => {
					graphs.splice(index, 1);
				}" @mspFn="(mspFn: number[]) => {
					graph.mspFn = mspFn;
				}" @updateFn="(updateFn: (history: number[][], total: number) => void) => {
					graph.updateFn = updateFn;
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
