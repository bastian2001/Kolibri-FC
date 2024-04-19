import { writable } from 'svelte/store';
import { invoke } from '@tauri-apps/api';
import { leBytesToInt } from './utils';

export const ConfigCmd = {
	STATUS: 0,
	TASK_STATUS: 1,
	REBOOT: 2,
	SAVE_SETTINGS: 3,
	PLAY_SOUND: 4,
	BB_FILE_LIST: 5,
	BB_FILE_INFO: 6,
	BB_FILE_DOWNLOAD: 7,
	BB_FILE_DELETE: 8,
	BB_FORMAT: 9,
	WRITE_OSD_FONT_CHARACTER: 10,
	SET_MOTORS: 11,
	GET_MOTORS: 12,
	BB_FILE_DOWNLOAD_RAW: 13,
	SET_DEBUG_LED: 14,
	CONFIGURATOR_PING: 15,
	REBOOT_TO_BOOTLOADER: 16,
	GET_NAME: 17,
	SET_NAME: 18,
	GET_PIDS: 19,
	SET_PIDS: 20,
	GET_RATES: 21,
	SET_RATES: 22,
	GET_BB_SETTINGS: 23,
	SET_BB_SETTINGS: 24,
	GET_ROTATION: 25,
	SERIAL_PASSTHROUGH: 26,
	GET_GPS_STATUS: 27,
	GET_GPS_ACCURACY: 28,
	GET_GPS_TIME: 29,
	GET_GPS_MOTION: 30,
	ESC_PASSTHROUGH: 31,
	GET_CRASH_DUMP: 32,
	CLEAR_CRASH_DUMP: 33,
	CALIBRATE_ACCELEROMETER: 34,
	GET_MAG_DATA: 35,
	MAG_CALIBRATE: 36,
	IND_MESSAGE: 0xc000
};

const MspState = {
	IDLE: 0, // waiting for $
	PACKET_START: 1, // receiving M or X
	TYPE_V1: 2, // got M, receiving type byte (<, >, !)
	LEN_V1: 3, // if 255 is received in this step, inject jumbo len bytes
	JUMBO_LEN_LO_V1: 4,
	JUMBO_LEN_HI_V1: 5,
	CMD_V1: 6,
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
	CHECKSUM_V2: 23
};

