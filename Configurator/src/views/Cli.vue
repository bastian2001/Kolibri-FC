<script lang="ts">
import { addOnCommandHandler, addOnConnectHandler, removeOnCommandHandler, removeOnConnectHandler, sendCommand } from "@/communication/serial";
import { MspFn, MspVersion } from "@/utils/msp";
import { Command } from "@/utils/types";
import { defineComponent } from "vue";
import { delay } from "@/utils/utils";

export default defineComponent({
	name: "Cli",
	data() {
		return {
			outputLines: [''], // purely controlled by the FC
			inputText: '' as string,
			history: [] as string[],
			historyIndex: -1 as number,
			historyBackup: '',
		};
	},
	methods: {
		sendCommand(_e: KeyboardEvent) {
			if (this.inputText) {
				const input = this.inputText.trim();
				if (input) {
					sendCommand('request', MspFn.CLI_COMMAND, MspVersion.V2, [], this.inputText);
					this.history.push(this.inputText);
					this.historyIndex = -1;
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
		onStart() {
			this.outputLines = [''];
			delay(10).then(() => { sendCommand('request', MspFn.CLI_INIT) });
			this.$refs.cliInput.focus();
		},
		navigateHistory(e: KeyboardEvent) {
			if (e.key === 'ArrowUp') {
				e.preventDefault();
				if (this.history.length > 0) {
					// Navigate up in history
					if (this.historyIndex === -1) {
						this.historyBackup = this.inputText;
						this.historyIndex = this.history.length - 1;
					} else if (this.historyIndex > 0) {
						this.historyIndex--;
						if (this.historyIndex === -1) this.historyIndex = 0; // Prevent going out of bounds
					}
					this.inputText = this.history[this.historyIndex];
				}
			} else if (e.key === 'ArrowDown') {
				e.preventDefault();
				if (this.history.length > 0 && this.historyIndex !== -1) {
					// Navigate down in history
					if (this.historyIndex < this.history.length - 1) {
						this.historyIndex++;
						this.inputText = this.history[this.historyIndex]
					} else {
						this.historyIndex = -1;
						this.inputText = this.historyBackup; // Restore the backup input
					}
				}
			}
		},
	},
	mounted() {
		this.onStart();
		// load last 20 commands from local storage
		const history = localStorage.getItem('cliHistory');
		if (history) {
			const parsedHistory = JSON.parse(history);
			if (Array.isArray(parsedHistory)) {
				this.history = parsedHistory.slice(-20); // Limit to last 20 commands
			}
		}
		addOnCommandHandler(this.onCommand);
		addOnConnectHandler(this.onStart)
	},
	beforeUnmount() {
		// save last 20 commands from history to local storage
		const history = this.history.slice(-20);
		localStorage.setItem('cliHistory', JSON.stringify(history));

		removeOnCommandHandler(this.onCommand);
		removeOnConnectHandler(this.onStart);
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
			<input type="text" id="cliInput" @keydown.enter="sendCommand" @keydown.up.prevent="navigateHistory"
				@keydown.down.prevent="navigateHistory" autocomplete="off" v-model="inputText" ref="cliInput" />
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
