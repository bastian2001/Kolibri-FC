import { defineStore } from "pinia"
import { ref } from "vue"

export const useLogStore = defineStore("log", () => {
	const logs = ref<string[]>([])

	const push = (entry: string) => {
		const date = new Date()
		entry =
			date.getHours().toString().padStart(2, "0") +
			":" +
			date.getMinutes().toString().padStart(2, "0") +
			":" +
			date.getSeconds().toString().padStart(2, "0") +
			" -> " +
			entry
		logs.value = [...logs.value, entry]
	}

	const clearEntries = () => {
		logs.value = []
	}

	const getEntries = () => {
		return logs.value
	}

	return { logs, push, clearEntries, getEntries }
})
