import { ActualCoeffs } from "./types"

export const leBytesToInt = (bytes: number[] | Uint8Array, signed = false) => {
	let value = 0
	let mul = 1
	for (let i = 0; i < bytes.length; i++) {
		value += bytes[i] * mul
		mul *= 256
	}
	if (signed && bytes[bytes.length - 1] & 0b10000000) {
		value -= mul
	}
	return value
}

export const leBytesToBigInt = (bytes: number[] | Uint8Array, signed = false) => {
	let value = 0n
	let mul = 1n
	for (let i = 0; i < bytes.length; i++) {
		value += BigInt(bytes[i]) * mul
		mul *= 256n
	}
	if (signed && bytes[bytes.length - 1] & 0b10000000) {
		value -= mul
	}
	return value
}

export const intToLeBytes = (value: number, length: number) => {
	const bytes = new Uint8Array(length)
	value = Math.round(value)
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

export function roundToDecimal(num: number, decimals: number) {
	return parseFloat(num.toFixed(decimals))
}

export function constrain(num: number, min: number, max: number) {
	return Math.min(max, Math.max(min, num))
}

export function prefixZeros(num: number | string, totalDigits: number, char: string = "0") {
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

export function getSetpointActual(stickPos: number, coeffs: ActualCoeffs): number {
	if (stickPos >= 1) stickPos = 1
	if (stickPos <= -1) stickPos = -1
	const linPart = stickPos * coeffs.center
	const expoPart = coeffs.expo * Math.pow(stickPos, 6) + (1 - coeffs.expo) * Math.pow(stickPos, 2)
	return linPart + expoPart * Math.sign(stickPos) * (coeffs.max - coeffs.center)
}
