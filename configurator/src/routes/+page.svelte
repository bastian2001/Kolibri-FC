<script lang="ts">
	import { invoke } from '@tauri-apps/api';
	import { port } from '../stores';

	// port.subscribe((p) => {
	// 	device = p;
	// });

	function send() {
		let data = [] as number[];
		for (let i = 0; i < 'B10'.length; i++) {
			data[i] = 'B10'.charCodeAt(i);
		}
		invoke('serial_write', { data })
			.then(() => {
				console.log('sent');
			})
			.catch((e) => {
				console.log(e);
			});
	}

	function print() {
		invoke('serial_read')
			.then((d: unknown) => {
				console.log(d);
			})
			.catch((e) => {
				console.log(e);
			});
	}
</script>

<button on:click={() => send()}>Send B10</button>
<button on:click={() => print()}>Print</button>
