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

<script setup lang="ts">
defineProps<{
	position: 'bottom' | 'top' | 'left' | 'right' | 'bottom-left' | 'bottom-right' | 'top-left' | 'top-right'
	width?: 's' | 'm' | 'l'
	tipStyle?: string
}>()
</script>

<template>
	<div class="infoWrapper">
		<div class="info" :class="{ [position]: true, [width || 'm']: true }" :style="tipStyle || ''">
			<slot></slot>
		</div>
		<p><i class="fa-solid fa-circle-info"></i></p>
	</div>
</template>

<style scoped>
.infoWrapper {
	position: relative;
	height: 100%;
	width: 100%;
	display: flex;
}

p {
	margin: auto auto;
	font-size: 1.5rem;
}

.s {
	width: 130px;
}

.m {
	width: 180px;
}

.l {
	width: 250px;
}

.info {
	position: absolute;
	opacity: 0;
	visibility: hidden;
	transition: all .1s ease-out;
	background-color: #fed;
	border: 1px solid black;
	border-radius: .3rem;
	padding: .3rem;
	color: black;
	white-space: preserve;
	z-index: 1;
	font-size: 0.7rem;
}

.infoWrapper:hover .info {
	visibility: visible;
	opacity: 1;
}

.infoWrapper:hover p {
	color: var(--accent-blue);
}

.bottom {
	top: 100%;
	left: 50%;
	transform: translateX(-50%);
}

.bottom-left {
	top: 100%;
	left: 100%;
	transform: translateX(-100%);
}
</style>
