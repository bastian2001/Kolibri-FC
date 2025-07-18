import { invoke } from "@tauri-apps/api/core"
import { MspFn, MspVersion } from "@/msp/protocol"
import { Command } from "@utils/types"
import { useLogStore } from "@stores/logStore"
import { runAsync } from "@utils/utils"

const MspState = {
	IDLE: 0, // waiting for $
	PACKET_START: 1, // receiving M or X
	TYPE_V1: 2, // got M, receiving type byte (<, >, !)
	LEN_V1: 3, // if 255 is received in this step, inject jumbo len bytes
	CMD_V1: 4,
	JUMBO_LEN_LO_V1: 5,
	JUMBO_LEN_HI_V1: 6,
	PAYLOAD_V1: 7,
	FLAG_V2_OVER_V1: 8,
	CMD_LO_V2_OVER_V1: 9,
	CMD_HI_V2_OVER_V1: 10,
	LEN_LO_V2_OVER_V1: 11,
	LEN_HI_V2_OVER_V1: 12,
	PAYLOAD_V2_OVER_V1: 13,
	CHECKSUM_V2_OVER_V1: 14,
	CHECKSUM_V1: 15,
	TYPE_V2: 16, // got X, receiving type byte (<, >, !)
	FLAG_V2: 17,
	CMD_LO_V2: 18,
	CMD_HI_V2: 19,
	LEN_LO_V2: 20,
	LEN_HI_V2: 21,
	PAYLOAD_V2: 22,
	CHECKSUM_V2: 23,
}

let configuratorLog
runAsync().then(() => {
	configuratorLog = useLogStore() // defer calling this until after pinia is ready
})

let command: Command = {
	command: NaN,
	length: 0,
	data: new Uint8Array(),
	dataStr: "",
	cmdType: "request",
	flag: 0,
	version: MspVersion.V2,
}

let connectType = "none" as "none" | "serial" | "tcp"

const commandHandlers: ((command: Command) => void)[] = []

export const setCommand = (cmd: Command) => {
	command.command = cmd.command
	command.length = cmd.length
	command.data = cmd.data
	command.dataStr = cmd.dataStr
	command.cmdType = cmd.cmdType
	command.flag = cmd.flag
	command.version = cmd.version

	commandHandlers.forEach(handler => handler(command))
}

export const getCommand = () => {
	return command
}

export const addOnCommandHandler = (handler: (command: Command) => void) => {
	commandHandlers.push(handler)
}

export const removeOnCommandHandler = (handler: (command: Command) => void) => {
	const i = commandHandlers.indexOf(handler)
	if (i > -1) commandHandlers.splice(i, 1)
}

let fcPing = -1

export const getPingTime = () => {
	return fcPing
}

addOnCommandHandler((c: Command) => {
	if (c.cmdType === "error") {
		console.error(structuredClone(c))
		configuratorLog?.push("Error, see console for details.")
	}
	if (c.cmdType === "response") {
		switch (c.command) {
			case MspFn.CONFIGURATOR_PING:
				// pong response from FC received
				fcPing = Date.now() - pingStarted
				break
		}
	}
	if (!Object.values(MspFn).includes(c.command) && !Number.isNaN(c.command)) {
		console.log(structuredClone(c))
		configuratorLog?.push("Unsupported command, see console for details.")
	}
})

function strToArray(str: string) {
	const data = []
	for (let i = 0; i < str.length; i++) {
		data[i] = str.charCodeAt(i)
	}
	return data
}
function charToInt(c: string) {
	return c.charCodeAt(0)
}
let cmdEnabled = true
export const enableCommands = (en: boolean) => {
	cmdEnabled = en
}

