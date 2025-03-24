<script lang="ts">
// // import { invoke } from '@tauri-apps/api';
// // import { onMount, onDestroy } from 'svelte';
// let cmah = $state(0);
// let file = $state('');
// const chars = $state([] as number[][]);

// function decode() {
// 	const lines = file.split('\n');
// 	for (let i = 0; i < lines.length - 1; i++) {
// 		//each line has 8 0/1s, and those are one char each
// 		const line = lines[i + 1];
// 		if (!chars[Math.floor(i / 64)]) chars[Math.floor(i / 64)] = [];
// 		chars[Math.floor(i / 64)][i % 64] = parseInt(line, 2);
// 		if (!(i % 64)) {
// 			console.log(Math.floor(i / 64));
// 		}
// 	}
// }
// function upload() {
// 	// const slice = [] as number[];
// 	// slice.push('O'.charCodeAt(0));
// 	// slice.push('S'.charCodeAt(0));
// 	// slice.push('D'.charCodeAt(0));
// 	// slice.push(cmah);
// 	// slice.push(...chars[cmah].slice(0, 54));
// 	// invoke('serial_write', { data: slice })
// 	// 	.then(() => {
// 	// 		console.log('sent', cmah);
// 	// 	})
// 	// 	.catch((e) => {
// 	// 		console.log(e);
// 	// 	});
// }
// function nextChar() {
// 	cmah++;
// }
// function nextCharAndUpload() {
// 	upload();
// 	nextChar();
// }
// // let int = 0;
// // onMount(() => {
// // 	int = setInterval(() => {
// // 		invoke('serial_read')
// // 			.then((d: unknown) => {
// // 				//uint8array to string
// // 				const data = d as number[];
// // 				let str = '';
// // 				for (let i = 0; i < data.length; i++) {
// // 					str += String.fromCharCode(data[i]);
// // 				}
// // 				console.log(str);
// // 			})
// // 			.catch((e) => {});
// // 	}, 100);
// // });
// // onDestroy(() => {
// // 	clearInterval(int);
// // });
import { defineComponent } from "vue";

export default defineComponent({
	name: "Osd",
})
</script>
<template>

	<div class="wrapper">
		OSD upload does not work currently
		<!-- <textarea name="font" id="fontInput" cols="30" rows="10" bind:value={file}></textarea>
		<button onclick={() => decode()}>Decode</button>
		<button onclick={() => upload()}>Upload</button>
		<button onclick={() => nextChar()}>Next Char</button>
		<button onclick={() => nextCharAndUpload()}>Upload and next char</button>
		<p>{cmah}</p>
		<p>{JSON.stringify(chars[cmah])}</p> -->
	</div>
</template>

<style scoped>
button,
textarea {
	color: black;
}
</style>