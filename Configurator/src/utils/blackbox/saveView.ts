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

import { BBLog, TraceInGraph, TraceInternalData } from "@/utils/types"

let cachedLog: BBLog | undefined

export function saveLog(log: BBLog | undefined) {
	cachedLog = log
}

export function getSavedLog(): BBLog | undefined {
	return cachedLog
}

let minFrame = 0
let maxFrame = 0

export function setFrameRange(min: number, max: number) {
	minFrame = min
	maxFrame = max
}

export function getFrameRange() {
	return { min: minFrame, max: maxFrame }
}

let graphs: { t: TraceInGraph; s: TraceInternalData | undefined }[][] = [[]]
export function setGraphs(g: { t: TraceInGraph; s: TraceInternalData | undefined }[][]) {
	graphs = g
}
export function getGraphs(): { t: TraceInGraph; s: TraceInternalData | undefined }[][] {
	return graphs
}
