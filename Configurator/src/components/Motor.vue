<script lang="ts">
import { defineComponent } from "vue";

export default defineComponent({
	name: "Motor",
	props: {
		throttlePct: Number
	},
	data() {
		return {
			cp: ''
		};
	},
	watch: {
		throttlePct(pct) {
			const points = ['50% 50%', '50% 0%'];
			if (pct < 12.5 || pct >= 87.5) {
				const y = '0%';
				const x = `${50 + Math.tan((pct / 100) * 2 * Math.PI) * 50}%`;
				if (pct >= 87.5) {
					points.push('100% 0%');
					points.push('100% 100%');
					points.push('0% 100%');
					points.push('0% 0%');
				}
				points.push(`${x} ${y}`);
			} else if (pct < 37.5) {
				const x = '100%';
				const y = `${50 + 50 * Math.tan(((pct - 25) / 100) * 2 * Math.PI)}%`;
				points.push('100% 0%');
				points.push(`${x} ${y}`);
			} else if (pct < 62.5) {
				const y = '100%';
				const x = `${50 - Math.tan(((pct - 50) / 100) * 2 * Math.PI) * 50}%`;
				points.push('100% 0%');
				points.push('100% 100%');
				points.push(`${x} ${y}`);
			} else {
				const x = '0%';
				const y = `${50 - 50 * Math.tan(((pct - 75) / 100) * 2 * Math.PI)}%`;
				points.push('100% 0%');
				points.push('100% 100%');
				points.push('0% 100%');
				points.push(`${x} ${y}`);
			}
			this.cp = `polygon(${points.join(', ')})`;
		}
	},
});
</script>

<template>
	<div class="motor" :style="`clip-path: ${cp}`"></div>
</template>

<style scoped>
.motor {
	width: 100%;
	height: 100%;
	background-color: red;
	border-radius: 50%;
}
</style>
