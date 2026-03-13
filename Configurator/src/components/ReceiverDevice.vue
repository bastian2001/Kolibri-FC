<script setup lang="ts">
import { addOnCommandHandler, removeOnCommandHandler, sendCommand, strToArray } from '@/msp/comm'
import { MspFn } from '@/msp/protocol'
import { Command, CrsfDevice } from '@/utils/types'
import { intToLeBytes, leBytesToInt } from '@/utils/utils'
import { onBeforeUnmount, onMounted, ref, watch } from 'vue'
import NumericInput from './NumericInput.vue'

type DataType = 'u8' | 'i8' | 'u16' | 'i16' | 'u32' | 'i32' | null | 'f32' | 'dropdown' | 'string' | 'folder' | 'info' | 'command'
const DATA_TYPE_LUT = ['u8', 'i8', 'u16', 'i16', 'u32', 'i32', null, null, 'f32', 'dropdown', 'string', 'folder', 'info', 'command'] as DataType[]
const DEVICE_TYPE_LUT = {
	0x00: 'Broadcast',
	0x0E: 'Cloud',
	0x10: 'USB Device',
	0x12: 'Bluetooth Module/WiFi',
	0x13: 'Wi-Fi receiver (mobile game/simulator)',
	0x14: 'Video Receiver',
	0x80: 'OSD / TBS CORE PNP PRO',
	0x8A: 'Reserved',
	0x90: 'ESC 1',
	0x91: 'ESC 2',
	0x92: 'ESC 3',
	0x93: 'ESC 4',
	0x94: 'ESC 5',
	0x95: 'ESC 6',
	0x96: 'ESC 7',
	0x97: 'ESC 8',
	0xB0: 'Crossfire reserved',
	0xB2: 'Crossfire reserved',
	0xC0: 'Voltage / Current Sensor',
	0xC2: 'GPS',
	0xC4: 'TBS Blackbox',
	0xC8: 'Flight Controller',
	0xCA: 'Reserved',
	0xCC: 'Race tag',
	0xCE: 'VTX',
	0xEA: 'Remote Control',
	0xEC: 'Receiver',
	0xEE: 'Transmitter Module',
	0xF0: 'reserved',
	0xF2: 'reserved'
} as { [key: number]: string }
type CrsfParam = {
	dataType: DataType
	hidden: boolean
	name: string
	options?: string[]
	value?: number | string
	min?: number
	max?: number
	default?: number
	decimals?: number
	step?: number
	unit?: string
	maxLength?: number
	children?: number[]
	parent: number
	info?: string
	cmdStatus?: number
	cmdTimeout?: number
	id: number
}

const props = defineProps<{
	dev: CrsfDevice,
	subbed: boolean
}>()

const emit = defineEmits(['sub', 'unsub'])

let paramPromiseResolve = (_: boolean) => { }
let paramPromiseReject = () => { }
let waitingForParam = -1

let stopFetching = () => { }

const paramBody = [] as number[]

const crsfParams = ref([] as CrsfParam[])