export type Command = {
	command: number;
	length: number;
	data: number[];
	dataStr: string;
	cmdType: 'request' | 'response' | 'error' | 'info';
};
function createPort() {
	let devices: string[] = [];

	const { subscribe, set } = writable({
		command: 65535,
		length: 0,
		data: [],
		dataStr: '',
		cmdType: 'request'
	} as Command);

	subscribe((c) => {
		console.log(c);
		switch (c.command) {
			case ConfigCmd.CONFIGURATOR_PING:
				//ping received from FC, confirm
				port.sendCommand(ConfigCmd.CONFIGURATOR_PING | 0x4000);
				break;
			case ConfigCmd.CONFIGURATOR_PING | 0x4000:
				// pong response from FC received
				pingTime.fromConfigurator = Date.now() - pingStarted;
				break;
			case ConfigCmd.CONFIGURATOR_PING | 0xc000:
				pingTime.fromFC = leBytesToInt(c.data);
				break;
		}
	});

	function strToArray(str: string) {
		const data = [];
		for (let i = 0; i < str.length; i++) {
			data[i] = str.charCodeAt(i);
		}
		return data;
	}
	let cmdEnabled = true;
	const enableCommands = (en: boolean) => {
		cmdEnabled = en;
	};
	const onDisconnectHandlers: (() => void)[] = [];
	const onConnectHandlers: (() => void)[] = [];

	const sendCommand = (command: number, data: number[] = [], dataStr: string = '') => {
		if (!cmdEnabled) return new Promise((resolve: any) => resolve());
		if (data.length === 0 && dataStr !== '') data = strToArray(dataStr);
		const len = data.length;
		let checksum = 0;
		for (let i = 0; i < len; i++) {
			checksum ^= data[i];
		}
		checksum ^= command & 0xff;
		checksum ^= (command >> 8) & 0xff;
		checksum ^= len & 0xff;
		checksum ^= (len >> 8) & 0xff;
		const cmd = [
			...strToArray('$X<'),
			0,
			command & 0xff,
			(command >> 8) & 0xff,
			len & 0xff,
			(len >> 8) & 0xff,
			...data,
			checksum
		];
		return new Promise((resolve: any, reject) => {
			invoke('serial_write', { data: cmd })
				.then(resolve)
				.catch((e) => {
					reject(e);
				});
		});
	};
	const sendRaw = (data: number[], dataStr: string = '') => {
		if (data.length === 0 && dataStr !== '') data = strToArray(dataStr);
		return new Promise((resolve: any, reject) => {
			invoke('serial_write', { data })
				.then(resolve)
				.catch((e) => {
					reject(e);
				});
		});
	};
	let rxBuf: number[] = [];
	let pingStarted = 0;
	let newCommand: Command = {
		command: 65535,
		length: 0,
		data: [],
		dataStr: '',
		cmdType: 'request'
	};
	let mspState = 0;
	const read = () => {
		invoke('serial_read')
			.then((d) => {
				rxBuf = d as number[];
			})
			.catch((e) => {
				if (e !== 'Custom { kind: TimedOut, error: "Operation timed out" }') console.error(e);
			})
			.finally(() => {
				rxBuf.forEach((c) => {
					switch (mspState) {
						case MspState.IDLE:
							if (c === 36) mspState = MspState.PACKET_START; /* $ */
							break;
						case MspState.PACKET_START:
							newCommand.data = [];
							newCommand.dataStr = '';
							if (c === 77) mspState = MspState.TYPE_V1; /* M */
							else if (c === 88) mspState = MspState.TYPE_V2; /* X */
							else mspState = MspState.IDLE;
							break;
						case MspState.TYPE_V1:
							mspState = MspState.LEN_V1;
							switch (c) {
								case 60: // '<'
									newCommand.cmdType = 'request';
									break;
								case 62: // '>'
									newCommand.cmdType = 'response';
									break;
								case 33: // '!'
									newCommand.cmdType = 'error';
									break;
								default:
									mspState = MspState.IDLE;
									break;
							}
							break;
						case MspState.LEN_V1:
							if (c === 255) {
								mspState = MspState.JUMBO_LEN_LO_V1;
							} else {
								newCommand.length = c;
								mspState = MspState.CMD_V1;
							}
							break;
						case MspState.JUMBO_LEN_LO_V1:
							newCommand.length = c;
							mspState = MspState.JUMBO_LEN_HI_V1;
							break;
						case MspState.JUMBO_LEN_HI_V1:
							newCommand.length += c << 8;
							mspState = MspState.CMD_V1;
							break;
						case MspState.CMD_V1:
							if (c === 255) {
								mspState = MspState.FLAG_V2_OVER_V1;
							} else {
								newCommand.command = c;
								mspState = newCommand.length > 0 ? MspState.PAYLOAD_V1 : MspState.CHECKSUM_V1;
							}
							break;
						case MspState.PAYLOAD_V1:
							newCommand.data.push(c);
							newCommand.dataStr += String.fromCharCode(c);
							if (newCommand.data.length === newCommand.length) mspState = MspState.CHECKSUM_V1;
							break;
						case MspState.FLAG_V2_OVER_V1:
							mspState = MspState.CMD_LO_V2_OVER_V1;
							break;
						case MspState.CMD_LO_V2_OVER_V1:
							newCommand.command = c;
							mspState = MspState.CMD_HI_V2_OVER_V1;
							break;
						case MspState.CMD_HI_V2_OVER_V1:
							newCommand.command += c << 8;
							mspState = MspState.LEN_LO_V2_OVER_V1;
							break;
						case MspState.LEN_LO_V2_OVER_V1:
							newCommand.length = c;
							mspState = MspState.LEN_HI_V2_OVER_V1;
							break;
						case MspState.LEN_HI_V2_OVER_V1:
							newCommand.length += c << 8;
							mspState =
								newCommand.length > 0 ? MspState.PAYLOAD_V2_OVER_V1 : MspState.CHECKSUM_V2_OVER_V1;
							break;
						case MspState.PAYLOAD_V2_OVER_V1:
							newCommand.data.push(c);
							newCommand.dataStr += String.fromCharCode(c);
							if (newCommand.data.length === newCommand.length)
								mspState = MspState.CHECKSUM_V2_OVER_V1;
							break;
						case MspState.CHECKSUM_V2_OVER_V1:
							mspState = MspState.CHECKSUM_V1;
							break;
						case MspState.CHECKSUM_V1:
							set(newCommand);
							mspState = MspState.IDLE;
							break;
						case MspState.TYPE_V2:
							mspState = MspState.FLAG_V2;
							switch (c) {
								case 60: // '<'
									newCommand.cmdType = 'request';
									break;
								case 62: // '>'
									newCommand.cmdType = 'response';
									break;
								case 33: // '!'
									newCommand.cmdType = 'error';
									break;
								default:
									mspState = MspState.IDLE;
									break;
							}
							break;
						case MspState.FLAG_V2:
							mspState = MspState.CMD_LO_V2;
							break;
						case MspState.CMD_LO_V2:
							newCommand.command = c;
							mspState = MspState.CMD_HI_V2;
							break;
						case MspState.CMD_HI_V2:
							newCommand.command += c << 8;
							mspState = MspState.LEN_LO_V2;
							break;
						case MspState.LEN_LO_V2:
							newCommand.length = c;
							mspState = MspState.LEN_HI_V2;
							break;
						case MspState.LEN_HI_V2:
							newCommand.length += c << 8;
							mspState = newCommand.length > 0 ? MspState.PAYLOAD_V2 : MspState.CHECKSUM_V2;
							break;
						case MspState.PAYLOAD_V2:
							newCommand.data.push(c);
							newCommand.dataStr += String.fromCharCode(c);
							if (newCommand.data.length === newCommand.length) mspState = MspState.CHECKSUM_V2;
							break;
						case MspState.CHECKSUM_V2:
							set(newCommand);
							mspState = MspState.IDLE;
							break;
					}
				});
			});
	};
	let pingInterval: number = -1;
	let statusInterval: number = -1;
	let readInterval: number = -1;

	const connect = (portToOpen: any) => {
		return new Promise((resolve: any, reject) => {
			invoke('serial_open', { path: portToOpen })
				.then(() => {
					cmdEnabled = true;
					readInterval = setInterval(read, 3);
					pingInterval = setInterval(() => {
						sendCommand(ConfigCmd.CONFIGURATOR_PING).catch(() => {});
						pingStarted = Date.now();
					}, 200);
					statusInterval = setInterval(() => {
						sendCommand(ConfigCmd.STATUS).catch(() => {});
					}, 1000);
					onConnectHandlers.forEach((h) => h());
					resolve();
				})
				.catch((e) => {
					console.error(e);
					disconnect();
					reject(e);
				});
		});
	};
	const disconnect = () => {
		return new Promise((resolve: any, reject) => {
			invoke('serial_close')
				.then(() => {
					onDisconnectHandlers.forEach((h) => h());
					resolve();
				})
				.catch(console.error)
				.finally(() => {
					cmdEnabled = false;
					clearInterval(readInterval);
					clearInterval(pingInterval);
					clearInterval(statusInterval);
					readInterval = -1;
					reject();
				});
		});
	};
	const getDevices = () => {
		return devices;
	};
	function listDevices() {
		invoke('serial_list').then((d: unknown) => {
			devices = d as any;
		});
	}
	setInterval(listDevices, 1000);
	listDevices();

	const addOnDisconnectHandler = (handler: () => void) => {
		onDisconnectHandlers.push(handler);
	};
	const removeOnDisconnectHandler = (handler: () => void) => {
		const i = onDisconnectHandlers.indexOf(handler);
		if (i >= 0) onDisconnectHandlers.splice(i, 1);
	};
	const addOnConnectHandler = (handler: () => void) => {
		onConnectHandlers.push(handler);
	};
	const removeOnConnectHandler = (handler: () => void) => {
		const i = onConnectHandlers.indexOf(handler);
		if (i >= 0) onConnectHandlers.splice(i, 1);
	};
	let pingTime = {
		fromConfigurator: -1,
		fromFC: -1
	};
	function getPingTime() {
		return pingTime;
	}
	return {
		subscribe,
		set,
		sendCommand,
		connect,
		disconnect,
		getDevices,
		enableCommands,
		sendRaw,
		addOnDisconnectHandler,
		removeOnDisconnectHandler,
		addOnConnectHandler,
		removeOnConnectHandler,
		getPingTime
	};
}
export const port = createPort();
