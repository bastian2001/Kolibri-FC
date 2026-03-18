import { defineStore } from "pinia"
import { computed, ref } from "vue"

export type AvailablePortTypes = {
	serials: {
		type: SerialType
		hwParam: number
	}[]
}
export type SerialType = "usb" | "uart" | "pio" | "pio-hdx" | "disabled" | "invalid"
export type SerialPort = {
	exists: boolean
	type: SerialType
	baud: number
	baudSet: number
	txPin: number
	rxPin: number
	functions: number
	no: number
	hwParam: number
	modified: boolean
}

export const usePortStore = defineStore("port", () => {
	const pioSetup = ref({
		version: 0,
		numPios: 0,
		hdxLength: 0,
		txLength: 0,
		rxLength: 0,
		freeSms: [] as number[],
		freeInstructions: [] as bigint[],
	})
	const serials = ref([] as SerialPort[])
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

	const reset = () => {
		serials.value.length = 0
		maxSerials.value = 0
		hwSerials.value = 0
		pins.value.length = 0
		pioSetup.value.freeInstructions.length = 0
		pioSetup.value.freeSms.length = 0
		pioSetup.value.version = 0
		pioSetup.value.numPios = 0
		pioSetup.value.hdxLength = 0
		pioSetup.value.txLength = 0
		pioSetup.value.rxLength = 0
	}

	const availablePortTypes = computed(() => {
		const ret: AvailablePortTypes = { serials: [] }
		// add hardware serials
		for (let i = 0; i < hwSerials.value; i++) {
			if (serials.value.filter(el => el.type === "uart" && el.hwParam === i).length === 0) {
				ret.serials.push({ type: "uart", hwParam: i })
			}
		}

		// add PIO serial and/or PIO HDx
		let currentPioSerials = 0
		serials.value.forEach(s => {
			if (s.exists && s.type !== "uart" && s.type !== "usb") currentPioSerials++
		})
		if (currentPioSerials < maxSerials.value - hwSerials.value) {
			const tasksCur = [] as {
				forSerial: number
				program: number
				isTx?: boolean
			}[]
			const input: AllocationRequest = {
				programs: [pioSetup.value.txLength, pioSetup.value.rxLength, pioSetup.value.hdxLength],
				freeSms: pioSetup.value.freeSms,
				freeInstructions: pioSetup.value.freeInstructions,
				tasks: [],
			}
			serials.value.forEach((s, i) => {
				if (s.type === "pio") {
					tasksCur.push({ forSerial: i, program: 0, isTx: true }, { forSerial: i, program: 1, isTx: false })
				}
				if (s.type === "pio-hdx") {
					tasksCur.push({ forSerial: i, program: 2 })
				}
			})
			// we got the current setup in tasks, and want to add an RX/TX pair now to test out whether we have capacity for it
			try {
				const tasks = structuredClone(tasksCur)
				tasks.push(
					{ forSerial: serials.value.length, program: 0, isTx: true },
					{ forSerial: serials.value.length, program: 1, isTx: false },
				)
				input.tasks = tasks.map(el => el.program)
				const distributor = new PioTaskDistributor(input)
				const result = distributor.distribute()
				// if distribute throws an error, the following part is not reached, so we can add a pio serial to the allowlist now
				ret.serials.push({ type: "pio", hwParam: 0 }) // dummy hwParam, as it is not used here
				console.log("Task Distribution with a sample pio uart (Index = Task, Value = PIO Block):")
				result.forEach((block, task) => {
					console.log(`Task ${task} (Program ${input.tasks[task]}) -> PIO Block ${block}`)
				})
			} catch {}
			try {
				const tasks = structuredClone(tasksCur)
				tasks.push({ forSerial: serials.value.length, program: 2 })
				input.tasks = tasks.map(el => el.program)
				const distributor = new PioTaskDistributor(input)
				const result = distributor.distribute()
				ret.serials.push({ type: "pio-hdx", hwParam: 0 })
				console.log("Task Distribution with a sample pio hdx (Index = Task, Value = PIO Block):")
				result.forEach((block, task) => {
					console.log(`Task ${task} (Program ${input.tasks[task]}) -> PIO Block ${block}`)
				})
			} catch {}
		}
		return ret
	})
	const addSerial = (type: SerialType, tx: number, rx: number, serialNum = -1) => {
		if (type === "pio" || type === "pio-hdx") {
			let newSerialNum = -1
			serials.value.forEach((s, i) => {
				if (!s.exists && i >= hwSerials.value) newSerialNum = i
			})
			if (newSerialNum === -1) return false
			if (type === "pio-hdx") rx = tx
			const tasks = [] as {
				forSerial: number
				program: number
				isTx?: boolean
			}[]
			const input: AllocationRequest = {
				programs: [pioSetup.value.txLength, pioSetup.value.rxLength, pioSetup.value.hdxLength],
				freeSms: pioSetup.value.freeSms,
				freeInstructions: pioSetup.value.freeInstructions,
				tasks: [],
			}
			serials.value.forEach((s, i) => {
				if (s.type === "pio") {
					tasks.push({ forSerial: i, program: 0, isTx: true }, { forSerial: i, program: 1, isTx: false })
				}
				if (s.type === "pio-hdx") {
					tasks.push({ forSerial: i, program: 2 })
				}
			})
			try {
				if (type === "pio")
					tasks.push(
						{ forSerial: newSerialNum, program: 0, isTx: true },
						{ forSerial: newSerialNum, program: 1, isTx: false },
					)
				if (type === "pio-hdx") tasks.push({ forSerial: newSerialNum, program: 2 })
				input.tasks = tasks.map(el => el.program)
				const distributor = new PioTaskDistributor(input)
				const result = distributor.distribute()
				serials.value[newSerialNum] = {
					baud: 115200,
					baudSet: 0,
					exists: true,
					functions: 0,
					hwParam: 0,
					modified: true,
					no: newSerialNum,
					rxPin: rx,
					txPin: tx,
					type: type,
				}
				serials.value.forEach(s => {
					if (s.type === "pio" || s.type === "pio-hdx") s.hwParam = 0xff
				})
				result.forEach((pioNum, taskIndex) => {
					const task = tasks[taskIndex]
					const serialNum = task.forSerial
					serials.value[serialNum].modified = true
					if (task.isTx !== undefined && task.isTx) serials.value[serialNum].hwParam &= 0xf0 | pioNum
					if (task.isTx !== undefined && !task.isTx) serials.value[serialNum].hwParam &= 0x0f | (pioNum << 4)
					if (task.isTx === undefined) serials.value[serialNum].hwParam = pioNum
				})
				return true
			} catch {
				console.log("could not create PIO serial")
				return false
			}
		} else if (type === "uart") {
			console.log("uart", serialNum, tx, rx)
			if (serialNum < 0 || serialNum >= hwSerials.value) return false
			console.log(5)
			if (serials.value[serialNum].exists) return false
			console.log(2)
			if (!pins.value[tx].uartTx[serialNum]) return false
			console.log(3)
			if (!pins.value[rx].uartRx[serialNum]) return false
			console.log(4)
			const s = serials.value[serialNum]
			s.baud = 115200
			s.baudSet = 0
			s.exists = true
			s.functions = 0
			s.hwParam = serialNum
			s.modified = true
			s.no = serialNum
			s.rxPin = rx
			s.txPin = tx
			s.type = "uart"
			return true
		}
		return false
	}
	return { pioSetup, serials, maxSerials, hwSerials, pins, availablePortTypes, addSerial, reset }
})