const onCommand = (c: Command) => {
	if (c.cmdType === 'request' && c.command === MspFn.CRSF_GOT_MESSAGE && c.length >= 2) {
		const payload = c.data.slice(2)
		const crsfAddress = c.data[0]
		if (crsfAddress !== props.dev.address) return
		const crsfFn = c.data[1]
		switch (crsfFn) {
			case 0x2B:
				if (waitingForParam === payload[0]) {
					const chunksRemaining = payload[1]
					paramBody.push(...payload.slice(2))
					if (chunksRemaining) {
						paramPromiseResolve(false)
						break
					}
					const b = paramBody
					const parent = b[0]
					const dataType = DATA_TYPE_LUT[b[1] & 0x7F]
					if (!dataType) {
						paramPromiseReject()
						break
					}
					const hidden = (b[1] >> 7) > 0
					const end = b.indexOf(0, 2)
					const name = String.fromCharCode(...b.slice(2, end))
					let rest = b.slice(end + 1)
					if (!rest.length) {
						paramPromiseReject()
						break
					}
					let fail = false;
					switch (dataType) {
						case 'u8':
						case 'i8':
						case 'u16':
						case 'i16':
						case 'u32':
						case 'i32':
						case 'f32': {
							const signed = !dataType.startsWith('u')
							const bytes = parseInt(dataType.substring(1)) / 8
							if (rest.length < 3 * bytes) {
								paramPromiseReject()
								fail = true
								break
							}
							const value = leBytesToInt(rest, 0, bytes, signed)
							const min = leBytesToInt(rest, bytes, bytes, signed)
							const max = leBytesToInt(rest, bytes * 2, bytes, signed)
							let precision = 0
							let step = 1
							rest = rest.slice(3 * bytes)
							if (dataType === 'f32') {
								if (rest.length < 5) {
									paramPromiseReject()
									fail = true
									break
								}
								precision = rest[0]
								step = leBytesToInt(rest, 1, 4, false)
								rest = rest.slice(5)
							}
							const unit = String.fromCharCode(...rest)
							crsfParams.value[waitingForParam] = {
								name,
								hidden,
								dataType,
								parent,
								id: waitingForParam,
								min,
								max,
								default: 0,
								value,
								decimals: precision,
								step,
								unit,
							}
						} break;
						case 'folder':
							crsfParams.value[waitingForParam] = {
								name,
								hidden,
								dataType,
								parent,
								id: waitingForParam,
								children: structuredClone(rest)
							}
							break;
						case 'info':
							crsfParams.value[waitingForParam] = {
								name,
								hidden,
								dataType,
								parent,
								id: waitingForParam,
								info: String.fromCharCode(...rest)
							}
							break;
						case 'dropdown': {
							const strLen = rest.indexOf(0)
							const str = String.fromCharCode(...rest.slice(0, strLen))
							rest = rest.slice(strLen + 1)
							console.log(str)
							crsfParams.value[waitingForParam] = {
								name,
								hidden,
								dataType,
								parent,
								id: waitingForParam,
								options: str.split(';'),
								value: rest[0],
								min: rest[1],
								max: rest[2],
								default: rest[3],
								unit: String.fromCharCode(...rest.slice(4))
							}
						} break;
						case 'command': {
							crsfParams.value[waitingForParam] = {
								name,
								hidden,
								dataType,
								parent,
								id: waitingForParam,
								cmdStatus: rest[0],
								cmdTimeout: rest[1],
								info: String.fromCharCode(...rest.slice(2))
							}
						} break;
						case 'string': {
							crsfParams.value[waitingForParam] = {
								name,
								hidden,
								dataType,
								parent,
								id: waitingForParam,
								maxLength: rest[rest.length - 1],
								value: String.fromCharCode(...rest.slice(0, rest.length - 1))
							}
						} break;
					}
					if (fail) break
					paramPromiseResolve(true)
				} else {
					paramPromiseReject()
				}
				break;
		}
	}
}

async function fetchParams(ids: number[]) {
	let stop = false
	stopFetching()
	stopFetching = () => {
		stop = true
		paramPromiseReject()
	}
	let chunk = 0;
	for (let i = 0; i < ids.length;) {
		// await delay(1000)
		if (stop) break;
		const id = ids[i]
		const prom = new Promise<boolean>((resolve, reject) => {
			setTimeout(reject, 4000)
			paramPromiseResolve = resolve
			paramPromiseReject = reject
			waitingForParam = id
			sendCommand(MspFn.CRSF_SEND_MESSAGE, [0x2C, props.dev.address, 0xC8, id, chunk])
		})
		try {
			const advance = await prom
			if (advance) {
				i++;
				chunk = 0;
				paramBody.length = 0
			}
			else {
				chunk++
			}
		} catch {
			continue
		}
	}
}

onMounted(() => {
	addOnCommandHandler(onCommand)

	watch(() => props.subbed, s => {
		if (s) {
			const ids: number[] = []
			crsfParams.value = []
			currentParent.value = 0
			for (let i = 0; i < props.dev.paramCount; i++) {
				ids[i] = i
				crsfParams.value[i] = {
					dataType: null,
					hidden: true,
					name: 'tset',
					parent: -1,
					id: -1,
				}
			}
			stopFetching()
			fetchParams(ids)
		} else {
			stopFetching()
		}
	}, { immediate: true })
})
onBeforeUnmount(() => {
	stopFetching()
	removeOnCommandHandler(onCommand)
})

function isNumberType(dataType: DataType) {
	return ['u8', 'i8', 'u16', 'i16', 'u32', 'i32', 'f32'].includes(dataType || '')
}

const currentParent = ref(0)

function getFolderChain(id: number): number[] {
	const res: number[] = []
	let p = id
	for (let i = 0; i < 5; i++) {
		if (!p) break;
		// up to 5 layers
		p = crsfParams.value[p].parent
		if (p > crsfParams.value.length || p < 0) break;
		res.push(p)
	}
	res.reverse()
	return res
}

