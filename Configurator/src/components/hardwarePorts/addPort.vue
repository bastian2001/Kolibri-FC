<script setup lang="ts">
import { useLogStore } from '@/stores/logStore';
import { usePortStore } from '@/stores/portStore';
import { computed, ref } from 'vue';

const ports = usePortStore()

const hdxPin = ref(-1)
const txPin = ref(-1)
const rxPin = ref(-1)
const newPortType = ref('none0')
const log = useLogStore()

const portNum = computed(() => {
	const str = newPortType.value.replace(/\D/g, '')
	try {
		return parseInt(str)
	} catch {
		return -1
	}
})
const portName = computed(() => {
	return newPortType.value.replace(/\d/g, '')
})

const pioPins = computed(() => {
	const pins: number[] = []
	const usedPins: number[] = []
	ports.serials.forEach(s => {
		if (!s.exists) return
		usedPins.push(s.txPin)
		usedPins.push(s.rxPin)
	})
	ports.pins.forEach((pin, index) => {
		if (!pin.allowed) return
		if (usedPins.includes(index)) return
		if (pin.pio) pins.push(index)
	})
	return pins
})
const txPins = computed(() => {
	const pins: number[] = []
	const usedPins: number[] = []
	ports.serials.forEach(s => {
		if (!s.exists) return
		usedPins.push(s.txPin)
		usedPins.push(s.rxPin)
	})
	ports.pins.forEach((pin, index) => {
		if (!pin.allowed) return
		if (usedPins.includes(index)) return
		if (pin.uartTx[portNum.value]) pins.push(index)
	})
	return pins
})
const rxPins = computed(() => {
	const pins: number[] = []
	const usedPins: number[] = []
	ports.serials.forEach(s => {
		if (!s.exists) return
		usedPins.push(s.txPin)
		usedPins.push(s.rxPin)
	})
	ports.pins.forEach((pin, index) => {
		if (!pin.allowed) return
		if (usedPins.includes(index)) return
		if (pin.uartRx[portNum.value]) pins.push(index)
	})
	return pins
})

function addPort2() {
	switch (portName.value) {
		case 'uart':
			if (!txPins.value.includes(txPin.value))
				return false
			if (!rxPins.value.includes(rxPin.value))
				return false
			return ports.addSerial('uart', txPin.value, rxPin.value, portNum.value)
		case 'pio':
			if (!pioPins.value.includes(txPin.value))
				return false
			if (!pioPins.value.includes(rxPin.value))
				return false
			return ports.addSerial('pio', txPin.value, rxPin.value)
		case 'pio-hdx':
			if (!pioPins.value.includes(hdxPin.value))
				return false
			return ports.addSerial('pio-hdx', txPin.value, rxPin.value)
		default:
			break;
	}
}
function addPort() {
	if (!addPort2()) log.push('failed to add serial')
	else {
		hdxPin.value = -1
		txPin.value = -1
		rxPin.value = -1
		newPortType.value = 'none0'
	}
}
</script>

<template>
	<!-- We do not want to destroy the component just because there is nothing to add left. This somehow leads to a death spiral where the available port types are continuously being recomputed -->
	<template v-if="ports.availablePortTypes.serials.length">
		<div>
			<div class="hardwareIcon"> </div>
			<h3>New Port</h3>
			<select name="portType" v-model="newPortType">
				<option value="none0" disabled="true">Choose Port Type</option>
				<option value="none1" disabled="true">UARTs</option>
				<option v-for="p in ports.availablePortTypes.serials" :value="p.type + p.hwParam">
					<template v-if="p.type === 'uart'">UART {{ p.hwParam + 1 }}</template>
					<template v-if="p.type === 'pio'">UART (PIO)</template>
					<template v-if="p.type === 'pio-hdx'">UART-Halfduplex (PIO)</template>
				</option>
			</select>
			<select name="hdxPin" v-if="newPortType.startsWith('pio-hdx')" v-model="hdxPin">
				<option disabled="true" value="-1">RX&TX Pin</option>
				<template v-for="p of pioPins">
					<option :value="p">{{ p }}</option>
				</template>
			</select>
			<select name="txPin" v-if="newPortType.startsWith('pio') && !newPortType.startsWith('pio-hdx')"
				v-model="txPin">
				<option disabled="true" value="-1">TX Pin</option>
				<template v-for="p of pioPins">
					<option :value="p">{{ p }}</option>
				</template>
			</select>
			<select name="rxPin" v-if="newPortType.startsWith('pio') && !newPortType.startsWith('pio-hdx')"
				v-model="rxPin">
				<option disabled="true" value="-1">RX Pin</option>
				<template v-for="p of pioPins">
					<option :value="p">{{ p }}</option>
				</template>
			</select>
			<select name="txPin" v-if="newPortType.startsWith('uart')" v-model="txPin">
				<option disabled="true" value="-1">TX Pin</option>
				<template v-for="p of txPins">
					<option :value="p">{{ p }}</option>
				</template>
			</select>
			<select name="rxPin" v-if="newPortType.startsWith('uart')" v-model="rxPin">
				<option disabled="true" value="-1">RX Pin</option>
				<template v-for="p of rxPins">
					<option :value="p">{{ p }}</option>
				</template>
			</select>
			<button @click="addPort">Add Port</button>
		</div>
	</template>
</template>

<style scoped>
select {
	color: black;
}

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

button {
	color: black
}
</style>
