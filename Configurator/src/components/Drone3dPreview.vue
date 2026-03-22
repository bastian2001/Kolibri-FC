<script setup lang="ts">
defineProps<{
	roll: number,
	pitch: number,
	yaw: number,
	size?: number,
	hideBase?: boolean,
}>()
</script>

<template>
	<div class="drone3dPreview" :style="{
		width: (size || 500) + 'px',
		height: (size || 500) + 'px',
		perspective: ((size || 500) * 1.2) + 'px'
	}">
		<div class="droneBase droneAxes" :class="{ hidden: hideBase }" :style="{
			transform: `rotateX(60deg) translateZ(${(size || 500) * -180 / 500}px)`,
			border: (size || 500) / 100 + 'px solid black',
		}">
			<div class="zBox droneAxes"
				:style="`transform: rotateZ(${(yaw || 0)}deg) translateZ(${(size || 500) / 50}px)`">
				<div class="yBox droneAxes" :style="`transform: rotateX(${-(pitch || 0)}deg)`">
					<div class="xBox droneAxes" :style="`transform: rotateY(${(roll || 0)}deg)`">
						<div class="droneFrame">
							<div class="flrrBar"></div>
							<div class="rlfrBar"></div>
						</div>
						<div class="arrowForward" :style="{
							transform: 'translate3d(-50%, -50%, ' + (size || 500) * 0.02 + 'px)',
						}"></div>
						<div class="props">
							<div class="rear" :style="{
								transform: 'translate3d(193.3%, 193.3%, ' + (size || 500) * 0.03 + 'px)',
							}"></div>
							<div class="front" :style="{
								transform: 'translate3d(193.3%, 40%, ' + (size || 500) * 0.03 + 'px)',
							}"></div>
							<div class="rear" :style="{
								transform: 'translate3d(40%, 193.3%, ' + (size || 500) * 0.03 + 'px)',
							}"></div>
							<div class="front" :style="{
								transform: 'translate3d(40%, 40%, ' + (size || 500) * 0.03 + 'px)',
							}"></div>
						</div>
					</div>
				</div>
			</div>
		</div>
	</div>
</template>

<style scoped>
.drone3dPreview {
	background-color: rgba(255, 255, 255, 0.4);
	position: relative;
	display: inline-block;
}

.droneBase {
	border-radius: 50%;
	top: -34%;
	background-image: url("/src/assets/grid.svg");
	background-size: 35px 35px;
	box-sizing: border-box;
}

.hidden {
	border: none;
	background-image: none;
	border: none !important;
}

.droneAxes {
	width: 100%;
	height: 100%;
	position: relative;
	transform-style: preserve-3d;
}

.flrrBar,
.rlfrBar {
	width: 70%;
	height: 4%;
	background-color: #000;
	position: absolute;
	transform-style: preserve-3d;
	top: 50%;
	left: 50%;
}

.flrrBar {
	transform: translate(-50%, -50%) rotatez(-45deg);
}

.rlfrBar {
	transform: translate(-50%, -50%) rotatez(45deg);
}

.props {
	transform-style: preserve-3d;
	height: 100%;
	width: 100%;
}

.props div {
	width: 30%;
	height: 30%;
	transform-style: preserve-3d;
	position: absolute;
	border-radius: 100%;
}

.props .front {
	background-color: #282;
}

.props .rear {
	background-color: #d00;
}

.arrowForward {
	width: 20%;
	height: 10%;
	position: absolute;
	left: 50%;
	top: 30%;
	background-color: #000;
	clip-path: polygon(0 100%, 50% 0, 100% 100%);
}
</style>