export function sendCommand(
	command: number,
	d:
		| string
		| number[]
		| {
				data?: string | number[]
				version?: number
				type?: "request" | "response" | "error"
		  } = []
) {
	if (!cmdEnabled) return new Promise((resolve: any) => resolve())
	let data: number[] = []
	let version = MspVersion.V2
	let type = "request" as "request" | "response" | "error"
	if (typeof d === "string") {
		data = strToArray(d)
	} else if (Array.isArray(d)) {
		data = d
	} else {
		if (typeof d.data === "string") {
			data = strToArray(d.data)
		} else if (Array.isArray(d.data)) {
			data = d.data
		}
		if (d.version !== undefined) {
			version = d.version
		}
		if (d.type !== undefined) {
			type = d.type
		}
	}
	const len = data.length

	if (len > 254 && version === MspVersion.V1) version = MspVersion.V1_JUMBO

	const typeLut = {
		request: 60,
		response: 62,
		error: 33,
	}
	const cmd = [charToInt("$"), charToInt(version === MspVersion.V2 ? "X" : "M"), typeLut[type]]
	switch (version) {
		case MspVersion.V1_JUMBO:
			cmd.push(0xff)
			cmd.push(command & 0xff)
			cmd.push(len & 0xff, len >> 8)
			break
		case MspVersion.V1:
			cmd.push(len & 0xff)
			cmd.push(command & 0xff)
			break
		case MspVersion.V2:
			cmd.push(0)
			cmd.push(command & 0xff, command >> 8)
			cmd.push(len & 0xff, len >> 8)
			break
		case MspVersion.V2_OVER_V1:
			cmd.push((len + 6) & 0xff) // V1 len
			cmd.push(0xff) //  V2 trigger
			cmd.push(0) // V2 flag
			cmd.push(command & 0xff, command >> 8) // V2 command
			cmd.push(len & 0xff, len >> 8) // V2 len
			break
	}
	cmd.push(...data)
	const crcV2StartLut = {
		[MspVersion.V2]: 3,
		[MspVersion.V2_OVER_V1]: 5,
		[MspVersion.V1]: 0x7fffffff, // such that slice returns empty array
		[MspVersion.V1_JUMBO]: 0x7fffffff, // such that slice returns empty array
	}

	let checksumV1 = cmd.slice(3).reduce((a, b) => a ^ b, 0),
		checksumV2 = cmd.slice(crcV2StartLut[version]).reduce(crc8DvbS2, 0)
	if ([MspVersion.V2, MspVersion.V2_OVER_V1].includes(version)) {
		cmd.push(checksumV2)
		checksumV1 ^= checksumV2
	}
	if ([MspVersion.V1, MspVersion.V1_JUMBO, MspVersion.V2_OVER_V1].includes(version)) {
		cmd.push(checksumV1)
	}
	if (connectType === "serial") {
		return new Promise((resolve, reject) => {
			invoke("serial_write", { data: cmd }).then(resolve).catch(reject)
		})
	} else if (connectType === "tcp") {
		return new Promise((resolve, reject) => {
			invoke("tcp_write", { data: cmd }).then(resolve).catch(reject)
		})
	} else {
		return new Promise((resolve: any) => resolve())
	}
}
export const sendRaw = (data: number[], dataStr: string = "") => {
	if (data.length === 0 && dataStr !== "") data = strToArray(dataStr)
	if (connectType === "serial") {
		return new Promise((resolve, reject) => {
			invoke("serial_write", { data }).then(resolve).catch(reject)
		})
	} else if (connectType === "tcp") {
		return new Promise((resolve, reject) => {
			invoke("tcp_write", { data }).then(resolve).catch(reject)
		})
	} else {
		return new Promise((resolve: any) => resolve())
	}
}
let pingStarted = 0
let newCommand: Command = {
	command: 65535,
	length: 0,
	data: new Uint8Array(),
	dataStr: "",
	cmdType: "request",
	flag: 0,
	version: MspVersion.V2,
}
let mspState = 0
let checksumV1 = 0,
	checksumV2 = 0
function crc8DvbS2(crc: number, data: number): number {
	crc ^= data
	for (let i = 0; i < 8; i++) {
		if (crc & 0x80) {
			crc = (crc << 1) ^ 0xd5
		} else {
			crc <<= 1
		}
	}
	return crc & 0xff
}
let receivedBytes = 0

