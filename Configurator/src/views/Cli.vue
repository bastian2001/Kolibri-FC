<script lang="ts">
import { addOnCommandHandler, addOnConnectHandler, removeOnCommandHandler, removeOnConnectHandler, sendCommand } from "@/communication/serial";
import { MspFn, MspVersion } from "@/utils/msp";
import { Command } from "@/utils/types";
import { defineComponent } from "vue";

export default defineComponent({
	name: "Cli",
	data() {
		return {
			outputLines: [''], // purely controlled by the FC
			inputText: '' as string,
		};
	},
	methods: {
		sendCommand(_e: KeyboardEvent) {
			if (this.inputText) {
				const input = this.inputText.trim();
				if (input) {
					sendCommand('request', MspFn.CLI_COMMAND, MspVersion.V2, [], this.inputText);
					this.inputText = ''; // Clear the input field
				}
			}
		},
		onCommand(command: Command) {
			switch (command.command) {
				case MspFn.CLI_INIT:
				case MspFn.CLI_COMMAND:
					if (command.dataStr) {
						const lines = command.dataStr.split('\n');
						this.outputLines[this.outputLines.length - 1] += lines[0]; // Add the first line to the output
						this.outputLines.push(...lines.slice(1)); // Add the rest of the lines to the output
					}
					const outputElement = document.querySelector('.output') as HTMLElement;
					if (outputElement) {
						outputElement.scrollTop = outputElement.scrollHeight;
					}
					break;
				default:
					break;
			}
		},
		sendCliStart() {
			this.outputLines = [''];
			sendCommand('request', MspFn.CLI_INIT);
		},
	},
	mounted() {
		this.sendCliStart();
		addOnCommandHandler(this.onCommand);
		addOnConnectHandler(this.sendCliStart)
	},
	beforeUnmount() {
		removeOnCommandHandler(this.onCommand);
		removeOnConnectHandler(this.sendCliStart);
	},
});
</script>

<template>
	<div class="wrapper">
		<div class="output">
			<p v-for="(line, index) in outputLines" :key="'cli' + index">{{ line }}</p>
			<div class="spacer"></div>
		</div>
		<div class="input">
			<input type="text" id="cliInput" @keydown.enter="sendCommand" autocomplete="off" v-model="inputText" />
		</div>
	</div>
</template>

<style scoped>
.wrapper {
	display: flex;
	flex-direction: column;
	height: 100%;
	padding: 20px;
	box-sizing: border-box;
	gap: 1rem;
}

.output {
	background-color: black;
	color: white;
	flex-grow: 1;
	border-radius: 10px;
	padding: 7px 20px;
	overflow-y: auto;
}

.output p {
	font-family: monospace;
	font-size: 1rem;
	font-weight: bolder;
	margin: 3px 0px;
}

.spacer {
	height: 15rem;
}

.input {
	display: flex;
}

.input input {
	background-color: white;
	color: black;
	font-family: monospace;
	font-weight: bolder;
	border-radius: 7px;
	outline: none;
	display: inline-block;
	width: 100%;
	border: none;
	font-size: 1rem;
	padding: 7px 20px;
}
</style>