function updateParam(id: number) {
	let type = crsfParams.value[id].dataType
	switch (type) {
		case 'u8':
		case 'i8':
		case 'u16':
		case 'i16':
		case 'u32':
		case 'i32':
		case 'f32':
		case 'dropdown': {
			if (type === 'dropdown') type = 'u8'
			const bytes = parseInt(type.substring(1)) / 8
			sendCommand(MspFn.CRSF_SEND_MESSAGE, [0x2D, props.dev.address, 0xC8, id, ...intToLeBytes(crsfParams.value[id].value! as number, bytes)])
		} break;
		case 'string':
			sendCommand(MspFn.CRSF_SEND_MESSAGE, [0x2D, props.dev.address, 0xC8, id, ...strToArray(crsfParams.value[id].value as string)])
			break;
		case 'command':
			sendCommand(MspFn.CRSF_SEND_MESSAGE, [0x2D, props.dev.address, 0xC8, id, 1])
			break;
	}
}

</script>

<template>
	<div class="wrapper" v-if="subbed">
		<template v-for="id in getFolderChain(currentParent)">
			<span @click="currentParent = id"
				style="cursor: pointer; background-color: var(--background-highlight); padding: 1px 3px; margin: 0px 3px; display: inline-block;">
				{{
					id === 0 ? dev.name : crsfParams[id].name
				}}
			</span>
			&nbsp;>&nbsp;
		</template>
		<span style="display: inline-block; padding: 1px 5px;">
			{{
				currentParent ? crsfParams[currentParent].name : dev.name
			}}
		</span>
		<table style="width: 100%">
			<template
				v-for="p in crsfParams.filter(p => !p.hidden && p.parent === currentParent && p.id !== currentParent)"
				:key="p.id">
				<tr class="number" v-if="isNumberType(p.dataType)">
					<td>{{ p.name }}</td>
					<td>
						<NumericInput v-model="(p.value as number)" :min="p.min" :max="p.max" :step="p.step"
							:io-divider="10 ** p.decimals!" :unit="p.unit" @change="updateParam(p.id)" />
					</td>
				</tr>
				<tr class="folder" v-if="p.dataType === 'folder'">
					<td colspan="2" @click="currentParent = p.id">{{ p.name }}</td>
				</tr>
				<tr class="dropdown" v-if="p.dataType === 'dropdown'">
					<td>{{ p.name }}</td>
					<td>
						<select name="" :id="'crsfSelect' + p.id" v-model="(p.value as number)" style="color:black"
							@change="updateParam(p.id)">
							<option
								v-for="o in p.options!.map((el, index) => ({ name: el, i: index })).filter((o, i) => i >= p.min! && i <= p.max! && o.name.length)"
								:key="o.i" :value="o.i">{{ o.name }}&thinsp;{{ p.unit || '' }}
							</option>
						</select>
					</td>
				</tr>
				<tr class="string" v-if="p.dataType === 'string'">
					<td>{{ p.name }}</td>
					<td><input type="text" v-model="(p.value as string)" @change="updateParam(p.id)"></td>
				</tr>
				<tr class="info" v-if="p.dataType === 'info'">
					<td>{{ p.name }}</td>
					<td>{{ p.info! }}</td>
				</tr>
				<tr class="command" v-if="p.dataType === 'command'">
					<td colspan="2" @click="updateParam(p.id)">
						{{ p.name }}
						{{ p.cmdStatus }}
						{{ p.info }}
					</td>
				</tr>
			</template>
		</table>
		<button @click="currentParent ? currentParent = 0 : emit('unsub')">Back</button>
	</div>
	<div class="wrapper" v-else>
		{{ DEVICE_TYPE_LUT[dev.address] || 'Unknown device type' }}<br>
		{{ dev.name }}<br>
		{{ String.fromCharCode(...dev.serialNo) }} (0x{{Array.from(dev.serialNo)
			.map(v => v.toString(16))
			.join('')}})<br>
		{{ dev.firmwareId.slice(1).join('.') }}<br>
		<button @click="emit('sub')">Settings</button>
	</div>
</template>

<style scoped>
.wrapper {
	box-sizing: border-box;
	background-color: var(--background-light);
	text-align: center;
	padding: 1rem;
	box-shadow: 0px 0px 0px 2px var(--border-color);
}

button {
	color: black
}

td {
	text-align: left;
}

td>select {
	width: 100%
}
</style>
