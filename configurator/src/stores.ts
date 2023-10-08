import { writable } from 'svelte/store';

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
	IND_MESSAGE: 0xc000
};

export type Command = {
	command: number;
	length: number;
	data: number[];
	dataStr: string;
	cmdType: 'request' | 'response' | 'error' | 'info';
};
function createPort() {
	let port: any = null;
	let devices: any = [];
	let reader: null | ReadableStreamDefaultReader = null;

	let resolveOnClose: any = null;
	let rejectOnClose: any = null;
	const { subscribe, set } = writable({
		command: 65535,
		length: 0,
		data: [],
		dataStr: '',
		cmdType: 'request'
	} as Command);

	function strToArray(str: string) {
		const data = [];
		for (let i = 0; i < str.length; i++) {
			data[i] = str.charCodeAt(i);
		}
		return data;
	}

	const sendCommand = (command: number, data: number[] = [], dataStr: string = '') => {
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
			...strToArray('_K'),
			len & 0xff,
			(len >> 8) & 0xff,
			command & 0xff,
			(command >> 8) & 0xff,
			...data,
			checksum
		];
		return new Promise((resolve: any, reject) => {
			if (port === null) reject('Port is not open');
			const writer: WritableStreamDefaultWriter = port.writable.getWriter();
			writer
				.write(new Uint8Array(cmd))
				.then(() => {
					writer.releaseLock();
					resolve();
				})
				.catch((e) => {
					disconnect();
					reject(e);
				});
		});
	};
	const read = () => {
		if (port === null) return;
		//https://developer.mozilla.org/en-US/docs/Web/API/Streams_API/Using_readable_streams
		reader = port.readable.getReader();
		let serialRxBuffer: number[] = [];
		reader!.read().then(function processReaderResult({ done, value }) {
			if (done) {
				reader!.releaseLock();

				port
					.close()
					.then(() => {
						port = null;
						clearInterval(pingInterval);
						clearInterval(statusInterval);
						resolveOnClose();
					})
					.catch(() => {
						port = null;
						rejectOnClose();
					});
				return;
			}

			serialRxBuffer = serialRxBuffer.concat(Array.from(value));
			reader!.read().then(processReaderResult);

			if (serialRxBuffer.length < 7) return;
			if (serialRxBuffer[0] != '_'.charCodeAt(0) || serialRxBuffer[1] != 'K'.charCodeAt(0)) {
				serialRxBuffer.splice(0, serialRxBuffer.length);
				return;
			}
			const len = serialRxBuffer[2] + serialRxBuffer[3] * 256;
			if (serialRxBuffer.length < len + 7) return;
			let checksum = 0;
			for (let i = 2; i < len + 7; i++) checksum ^= serialRxBuffer[i];
			if (checksum !== 0) {
				serialRxBuffer.splice(0, serialRxBuffer.length);
				return;
			}
			const data = serialRxBuffer.slice(6, len + 6);
			const command = serialRxBuffer[4] + serialRxBuffer[5] * 256;
			serialRxBuffer.splice(0, len + 7);
			let dataStr = '';
			for (let i = 0; i < data.length; i++) {
				dataStr += String.fromCharCode(data[i]);
			}
			let cmdType: 'request' | 'info' | 'response' | 'error' = 'info';
			if (command < 0x4000) cmdType = 'request';
			else if (command < 0x8000) cmdType = 'response';
			else if (command < 0xc000) cmdType = 'error';
			const c: Command = {
				command,
				data,
				dataStr,
				cmdType,
				length: len
			};
			set(c);
		});
	};
	let pingInterval: number = -1;
	let statusInterval: number = -1;

	const connect = (portToOpen: any) => {
		return new Promise((resolve: any, reject) => {
			if (port !== null) reject('Port is already open');
			if (portToOpen === null) reject('Port is null');
			portToOpen
				.open({ baudRate: 115200 })
				.then(() => {
					port = portToOpen;
					read();
					pingInterval = setInterval(() => {
						sendCommand(ConfigCmd.CONFIGURATOR_PING).catch(() => {});
					}, 200);
					statusInterval = setInterval(() => {
						sendCommand(ConfigCmd.STATUS).catch(() => {});
					}, 1000);
					resolve();
				})
				.catch(reject);
		});
	};
	const disconnect = () => {
		if (port === null) return;
		return new Promise((resolve: any, reject) => {
			resolveOnClose = resolve;
			rejectOnClose = reject;
			try {
				reader?.cancel();
				reader?.releaseLock();
			} catch (e) {
				console.error(e);
				port = null;
			}
		});
	};
	const addDevice = () => {
		(navigator as any).serial.requestPort();
	};
	const getDevices = () => {
		return devices;
	};
	function listDevices() {
		(navigator as any).serial.getPorts().then((ports: any) => {
			devices = ports;
		});
	}
	setInterval(listDevices, 1000);
	listDevices();

	return {
		subscribe,
		set,
		sendCommand,
		addDevice,
		connect,
		disconnect,
		getDevices
	};
}
export const port = createPort();
