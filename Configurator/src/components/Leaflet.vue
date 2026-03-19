<template>
	<div ref="mapContainer" class="map-wrapper"></div>
</template>

<script setup>
import { onMounted, onBeforeUnmount, shallowRef, useTemplateRef, watch } from 'vue';
import L from 'leaflet';
// This import is crucial for the buttons to look right!
import 'leaflet/dist/leaflet.css';

const props = defineProps({
	lat: { type: Number, required: true },
	lng: { type: Number, required: true },
	zoom: { type: Number, default: 15 }
});

const mapContainer = useTemplateRef('mapContainer');
const map = shallowRef(null);
const marker = shallowRef(null);

// Function to update map position and marker
const updateMap = (lat, lng, zoom) => {
	if (map.value) {
		map.value.setView([lat, lng], zoom);

		// Update or create marker
		if (marker.value) {
			marker.value.setLatLng([lat, lng]);
		} else {
			marker.value = L.marker([lat, lng]).addTo(map.value);
		}
	}
};

onMounted(() => {
	if (!mapContainer.value) return;

	// Initialize Map
	map.value = L.map(mapContainer.value).setView([props.lat, props.lng], props.zoom);

	L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
		attribution: '&copy; OpenStreetMap'
	}).addTo(map.value);
	// Initial marker placement
	updateMap(props.lat, props.lng, props.zoom);
});

// Watch for prop changes
watch([() => props.lat, () => props.lng, () => props.zoom], ([newLat, newLng, newZoom]) => {
	updateMap(newLat, newLng, newZoom);
});

onBeforeUnmount(() => {
	if (map.value) map.value.remove();
});
</script>

<style scoped>
.map-wrapper {
	height: 100%;
	width: 100%;
}
</style>
