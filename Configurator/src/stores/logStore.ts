/*
 * Copyright (c) 2026 Kolibri-FC contributors
 * 
 * This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 * 
 * Kolibri-FC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Kolibri-FC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
 */

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