function handleRead(rxBuf: number[]) {
	rxBuf.forEach(c => {
		switch (mspState) {
			case MspState.IDLE:
				if (c === 36) mspState = MspState.PACKET_START /* $ */
				break
			case MspState.PACKET_START:
				receivedBytes = 0
				newCommand.dataStr = ""
				if (c === 77) mspState = MspState.TYPE_V1 /* M */
				else if (c === 88) mspState = MspState.TYPE_V2 /* X */
				else mspState = MspState.IDLE
				break
			case MspState.TYPE_V1:
				mspState = MspState.LEN_V1
				checksumV1 = 0
				newCommand.version = MspVersion.V1
				switch (c) {
					case 60: // '<'
						newCommand.cmdType = "request"
						break
					case 62: // '>'
						newCommand.cmdType = "response"
						break
					case 33: // '!'
						newCommand.cmdType = "error"
						break
					default:
						mspState = MspState.IDLE
						break
				}
				break
			case MspState.LEN_V1:
				checksumV1 ^= c
				newCommand.length = c
				mspState = MspState.CMD_V1
				break
			case MspState.CMD_V1:
				checksumV1 ^= c
				newCommand.command = c

				if (newCommand.length === 255) {
					newCommand.version = MspVersion.V1_JUMBO
					mspState = MspState.JUMBO_LEN_LO_V1
				} else if (c === 255) {
					checksumV2 = 0
					newCommand.version = MspVersion.V2_OVER_V1
					mspState = MspState.FLAG_V2_OVER_V1
				} else {
					newCommand.data = new Uint8Array(newCommand.length)
					mspState = newCommand.length > 0 ? MspState.PAYLOAD_V1 : MspState.CHECKSUM_V1
				}
				break
			case MspState.JUMBO_LEN_LO_V1:
				checksumV1 ^= c
				newCommand.length = c
				mspState = MspState.JUMBO_LEN_HI_V1
				break
			case MspState.JUMBO_LEN_HI_V1:
				checksumV1 ^= c
				newCommand.length += c << 8
				newCommand.data = new Uint8Array(newCommand.length)
				mspState = newCommand.length > 0 ? MspState.PAYLOAD_V1 : MspState.CHECKSUM_V1
				break
			case MspState.PAYLOAD_V1:
				newCommand.data[receivedBytes++] = c
				newCommand.dataStr += String.fromCharCode(c)
				checksumV1 ^= c
				if (receivedBytes === newCommand.length) mspState = MspState.CHECKSUM_V1
				break
			case MspState.FLAG_V2_OVER_V1:
				newCommand.flag = c
				checksumV1 ^= c
				checksumV2 = crc8DvbS2(checksumV2, c)
				mspState = MspState.CMD_LO_V2_OVER_V1
				break
			case MspState.CMD_LO_V2_OVER_V1:
				newCommand.command = c
				checksumV1 ^= c
				checksumV2 = crc8DvbS2(checksumV2, c)
				mspState = MspState.CMD_HI_V2_OVER_V1
				break
			case MspState.CMD_HI_V2_OVER_V1:
				newCommand.command += c << 8
				checksumV1 ^= c
				checksumV2 = crc8DvbS2(checksumV2, c)
				mspState = MspState.LEN_LO_V2_OVER_V1
				break
			case MspState.LEN_LO_V2_OVER_V1:
				newCommand.length = c
				checksumV1 ^= c
				checksumV2 = crc8DvbS2(checksumV2, c)
				mspState = MspState.LEN_HI_V2_OVER_V1
				break
			case MspState.LEN_HI_V2_OVER_V1:
				newCommand.length += c << 8
				newCommand.data = new Uint8Array(newCommand.length)
				checksumV1 ^= c
				checksumV2 = crc8DvbS2(checksumV2, c)
				mspState = newCommand.length > 0 ? MspState.PAYLOAD_V2_OVER_V1 : MspState.CHECKSUM_V2_OVER_V1
				break
			case MspState.PAYLOAD_V2_OVER_V1:
				newCommand.data[receivedBytes++] = c
				newCommand.dataStr += String.fromCharCode(c)
				checksumV1 ^= c
				checksumV2 = crc8DvbS2(checksumV2, c)
				if (receivedBytes === newCommand.length) mspState = MspState.CHECKSUM_V2_OVER_V1
				break
			case MspState.CHECKSUM_V2_OVER_V1:
				if (checksumV2 !== c) {
					mspState = MspState.IDLE
					break
				}
				checksumV1 ^= c
				mspState = MspState.CHECKSUM_V1
				break
			case MspState.CHECKSUM_V1:
				if (checksumV1 === c) setCommand(newCommand)
				mspState = MspState.IDLE
				break
			case MspState.TYPE_V2:
				mspState = MspState.FLAG_V2
				newCommand.version = MspVersion.V2
				checksumV2 = 0
				switch (c) {
					case 60: // '<'
						newCommand.cmdType = "request"
						break
					case 62: // '>'
						newCommand.cmdType = "response"
						break
					case 33: // '!'
						newCommand.cmdType = "error"
						break
					default:
						mspState = MspState.IDLE
						break
				}
				break
			case MspState.FLAG_V2:
				newCommand.flag = c
				checksumV2 = crc8DvbS2(checksumV2, c)
				mspState = MspState.CMD_LO_V2
				break
			case MspState.CMD_LO_V2:
				newCommand.command = c
				checksumV2 = crc8DvbS2(checksumV2, c)
				mspState = MspState.CMD_HI_V2
				break
			case MspState.CMD_HI_V2:
				newCommand.command += c << 8
				checksumV2 = crc8DvbS2(checksumV2, c)
				mspState = MspState.LEN_LO_V2
				break
			case MspState.LEN_LO_V2:
				newCommand.length = c
				checksumV2 = crc8DvbS2(checksumV2, c)
				mspState = MspState.LEN_HI_V2
				break
			case MspState.LEN_HI_V2:
				newCommand.length += c << 8
				newCommand.data = new Uint8Array(newCommand.length)
				checksumV2 = crc8DvbS2(checksumV2, c)
				mspState = newCommand.length > 0 ? MspState.PAYLOAD_V2 : MspState.CHECKSUM_V2
				break
			case MspState.PAYLOAD_V2:
				newCommand.data[receivedBytes++] = c
				newCommand.dataStr += String.fromCharCode(c)
				checksumV2 = crc8DvbS2(checksumV2, c)
				if (receivedBytes === newCommand.length) mspState = MspState.CHECKSUM_V2
				break
			case MspState.CHECKSUM_V2:
				if (checksumV2 === c) setCommand(newCommand)
				mspState = MspState.IDLE
				break
		}
	})
}

