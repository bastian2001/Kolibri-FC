<script lang="ts">
import { sendCommand } from "@/communication/serial";
import { MspFn, MspVersion } from "@/utils/msp";
import { defineComponent } from "vue";
import fonts from "@/utils/fonts";

export default defineComponent({
	name: "Osd",
	data() {
		return {
			cmah: 0,
			file: fonts.clarity,
			chars: [] as number[][],
			fonts
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
		uploadChar() {
			sendCommand('request', MspFn.WRITE_OSD_FONT_CHARACTER, MspVersion.V2, [this.cmah, ...this.chars[this.cmah].slice(0, 54)]);
		},
		nextChar() {
			this.cmah++;
		},
		upload() {
			this.chars = [];
			this.cmah = 0;
			this.decode();
			if (this.chars.length >= 128) {
				for (let i = 0; i < this.chars.length; i++) {
					setTimeout(() => {
						sendCommand('request', MspFn.WRITE_OSD_FONT_CHARACTER, MspVersion.V2, [this.cmah, ...this.chars[this.cmah].slice(0, 54)]);
						this.cmah++;
					}, i * 50);
				}
			}
		}
	}
});
</script>
<template>

	<div class="wrapper">
		Paste the .mcm file here, using an ASCII character table.<br>
		<textarea name="font" id="fontInput" cols="30" rows="10" v-model="file"></textarea>
		<button @click="() => upload()">Upload</button>
	</div>
</template>

<style scoped>
button,
textarea {
	color: black;
}
</style>