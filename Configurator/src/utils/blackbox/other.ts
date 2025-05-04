import { LogData, LogDataType } from "@utils/types"

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
