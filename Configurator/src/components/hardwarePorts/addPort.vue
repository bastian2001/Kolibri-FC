<script setup lang="ts">
import type { AvailablePortTypes } from '@/stores/portStore';

const props = defineProps<{
	ports: AvailablePortTypes,
}>()
</script>

<template>
	<div>
		<div class="hardwareIcon"> </div>
		<h3>New Port</h3>
		<select name="portType">
			<option value="none" disabled="true">Choose Port Type</option>
			<option v-for="p in ports.availableHwSerials">Serial {{ p + 1 }}</option>
			<option value=""></option>
			<option value="none" disabled="true">Already Used</option>
			<option v-for="p in ports.usedHwSerials" disabled="true">Serial {{ p + 1 }}</option>
		</select>
	</div>
</template>

<style scoped>
.hardwareIcon {
	-webkit-mask: url(@assets/add_serial_app.svg) no-repeat center;
	mask: url(@assets/add_serial_app.svg) no-repeat center;
}

h3 {
	margin-top: 6px;
	margin-bottom: 0px;
}

p {
	font-size: .85rem;
	margin-top: 6px;
}
</style>