// Yes, the following is written by Gemini
type PioProgram = number // Length (number of instructions)

interface AllocationRequest {
	programs: PioProgram[]
	freeSms: number[] // Bitmasks (e.g., 0b1111)
	freeInstructions: bigint[] // Bitmasks (e.g., 0x0000FFFFn)
	tasks: number[] // Indices of programs needed
}

class PioTaskDistributor {
	private programLengths: number[]
	private taskProgramIndices: number[]

	// Resources per PIO block
	private smCounts: number[]
	private instrCapacities: number[]

	// Tracking state for backtracking
	private loadedPrograms: Set<number>[]

	constructor(req: AllocationRequest) {
		this.programLengths = req.programs
		this.taskProgramIndices = req.tasks

		// Convert bitmasks to simple capacity counts
		this.smCounts = req.freeSms.map(mask => this.countSetBits(BigInt(mask)))
		this.instrCapacities = req.freeInstructions.map(mask => this.countSetBits(mask))

		// Initialize an empty set of loaded programs for each PIO block
		this.loadedPrograms = req.freeSms.map(() => new Set<number>())
	}

	private countSetBits(n: bigint): number {
		let count = 0
		let temp = n
		while (temp > 0n) {
			temp &= temp - 1n
			count++
		}
		return count
	}

	public distribute(): number[] {
		const solution: number[] = new Array(this.taskProgramIndices.length).fill(-1)

		if (this.backtrack(0, solution)) {
			return solution
		}

		throw new Error("No valid combination exists for the given PIO constraints.")
	}

	private backtrack(taskIdx: number, solution: number[]): boolean {
		// Base Case: All tasks assigned
		if (taskIdx === this.taskProgramIndices.length) return true

		const progIdx = this.taskProgramIndices[taskIdx]
		const progLen = this.programLengths[progIdx]

		// Try placing this task on each PIO block (RP2350 usually has 3)
		for (let blockIdx = 0; blockIdx < this.smCounts.length; blockIdx++) {
			const alreadyLoaded = this.loadedPrograms[blockIdx].has(progIdx)
			const cost = alreadyLoaded ? 0 : progLen

			// Check if we have an available State Machine and enough instruction space
			if (this.smCounts[blockIdx] > 0 && this.instrCapacities[blockIdx] >= cost) {
				// --- Apply Selection ---
				this.smCounts[blockIdx]--
				this.instrCapacities[blockIdx] -= cost
				const wasNewlyAdded = !alreadyLoaded
				if (wasNewlyAdded) this.loadedPrograms[blockIdx].add(progIdx)
				solution[taskIdx] = blockIdx

				// --- Recurse ---
				if (this.backtrack(taskIdx + 1, solution)) return true

				// --- Backtrack (Revert) ---
				solution[taskIdx] = -1
				if (wasNewlyAdded) this.loadedPrograms[blockIdx].delete(progIdx)
				this.instrCapacities[blockIdx] += cost
				this.smCounts[blockIdx]++
			}
		}

		return false
	}
}
