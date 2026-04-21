<script lang="ts">
import { onCommandHandler, onConnectHandler, sendCommand } from "@/msp/comm";
import { MspFn } from "@/msp/protocol";
import { Command } from "@/utils/types";
import { defineComponent } from "vue";
import { delay } from "@/utils/utils";

const TEXT_COLORS = ['white', 'red', 'lightgreen', 'yellow', 'blue', 'magenta', 'cyan', 'grey']

export default defineComponent({
	name: "Cli",
	data() {
		return {
			outputLines: [[]] as { color: string, text: string }[][], // purely controlled by the FC
			inputText: '',
			history: [] as string[],
			historyIndex: -1,
			historyBackup: '',
			commandRunning: false,
			runningCheckInterval: -1,
			cursorX: 0,
			cursorY: 0,
			stashedOutputLines: [] as { color: string, text: string }[][][],
			colors: TEXT_COLORS,
			currentColor: 'white',
		};
	},
	methods: {
		startCommand() {
			if (this.inputText) {
				const input = this.inputText.trim();
				if (input) {
					sendCommand(MspFn.CLI_COMMAND, this.inputText);
					if (this.history[this.history.length - 1] !== this.inputText) {
						this.history.push(this.inputText);
					}
					this.historyIndex = -1;
					this.inputText = ''; // Clear the input field
				}
			}
			this.runningCheckFn();
		},
		onCommand(command: Command) {
			switch (command.command) {
				case MspFn.CLI_INIT:
				case MspFn.CLI_COMMAND:
					if (command.dataStr) {
						for (let i = 0; i < command.dataStr.length; i++) {
							const char = command.dataStr[i];
							switch (char) {
								case '\n':
									this.cursorX = 0;
									this.cursorY++;
									if (this.cursorY >= this.outputLines.length) {
										this.outputLines.push([]);
									}
									break;
								case '\x01': // like \n but only works if the line is not empty
									if (this.outputLines[this.cursorY].length > 0) {
										this.cursorX = 0;
										this.cursorY++;
										if (this.cursorY >= this.outputLines.length) {
											this.outputLines.push([]);
										}
									}
									break;
								case '\r':
									this.cursorX = 0;
									break;
								case '\t':
									const nextTabStop = Math.ceil((this.cursorX + 1) / 4) * 4;
									this.replaceLineChars(this.cursorY, this.cursorX, ' '.repeat(nextTabStop - this.cursorX));
									this.cursorX = nextTabStop;
									break;
								case '\b':
									if (this.cursorX > 0) {
										this.cursorX--;
										this.replaceLineChars(this.cursorY, this.cursorX, ' ');
									}
									break;
								case '\v': // vertical tab, clear the screen
									this.outputLines = [[]];
									this.cursorX = 0;
									this.cursorY = 0;
									break;
								case '\x0f': // shift in, also clear the screen but stashed
									this.stashedOutputLines.push(this.outputLines);
									this.outputLines = [[]];
									this.cursorX = 0;
									this.cursorY = 0;
									break;
								case '\x0e': // shift out, restore stashed screen if available
									if (this.stashedOutputLines.length > 0) {
										this.outputLines = this.stashedOutputLines.pop()!;
										this.cursorY = this.outputLines.length - 1;
										this.cursorX = this.outputLines[this.cursorY].length;
									} else {
										this.outputLines = [[]];
										this.cursorX = 0;
										this.cursorY = 0;
									}
									break;
								case '\x10':
								case '\x11':
								case '\x12':
								case '\x13':
								case '\x14':
								case '\x15':
								case '\x16':
								case '\x17':
									const colorIndex = char.charCodeAt(0) - 0x10;
									if (colorIndex >= 0 && colorIndex < this.colors.length) {
										this.currentColor = this.colors[colorIndex];
									}
									break;
								default:
									// regular character, we can add multiple at once to reduce the number of calls
									let nextSpecialCharIndex = command.dataStr.length;
									for (let j = i + 1; j < command.dataStr.length; j++) {
										const c = command.dataStr[j];
										if ('\n\x01\x0e\x0f\r\t\b\v'.includes(c) || c >= '\x10' && c <= '\x17') {
											nextSpecialCharIndex = j;
											break;
										}
									}
									const charsToAdd = command.dataStr.substring(i, nextSpecialCharIndex);
									this.replaceLineChars(this.cursorY, this.cursorX, charsToAdd);
									this.cursorX += charsToAdd.length;
									i += charsToAdd.length - 1; // -1 because the for loop will also increment i
									break;
							}
						}
						const outputElement = document.querySelector('.output') as HTMLElement;
						if (outputElement) {
							this.$nextTick(() => { outputElement.scrollTop = outputElement.scrollHeight })
						}
					}
					break;
				default:
					break;
			}
		},
		replaceLineChars(lineIndex: number, charIndex: number, chars: string) {
			if (lineIndex >= this.outputLines.length) return;
			const line = this.outputLines[lineIndex];
			const lineChars: { char: string, color: string }[] = []
			console.log('Replacing chars at line', lineIndex, 'char', charIndex, 'with', chars);
			console.log(line, lineChars);
			console.log('Current color:', this.currentColor);


			// separate the line into characters with their colors
			for (let i = 0; i < line.length; i++) {
				for (let j = 0; j < line[i].text.length; j++) {
					lineChars.push({ char: line[i].text[j], color: line[i].color });
				}
			}

			// replace the characters at the specified position
			const insert: { char: string, color: string }[] = chars.split('').map(c => ({ char: c, color: this.currentColor }));
			lineChars.splice(charIndex, chars.length, ...insert);

			// convert back to the line format with color segments
			const newLine: { color: string, text: string }[] = [];
			let currentColor = lineChars[0]?.color || 'white';
			let currentText = '';
			for (const c of lineChars) {
				if (c.color !== currentColor) {
					newLine.push({ color: currentColor, text: currentText });
					currentColor = c.color;
					currentText = c.char;
				} else {
					currentText += c.char;
				}
			}
			if (currentText) {
				newLine.push({ color: currentColor, text: currentText });
			}
			this.outputLines[lineIndex] = newLine;
		},
		onStart() {
			this.outputLines = [[]];
			this.stashedOutputLines = [];
			this.cursorX = 0;
			this.cursorY = 0;
			delay(10).then(() => { sendCommand(MspFn.CLI_INIT) });
			(this.$refs.cliInput as HTMLInputElement).focus();
		},
		navigateHistory(e: KeyboardEvent) {
			if (e.key === 'ArrowUp') {
				e.preventDefault();
				if (this.historyIndex === -1) {
					this.historyBackup = this.inputText;
				}
				if (this.filteredHistory.length > 0) {
					// Navigate up in history
					if (this.historyIndex === -1) {
						this.historyBackup = this.inputText;
						this.historyIndex = this.filteredHistory.length - 1;
					} else if (this.historyIndex > 0) {
						this.historyIndex--;
						if (this.historyIndex === -1) this.historyIndex = 0; // Prevent going out of bounds
					}
					this.inputText = this.filteredHistory[this.historyIndex];
				}
			} else if (e.key === 'ArrowDown') {
				e.preventDefault();
				if (this.filteredHistory.length > 0 && this.historyIndex !== -1) {
					// Navigate down in history
					if (this.historyIndex < this.filteredHistory.length - 1) {
						this.historyIndex++;
						this.inputText = this.filteredHistory[this.historyIndex]
					} else {
						this.historyIndex = -1;
						this.inputText = this.historyBackup; // Restore the backup input
					}
				}
			}
		},
		runningCheckFn() {
			sendCommand(MspFn.CLI_CHECK_RUNNING)
				.then(({ data }) => {
					this.commandRunning = data[0] === 1;
				})
				.catch(() => { });
		},
		buttonPress() {
			if (this.commandRunning) {
				sendCommand(MspFn.CLI_ABORT_COMMAND);
			} else {
				this.startCommand();
			}
		},
		checkAbort(e: KeyboardEvent) {
			if (e.ctrlKey && e.key === 'c') {
				const input = (e.target as HTMLInputElement);
				if (typeof input.selectionStart === 'number' && input.selectionStart !== input.selectionEnd) {
					return; // Don't abort if there's a text selection
				}
				if (this.commandRunning) {
					sendCommand(MspFn.CLI_ABORT_COMMAND);
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
				this.history = parsedHistory.slice(-100);
			}
		}
		onCommandHandler(this.onCommand);
		onConnectHandler(this.onStart);
		this.runningCheckInterval = setInterval(this.runningCheckFn, 300);
	},
	beforeUnmount() {
		// save last 20 commands from history to local storage
		const history = this.history.slice(-100); // Limit to last 100 commands
		localStorage.setItem('cliHistory', JSON.stringify(history));
		clearInterval(this.runningCheckInterval);
	},
	computed: {
		filteredHistory() {
			const h = this.history.filter((item) => item.toLowerCase().includes(this.historyBackup.toLowerCase()))
			// reduce successive duplicates to one
			for (let i = h.length - 1; i > 0; i--) {
				if (h[i] === h[i - 1]) {
					h.splice(i, 1);
				}
			}
			return h;
		},
	},
});
</script>

<template>
	<div class="wrapper">
		<div class="output">
			<p v-for="(line, index) in outputLines" :key="'cli' + index">
				<span v-for="(part, i) in line" :key="i" :style="'color: ' + part.color + ';'">{{ part.text || '\u200b'
				}}</span>
			</p>
		</div>
		<div class="input">
			<input type="text" id="cliInput" @keydown.enter="startCommand" @keydown="checkAbort"
				@keydown.up.prevent="navigateHistory" @keydown.down.prevent="navigateHistory" autocomplete="off"
				v-model="inputText" ref="cliInput" />
			<button class="defaultBtn medium" :class="{ red: commandRunning }" @click="buttonPress">
				<i class="fa-solid fa-stop" v-if="commandRunning"></i>
				<i class="fa-solid fa-play" v-else></i>
			</button>
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

.output p,
.output span {
	font-family: monospace;
	font-size: 1rem;
	font-weight: bolder;
	margin: 3px 0px;
	white-space: pre-wrap;
}

.spacer {
	height: 15rem;
}

.input {
	display: grid;
	grid-template-columns: 1fr 0fr;
	gap: 1rem;
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
	box-sizing: border-box;
	padding: 7px 20px;
}
</style>
