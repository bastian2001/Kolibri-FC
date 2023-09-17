<script lang="ts">
	import { port, type Command, ConfigCmd } from '../../stores';
	import { onMount } from 'svelte';
	import { leBytesToInt } from '../../utils';

	let pids = [[], [], []] as number[][];
	let rateFactors = [[], [], [], [], []] as number[][];

	$: handleCommand($port);
	function handleCommand(command: Command) {
		switch (command.command) {
			case ConfigCmd.GET_PIDS | 0x4000:
				if (command.length !== 3 * 2 * 7) break;
				for (let ax = 0; ax < 3; ax++)
					for (let i = 0; i < 7; i++)
						pids[ax][i] = leBytesToInt(command.data.slice(ax * 14 + i * 2, ax * 14 + i * 2 + 2));
				break;
			case ConfigCmd.GET_RATES | 0x4000:
				if (command.length !== 5 * 2 * 3) break;
				for (let ax = 0; ax < 5; ax++)
					for (let i = 0; i < 7; i++)
						rateFactors[ax][i] = leBytesToInt(
							command.data.slice(ax * 14 + i * 2, ax * 14 + i * 2 + 2)
						);
				break;
		}
	}

	onMount(() => {
		port.sendCommand(ConfigCmd.GET_PIDS);
		port.sendCommand(ConfigCmd.GET_RATES);
	});
</script>

<div>Tuning</div>

<style>
	div {
		color: red;
	}
</style>
