<script setup lang="ts">
import { onCommandHandler, sendCommand, strToArray } from '@/msp/comm'
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
				if (waitingForParam !== payload[0]) break; // do not reject here, as this might just be a still-in-flight packet when two updates are overlaying. Just wait for the original promise to time out

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
						if (pollingFor === waitingForParam && rest[0] !== 2) {
							clearInterval(pollCommandInterval)
							pollCommandInterval = -1
							pollingFor = -1
						}
						crsfParams.value[waitingForParam] = {
							name,
							hidden,
							dataType,
							parent,
							id: waitingForParam,
							cmdStatus: rest[0],
							cmdTimeout: rest[1] * 10,
							info: String.fromCharCode(...rest.slice(2))
						}
					} break;
					case 'string': {
						const strLen = rest.indexOf(0)
						const str = String.fromCharCode(...rest.slice(0, strLen))
						rest = rest.slice(strLen + 1)
						crsfParams.value[waitingForParam] = {
							name,
							hidden,
							dataType,
							parent,
							id: waitingForParam,
							maxLength: rest.length ? rest[0] : 16,
							value: str
						}
					} break;
				}
				if (!fail) paramPromiseResolve(true)
				break;
		}
	}
}

// set can be set to false, in case the configurator knows through another way that the parameter is coming
async function fetchParams(ids: number[], optimize = false, send = true) {
	let stop = false
	stopFetching()
	stopFetching = () => {
		stop = true
		paramPromiseReject()
	}
	let chunk = 0;
	let first = true
	for (let i = 0; i < ids.length;) {
		if (stop) break;
		if (optimize && i === 1) {
			const todo = ids.slice(1)
			const children = crsfParams.value[ids[0]]?.children
			if (children) {
				ids.length = 1
				todo.sort((a, b) => {
					if (children.includes(a) == children.includes(b)) return 0;
					if (children.includes(a)) return -1
					return 1
				})
				ids.push(...todo)
			}
		}
		const id = ids[i]
		const prom = new Promise<boolean>((resolve, reject) => {
			setTimeout(reject, 4000)
			paramPromiseResolve = resolve
			paramPromiseReject = reject
			if (first) {
				waitingForParam = id
				chunk = 0
				paramBody.length = 0
				first = false
			}
			if (send) sendCommand(MspFn.CRSF_SEND_MESSAGE, [0x2C, props.dev.address, 0xC8, id, chunk])
		})
		try {
			const advance = await prom
			if (advance) {
				i++
				first = true
			} else {
				chunk++
			}
		} catch {
			first = true
			continue
		}
	}
}

let pollCommandInterval = -1
let pollingFor = -1

