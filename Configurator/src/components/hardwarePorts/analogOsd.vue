<script setup lang="ts">
import { onBeforeUnmount } from 'vue';

// shows analog OSD state
// - OSD type (MAX/AT or pixel)
// - OSD chip detected / setup success
// - detected video type
const fetchInterval = setInterval(fetchState, 1000)
fetchState()
function fetchState() {

}

onBeforeUnmount(() => {
	clearInterval(fetchInterval)
})


</script>

<template>
	<div>

	</div>
</template>

<style scoped></style>
