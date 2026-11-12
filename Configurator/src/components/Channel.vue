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
import { map, constrain } from "@utils/utils";

export default defineComponent({
	name: "RcChannel",
	props: {
		value: {
			type: Number,
			required: true,
		},
		title: {
			type: String,
			required: false,
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
			{{ (title + ': ' || '') + Math.round(value) }}
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
	font-size: 0.9rem;
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
