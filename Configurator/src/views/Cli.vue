<script lang="ts">
import { onCommandHandler, onConnectHandler, sendCommand } from "@/msp/comm";
import { MspFn } from "@/msp/protocol";
import { Command } from "@/utils/types";
import { defineComponent } from "vue";
import { delay } from "@/utils/utils";

const TEXT_COLORS = ['white', 'red',/*green*/ '#6f8', /*yellow*/'#fd0', /*blue*/ '#68f', 'magenta', 'cyan', 'grey']

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
			currentColor: TEXT_COLORS[0],
			dataHeldBack: '',
			holdingBack: false,
			suggestions: [] as string[][],
			selectedSuggestionIndex: -1,
			suggestionsFor: '',
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
					this.clearSuggestions();
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
							if (this.holdingBack && char !== '\x03' && char !== '\x02') {
								this.dataHeldBack += char;
								continue;
							}
							switch (char) {
								case '\x01': // like \n but only works if the line is not empty
									if (this.outputLines[this.cursorY].length > 0) {
										this.cursorX = 0;
										this.cursorY++;
										if (this.cursorY >= this.outputLines.length) {
											this.outputLines.push([]);
										}
									}
									break;
								case '\x02': // prevents incoming characters from being processed until \x03 is seen, used to prevent screen corruption during long outputs, we can stash the incoming data in the meantime
									this.dataHeldBack = '';
									this.holdingBack = true;
									break;
								case '\x03':
									// process the held back data by reusing this function
									const heldBack = this.dataHeldBack;
									this.dataHeldBack = '';
									this.holdingBack = false;
									this.onCommand({ command: MspFn.CLI_COMMAND, dataStr: heldBack, data: new Uint8Array(), cmdType: 'request', length: 0, flag: 0, version: 0 } as Command);
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
								case '\x0f': // shift in, also clear the screen but stashed
									this.stashedOutputLines.push(this.outputLines);
									this.outputLines = [[]];
									this.cursorX = 0;
									this.cursorY = 0;
									break;
								case '\n':
									this.cursorX = 0;
									this.cursorY++;
									if (this.cursorY >= this.outputLines.length) {
										this.outputLines.push([]);
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
										if (c < '\x20') {
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
			let currentColor = lineChars[0]?.color || TEXT_COLORS[0];
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
		navigateInput(e: KeyboardEvent) {
			if (this.suggestions.length) {
				let selected = false;
				if (e.key === 'ArrowUp') {
					e.preventDefault();
					if (this.selectedSuggestionIndex < this.suggestions.length - 1) {
						this.selectedSuggestionIndex++;
						selected = true;
					} else {
						this.selectedSuggestionIndex = -1;
					}
				} else if (e.key === 'ArrowDown') {
					e.preventDefault();
					if (this.selectedSuggestionIndex >= 0) {
						this.selectedSuggestionIndex--;
					}
					selected = this.selectedSuggestionIndex >= 0;
				}
				if (selected) {
					this.$nextTick(() => {
						const sugs = this.$refs.suggestions as HTMLDivElement | null;
						if (!sugs) return;
						const selected = sugs.querySelector('.suggestion.selected') as HTMLDivElement | null;
						if (!selected) return;
						selected.scrollIntoView({ block: 'nearest' });
					});
					const text = this.suggestions[this.selectedSuggestionIndex][0];
					if (text) {
						this.inputText = text;
						const base = this.suggestionsFor;
						if (text.startsWith(base) && text.length > base.length) {
							this.$nextTick(() => {
								const input = this.$refs.cliInput as HTMLInputElement | null;
								if (!input) return;
								input.setSelectionRange(base.length, text.length);
							});
						}
					}
				} else {
					this.inputText = this.suggestionsFor;
				}
				return;
			}
			else {
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
		clearSuggestions() {
			this.suggestions = [];
			this.selectedSuggestionIndex = -1;
		},
		getSuggestions(jumpToFirst = false) {
			const text = this.inputText;
			sendCommand(MspFn.CLI_GET_SUGGESTION, text)
				.then(({ dataStr }) => {
					// the suggestions that are returned are separated by \n
					// \r is used as a tab stop, so that when pressing the tab key on --arg \r0...99, it will only autocomplete to "--arg " and not "--arg 0...99"
					this.suggestions = dataStr.split('\n').filter(s => s.trim().length > 0).map(s => s.split('\r'));
					if (this.suggestions.length > 0 && jumpToFirst) {
						this.selectedSuggestionIndex = 0;
					} else {
						this.selectedSuggestionIndex = -1;
					}
					this.suggestionsFor = text;
				})
				.catch(() => { });
		},
		oninput() {
			this.getSuggestions();
		},
		onkey(e: KeyboardEvent) {
			// check for Ctrl + C to abort the running command, but only if there's no text selection to allow copying text
			if (e.ctrlKey && e.key === 'c') {
				const input = (e.target as HTMLInputElement);
				if (typeof input.selectionStart === 'number' && input.selectionStart !== input.selectionEnd) {
					return; // Don't abort if there's a text selection
				}
				if (this.commandRunning) {
					sendCommand(MspFn.CLI_ABORT_COMMAND);
				}
				return;
			}

			if (e.ctrlKey && e.key === ' ') {
				this.getSuggestions(true);
				e.preventDefault();
				return;
			}

			if (e.key === 'Escape') {
				this.clearSuggestions();
				return;
			}

			if (e.key === 'Tab') {
				if (this.useSuggestion(this.selectedSuggestionIndex)) {
					e.preventDefault();
					return;
				}
			}
		},
		useSuggestion(i: number) {
			if (i >= 0 && i < this.suggestions.length) {
				this.inputText = this.suggestions[i][0];
				this.clearSuggestions();
				const input = this.$refs.cliInput as HTMLInputElement | null;
				if (!input) return;
				input.focus();
				input.setSelectionRange(this.inputText.length, this.inputText.length);
				this.getSuggestions();
				return true;
			}
			return false;
		},
		splitSuggestion(suggestion: string) {
			// split the suggestion into multiple parts. Parts that are the same as the input will be rendered black and the rest grey
			const parts: { text: string, solid: boolean }[] = [];
			let currentIndex = 0;
			for (let i = 0; i < suggestion.length; i++) {
				const inputChar = this.suggestionsFor[currentIndex];
				const suggestionChar = suggestion[i];
				if (inputChar && inputChar.toLowerCase() === suggestionChar.toLowerCase()) {
					if ((parts.length > 0 && !parts[parts.length - 1].solid) || parts.length === 0) {
						parts.push({ text: '', solid: true });
					}
					parts[parts.length - 1].text += suggestionChar;
					currentIndex++;
				} else {
					if ((parts.length > 0 && parts[parts.length - 1].solid) || parts.length === 0) {
						parts.push({ text: '', solid: false });
					}
					parts[parts.length - 1].text += suggestionChar;
				}
			}
			return parts;
		}
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
			<div class="innerInput">
				<input :class="{ forceRadius: suggestions.length === 0 }" type="text" id="cliInput"
					@keydown.enter="startCommand" @keydown="onkey" @input="oninput" @keydown.up.prevent="navigateInput"
					@keydown.down.prevent="navigateInput" autocomplete="off" v-model="inputText" ref="cliInput"
					placeholder="Try 'help' or press Ctrl + Space for suggestions" />
				<div class="suggestions" ref="suggestions">
					<div class="suggestion" v-for="(s, i) in suggestions" @mousedown.prevent="() => { }"
						@click="() => useSuggestion(i)" :class="{ selected: i === selectedSuggestionIndex }">
						<p class="fixed">
							<span v-for="part in splitSuggestion(s[0].trim())"
								:class="{ solid: part.solid, dim: !part.solid }">{{ part.text }}</span>
						</p>&nbsp;
						<p class="ghost" v-if="s.length > 1">{{ s[1].trim() }}</p>
						<span class="tabIndicator" v-if="selectedSuggestionIndex === i">
							Tab
							<i class="fa-solid fa-arrow-right"></i>
						</span>
					</div>
				</div>
			</div>
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
	font-weight: bold;
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

.innerInput {
	position: relative;
}

.input input {
	background-color: white;
	color: black;
	font-family: monospace;
	font-weight: bold;
	border-radius: 7px;
	outline: none;
	display: inline-block;
	width: 100%;
	border: none;
	font-size: 1rem;
	box-sizing: border-box;
	padding: 7px 20px;
}

.input input:focus {
	border-top-left-radius: 0px;
	border-top-right-radius: 0px;
}

.input input.forceRadius {
	border-radius: 7px;
}

.suggestions {
	position: absolute;
	bottom: 100%;
	width: 100%;
	left: 0;
	flex-direction: column-reverse;
	display: none;
	border-top-left-radius: 7px;
	border-top-right-radius: 7px;
	max-height: 24rem;
	overflow: auto;
	font-size: 1rem;
}

.input input:focus+.suggestions {
	display: flex;
}

.suggestion {
	justify-content: flex-start;
	align-items: center;
	padding: 3px 20px;
	cursor: pointer;
	display: flex;
	background-color: #ccc;
}

.suggestion:hover,
.suggestion.selected {
	background-color: #bbb;
}

.suggestion p {
	margin: 0;
	padding: 3px 0px;
}

.suggestion .fixed span {
	font-family: monospace;
	font-weight: bold;
}

.suggestion .fixed .solid {
	color: black;
	font-weight: bold;
}

.suggestion .fixed .dim {
	color: #666;
}

.suggestion .ghost {
	font-family: monospace;
	font-weight: bold;
	color: #777;
	margin-left: 5px;
}

.suggestion .tabIndicator {
	align-items: center;
	font-size: 0.8rem;
	background-color: #999;
	border: 1px solid #555;
	color: #444;
	border-radius: 3px;
	padding: 2px 5px;
	margin-left: 1.5rem;
}
</style>
