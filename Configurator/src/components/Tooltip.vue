<script setup lang="ts">
const props = defineProps<{
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
