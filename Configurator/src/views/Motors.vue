<!--
 + Copyright (c) 2026 Kolibri-FC contributors
 + 
 + This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 + 
 + Kolibri-FC is free software: you can redistribute it and/or modify
 + it under the terms of the GNU General Public License as published by
 + the Free Software Foundation, either version 3 of the License, or
 + (at your option) any later version.
 + 
 + Kolibri-FC is distributed in the hope that it will be useful,
 + but WITHOUT ANY WARRANTY; without even the implied warranty of
 + MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 + GNU General Public License for more details.
 + 
 + You should have received a copy of the GNU General Public License
 + along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
-->

<script lang="ts">
import Motor from "@components/Motor.vue";
import { defineComponent } from "vue";
import { MspFn } from "@/msp/protocol";
import { intToLeBytes, leBytesToInt } from "@utils/utils";
import { useLogStore } from "@stores/logStore";
import { sendCommand, onDisconnectHandler } from "@/msp/comm";
import RemapMotors from "@/components/RemapMotors.vue";

export default defineComponent({
	name: "Motors",
	components: { Motor, RemapMotors },
	data() {
		return {
			motors: [1000, 1000, 1000, 1000], // current values
			throttles: [1000, 1000, 1000, 1000], // commands
			motorMapping: [3, 1, 2, 0],
			sendInterval: -1,
			getMotorsInterval: -1,
			configuratorLog: useLogStore(),
			motorRemap: false,
		};
	},
	mounted() {
		this.getMotorsInterval = setInterval(() => {
			sendCommand(MspFn.GET_MOTOR).then(c => {
				for (let i = 0; i < 4; i++)
					this.motors[i] = leBytesToInt(c.data, i * 2, 2)
			}).catch(() => { })
		}, 100);
		onDisconnectHandler(this.stopMotors);
	},
	unmounted() {
		clearInterval(this.getMotorsInterval);
		clearInterval(this.sendInterval);
	},
	methods: {
		spinMotor(motor: number) {
			this.throttles = [1000, 1000, 1000, 1000];
			this.throttles[motor] = 1075;
		},
		stopMotors() {
			this.throttles = [1000, 1000, 1000, 1000];
			setTimeout(() => {
				clearInterval(this.sendInterval);
			}, 200);
		},
		startMotors() {
			clearInterval(this.sendInterval);
			this.sendInterval = setInterval(() => {
				sendCommand(MspFn.SET_MOTOR, this.throttlesU8).catch(() => { })
			}, 100);
		},
	},
	computed: {
		throttlesU8() {
			return [
				...intToLeBytes(this.throttles[0], 2),
				...intToLeBytes(this.throttles[1], 2),
				...intToLeBytes(this.throttles[2], 2),
				...intToLeBytes(this.throttles[3], 2),
			]
		},
	},
})
</script>
<template>
	<div>
		<button @click="() => spinMotor(0)">Spin RR</button>
		<button @click="() => spinMotor(1)">Spin FR</button>
		<button @click="() => spinMotor(2)">Spin RL</button>
		<button @click="() => spinMotor(3)">Spin FL</button>
		<button @click="() => stopMotors()">Stop</button>
		<button @click="() => startMotors()">Start</button>
		<button @click="() => { stopMotors(); motorRemap = true }">Remap</button>
		<div class="quadPreview">
			<Motor v-for="m in motorMapping" :throttlePct="(motors[m] - 1000) / 10" />
		</div>
		<RemapMotors @close="motorRemap = false" v-if="motorRemap" />
	</div>
</template>

<style scoped>
button {
	color: black;
}

.quadPreview {
	display: grid;
	grid-template-columns: 1fr 1fr;
	grid-template-rows: 1fr 1fr;
	grid-gap: 2rem;
	width: 30vw;
	height: 30vw;
}
</style>
