import { writable } from 'svelte/store';
import { invoke } from '@tauri-apps/api';

export const ConfigCmd = {
	STATUS: 0,
	TASK_STATUS: 1,
	GET_SETTING: 2,
	SET_SETTING: 3,
	PLAY_SOUND: 4,
	BB_FILE_LIST: 5,
	BB_FILE_INFO: 6,
	BB_FILE_DOWNLOAD: 7,
	BB_FILE_DELETE: 8,
	BB_FORMAT: 9,
	WRITE_OSD_FONT_CHARACTER: 10
};

export type Command = {
	command: number;
	length: number;
	data: number[];
	dataStr: string;
	cmdType: 'request' | 'response' | 'error' | 'info';
};
function createPort() {
	const { subscribe, set, update } = writable({
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
		return new Promise((resolve, reject) => {
			invoke('serial_write', { data: cmd }).then(resolve).catch(reject);
		});
	};

	return {
		subscribe,
		set,
		sendCommand
	};
}
export const port = createPort();
