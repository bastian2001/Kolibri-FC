<script setup lang="ts">
import { onBeforeUnmount, ref } from 'vue'
import { sendCommand } from '@/msp/comm'
import { MspFn } from '@/msp/protocol';
import { intToLeBytes } from '@/utils/utils';
import { useLogStore } from '@/stores/logStore';

const $emit = defineEmits<{ close: [] }>()

const configuratorLog = useLogStore()
const mapped = ref([false, false, false, false])
const pins = [-1, -1, -1, -1]
const mappedTo = [-1, -1, -1, -1]
let disabled = ref(true)
let showNext = ref(true)
let step = ref(0);
let confirmPropsRemoved = ref(false)
let spinMotor = -1
let spinMotorInterval = -1;

const sendMotorInterval = setInterval(() => {
	const throttles = [1000, 1000, 1000, 1000]
	switch (step.value) {
		case 1:
		case 2:
		case 3:
		case 4:
			throttles[step.value - 1] = 1075
			break;
		case 5:
			if (spinMotor !== -1) throttles[spinMotor] = 1075
			break;
		case 0:
		default:
			break;

	}
	sendCommand(MspFn.SET_MOTOR, [
		...intToLeBytes(throttles[0], 2),
		...intToLeBytes(throttles[1], 2),
		...intToLeBytes(throttles[2], 2),
		...intToLeBytes(throttles[3], 2),
	]).catch(() => { })
}, 100)

window.addEventListener('keydown', onKey)
sendCommand(MspFn.GET_MOTOR_LAYOUT).then(c => {
	if (c.length < 4) return;
	for (let i = 0; i < 4; i++) {
		pins[i] = c.data[i]
	}
}).catch(() => { })

function nextBtn() {
	if (step.value === 0) {
		if (confirmPropsRemoved.value && pins[0] !== -1) {
			step.value++
			showNext.value = false
			disabled.value = false
		}
	}
	if (step.value === 5) {
		sendCommand(MspFn.SET_MOTOR_LAYOUT, mappedTo.map(v => pins[v]))
			.then(() => {
				configuratorLog.push('Motor Mapping updated')
				return sendCommand(MspFn.SAVE_SETTINGS)
			})
			.then(() => {
				$emit('close')
			})
			.catch(() => { })
	}
}

function clickMotor(i: number) {
	if (step.value == 5) {
		clearInterval(spinMotorInterval)
		spinMotor = mappedTo[i]
		spinMotorInterval = setInterval(() => spinMotor = -1, 1000)
		return;
	}
	const m = step.value - 1
	if (m < 0 || m > 3) return;
	if (mapped.value[i]) return;
	mappedTo[i] = m
	mapped.value[i] = true
	step.value++
	if (step.value === 5) showNext.value = true
}

function onKey(e: KeyboardEvent) {
	const motor = ['k', 'i', 'j', 'u'].findIndex(el => el === e.key)
	if (motor !== -1) clickMotor(motor)
}

onBeforeUnmount(() => {
	window.removeEventListener('keydown', onKey)
	clearInterval(sendMotorInterval)
	clearInterval(spinMotorInterval)
})
</script>

<template>
	<div class="background">
		<div class="wrapper">
			<div class="drone">
				<div class="bar0 bar"></div>
				<div class="bar1 bar"></div>
				<div class="arrowForward"></div>
				<div v-for="i in 4" :key="i"
					:class="{ motor: true, ['motor' + (i - 1)]: true, mapped: mapped[i - 1], disabled }"
					@click="clickMotor(i - 1)">
					<div class="motorLetter" v-if="!disabled">{{ ['K', 'I', 'J', 'U'][i - 1] }}</div>
					<!-- Motor order is changed visually via CSS grid -->
				</div>
			</div>
			<div class="info">
				<h2>Remap motors</h2>
				<div class="slideWrapper">
					<Transition name="slide" mode="default">
						<div class="step0 step" v-if="step === 0" :key="'s0'">
							<p>This wizard will spin your motors one by one to determine which motor is where. To stop,
								press
								the space bar or the cancel button.</p>
							<input type="checkbox" id="remapPropsRemoved" v-model="confirmPropsRemoved">
							<label for="remapPropsRemoved">
								I understand the risks – the propellers are removed and I want to proceed.
							</label>
							<p v-if="confirmPropsRemoved && pins[0] === -1" style="color:red">
								Still waiting for response from FC, check your connection.
							</p>
						</div>
						<div class="step14 step" v-else-if="step >= 1 && step <= 4" :key="'s14'">
							<p>Click on the motor that is spinning right now.</p>
							<p>You can also press the corresponding key.</p>
						</div>
						<div class="step5 step" v-else-if="step == 5" :key="'s5'">
							Click on the motors or press their key to test the mapping.
						</div>
					</Transition>
				</div>
				<div class="buttons">
					<button class="nextBtn" v-if="showNext" @click="nextBtn()">{{ step === 5 ? "Finish" :
						"Next" }}</button>
					<button class="cancelBtn" @click="$emit('close')">Cancel</button>
				</div>
			</div>
		</div>
	</div>
