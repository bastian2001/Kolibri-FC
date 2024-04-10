import { writable } from 'svelte/store';

function createLog() {
	const { subscribe, set, update } = writable([] as string[]);
	const push = (entry: string) => {
		const date = new Date();
		entry =
			date.getHours().toString().padStart(2, '0') +
			':' +
			date.getMinutes().toString().padStart(2, '0') +
			':' +
			date.getSeconds().toString().padStart(2, '0') +
			' -> ' +
			entry;
		update((entries) => [...entries, entry]);
	};
	const clearEntries = () => {
		set([]);
	};
	return {
		subscribe,
		push,
		clearEntries
	};
}

export const configuratorLog = createLog();
