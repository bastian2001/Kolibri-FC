<script lang="ts">
import { defineComponent } from "vue";
import { map, constrain } from "@utils/utils";

export default defineComponent({
	name: "RcChannel",
	props: {
		value: {
			type: Number,
			required: true,
		}
	},
	data() {
		return {
			map,
			constrain,
		};
	},
});
</script>

<template>
	<div class="channelOuter">
		<div class="channelProgress" :style="`width: ${map(constrain(value, 800, 2200), 800, 2200, 0, 100)}%`"></div>
		<div class="channelValue">
			{{ Math.round(value) }}
		</div>
	</div>
</template>

<style scoped>
.channelOuter {
	height: 1.6rem;
	margin: 0.5rem 0px;
	position: relative;
	border-radius: 3px;
	width: 100%;
	border: .1rem solid white;
	background-color: #444;
	box-sizing: border-box;
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
