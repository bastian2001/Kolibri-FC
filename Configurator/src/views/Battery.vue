<script lang="ts">
import NumericInput from "@/components/NumericInput.vue";
import { addOnCommandHandler, addOnConnectHandler, removeOnCommandHandler, removeOnConnectHandler, sendCommand } from "@/msp/comm";
import { MspFn } from "@/msp/protocol";
import { Command } from "@/utils/types";
import { intToLeBytes, leBytesToInt } from "@/utils/utils";
import { defineComponent } from "vue";

export default defineComponent({
	name: "Battery",
	components: { NumericInput },
	data() {
		return {
			autoCellCount: true,
			cellCount: 5,
			fcCells: 5,
			getBatStateInterval: -1,
			voltage: 0,
			batState: 0,
			emptyVoltage: 3,
		};
	},
	mounted() {
		this.getBatStateInterval = setInterval(this.getBatState, 100);
		addOnCommandHandler(this.onCommand)
		addOnConnectHandler(this.getBatSettings)
		this.getBatSettings()
	},
	unmounted() {
		clearInterval(this.getBatStateInterval)
		removeOnCommandHandler(this.onCommand)
		removeOnConnectHandler(this.getBatSettings)
	},
	methods: {
		getBatState() {
			sendCommand(MspFn.MSP_BATTERY_STATE)
		},
		getBatSettings() {
			sendCommand(MspFn.GET_BATTERY_SETTINGS)
		},
		onCommand(command: Command) {
			switch (command.command) {
				case MspFn.MSP_BATTERY_STATE: {
					this.fcCells = command.data[0]
					if (this.autoCellCount) this.cellCount = this.fcCells
					this.batState = command.data[8]
					this.voltage = leBytesToInt(command.data, 9, 2) / 100
					// unused: 1,2: capacity; 4,5: mAh drawn; 6,7: amps (10mA steps)
				} break;
				case MspFn.GET_BATTERY_SETTINGS: {
					this.cellCount = command.data[0]
					this.autoCellCount = this.cellCount === 0
					this.emptyVoltage = leBytesToInt(command.data, 1, 2) / 100
				} break;
				case MspFn.SET_BATTERY_SETTINGS: {
					sendCommand(MspFn.SAVE_SETTINGS)
				} break;
				case MspFn.SAVE_SETTINGS: {
					this.getBatSettings();
				} break;
			}
		},
		saveSettings() {
			const data = [
				this.autoCellCount ? 0 : this.cellCount,
				...intToLeBytes(this.emptyVoltage * 100, 2)
			]
			sendCommand(MspFn.SET_BATTERY_SETTINGS, data)
		}
	},
	computed: {
		voltagePerCell() {
			return this.voltage / this.fcCells
		}
	}
})
</script>

<template>
	<div class="wrapper">
		<div class="header">
			<button class="saveBtn" @click="() => { saveSettings() }">Save Settings</button>
		</div>
		<input type="checkbox" id="batteryAutoCellsCheckbox" v-model="autoCellCount"><label
			for="batteryAutoCellsCheckbox">Automatically detect cell count</label>

		<NumericInput v-model="cellCount" :min="1" :max="12" :step="1" :fix-to-steps="true" :disabled="autoCellCount" />
		Total Voltage: {{ voltage.toFixed(2) }} V<br>
		Cell Voltage: {{ voltagePerCell.toFixed(2) }} V<br>
		Battery State: {{ ['OK', 'Warning', 'Critical', 'Not present', 'Initialising'][batState] }}<br>
		<NumericInput v-model="emptyVoltage" :min="2" :max="4.5" :step="0.01" unit="V" :fix-to-steps="true" />
	</div>
</template>

<style scoped>
.wrapper {
	margin: 1.5rem auto;
	width: 90vw;
	max-width: 200vh;
}

.saveBtn {
	float: right;
	background-color: transparent;
	border: 1px solid var(--border-green);
	border-radius: 5px;
	padding: 0.5rem 1rem;
	font-size: 1rem;
	color: var(--text-color);
	transition: background-color 0.2s ease-out;
	margin-bottom: 1rem;
}

.saveBtn:hover {
	background-color: #fff1;
}

input {
	color: black;
}
</style>
