<script lang="ts">
import { sendCommand } from "@/msp/comm";
import { MspFn } from "@/msp/protocol";
import { defineComponent } from "vue";
import fonts from "@/utils/fonts";
import { useLogStore } from "@/stores/logStore";

export default defineComponent({
	name: "Osd",
	data() {
		return {
			file: fonts.clarity,
			chars: [] as number[][],
			fonts,
			configuratorLog: useLogStore(),
			charsDone: 0,
		};
	},
	methods: {
		decode() {
			const lines = this.file.split('\n');
			for (let i = 0; i < lines.length - 1; i++) {
				//each line has 8 0/1s, and those are one char each
				const line = lines[i + 1];
				if (!this.chars[Math.floor(i / 64)]) this.chars[Math.floor(i / 64)] = [];
				this.chars[Math.floor(i / 64)][i % 64] = parseInt(line, 2);
				if (!(i % 64)) {
					console.log(Math.floor(i / 64));
				}
			}
		},
		async upload() {
			this.chars = [];
			this.decode();
			if (this.chars.length >= 128) {
				for (let i = 0; i < this.chars.length; i++) {
					try {
						await sendCommand(MspFn.WRITE_OSD_FONT_CHARACTER, { data: [i, ...this.chars[i].slice(0, 54)], verifyFn: (req, res) => req.command === res.command && res.length === 1 && req.data[0] === res.data[0] })
						this.charsDone = i + 1
						if (i + 1 >= this.chars.length) this.configuratorLog.push('Successfully uploaded OSD font')
					} catch (er) {
						this.configuratorLog.push(`There was an error uploading your font (${er}), please try again`)
						break
					}
				}
			} else {
				this.configuratorLog.push('Please provide a full file')
			}
		}
	}
});
</script>
<template>

	<div class="wrapper">
		Paste the .mcm file here, using an ASCII character table.<br>
		<textarea id="fontInput" cols="30" rows="10" v-model="file"></textarea>
		<button @click="upload">Upload</button>
		<p>Done: {{ charsDone }} / {{ chars.length }}</p>
	</div>
</template>

<style scoped>
button,
textarea {
	color: black;
}
</style>
