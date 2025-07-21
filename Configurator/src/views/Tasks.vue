<script lang="ts">
import { defineComponent } from "vue";
import { sendCommand, addOnCommandHandler, removeOnCommandHandler } from "@/msp/comm";
import { MspFn } from "@/msp/protocol";
import { Command } from "@utils/types";
import { leBytesToInt } from "@utils/utils";

const TASK_NAMES = [
	'Loop 0',
	'    - Speaker',
	'    - Baro',
	'        - Baro Check',
	'        - Baro Read',
	'        - Baro Eval',
	'    - Blackbox save',
	'    - ELRS',
	'    - Modes',
	'    - ADC',
	'    - Serial',
	'    - Configurator',
	'    - GPS',
	'    - Magnetometer',
	'    - OSD',
	'    - Task Manager',
	'Loop 1',
	'    - Gyro Read',
	'    - IMU',
	'        - IMU Gyro',
	'        - IMU Accel',
	'        - IMU Angle',
	'        - IMU Speeds',
	'    - ESC RPM',
	'    - PID, Blackbox compute',
];

export default defineComponent({
	name: "Tasks",
	data() {
		return {
			interval: -1,
			tasks: [] as {
				name: string;
				maxDuration: number;
				minDuration: number;
				avgDuration: number;
				frequency: number;
				errorCount: number;
				lastError: number;
				debugInfo: number;
				maxGap: number;
			}[]
		};
	},
	mounted() {
		for (let i = 0; i < TASK_NAMES.length; i++) {
			this.tasks.push({
				name: TASK_NAMES[i],
				maxDuration: 0,
				minDuration: 0,
				avgDuration: 0,
				frequency: 0,
				errorCount: 0,
				lastError: 0,
				debugInfo: 0,
				maxGap: 0
			});
		}
		this.interval = setInterval(() => {
			sendCommand(MspFn.TASK_STATUS);
		}, 200);
		addOnCommandHandler(this.onCommand);
	},
	unmounted() {
		clearInterval(this.interval);
		removeOnCommandHandler(this.onCommand);
	},
	methods: {
		onCommand(command: Command) {
			if (command.cmdType === 'response') {
				switch (command.command) {
					case MspFn.TASK_STATUS:
						for (let i = 0; i < this.tasks.length; i++) {
							this.tasks[i].debugInfo = leBytesToInt(command.data.slice(i * 32, i * 32 + 4));
							this.tasks[i].minDuration = leBytesToInt(command.data.slice(i * 32 + 4, i * 32 + 8));
							this.tasks[i].maxDuration = leBytesToInt(command.data.slice(i * 32 + 8, i * 32 + 12));
							this.tasks[i].frequency = leBytesToInt(command.data.slice(i * 32 + 12, i * 32 + 16));
							this.tasks[i].avgDuration = leBytesToInt(command.data.slice(i * 32 + 16, i * 32 + 20));
							this.tasks[i].errorCount = leBytesToInt(command.data.slice(i * 32 + 20, i * 32 + 24));
							this.tasks[i].lastError = leBytesToInt(command.data.slice(i * 32 + 24, i * 32 + 28));
							this.tasks[i].maxGap = leBytesToInt(command.data.slice(i * 32 + 28, i * 32 + 32));
						}
						break;
				}
			}
		}
	}
})
</script>
<template>

	<table>
		<thead>
			<tr>
				<th>Task</th>
				<th>Min Duration</th>
				<th>Max Duration</th>
				<th>Avg Duration</th>
				<th>Frequency</th>
				<th>Error Count</th>
				<th>Last Error</th>
				<th>Debug Info</th>
				<th>Max Gap</th>
			</tr>
		</thead>
		<tbody>
			<tr v-for="task in tasks">
				<td style="white-space:pre">{{ task.name }}</td>
				<td>{{ task.minDuration }}</td>
				<td>{{ task.maxDuration }}</td>
				<td>{{ task.avgDuration }}</td>
				<td>{{ task.frequency }}</td>
				<td>{{ task.errorCount }}</td>
				<td>{{ task.lastError }}</td>
				<td>{{ task.debugInfo }}</td>
				<td>{{ task.maxGap }}</td>
			</tr>
		</tbody>
	</table>
</template>

<style scoped>
td,
th {
	text-align: left;
	padding: 3px 20px;
}
</style>