onMounted(() => {
	onCommandHandler(onCommand)

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
			fetchParams(ids, true)
		} else {
			stopFetching()
		}
	}, { immediate: true })
})
onBeforeUnmount(() => {
	stopFetching()
	clearInterval(pollCommandInterval)
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

function updateParam(id: number, opt = 0) {
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
			sendCommand(MspFn.CRSF_SEND_MESSAGE, [0x2D, props.dev.address, 0xC8, id, ...strToArray((crsfParams.value[id].value as string).substring(0, (crsfParams.value[id].maxLength || 16) - 1)), 0])
			break;
		case 'command':
			sendCommand(MspFn.CRSF_SEND_MESSAGE, [0x2D, props.dev.address, 0xC8, id, opt])
			fetchParams([id], false, false)
			if (opt !== 6) {
				clearInterval(pollCommandInterval)
				pollCommandInterval = setInterval(() => {
					updateParam(id, 6)
				}, crsfParams.value[id]?.cmdTimeout || 2000)
				pollingFor = id
			}
			break;
	}
	if (type !== 'command') {
		const refreshOrder = [id]
		let last = id
		// go up the tree first (sort of DFS)
		for (let i = 0; i < 256; i++) {
			const next = crsfParams.value[last].parent
			if (!crsfParams.value[next] || refreshOrder.includes(next)) break;
			refreshOrder.push(next)
			last = next
			if (!next) break;
		}
		// then take children (sort of BFS)
		// because the length of the list changes, we iterate manually until the (potentially updated) length
		for (let i = 0; i < refreshOrder.length; i++) {
			const id = refreshOrder[i]
			if (crsfParams.value[id].dataType !== 'folder') continue
			// push all children of id
			crsfParams.value.forEach(el => {
				if (el.parent !== id) return
				if (refreshOrder.includes(el.id)) return
				refreshOrder.push(el.id)
			})
		}
		// then take any dangling elements
		// should only ever happen when there are broken params, such as when changing values during initial load
		// (or when carefully crafting the timing of updating values as a user)
		for (let i = 0; i < props.dev.paramCount; i++) {
			if (!refreshOrder.includes(i)) refreshOrder.push(i)
		}
		fetchParams(refreshOrder)
	}
}

</script>

<template>
	<div class="wrapper" v-if="subbed">
		<template v-for="id in getFolderChain(currentParent)">
			<span @click="currentParent = id" class="path clickable" tabindex="0">
				{{
					id === 0 ? dev.name : crsfParams[id].name
				}}
			</span>
			&nbsp;>&nbsp;
		</template>
		<span class="path">
			{{
				currentParent ? crsfParams[currentParent].name : dev.name
			}}
		</span>
		<table>
			<template
				v-for="p in crsfParams.filter(p => !p.hidden && p.parent === currentParent && p.id !== currentParent)"
				:key="p.id">
				<tr class="number" v-if="isNumberType(p.dataType)">
					<td class="pad">{{ p.name }}</td>
					<td>
						<NumericInput v-model="(p.value as number)" :min="p.min" :max="p.max" :step="p.step"
							:io-divider="10 ** p.decimals!" :unit="p.unit" @change="updateParam(p.id)" />
					</td>
				</tr>
				<tr class="folder" v-if="p.dataType === 'folder'">
					<td class="pad" colspan="2" @click="currentParent = p.id" tabindex="0">
						<i class="fa-regular fa-folder close"></i>
						<i class="fa-regular fa-folder-open open"></i>
						{{ p.name }}
					</td>
				</tr>
				<tr class="dropdown" v-if="p.dataType === 'dropdown'">
					<td class="pad">{{ p.name }}</td>
					<td>
						<select name="" :id="'crsfSelect' + p.id" v-model="(p.value as number)"
							@change="updateParam(p.id)">
							<option
								v-for="o in p.options!.map((el, index) => ({ name: el, i: index })).filter((o, i) => i >= p.min! && i <= p.max! && o.name.length)"
								:key="o.i" :value="o.i">{{ o.name + p.unit || '' }}
							</option>
						</select>
					</td>
				</tr>
				<tr class="string" v-if="p.dataType === 'string'">
					<td class="pad">{{ p.name }}</td>
					<td><input type="text" v-model="(p.value as string)" @change="updateParam(p.id)"
							:maxlength="p.maxLength! - 1"></td>
				</tr>
				<tr class="info" v-if="p.dataType === 'info'">
					<td class="pad">
						<i class="fa-solid fa-info"></i>
						{{ p.name }}
					</td>
					<td class="pad">{{ p.info! }}</td>
				</tr>
				<tr class="command" v-if="p.dataType === 'command'">
					<td class="pad" colspan="2" @click="updateParam(p.id, 1)" tabindex="0">
						<template v-if="p.cmdStatus === 0">
							<i class="fa-solid fa-forward"></i>
							{{ p.name }}
						</template>
						<template v-else-if="p.cmdStatus === 3">
							{{ p.info }}
							<button @click="updateParam(p.id, 4)">Yes</button>
							<button @click="updateParam(p.id, 5)">No</button>
						</template>
						<template v-else>
							{{ p.info }}
							<button @click="updateParam(p.id, 5)">Stop</button>
						</template>
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

.path {
	display: inline-block;
	padding: 1px 5px;
}

.path.clickable {
	cursor: pointer;
	background-color: var(--background-highlight);
	padding: 1px 3px;
	margin: 0px 3px;
}

table {
	width: 100%;
}

button {
	color: black
}

td {
	text-align: left;
}

td.pad {
	padding: 2px 5px
}

.folder>td,
.command>td {
	cursor: pointer;
	transition: 0.2s ease-out
}

.folder>td:hover,
.command>td:hover {
	background-color: var(--background-highlight);
}

.folder>td>.open {
	display: none;
	width: 19px;
}

.folder>td:hover>.open {
	display: inline-block;
}

.folder>td:hover>.close {
	display: none;
}

.folder>td>.close {
	width: 19px;
}

.info>td>i {
	width: 19px;
	transform: translate(5px) scale(0.9);
}

td>select {
	width: 100%;
	color: black;
}

.command>td>i {
	width: 19px;
	transform: scale(0.8);
	transition: 0.2s ease-out;
}

.command>td:hover>i {
	transform: scale(0.8) translateX(5px);
}
</style>
