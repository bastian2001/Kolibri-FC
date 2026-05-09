<!--
 + Copyright (c) 2026 Kolibri-FC contributors
 + 
 + This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 + 
 + Kolibri-FC is free software: you can redistribute it and/or modify
 + it under the terms of the GNU General Public License as published by
 + the Free Software Foundation, either version 3 of the License, or
 + (at your option) any later version.
 + 
 + Kolibri-FC is distributed in the hope that it will be useful,
 + but WITHOUT ANY WARRANTY; without even the implied warranty of
 + MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 + GNU General Public License for more details.
 + 
 + You should have received a copy of the GNU General Public License
 + along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
-->

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
	mounted() {
		this.onThrottleChange(this.throttlePct || 0);
	},
	methods: {
		onThrottleChange(pct: number) {
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
	watch: {
		throttlePct: {
			immediate: true,
			handler(newVal) {
				this.onThrottleChange(newVal);
			}
		}
	},
});
</script>

<template>
	<div class="wrapper">
		<div class="motor" :style="`clip-path: ${cp}`"></div>
	</div>
</template>

<style scoped>
.wrapper {
	border: 2px solid white;
	width: 100%;
	height: 100%;
	border-radius: 50%;
}

.motor {
	width: 100%;
	height: 100%;
	background-color: red;
	border-radius: 50%;
}
</style>
