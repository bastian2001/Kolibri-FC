<script lang="ts">
	import { constrain, map } from '../../utils';
	interface Props {
		value?: number;
	}

	let { value = 1000 + 1000 * Math.random() }: Props = $props();

	// let valDiv: HTMLDivElement;

	// $: updateVal();
	// function updateVal() {
	// 	const width = map(value, 800, 2200, 0, 100) + '%';
	// 	if (!valDiv) return;
	// 	valDiv.style.width = width;
	// }
</script>

<div class="channelOuter">
	<div
		class="channelProgress"
		style:width={map(constrain(value, 800, 2200), 800, 2200, 0, 100) + '%'}
	></div>
	<div class="channelValue">
		{Math.round(value)}
	</div>
</div>

<style>
	.channelOuter {
		height: 1.4rem;
		margin: 0.5rem 0px;
		position: relative;
		border-radius: 3px;
		width: 100%;
		border: 1px solid white;
		background-color: #444;
	}
	.channelValue {
		position: absolute;
		top: 0px;
		left: 0px;
		width: 100%;
		height: 100%;
		padding: 0.2rem;
		text-align: center;
		color: white;
		font-weight: bold;
	}
	.channelProgress {
		position: absolute;
		top: 0px;
		left: 0px;
		width: 0%;
		height: 100%;
		background-color: #080;
	}
</style>
