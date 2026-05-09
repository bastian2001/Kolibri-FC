/*
 * Copyright (c) 2026 Kolibri-FC contributors
 * 
 * This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 * 
 * Kolibri-FC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Kolibri-FC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
 */

import Mexp from "math-expression-evaluator"
import { ActualCoeffs } from "./types"

export const leBytesToInt = (bytes: number[] | Uint8Array, start: number, length: number, signed = false) => {
	let value = 0
	const end = start + length
	let mul = 1

	for (let i = start; i < end; i++) {
		value += bytes[i] * mul
		mul *= 256
	}
	if (signed && bytes[end - 1] & 0b10000000) {
		value -= mul
	}
	return value
}

export const leBytesToBigInt = (bytes: number[] | Uint8Array, start: number, length: number, signed = false) => {
	let value = 0n
	const end = start + length
	let shift = 0n

	for (let i = start; i < end; i++) {
		value |= BigInt(bytes[i]) << shift
		shift += 8n
	}
	if (signed) {
		value = BigInt.asIntN(Number(shift), value)
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

export const bigIntToLeBytes = (value: bigint, length: number) => {
	const bytes = new Uint8Array(length)
	for (let i = 0; i < length; i++) {
		bytes[i] = Number(value & 0xffn)
		value >>= 8n
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

export function mathEval(str: string): number {
	const mexp = new Mexp()
	str = str.replace(/,/g, ".")
	str = str.replace(/;/g, ",")
	return mexp.eval(str)
}

export function uint8ArrayEquals(a: Uint8Array, b: Uint8Array): boolean {
	if (a.length !== b.length) return false

	for (let i = 0; i < a.length; i++) {
		if (a[i] !== b[i]) return false
	}
	return true
}
