import { LogData } from "./types"

export const leBytesToInt = (bytes: number[] | Uint8Array, signed = false) => {
	let value = 0
	for (let i = 0; i < bytes.length; i++) {
		value += bytes[i] * Math.pow(256, i)
	}
	if (signed && bytes[bytes.length - 1] & 0b10000000) {
		value -= Math.pow(256, bytes.length)
	}
	return value
}

export const intToLeBytes = (value: number, length: number) => {
	const bytes = new Uint8Array(length)
	const negative = value < 0
	if (negative) {
		value += Math.pow(256, length)
	}
	for (let i = 0; i < length; i++) {
		bytes[i] = value & 0xff
		value >>= 8
	}
	return bytes
}

export function map(value: number, inMin: number, inMax: number, outMin: number, outMax: number) {
	return ((value - inMin) * (outMax - outMin)) / (inMax - inMin) + outMin
}

export function getNestedProperty(
	obj: any,
	path: string,
	options: { defaultValue?: any; max?: number; min?: number } = {}
) {
	const pathParts = path.split(".")
	let current = obj
	for (let i = 0; i < pathParts.length; i++) {
		if (current[pathParts[i]] === undefined) return options.defaultValue
		current = current[pathParts[i]]
	}
	if (options.max !== undefined && current > options.max) return options.max
	if (options.min !== undefined && current < options.min) return options.min
	return current
}

export function roundToDecimal(num: number, places: number) {
	return Math.round(num * Math.pow(10, places)) / Math.pow(10, places)
}

export function constrain(num: number, min: number, max: number) {
	return Math.min(max, Math.max(min, num))
}

export function prefixZeros(num: number, totalDigits: number, char: string = "0") {
	let str = num.toString()
	while (str.length < totalDigits) str = char + str
	return str
}

export function delay(ms: number) {
	return new Promise(resolve => {
		setTimeout(resolve, ms)
	})
}

export const runAsync = () => Promise.resolve()

export function skipValues(slice: LogData, everyNth: number): LogData {
	const skipped: LogData = {}
	for (const key in slice) {
		// @ts-expect-error
		const input = slice[key] as LogDataType | number[]
		let output
		if (input instanceof Array) {
			output = input.filter((_, i) => i % everyNth === 0)
		} else if (input instanceof Uint8Array) {
			output = new Uint8Array(input.length / everyNth)
			for (let i = 0; i < output.length; i++) {
				output[i] = input[i * everyNth]
			}
		} else if (input instanceof Int8Array) {
			output = new Int8Array(input.length / everyNth)
			for (let i = 0; i < output.length; i++) {
				output[i] = input[i * everyNth]
			}
		} else if (input instanceof Uint16Array) {
			output = new Uint16Array(input.length / everyNth)
			for (let i = 0; i < output.length; i++) {
				output[i] = input[i * everyNth]
			}
		} else if (input instanceof Int16Array) {
			output = new Int16Array(input.length / everyNth)
			for (let i = 0; i < output.length; i++) {
				output[i] = input[i * everyNth]
			}
		} else if (input instanceof Uint32Array) {
			output = new Uint32Array(input.length / everyNth)
			for (let i = 0; i < output.length; i++) {
				output[i] = input[i * everyNth]
			}
		} else if (input instanceof Int32Array) {
			output = new Int32Array(input.length / everyNth)
			for (let i = 0; i < output.length; i++) {
				output[i] = input[i * everyNth]
			}
		} else if (input instanceof Float32Array) {
			output = new Float32Array(input.length / everyNth)
			for (let i = 0; i < output.length; i++) {
				output[i] = input[i * everyNth]
			}
		} else if (input instanceof Float64Array) {
			output = new Float64Array(input.length / everyNth)
			for (let i = 0; i < output.length; i++) {
				output[i] = input[i * everyNth]
			}
		} else {
			output = input
		}
		// @ts-expect-error
		skipped[key] = output
	}
	return skipped
}
