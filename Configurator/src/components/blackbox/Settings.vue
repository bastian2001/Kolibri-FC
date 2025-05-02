<script lang="ts">
import { sendCommand, addOnCommandHandler, removeOnCommandHandler } from '@/communication/serial';
import { MspFn, MspVersion } from '@utils/msp';
import { Command, FlagProps } from '@utils/types';
import { defineComponent, type PropType } from 'vue';
import { leBytesToInt } from '@utils/utils';

export default defineComponent({
	name: 'BlackboxSettings',
	data() {
		return {
			flagNames: [] as string[],
			selected: [] as string[],
			groups: [] as string[][],
			divider: 0,
		};
	},
	props: {
		flags: {
			type: Object as PropType<{ [key: string]: FlagProps }>,
			required: true,
		},
	},
	mounted() {
		this.flagNames = Object.keys(this.flags);
		let i = 0;
		const groupSizes = [4, 4, 3, 5, 5, 5, 3, 3, 3, 1, 6, 2, 20];
		const g: string[][] = [];
		for (const size of groupSizes) {
			g.push(this.flagNames.slice(i, i + size));
			i += size;
		}
		this.groups = g;
		sendCommand('request', MspFn.GET_BB_SETTINGS)
		console.log('Requesting blackbox settings');

		addOnCommandHandler(this.onCommand);
	},
	unmounted() {
		removeOnCommandHandler(this.onCommand);
	},
	emits: ['close'],
	methods: {
		onCommand(command: Command) {
			if (command.cmdType === 'response') {
				switch (command.command) {
					case MspFn.GET_BB_SETTINGS:
						if (command.length !== 9) break;
						this.divider = command.data[0];
						const selectedBin = leBytesToInt(command.data.slice(1, 5), false);
						const selectedBin2 = leBytesToInt(command.data.slice(5, 9), false);
						const sel = [];
						for (let i = 0; i < 32; i++) {
							if (selectedBin & (1 << i)) sel.push(this.flagNames[i]);
							if (selectedBin2 & (1 << i)) sel.push(this.flagNames[i + 32]);
						}
						this.selected = sel;
						break;
					case MspFn.SET_BB_SETTINGS:
						sendCommand('request', MspFn.SAVE_SETTINGS);
						break;
					case MspFn.SAVE_SETTINGS:
						this.$emit('close');
						break;
				}
			}
		},
		saveSettings() {
			const bytes = [0, 0, 0, 0, 0, 0, 0, 0];
			for (const name of this.selected) {
				const index = this.flagNames.indexOf(name);
				const byte = Math.floor(index / 8);
				const bit = index % 8;
				bytes[byte] |= 1 << bit;
			}

			sendCommand('request', MspFn.SET_BB_SETTINGS, MspVersion.V2, [this.divider, ...bytes]);
		}
	}
})
</script>

<template>
	<div class="background">
		<div class="settingsWindow">
			<div v-for="group in groups" class="group">
				<label v-for="name in group" :for="`check-${name}`">
					<input :id="`check-${name}`" type="checkbox" v-model="selected" name="selected" :value="name" />
					{{ flags[name].name }}
				</label>
			</div>
			<div class="dividerSetting"><input type="number" v-model="divider" /></div>
			<div class="apply">
				<button class="saveBtn" @click="saveSettings">Save settings</button>
				<button class="cancelBtn" @click="$emit('close')">Cancel</button>
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

.settingsWindow {
	position: absolute;
	top: 50%;
	left: 50%;
	transform: translate(-50%, -50%);
	width: 500px;
	height: 500px;
	min-width: 70%;
	min-height: 70%;
	background-color: var(--background-blue);
	box-sizing: content-box;
	border-radius: 1rem;
	padding: 1rem;
}

label {
	display: block;
}

.settingsWindow {
	display: grid;
	grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
	gap: 1rem;
}

.apply {
	grid-column: 1 / -1;
	text-align: right;
}

.saveBtn,
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

.saveBtn:hover,
.cancelBtn:hover {
	background-color: #fff1;
}
</style>
