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
