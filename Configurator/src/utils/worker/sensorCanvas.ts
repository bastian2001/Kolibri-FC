// worker thread to draw a single sensor canvas
let canvas: HTMLCanvasElement
const MAX_WIDTH = 600 // max frame count

const data = [] as number[][] // first dimension is sensor id, second is the frames
let traces = [] as { id: number; color: string }[] // first dimension is sensor id
const sensors = [
	{ min: -2, max: 2, unit: "g" }, // accel
	{ min: -2, max: 2, unit: "g" },
	{ min: -2, max: 2, unit: "g" },
	{ min: -800, max: 800, unit: "°/s" }, // gyro
	{ min: -800, max: 800, unit: "°/s" },
	{ min: -800, max: 800, unit: "°/s" },
	{ min: -600, max: 600 }, // mag
	{ min: -600, max: 600 },
	{ min: -600, max: 600 },
	{ min: -180, max: 180, unit: "°" }, // atti
	{ min: -180, max: 180, unit: "°" },
	{ min: -180, max: 180, unit: "°" },
	{ min: -180, max: 180, unit: "°" },
	{ unit: "m" }, // baro
	{ min: 1000, max: 2000, unit: "µs" }, // RC channels
	{ min: 1000, max: 2000, unit: "µs" },
	{ min: 1000, max: 2000, unit: "µs" },
	{ min: 1000, max: 2000, unit: "µs" },
	{ min: 1000, max: 2000, unit: "µs" },
	{ min: 1000, max: 2000, unit: "µs" },
	{ min: 1000, max: 2000, unit: "µs" },
	{ min: 1000, max: 2000, unit: "µs" },
	{ min: -100, max: 100 }, // debug
	{ min: -1000, max: 1000 }, // debug
	{ min: -10000, max: 10000 }, // debug
	{ min: -256, max: 256 }, // debug
] as { min?: number; max?: number; unit?: string }[]
let offset = 0
let windowSize = 300
let redraw = false

onmessage = e => {
	switch (e.data.type) {
		case "init":
			{
				canvas = e.data.canvas
				canvas.width = e.data.width
				canvas.height = e.data.height
				e.data.traces.forEach(
					(trace: { name: string; color: string; sensorId: number; id: number; mspFn: number }) => {
						traces.push({
							id: trace.sensorId,
							color: trace.color,
						})
						if (!data[trace.sensorId]) data[trace.sensorId] = new Array(MAX_WIDTH).fill(0)
					}
				)
				windowSize = e.data.window
				requestAnimationFrame(draw)
				redraw = true
			}
			break
		case "resize":
			{
				canvas.width = e.data.width
				canvas.height = e.data.height
				redraw = true
			}
			break
		case "new":
			if (!data[e.data.sensorId]) data[e.data.sensorId] = new Array(MAX_WIDTH).fill(0)
			traces.push({
				id: e.data.sensorId,
				color: e.data.color,
			})
			redraw = true
			break
		case "window":
			windowSize = e.data.window
			redraw = true
			break
		case "delete":
			{
				const index = e.data.index
				const trace = traces.splice(index, 1)[0]
				if (trace) {
					// check if there are any other traces with the same sensor id
					const id = trace.id
					let found = false
					for (let i = 0; i < traces.length; i++) {
						if (traces[i].id === id) {
							found = true
							break
						}
					}
					if (!found) {
						// if not, remove the data array
						delete data[id]
					}
					redraw = true
				}
			}
			break
		case "sensorId":
			{
				const trace = traces[e.data.index]
				if (trace.id !== e.data.sensorId) {
					// check if there are any other traces with the old sensor id
					let found = false
					for (let i = 0; i < traces.length; i++) {
						if (traces[i].id === trace.id && i !== e.data.index) {
							found = true
							break
						}
					}
					if (!found) {
						delete data[trace.id]
					}

					if (!data[e.data.sensorId]) data[e.data.sensorId] = new Array(MAX_WIDTH).fill(0)

					trace.id = e.data.sensorId
					redraw = true
				}
			}
			break
		case "color":
			{
				const trace = traces[e.data.index]
				if (trace) {
					trace.color = e.data.color
					redraw = true
				}
			}
			break
		case "data": {
			e.data.data.forEach((d: number, i: number) => {
				if (data[i]) {
					data[i].push(d)
					data[i].shift()
				}
			})
			offset = e.data.offset
			redraw = true
		}
	}
}

