import { writable } from 'svelte/store';
import { invoke } from '@tauri-apps/api';
import { configuratorLog } from './logStore';

export const MspFn = {
	STATUS: 0x4000,
	TASK_STATUS: 0x4001,
	REBOOT: 0x4002,
	SAVE_SETTINGS: 0x4003,
	PLAY_SOUND: 0x4004,
	BB_FILE_LIST: 0x4005,
	BB_FILE_INFO: 0x4006,
	BB_FILE_DOWNLOAD: 0x4007,
	BB_FILE_DELETE: 0x4008,
	BB_FORMAT: 0x4009,
	WRITE_OSD_FONT_CHARACTER: 0x400a,
	SET_MOTORS: 0x400b,
	GET_MOTORS: 0x400c,
	BB_FILE_DOWNLOAD_RAW: 0x400d,
	SET_DEBUG_LED: 0x400e,
	CONFIGURATOR_PING: 0x400f,
	REBOOT_TO_BOOTLOADER: 0x4010,
	GET_NAME: 0x4011,
	SET_NAME: 0x4012,
	GET_PIDS: 0x4013,
	SET_PIDS: 0x4014,
	GET_RATES: 0x4015,
	SET_RATES: 0x4016,
	GET_BB_SETTINGS: 0x4017,
	SET_BB_SETTINGS: 0x4018,
	GET_ROTATION: 0x4019,
	SERIAL_PASSTHROUGH: 0x401a,
	GET_GPS_STATUS: 0x401b,
	GET_GPS_ACCURACY: 0x401c,
	GET_GPS_TIME: 0x401d,
	GET_GPS_MOTION: 0x401e,
	ESC_PASSTHROUGH: 0x401f,
	GET_CRASH_DUMP: 0x4020,
	CLEAR_CRASH_DUMP: 0x4021,
	CALIBRATE_ACCELEROMETER: 0x4022,
	GET_MAG_DATA: 0x4023,
	MAG_CALIBRATE: 0x4024,
	IND_MESSAGE: 0x4025
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

const MspVersion = {
	V2: 0,
	V1: 1,
	V1_JUMBO: 2,
	V2_OVER_V1: 3,
	V2_OVER_V1_JUMBO: 4
};

export type Command = {
	command: number;
	length: number;
	data: number[];
	dataStr: string;
	cmdType: 'request' | 'response' | 'error';
};
function createPort() {
	let devices: string[] = [];

	const { subscribe, set } = writable({
		command: NaN,
		length: 0,
		data: [],
		dataStr: '',
		cmdType: 'request'
	} as Command);

	subscribe(c => {
		if (c.cmdType === 'response') {
			switch (c.command) {
				case MspFn.CONFIGURATOR_PING:
					// pong response from FC received
					fcPing = Date.now() - pingStarted;
					break;
			}
		}
		if (!Object.values(MspFn).includes(c.command) && !Number.isNaN(c.command)) {
			console.log(structuredClone(c));
			configuratorLog.push('Unsupported command, see console for details.');
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

	const sendCommand = (
		type: 'request' | 'response' | 'error',
		command: number,
		data: number[] = [],
		dataStr: string = ''
	) => {
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
		const typeLut = {
			request: 60,
			response: 62,
			error: 33
		};
		const cmd = [
			...strToArray('$X'),
			typeLut[type],
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
				.catch(e => {
					reject(e);
				});
		});
	};
	const sendRaw = (data: number[], dataStr: string = '') => {
		if (data.length === 0 && dataStr !== '') data = strToArray(dataStr);
		return new Promise((resolve: any, reject) => {
			invoke('serial_write', { data })
				.then(resolve)
				.catch(e => {
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
			.then(d => {
				rxBuf = d as number[];
			})
			.catch(e => {
				if (e !== 'Custom { kind: TimedOut, error: "Operation timed out" }') console.error(e);
			})
			.finally(() => {
				rxBuf.forEach(c => {
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
							mspState = MspState.IDLE;
							set(newCommand);
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
						sendCommand('request', MspFn.CONFIGURATOR_PING).catch(() => {});
						pingStarted = Date.now();
					}, 200);
					statusInterval = setInterval(() => {
						sendCommand('request', MspFn.STATUS).catch(() => {});
					}, 1000);
					onConnectHandlers.forEach(h => h());
					resolve();
				})
				.catch(e => {
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
					onDisconnectHandlers.forEach(h => h());
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
	let fcPing = -1;
	function getPingTime() {
		return fcPing;
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
