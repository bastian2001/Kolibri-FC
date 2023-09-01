import { writable } from 'svelte/store';
import type { Serialport } from 'tauri-plugin-serialport-api';

type Port = {
	path: string;
	port: Serialport | undefined;
	baudRate: number;
	connected: boolean;
};

function createPort() {
	const { subscribe, set, update } = writable({
		path: 'COM1',
		port: undefined,
		baudRate: 115200,
		connected: false
	} as Port);
	return {
		subscribe,
		set
	};
}
export const port = createPort();
