<script lang="ts">
import Motor from "@components/Motor.vue";
import { defineComponent } from "vue";
import { MspFn } from "@/msp/protocol";
import { intToLeBytes, leBytesToInt } from "@utils/utils";
import { useLogStore } from "@stores/logStore";
import { Command } from "@utils/types";
import { sendCommand, addOnDisconnectHandler, removeOnDisconnectHandler, addOnCommandHandler, removeOnCommandHandler } from "@/msp/comm";
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
			sendCommand(MspFn.GET_MOTOR);
		}, 100);
		addOnDisconnectHandler(this.stopMotors);
		addOnCommandHandler(this.onCommand);
	},
	unmounted() {
		clearInterval(this.getMotorsInterval);
		clearInterval(this.sendInterval);
		removeOnDisconnectHandler(this.stopMotors);
		removeOnCommandHandler(this.onCommand);
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
				sendCommand(MspFn.SET_MOTOR, this.throttlesU8);
			}, 100);
		},
		onCommand(command: Command) {
			if (command.cmdType === 'response') {
				switch (command.command) {
					case MspFn.GET_MOTOR:
						{
							const m: number[] = [];
							for (let i = 0; i < 4; i++) {
								m.push(leBytesToInt(command.data, i * 2, 2));
							}
							this.motors = m;
						}
						break;
				}
			}
		}
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