function getRange(sensorId: number) {
	const sensor = sensors[sensorId]
	if (sensor.min !== undefined && sensor.max !== undefined) {
		return { min: sensor.min, max: sensor.max }
	} else {
		// look at last windowSize values
		const values = data[sensorId].slice(-windowSize)
		return { min: Math.min(...values), max: Math.max(...values) }
	}
}

const ranges = [] as { min: number; max: number }[]
function getLimitTexts(sensorId: number) {
	const sensor = sensors[sensorId]
	const range = ranges[sensorId] || getRange(sensorId)
	return {
		min: range.min + " " + (sensor.unit || ""),
		max: range.max + " " + (sensor.unit || ""),
	}
}

function draw() {
	if (!redraw) {
		requestAnimationFrame(draw)
		return
	}
	redraw = false
	const ctx = canvas.getContext("2d")
	if (!ctx || !traces.length) {
		requestAnimationFrame(draw)
		return
	}
	// draw frame
	ctx.fillStyle = "black"
	ctx.fillRect(0, 0, canvas.width, canvas.height)
	// draw grid, one line every 50 frames, using total%50 as the offset
	ctx.strokeStyle = "rgba(255, 255, 255,0.8)"
	ctx.lineWidth = 1
	for (let i = offset % 50; i < windowSize; i += 50) {
		ctx.beginPath()
		const x = Math.round((canvas.width * (windowSize - i)) / (windowSize - 1))
		ctx.moveTo(x, 0)
		ctx.lineTo(x, canvas.height)
		ctx.stroke()
	}
	// horizontal center lines
	ctx.strokeStyle = "rgba(255, 255, 255, 0.5)"
	for (let i = 1; i < 4; i++) {
		ctx.beginPath()
		const y = Math.round((canvas.height * i) / 4)
		ctx.moveTo(0, y)
		ctx.lineTo(canvas.width, y)
		ctx.stroke()
	}
	// draw all traces
	for (const trace of traces) {
		ctx.beginPath()
		ctx.strokeStyle = trace.color
		ctx.lineWidth = 2
		const { min, max } = getRange(trace.id)
		ranges[trace.id] = { min, max }
		const values = data[trace.id].slice(-windowSize)
		for (let i = 0; i < values.length; i++) {
			const x = (canvas.width * i) / (windowSize - 1)
			let value = values[i]
			value = Math.max(min, Math.min(max, value))
			const y = canvas.height * (1 - (value - min) / (max - min))
			ctx.lineTo(x, y)
		}
		ctx.stroke()
	}
	// draw min/max
	ctx.font = "14px sans-serif"
	ctx.textAlign = "right"
	ctx.textBaseline = "top"
	const topText = getLimitTexts(traces[0].id).max
	let needsIndividualTexts = false
	for (const trace of traces) {
		const text = getLimitTexts(trace.id).max
		if (text !== topText) {
			needsIndividualTexts = true
			break
		}
	}
	if (needsIndividualTexts) {
		let right = 0
		for (let i = traces.length - 1; i >= 0; i--) {
			const texts = getLimitTexts(traces[i].id)
			ctx.fillStyle = traces[i].color
			ctx.textBaseline = "top"
			ctx.fillText(texts.max, canvas.width - 5 - right, 5)
			ctx.textBaseline = "bottom"
			ctx.fillText(texts.min, canvas.width - 5 - right, canvas.height - 5)
			const w0 = ctx.measureText(texts.max).width
			const w1 = ctx.measureText(texts.min).width
			right += Math.max(w0, w1) + 15
		}
	} else {
		ctx.fillStyle = "white"
		ctx.fillText(topText, canvas.width - 5, 5)
		const bottomText = getLimitTexts(traces[0].id).min
		ctx.textBaseline = "bottom"
		ctx.fillText(bottomText, canvas.width - 5, canvas.height - 5)
	}
	requestAnimationFrame(draw)
}
