<script lang="ts">
	interface Props {
		throttlePct?: number;
	}

	let { throttlePct = 0 }: Props = $props();
	let cp = $state('');
	function updateClipPath() {
		const points = ['50% 50%', '50% 0%'];
		if (throttlePct < 12.5 || throttlePct >= 87.5) {
			const y = '0%';
			const x = `${50 + Math.tan((throttlePct / 100) * 2 * Math.PI) * 50}%`;
			if (throttlePct >= 87.5) {
				points.push('100% 0%');
				points.push('100% 100%');
				points.push('0% 100%');
				points.push('0% 0%');
			}
			points.push(`${x} ${y}`);
		} else if (throttlePct < 37.5) {
			const x = '100%';
			const y = `${50 + 50 * Math.tan(((throttlePct - 25) / 100) * 2 * Math.PI)}%`;
			points.push('100% 0%');
			points.push(`${x} ${y}`);
		} else if (throttlePct < 62.5) {
			const y = '100%';
			const x = `${50 - Math.tan(((throttlePct - 50) / 100) * 2 * Math.PI) * 50}%`;
			points.push('100% 0%');
			points.push('100% 100%');
			points.push(`${x} ${y}`);
		} else {
			const x = '0%';
			const y = `${50 - 50 * Math.tan(((throttlePct - 75) / 100) * 2 * Math.PI)}%`;
			points.push('100% 0%');
			points.push('100% 100%');
			points.push('0% 100%');
			points.push(`${x} ${y}`);
		}
		cp = `polygon(${points.join(', ')})`;
	}
	$effect(updateClipPath);
</script>

<div class="motor" style:clip-path={cp}></div>

<style>
	.motor {
		width: 100%;
		height: 100%;
		background-color: red;
		border-radius: 50%;
	}
</style>