</template>

<style scoped>
.background {
	position: absolute;
	top: 0;
	left: 0;
	width: 100%;
	height: 100%;
	background-color: #0007;
	z-index: 1;
}

.wrapper {
	position: absolute;
	top: 50%;
	left: 50%;
	transform: translate(-50%, -50%);
	background-color: var(--background-dark-blue);
	box-sizing: content-box;
	border-radius: 1.5rem;
	padding: 2rem;
	display: flex;
	flex-direction: row;
	gap: 2rem;
	width: 50rem;
	max-width: 70vw;
}

.drone {
	display: grid;
	grid-template-columns: 10rem 10rem;
	gap: 1rem;
	position: relative;
}

.bar {
	background-color: #666;
	height: 1rem;
	grid-row: 1 / 3;
	grid-column: 1 / 3;
	width: 17rem;
	left: 50%;
	position: absolute;
	border-radius: 0.5rem;
}

.bar0 {
	transform: translateY(10rem) translateX(-50%) rotate(45deg);
}

.bar1 {
	transform: translateY(10rem) translateX(-50%) rotate(-45deg);
}

.arrowForward {
	grid-row: 1 / 2;
	grid-column: 1 / 3;
	width: 60px;
	height: 30px;
	position: absolute;
	left: 50%;
	top: 15px;
	z-index: 2;
	transform: translate(-50%, -50%);
	background-color: #aaa;
	clip-path: polygon(0 100%, 50% 0, 100% 100%);
}

.motor {
	border-radius: 100%;
	z-index: 2;
	border: 3px solid var(--border-blue);
	height: 10rem;
	box-sizing: border-box;
	cursor: pointer;
	display: flex;
	flex-direction: column;
	align-items: center;
	justify-content: center;
	background-color: #0004;
	transition: background-color .3s ease-out;
}

.motor0 {
	grid-row: 2;
	grid-column: 2;
}

.motor1 {
	grid-row: 1;
	grid-column: 2;
}

.motor2 {
	grid-row: 2;
	grid-column: 1;
}

.motor3 {
	grid-row: 1;
	grid-column: 1;
}

.motor:hover {
	background-color: #fff2;
}

.motor:active {
	background-color: #fff4;
}

.motor.mapped {
	background-color: #fff2;
	cursor: initial;
}

.motor.disabled {
	background-color: #0004;
	cursor: initial;
}

.motorLetter {
	color: var(--text-color);
	font-size: 3rem;
	font-weight: bold;
}

.info {
	display: flex;
	flex-direction: column;
	justify-content: space-between;
	flex-grow: 1;
	overflow-x: hidden;
}

.info h2 {
	margin-top: 0px
}

.buttons {
	margin-top: 1rem;
}

.nextBtn,
.cancelBtn {
	float: right;
	background-color: transparent;
	border: 1px solid var(--border-green);
	border-radius: 5px;
	padding: 0.5rem 1rem;
	font-size: 1rem;
	color: var(--text-color);
	transition: background-color 0.2s ease-out;
	margin-left: 1rem;
}

.slideWrapper {
	position: relative;
}

.step {
	position: absolute;
	top: 50%;
	transform: translateY(-50%) translateX(0%);
	width: 100%;
}

.nextBtn:hover,
.cancelBtn:hover {
	background-color: #fff1;
}

.slide-enter-from {
	transform: translateY(-50%) translateX(110%);
}

.slide-leave-to {
	transform: translateY(-50%) translateX(-110%);
}

.slide-leave-active,
.slide-enter-active {
	transition: all ease-in-out .7s;
}
</style>
