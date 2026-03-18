import { defineStore } from "pinia"
import { ref } from "vue"

export type AvailablePortTypes = {
	usedHwSerials: number[]
	availableHwSerials: number[]
}
export type SerialType = "usb" | "uart" | "pio" | "pio-hdx" | "disabled" | "invalid"

export const usePortStore = defineStore("port", () => {
	const pioSetup = ref({
		version: 0,
		numPios: 0,
		hdxLength: 0,
		txLength: 0,
		rxLength: 0,
		freeSms: [] as number[],
		freeInstructions: [] as BigInt[],
	})
	const serials = ref(
		[] as {
			exists: boolean
			type: SerialType
			running: boolean
			baud: number
			baudSet: number
			txPin: number
			rxPin: number
			functions: number
			no: number
			hwParam: number
		}[],
	)
	const maxSerials = ref(0)
	const hwSerials = ref(0)

	const pins = ref(
		[] as {
			hstx: boolean
			spi: boolean
			uartTx: boolean[]
			uartRx: boolean[]
			sda: boolean[]
			scl: boolean[]
			pio: boolean[]
			recommended: boolean
			allowed: boolean
			pads: {
				x: number
				y: number
				width: number
				height: number
				label: string
				shape: "oval" | "rect"
			}[]
		}[],
	)

	const getAvailablePortTypes = () => {
		const ret: AvailablePortTypes = {
			usedHwSerials: [],
			availableHwSerials: [],
		}
		for (let i = 0; i < hwSerials.value; i++) {
			if (
				serials.value.reduce((acc, s) => {
					if (s.type === "uart" && s.hwParam === i) return acc
					else return true
				}, false)
			) {
				ret.usedHwSerials.push(i)
			} else {
				ret.availableHwSerials.push(i)
			}
		}
		return ret
	}
	const addSerial = (type: SerialType, tx: number, rx: number) => {
		return true
	}
	return { pioSetup, serials, maxSerials, hwSerials, pins, getAvailablePortTypes }
})