const read = () => {
	if (connectType === "serial" || connectType === "tcp") {
		invoke(connectType == "serial" ? "serial_read" : "tcp_read")
			.then(d => {
				handleRead(d as number[])
			})
			.catch(e => {
				if (
					e !== 'Custom { kind: TimedOut, error: "Operation timed out" }' &&
					e !== 'Custom { kind: BrokenPipe, error: "Broken pipe" }'
				)
					console.error(e)
			})
	}
}

let pingInterval = -1
let statusInterval = -1
let readInterval = -1
export const connect = (portToOpen: string) => {
	if (connectType === "tcp") {
		return new Promise((_resolve, reject) => {
			console.error("TCP connected, disconnecting")
			disconnect()
			reject("TCP connected, disconnecting")
		})
	} else if (connectType === "serial") {
		return new Promise((_resolve, reject) => {
			console.error("Serial connected, disconnecting")
			disconnect()
			reject("Serial connected, disconnecting")
		})
	}

	if (portToOpen.startsWith("tcp://")) {
		return new Promise((resolve: any, reject) => {
			let path = portToOpen.substring(6)
			if (path.indexOf(":") === -1) {
				path += ":5761" // default port
			}
			invoke("tcp_open", { path })
				.then(() => {
					connectType = "tcp"
					cmdEnabled = true
					console.log("TCP connected to", path)
					readInterval = setInterval(read, 3)
					pingInterval = setInterval(() => {
						sendCommand(MspFn.CONFIGURATOR_PING).catch(() => {})
						pingStarted = Date.now()
					}, 200)
					statusInterval = setInterval(() => {
						sendCommand(MspFn.STATUS).catch(() => {})
					}, 1000)
					onConnectHandlers.forEach(h => h())
					resolve()
				})
				.catch(e => {
					console.error(e)
					disconnect()
					reject(e)
				})
		})
	}
	// nothing connected, can connect
	return new Promise((resolve: any, reject) => {
		invoke("serial_open", { path: portToOpen })
			.then(() => {
				connectType = "serial"
				cmdEnabled = true
				readInterval = setInterval(read, 50)
				pingInterval = setInterval(() => {
					sendCommand(MspFn.CONFIGURATOR_PING).catch(() => {})
					pingStarted = Date.now()
				}, 200)
				statusInterval = setInterval(() => {
					sendCommand(MspFn.STATUS).catch(() => {})
				}, 1000)
				onConnectHandlers.forEach(h => h())
				resolve()
			})
			.catch(e => {
				console.error(e)
				disconnect()
				reject(e)
			})
	})
}

