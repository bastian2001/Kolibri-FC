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
	'        - ELRS Message',
	'    - Modes',
	'    - ADC',
	'    - Serial',
	'    - Configurator',
	'    - GPS',
	'        - GPS Message',
	'    - Magnetometer',
	'        - Mag Check',
	'        - Mag Read',
	'        - Mag Eval',
	'    - OSD',
	'    - Task Manager',
	'Loop 1',
	'    - Gyro Read',
	'    - IMU',
	'        - IMU Gyro',
	'        - IMU Accel 1',
	'        - IMU Accel 2',
	'        - IMU Angle',
	'        - IMU Speeds',
	'    - Control',
	'        - Control 1',
	'        - Control 2',
	'        - Control 3',
	'        - Control 4',
	'        - Control 5',
	'        - Control 6',
	'        - Control 7',
	'        - Control 8',
	'    - ESC RPM',
	'    - PID',
	'    - Blackbox compute',
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
				totalDuration: number;
				avgDuration: number
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
				totalDuration: 0,
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
							this.tasks[i].debugInfo = leBytesToInt(command.data.slice(i * 28, i * 28 + 4));
							this.tasks[i].minDuration = leBytesToInt(command.data.slice(i * 28 + 6, i * 28 + 8));
							this.tasks[i].maxDuration = leBytesToInt(command.data.slice(i * 28 + 4, i * 28 + 6));
							this.tasks[i].frequency = leBytesToInt(command.data.slice(i * 28 + 8, i * 28 + 12));
							this.tasks[i].totalDuration = leBytesToInt(command.data.slice(i * 28 + 12, i * 28 + 16));
							this.tasks[i].avgDuration = this.tasks[i].totalDuration / this.tasks[i].frequency;
							this.tasks[i].errorCount = leBytesToInt(command.data.slice(i * 28 + 16, i * 28 + 20));
							this.tasks[i].lastError = leBytesToInt(command.data.slice(i * 28 + 20, i * 28 + 24));
							this.tasks[i].maxGap = leBytesToInt(command.data.slice(i * 28 + 24, i * 28 + 28));
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
				<th>CPU</th>
				<th>Frequency</th>
				<th>Error Count</th>
				<th>Last Error</th>
				<th>Debug Info</th>
				<th>Max Gap</th>
			</tr>
		</thead>
		<tbody>
			<tr v-for="task in tasks" :class="{ grey: task.name.startsWith('     ') }">
				<td style="white-space:pre">{{ task.name }}</td>
				<td>{{ task.minDuration }}</td>
				<td>{{ task.maxDuration }}</td>
				<td>{{ task.avgDuration.toFixed(task.avgDuration < 20 ? 1 : 0) }}</td>
				<td style="text-align: right;">{{ (task.totalDuration / 1000000 * 100).toFixed(1) }} %</td>
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
th {
	position: sticky;
	top: 0;
	background-color: var(--background-color);
	z-index: 1;
}

td,
th {
	text-align: left;
	padding: 3px 20px;
}

.grey {
	opacity: 0.7;
}
</style>