function onDisconnect() {
	onDisconnectHandlers.forEach(h => h())
	connectType = "none"
	cmdEnabled = false
	clearInterval(readInterval)
	clearInterval(pingInterval)
	clearInterval(statusInterval)
	readInterval = -1
}

export const disconnect = () => {
	if (connectType === "serial" || connectType === "tcp") {
		return new Promise((resolve: any, reject) => {
			invoke(connectType === "serial" ? "serial_close" : "tcp_close")
				.then(() => {
					resolve()
				})
				.catch(console.error)
				.finally(() => {
					onDisconnect()
					reject("Could not properly close connection to " + connectType)
				})
		})
	}
	return new Promise((resolve: any, reject) => {
		let closed = false
		//just try to disconnect both
		invoke("serial_close")
			.then(() => {
				closed = true
			})
			.catch(() => {})
			.finally(() => {
				return invoke("tcp_close")
			})
			.then(() => {
				closed = true
			})
			.catch(() => {})
			.finally(() => {
				onDisconnect()
				if (closed) {
					resolve()
				} else {
					reject("No connection to close")
				}
			})
	})
}

let serialDevices: string[] = []
function listSerialDevices() {
	invoke("serial_list").then((d: unknown) => {
		serialDevices = d as string[]
	})
}
let wifiDevices: string[][] = []
function listWifiDevices() {
	invoke("tcp_list")
		.then((result: unknown) => {
			wifiDevices = (result as string[]).map(d => {
				const parts = d.split(",")
				const host = parts[0]
				const ip = parts[1]
				return [`tcp://${ip}`, host]
			})
		})
		.catch(err => {
			console.error(err)
		})
}
listSerialDevices()
listWifiDevices()
setInterval(listSerialDevices, 1000)
setInterval(listWifiDevices, 20000)
export const getSerialDevices = () => serialDevices
export const getWifiDevices = () => wifiDevices

const onDisconnectHandlers: (() => void)[] = []
const onConnectHandlers: (() => void)[] = []
export const addOnDisconnectHandler = (handler: () => void) => {
	onDisconnectHandlers.push(handler)
}
export const removeOnDisconnectHandler = (handler: () => void) => {
	const i = onDisconnectHandlers.indexOf(handler)
	if (i >= 0) onDisconnectHandlers.splice(i, 1)
}
export const addOnConnectHandler = (handler: () => void) => {
	onConnectHandlers.push(handler)
}
export const removeOnConnectHandler = (handler: () => void) => {
	const i = onConnectHandlers.indexOf(handler)
	if (i >= 0) onConnectHandlers.splice(i, 1)
}
