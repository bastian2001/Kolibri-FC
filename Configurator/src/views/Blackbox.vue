<script lang="ts">
import { defineComponent } from "vue";
import Timeline from "@components/blackbox/Timeline.vue";
import Settings from "@components/blackbox/Settings.vue";
import { BBLog, TraceInGraph, Command, LogData, LogDataType, TraceInternalData } from "@utils/types";
import { constrain, intToLeBytes, leBytesToInt, prefixZeros } from "@utils/utils";
import { skipValues } from "@/utils/blackbox/other";
import { MspFn } from "@/msp/protocol";
import { useLogStore } from "@stores/logStore";
import { addOnCommandHandler, addOnConnectHandler, removeOnCommandHandler, removeOnConnectHandler, sendCommand } from "@/msp/comm";
import TracePlacer from "@components/blackbox/TracePlacer.vue";
import { BB_ALL_FLAGS, BB_GEN_FLAGS } from "@/utils/blackbox/bbFlags";
import { parseBlackbox } from "@/utils/blackbox/parsing";
import { fillLogWithGenFlags } from "@/utils/blackbox/flagGen";
import { getFrameRange, getGraphs, getSavedLog, saveLog, setFrameRange, setGraphs } from "@/utils/blackbox/saveView";
import { TRACE_COLORS_FOR_BLACK_BACKGROUND } from "@/utils/constants";

const DURATION_BAR_RASTER = ['100us', '200us', '500us', '1ms', '2ms', '5ms', '10ms', '20ms', '50ms', '100ms', '200ms', '0.5s', '1s', '2s', '5s', '10s', '20s', '30s', '1min', '2min', '5min', '10min', '20min', '30min', '1h'
];

const FLIGHT_MODES = [
	"Acro",
	"Angle",
	"Altitude Hold",
	"GPS Velocity",
	"GPS Position"
];


export default defineComponent({
	name: "Blackbox",
	components: {
		Timeline,
		Settings,
		TracePlacer
	},
	data() {
		return {
			graphs: [[]] as TraceInGraph[][],
			startFrame: 0,
			endFrame: 0,
			loadedLog: undefined as BBLog | undefined,
			drawFullCanvasTimeout: -1,
			logNums: [] as { text: string; num: number }[],
			binFile: new Uint8Array(),
			binFileNumber: -1,
			receivedChunks: [] as boolean[],
			totalChunks: -1,
			chunkSize: -1,
			resolveWhenReady: (_log: BBLog) => { },
			rejectWrongFile: (_: string) => { },
			showSettings: false,
			logInfoPosition: 0,
			logInfoInterval: -1,
			selected: -1,
			canvas: document.createElement("canvas"),
			selectionCanvas: document.createElement("canvas"),
			sliceAndSkip: {} as LogData,
			skipValue: 0,
			trackingStartX: -1,  // -1 = idle, -2 = move window, 0+ = selection
			trackingEndX: 0,
			firstX: 0,
			startStartFrame: 0,
			startEndFrame: 0,
			touchStartX: 0,
			frame0: 0,
			frame1: 0,
			touchMode: 'none' as 'none' | 'move' | 'zoom',
			prefixZeros,
			BB_ALL_FLAGS,
			BB_GEN_FLAGS,
			configuratorLog: useLogStore(),
			getChunkInterval: -1,
			traceInternalData: [[]] as TraceInternalData[][],
			traceInternalBackupFn: [[]] as (() => (TraceInternalData | void))[][],
		};
	},
	computed: {
		dataSlice(): LogData {
			if (!this.loadedLog) return {}
			const slice: LogData = {}
			for (const key in this.loadedLog.logData) {
				// @ts-expect-error
				slice[key] = (this.loadedLog.logData[key] as unknown as LogDataType)?.slice(this.startFrame, this.endFrame + 1)
			}
			return slice
		},
		dataViewerWrapper(): HTMLDivElement {
			return this.$refs.dataViewerWrapper as HTMLDivElement;
		},
		domCanvas(): HTMLCanvasElement {
			return this.$refs.bbDataViewer as HTMLCanvasElement;
		},
	},
	watch: {
		dataSlice: {
			handler() {
				this.drawCanvas();
			}
		},
		graphs: {
			handler() {
				this.drawCanvas();
			},
			deep: 1
		},
	},
	methods: {
		onCommand(command: Command) {
			if (command.cmdType === 'response') {
				switch (command.command) {
					case MspFn.BB_FILE_LIST:
						const nums = [];
						for (let i = 0; i < command.data.length; i += 2) {
							const num = leBytesToInt(command.data, i, 2);
							nums.push(num);
						}
						nums.sort((a, b) => a - b);
						this.logNums = nums.map(n => ({ text: '', num: n }));
						if (!this.logNums.length) {
							this.logNums = [{ text: 'No logs found', num: -1 }];
							this.selected = -1;
							return;
						}
						this.selected = this.logNums[0].num;
						this.logInfoPosition = 0;
						this.logInfoInterval = setInterval(this.getLogInfo, 100);
						break;
					case MspFn.BB_FILE_INFO:
						this.processLogInfo(command.data);
						break;
					case MspFn.BB_FILE_DOWNLOAD:
						this.handleFileChunk(command.data);
						break;
					case MspFn.BB_FILE_DELETE:
						const index = this.logNums.findIndex(l => l.num === this.selected);
						if (index !== -1) this.logNums.splice(index, 1);
						this.logNums = [...this.logNums];
						if (!this.logNums.length) {
							this.logNums = [{ text: 'No logs found', num: -1 }];
							this.selected = -1;
						} else if (index >= this.logNums.length) this.selected = this.logNums[this.logNums.length - 1].num;
						else this.selected = this.logNums[index].num;
						break;
					case MspFn.BB_FORMAT:
						this.configuratorLog.push('Blackbox formatted');
						this.logNums = [{ text: 'No logs found', num: -1 }];
						break;
					case MspFn.BB_FILE_INIT:
						this.prepareFileDownload(command.data);
						break;
				}
			} else if (command.cmdType === 'error') {
				switch (command.command) {
					case MspFn.BB_FORMAT:
						this.configuratorLog.push('Blackbox format failed');
						break;
					case MspFn.BB_FILE_DELETE:
						this.configuratorLog.push(`Deleting file ${command.data[0]} failed`);
						break;
				}
			}
		},
		getFileList() {
			sendCommand(MspFn.BB_FILE_LIST).catch(console.error);
		},
		onResize() {
			this.domCanvas.height = this.dataViewerWrapper.clientHeight;
			this.domCanvas.width = this.dataViewerWrapper.clientWidth;
			this.selectionCanvas.height = this.dataViewerWrapper.clientHeight;
			this.selectionCanvas.width = this.dataViewerWrapper.clientWidth;

			this.drawCanvas();
		},
		addGraph() {
			this.graphs.push([]);
			this.traceInternalBackupFn.push([]);
			this.traceInternalData.push([]);
		},
		deleteGraph(g: number) {
			this.graphs = this.graphs.filter((_, i) => i !== g);
			this.traceInternalBackupFn = this.traceInternalBackupFn.filter((_, i) => i !== g);
			this.traceInternalData = this.traceInternalData.filter((_, i) => i !== g);
		},
		deleteTrace(g: number, t: number) {
			this.graphs[g] = this.graphs[g].filter((_, i) => i !== t);
			this.traceInternalBackupFn[g] = this.traceInternalBackupFn[g].filter((_, i) => i !== t);
			this.traceInternalData[g] = this.traceInternalData[g].filter((_, i) => i !== t);
		},
		addTrace(graphIndex: number) {
			const defaultTrace: TraceInGraph = {
				color: 'transparent',
				maxValue: 10,
				minValue: 0,
				strokeWidth: 1,
				path: '',
				decimals: 0,
				unit: '',
				displayName: '',
				id: Math.random(),
				hasSetData: false
			};

			const c = TRACE_COLORS_FOR_BLACK_BACKGROUND[this.graphs[graphIndex].length]
			if (c) defaultTrace.color = c;
			this.traceInternalBackupFn[graphIndex].push(() => { })
			this.graphs[graphIndex].push(defaultTrace)

		},
		formatBB() {
			sendCommand(MspFn.BB_FORMAT);
		},
		getLog(num: number): Promise<BBLog> {
			this.binFileNumber = -1;

			sendCommand(MspFn.BB_FILE_INIT, [...intToLeBytes(num, 2)]);
			return new Promise((resolve, reject) => {
				this.resolveWhenReady = resolve;
				this.rejectWrongFile = reject;
			});
		},
		openLogFromFile() {
			const input = document.createElement('input');
			input.type = 'file';
			input.accept = '.kbb, .json';
			input.onchange = () => {
				const file = input.files?.[0];
				if (!file) return;
				if (file.name.endsWith('.json')) {
					const reader = new FileReader();
					reader.onload = () => {
						const str = reader.result as string;
						try {
							const data = JSON.parse(str);
							if (data.version[0] !== 0) {
								alert('Wrong version number: ' + data.version);
								return;
							}
							data.rawFile = new Uint8Array(data.rawFile);
							this.loadedLog = data as BBLog;
							this.startFrame = 0;
							this.endFrame = data.frameCount - 1;
						} catch (e) { }
					};
					reader.readAsText(file);
				} else {
					const reader = new FileReader();
					reader.onload = () => {
						const data = reader.result as ArrayBuffer;
						this.binFile = new Uint8Array(data);

						this.decodeBinFile();
						this.startFrame = 0;
						this.endFrame = (this.loadedLog?.frameCount || 1) - 1;
					};
					reader.readAsArrayBuffer(file);
				}
			};
			input.click();
		},
		exportLog(type: 'kbb' | 'json' = 'kbb') {
			if (!this.loadedLog) return;
			let blob: Blob;
			// temporarily convert rawFile to Array to prevent having an object as the rawFile
			const raw = this.loadedLog.rawFile;
			const raw2 = Array.from(raw);
			this.loadedLog.rawFile = raw2 as unknown as Uint8Array;
			if (type === 'kbb')
				blob = new Blob([new Uint8Array(this.loadedLog.rawFile)], { type: 'application/octet-stream' });
			else
				blob = new Blob([JSON.stringify(this.loadedLog)], { type: 'application/json' });
			const url = URL.createObjectURL(blob);
			//download file
			const a = document.createElement('a');
			a.href = url;
			a.download = `bb${prefixZeros(this.selected, 2)} ${this.loadedLog.startTime
				.toISOString()
				.replace('T', ' ')
				.replace('.000Z', '')
				.replace(/_/g, '-')}.${type}`;
			a.click();
			URL.revokeObjectURL(url);
			// restore rawFile
			this.loadedLog.rawFile = raw;
		},
		openLog() {
			this.getLog(this.selected)
				.then(data => {
					this.startFrame = 0;
					this.endFrame = data.frameCount - 1;
				})
				.catch(console.error);
		},
		deleteLog() {
			sendCommand(MspFn.BB_FILE_DELETE, [this.selected]);
		},
		onTouchDown(e: TouchEvent) {
			e.preventDefault();
			if (!this.loadedLog) return;
			const touches = [];
			const changedTouches = [];
			for (let i = 0; i < e.touches.length; i++)
				if (e.touches[i].identifier < 2) touches.push(e.touches[i]);
			for (let i = 0; i < e.changedTouches.length; i++)
				if (e.changedTouches[i].identifier < 2) changedTouches.push(e.changedTouches[i]);

			if (changedTouches.length === 1) {
				if (this.touchMode === 'none') {
					this.touchMode = 'move';
					this.touchStartX = changedTouches[0].clientX;
					this.startStartFrame = this.startFrame;
					this.startEndFrame = this.endFrame;
				} else {
					this.touchMode = 'zoom';
					this.frame0 =
						this.startFrame + Math.round((this.endFrame - this.startFrame) * (touches[0].clientX / this.domCanvas.width));
					this.frame1 =
						this.startFrame + Math.round((this.endFrame - this.startFrame) * (touches[1].clientX / this.domCanvas.width));
					if (this.frame1 === this.frame0) {
						this.frame1++;
						if (this.frame1 > this.loadedLog.frameCount - 1) {
							this.frame1 = this.loadedLog.frameCount - 1;
							this.frame0 = this.frame1 - 1;
						}
					}
				}
			} else {
				this.touchMode = 'zoom';
				this.frame0 =
					this.startFrame + Math.round((this.endFrame - this.startFrame) * (touches[0].clientX / this.domCanvas.width));
				this.frame1 =
					this.startFrame + Math.round((this.endFrame - this.startFrame) * (touches[1].clientX / this.domCanvas.width));
				if (this.frame1 === this.frame0) {
					this.frame1++;
					if (this.frame1 > this.loadedLog.frameCount - 1) {
						this.frame1 = this.loadedLog.frameCount - 1;
						this.frame0 = this.frame1 - 1;
					}
				}
			}
		},
		onTouchMove(e: TouchEvent) {
			e.preventDefault();
			if (!this.loadedLog) return;
			const touches = [];
			for (let i = 0; i < e.touches.length; i++)
				if (e.touches[i].identifier < 2) touches.push(e.touches[i]);
			if (this.touchMode === 'move') {
				const diff = touches[0].clientX - this.touchStartX;
				const ratio = (this.startStartFrame - this.startEndFrame) / this.domCanvas.width;
				let deltaFrames = Math.floor(diff * ratio);
				if (this.startEndFrame + deltaFrames > this.loadedLog!.frameCount - 1)
					deltaFrames = this.loadedLog!.frameCount - 1 - this.startEndFrame;
				if (this.startStartFrame + deltaFrames < 0) deltaFrames = -this.startStartFrame;
				this.startFrame = this.startStartFrame + deltaFrames;
				this.endFrame = this.startEndFrame + deltaFrames;
			}
			if (this.touchMode === 'zoom') {
				let span = Math.round(
					((this.frame1 - this.frame0) * this.domCanvas.width) / (e.touches[1].clientX - e.touches[0].clientX)
				);
				if (span < 0) span = this.loadedLog.frameCount - 1;
				if (span >= this.loadedLog.frameCount - 1) {
					this.startFrame = 0;
					this.endFrame = this.loadedLog.frameCount - 1;
					return;
				}
				const frameCenter = Math.round((this.frame0 + this.frame1) / 2);
				const centerPos = (e.touches[0].clientX + e.touches[1].clientX) / 2;
				this.startFrame = frameCenter - Math.round(span * (centerPos / this.domCanvas.width));
				this.endFrame = frameCenter + Math.round(span * (1 - centerPos / this.domCanvas.width));
				if (this.startFrame < 0) {
					this.endFrame -= this.startFrame;
					this.startFrame = 0;
				}
				if (this.endFrame >= this.loadedLog.frameCount) {
					this.startFrame -= this.endFrame - this.loadedLog.frameCount + 1;
					this.endFrame = this.loadedLog.frameCount - 1;
				}
			}
		},
		onTouchUp(e: TouchEvent) {
			e.preventDefault();
			if (!this.loadedLog) return;
			if (this.touchMode === 'none') return;
			if (this.touchMode === 'move' && (e.touches[0]?.identifier || 2) >= 2) {
				this.touchMode = 'none';
				return;
			}
			if (this.touchMode === 'zoom') {
				if (
					(e.touches[0]?.identifier === 0 || e.touches[0]?.identifier === 1) &&
					e.touches[1]?.identifier !== 1
				) {
					this.touchMode = 'move';
					this.touchStartX = e.touches[0].clientX;
					this.startStartFrame = this.startFrame;
					this.startEndFrame = this.endFrame;
					return;
				} else if (e.touches[1]?.identifier !== 1) {
					this.touchMode = 'none';
				}
			}
		},
		drawSelection(startX: number, endX: number) {
			const ctx = this.selectionCanvas.getContext('2d') as CanvasRenderingContext2D;
			ctx.clearRect(0, 0, this.selectionCanvas.width, this.selectionCanvas.height);
			ctx.fillStyle = 'rgba(0,0,0,0.2)';
			ctx.fillRect(0, 0, this.selectionCanvas.width, this.selectionCanvas.height);
			ctx.clearRect(startX, 0, endX - startX, this.selectionCanvas.height);
			ctx.strokeStyle = 'white';
			ctx.lineWidth = 1;
			ctx.beginPath();
			ctx.moveTo(startX, 0);
			ctx.lineTo(startX, this.selectionCanvas.height);
			ctx.stroke();
			ctx.beginPath();
			ctx.moveTo(endX, 0);
			ctx.lineTo(endX, this.selectionCanvas.height);
			ctx.stroke();
			const domCtx = this.domCanvas.getContext('2d') as CanvasRenderingContext2D;
			domCtx.clearRect(0, 0, this.domCanvas.width, this.domCanvas.height);
			domCtx.drawImage(this.canvas, 0, 0);
			domCtx.drawImage(this.selectionCanvas, 0, 0);
		},
		onMouseMove(e: MouseEvent) {
			if (!this.loadedLog) return;
			if (e.buttons !== 1) {
				this.onMouseUp();
				// highlight all points on the current frame
				const domCanvas = this.domCanvas;
				const domCtx = domCanvas.getContext('2d') as CanvasRenderingContext2D;
				domCtx.clearRect(0, 0, domCanvas.width, domCanvas.height);
				domCtx.drawImage(this.canvas, 0, 0);
				// @ts-expect-error
				const length = this.sliceAndSkip[Object.keys(this.sliceAndSkip)[0]].length as number;
				const closestFrameSliceSkip = Math.round(
					(e.offsetX / domCanvas.width) * (length - 1)
				);
				const closestFrameNum = this.startFrame + closestFrameSliceSkip * this.skipValue;
				//draw vertical line
				const highlightCanvas = document.createElement('canvas');
				highlightCanvas.width = domCanvas.width;
				highlightCanvas.height = domCanvas.height;
				const ctx = highlightCanvas.getContext('2d') as CanvasRenderingContext2D;
				ctx.strokeStyle = 'white';
				ctx.lineWidth = 1;
				ctx.beginPath();
				const height = this.dataViewerWrapper.clientHeight * 0.98; //1% free space top and bottom
				const width = this.dataViewerWrapper.clientWidth;
				const frameWidth = width / (length - 1);
				const frameX = closestFrameSliceSkip * frameWidth;
				ctx.moveTo(frameX, 0);
				ctx.lineTo(frameX, highlightCanvas.height);
				ctx.stroke();
				//iterate over all graphs and draw the points
				const numGraphs = this.graphs.length;
				const heightPerGraph =
					(height - this.dataViewerWrapper.clientHeight * 0.02 * (numGraphs - 1)) / numGraphs;
				let heightOffset = 0.01 * this.dataViewerWrapper.clientHeight;
				const frame: LogData = {} // contains arrays with length 1 to reduce the amount of types
				for (const key in this.sliceAndSkip) {
					// @ts-expect-error
					frame[key] = this.sliceAndSkip[key].slice(closestFrameSliceSkip, closestFrameSliceSkip + 1);
				}
				for (let i = 0; i < numGraphs; i++) {
					const graph = this.graphs[i];
					const numTraces = graph.length;
					for (let j = 0; j < numTraces; j++) {
						const trace = graph[j];
						const range = trace.maxValue - trace.minValue;
						const scale = heightPerGraph / range;
						ctx.strokeStyle = trace.color;
						ctx.lineWidth = 1 + trace.strokeWidth;
						if (!trace.path) continue
						const min = Math.min(trace.minValue, trace.maxValue);
						const max = Math.max(trace.minValue, trace.maxValue);
						// @ts-expect-error
						const data = constrain(trace.overrideData ? trace.overrideSliceAndSkip![closestFrameSliceSkip] : frame[trace.path][0], min, max);
						const pointY = heightOffset + heightPerGraph - (data - trace.minValue) * scale;
						ctx.beginPath();
						ctx.arc(frameX, pointY, 2.5 + trace.strokeWidth * 1.5, 0, Math.PI * 2);
						ctx.stroke();
					}
					heightOffset += heightPerGraph + 0.02 * this.dataViewerWrapper.clientHeight;
				}
				//write down frame number, time in s after start and values next to the cursor at the top
				let t = (closestFrameNum / this.loadedLog!.framesPerSecond).toFixed(3)
				if (this.loadedLog!.logData.timestamp) t = (this.loadedLog!.logData.timestamp[closestFrameNum] / 1e6).toFixed(4)
				const timeText = t + 's, Frame ' + closestFrameNum;
				const valueTexts: string[] = [];
				for (let i = 0; i < numGraphs; i++) {
					const graph = this.graphs[i];
					const numTraces = graph.length;
					for (let j = 0; j < numTraces; j++) {
						const trace = graph[j];
						if (!trace.path) continue;
						let value: number | string = trace.overrideData
							? trace.overrideSliceAndSkip![closestFrameSliceSkip]
							//@ts-expect-error
							: frame[trace.path][0] as number;
						value = trace.states ? trace.states[value] || value : value.toFixed(trace.decimals)
						valueTexts.push(trace.displayName + ': ' + value + ' ' + trace.unit);
					}
				}
				const textHeight = 14;
				const textPadding = 6;
				const textBorderRadius = 8;
				ctx.font = '14px sans-serif';
				const textWidth = Math.max(
					...valueTexts.map(t => ctx.measureText(t).width),
					ctx.measureText(timeText).width
				);
				const textHeightTotal = textHeight * (valueTexts.length + 1) + textPadding * 2;
				const textX = Math.min(
					frameX + 8,
					domCanvas.width - textWidth - textPadding * 2 - textBorderRadius * 2 - 10
				);
				const textY = 8;
				ctx.fillStyle = 'rgba(255, 255, 255, 0.7)';
				ctx.beginPath();
				ctx.moveTo(textX + textBorderRadius, textY);
				ctx.lineTo(textX + textWidth + textPadding * 2 + textBorderRadius + 10, textY);
				ctx.arc(
					textX + textWidth + textPadding * 2 + textBorderRadius + 10,
					textY + textBorderRadius,
					textBorderRadius,
					Math.PI * 1.5,
					Math.PI * 2
				);
				ctx.lineTo(
					textX + textWidth + textPadding * 2 + textBorderRadius * 2 + 10,
					textY + textHeightTotal - textBorderRadius
				);
				ctx.arc(
					textX + textWidth + textPadding * 2 + textBorderRadius + 10,
					textY + textHeightTotal - textBorderRadius,
					textBorderRadius,
					0,
					Math.PI * 0.5
				);
				ctx.lineTo(textX + textBorderRadius, textY + textHeightTotal);
				ctx.arc(
					textX + textBorderRadius,
					textY + textHeightTotal - textBorderRadius,
					textBorderRadius,
					Math.PI * 0.5,
					Math.PI
				);
				ctx.lineTo(textX, textY + textBorderRadius);
				ctx.arc(
					textX + textBorderRadius,
					textY + textBorderRadius,
					textBorderRadius,
					Math.PI,
					Math.PI * 1.5
				);
				ctx.fill();
				ctx.fillStyle = 'black';
				ctx.textBaseline = 'top';
				ctx.fillText(timeText, textX + textPadding, textY + textPadding);
				for (let i = 0; i < valueTexts.length; i++) {
					ctx.fillStyle = 'black';
					ctx.fillText(
						valueTexts[i],
						textX + textPadding + 20,
						textY + textPadding + textHeight * (i + 1)
					);
				}
				let pointY = textY + textPadding + textHeight + 6;
				for (let i = 0; i < this.graphs.length; i++) {
					for (let j = 0; j < this.graphs[i].length; j++) {
						if (!this.graphs[i][j].path) continue;
						ctx.fillStyle = this.graphs[i][j].color;
						ctx.beginPath();
						ctx.arc(textX + textPadding + 8, pointY, 5, 0, Math.PI * 2);
						ctx.fill();
						pointY += textHeight;
					}
				}
				domCtx.drawImage(highlightCanvas, 0, 0);
				return;
			}
			if (this.trackingStartX === -1) return;
			if (this.trackingStartX === -2) {
				const ratio = (this.startStartFrame - this.startEndFrame) / this.domCanvas.width;
				const diff = e.offsetX - this.firstX;
				let deltaFrames = Math.floor(diff * ratio);
				if (this.startEndFrame + deltaFrames > this.loadedLog!.frameCount - 1)
					deltaFrames = this.loadedLog!.frameCount - 1 - this.startEndFrame;
				if (this.startStartFrame + deltaFrames < 0) deltaFrames = -this.startStartFrame;
				this.startFrame = this.startStartFrame + deltaFrames;
				this.endFrame = this.startEndFrame + deltaFrames;
				return;
			}
			this.trackingEndX = e.clientX;
			this.drawSelection(this.trackingStartX, this.trackingEndX);
		},
		onMouseDown(e: MouseEvent) {
			if (!this.loadedLog || e.button !== 0) return;
			if (!e.shiftKey && (this.startFrame !== 0 || this.endFrame !== this.loadedLog.frameCount - 1)) {
				this.trackingStartX = -2;
				this.firstX = e.offsetX;
				this.startStartFrame = this.startFrame;
				this.startEndFrame = this.endFrame;
				return;
			}
			this.trackingStartX = e.offsetX;
			this.trackingEndX = e.offsetX;
			this.drawSelection(this.trackingStartX, this.trackingEndX);
		},
		onMouseUp() {
			if (this.trackingStartX === -1) return;
			if (this.trackingStartX === -2) {
				this.trackingStartX = -1;
				return;
			}
			if (Math.abs(this.trackingStartX - this.trackingEndX) < 2) {
				this.trackingStartX = -1;
				this.redrawSameCanvas();
				return;
			}
			if (this.trackingStartX > this.trackingEndX) {
				const p = this.trackingStartX;
				this.trackingStartX = this.trackingEndX;
				this.trackingEndX = p;
			}
			const nStart =
				this.startFrame + Math.floor((this.endFrame - this.startFrame) * (this.trackingStartX / this.domCanvas.width));
			const nEnd =
				this.startFrame + Math.floor((this.endFrame - this.startFrame) * (this.trackingEndX / this.domCanvas.width));
			this.startFrame = Math.min(nStart, nEnd);
			this.endFrame = Math.max(nStart, nEnd);
			this.trackingStartX = -1;
		},
		onMouseWheel(e: WheelEvent) {
			e.preventDefault();
			if (!this.loadedLog) return;
			if (e.getModifierState('Control')) {
				let visibleFrames = this.endFrame - this.startFrame;
				const leftPct = e.offsetX / this.dataViewerWrapper.clientWidth;
				const grabFrame = this.startFrame + visibleFrames * leftPct;
				const zoomFactor = e.deltaY > 0 ? 1.333 : 0.75;
				visibleFrames *= zoomFactor;
				if (visibleFrames < 10) visibleFrames = 10;
				this.startFrame = grabFrame - visibleFrames * leftPct;
				this.endFrame = this.startFrame + visibleFrames;
				this.startFrame = Math.round(this.startFrame);
				this.endFrame = Math.round(this.endFrame);
				visibleFrames = this.endFrame - this.startFrame;
				if (this.startFrame < 0) {
					this.startFrame = 0;
					this.endFrame = visibleFrames;
				}
				if (this.endFrame > this.loadedLog.frameCount - 1) {
					this.endFrame = this.loadedLog.frameCount - 1;
					this.startFrame = this.endFrame - visibleFrames;
				}
				if (this.startFrame < 0) {
					this.startFrame = 0;
					this.endFrame = this.loadedLog.frameCount - 1;
				}
				return;
			}
			const visibleFrames = this.endFrame - this.startFrame;
			let moveBy = e.deltaY * 0.002 * visibleFrames;
			if (moveBy > 0 && moveBy < 1) moveBy = 1;
			if (moveBy < 0 && moveBy > -1) moveBy = -1;
			if (moveBy > visibleFrames * 0.3) moveBy = visibleFrames * 0.3;
			if (moveBy < -visibleFrames * 0.3) moveBy = -visibleFrames * 0.3;
			moveBy = Math.round(moveBy);
			if (this.startFrame + moveBy < 0) moveBy = -this.startFrame;
			if (this.endFrame + moveBy > this.loadedLog.frameCount - 1) moveBy = this.loadedLog.frameCount - 1 - this.endFrame;
			this.startFrame += moveBy;
			this.endFrame += moveBy;
		},
		onMouseLeave(e: MouseEvent) {
			if (e.buttons !== 1) {
				this.redrawSameCanvas();
			}
		},
		drawCanvas(allowShortening = true) {
			if (!this.loadedLog) return;
			this.canvas.width = this.domCanvas.width;
			this.canvas.height = this.domCanvas.height;
			const ctx = this.canvas.getContext('2d') as CanvasRenderingContext2D;
			/**
			 * the drawing canvas has several graphs in it (in one column)
			 * each graph has a set of traces
			 * the whole drawing board has 1% of the height of free space on the top and bottom, as well as 2% oh the height of free space between each graph, no free space left and right
			 * each trace has a range, which represents the top and bottom on the graph for that trace
			 * a modifier appears for some flags, like motor outputs to define one specific motor for example
			 */
			const height = this.dataViewerWrapper.clientHeight;
			const width = this.dataViewerWrapper.clientWidth;
			if (Object.keys(this.dataSlice).length === 0 || this.startFrame === this.endFrame) return;
			this.sliceAndSkip = {}
			this.skipValue = 1;
			//@ts-expect-error
			let length = this.dataSlice[Object.keys(this.dataSlice)[0]].length;
			const everyNth = Math.floor(length / width);
			if (everyNth > 2 && allowShortening) {
				this.skipValue = everyNth;
				this.sliceAndSkip = skipValues(this.dataSlice, everyNth);
				clearTimeout(this.drawFullCanvasTimeout);
				this.drawFullCanvasTimeout = setTimeout(() => this.drawCanvas(false), 250);
			} else {
				this.sliceAndSkip = this.dataSlice;
			}
			const pixelsPerSec =
				(this.dataViewerWrapper.clientWidth * this.loadedLog.framesPerSecond) / (length - 1);
			//filter out all the ones that don't fit
			let durations = DURATION_BAR_RASTER.filter(el => {
				const seconds = this.decodeDuration(el);
				if (seconds * pixelsPerSec >= this.dataViewerWrapper.clientWidth - 80) return false;
				if (seconds * pixelsPerSec <= (this.dataViewerWrapper.clientWidth - 80) * 0.1) return false;
				return true;
			});
			let barDuration = '';
			for (let i = 0; i < durations.length - 1; i++) {
				if (pixelsPerSec * this.decodeDuration(durations[i]) > 200) {
					barDuration = durations[i];
					break;
				}
			}
			barDuration = barDuration || durations[durations.length - 1];
			const barLength = pixelsPerSec * this.decodeDuration(barDuration || '1s');
			ctx.clearRect(0, 0, this.dataViewerWrapper.clientWidth, this.dataViewerWrapper.clientHeight);
			ctx.strokeStyle = 'rgba(255, 255, 255, 1)';
			ctx.lineWidth = 3;
			ctx.textAlign = 'center';
			ctx.font = '16px sans-serif';
			ctx.textBaseline = 'bottom';
			ctx.fillStyle = 'white';
			if (barDuration) {
				ctx.beginPath();
				ctx.moveTo(16, 40);
				ctx.lineTo(16 + barLength, 40);
				ctx.stroke();
				ctx.fillText(barDuration, 16 + barLength / 2, 35);
			}

			//@ts-expect-error
			length = this.sliceAndSkip[Object.keys(this.sliceAndSkip)[0]].length;

			const frameWidth = width / (length - 1);
			const numGraphs = this.graphs.length;
			const heightPerGraph = (height - this.dataViewerWrapper.clientHeight * 0.02 * numGraphs) / numGraphs;
			let heightOffset = 0.01 * this.dataViewerWrapper.clientHeight;
			for (let i = 0; i < numGraphs; i++) {
				ctx.strokeStyle = 'rgba(255, 255, 255, 0.5)';
				ctx.lineWidth = 1;
				ctx.beginPath();
				ctx.moveTo(0, heightOffset);
				ctx.lineTo(width, heightOffset);
				ctx.stroke();
				ctx.beginPath();
				ctx.moveTo(0, heightOffset + heightPerGraph);
				ctx.lineTo(width, heightOffset + heightPerGraph);
				ctx.stroke();
				ctx.beginPath();
				ctx.moveTo(0, heightOffset + heightPerGraph / 2);
				ctx.lineTo(width, heightOffset + heightPerGraph / 2);
				ctx.stroke();

				const graph = this.graphs[i];
				const numTraces = graph.length;
				for (let j = 0; j < numTraces; j++) {
					const trace = graph[j];
					const range = trace.maxValue - trace.minValue;
					const scale = heightPerGraph / range;
					ctx.strokeStyle = trace.color;
					ctx.lineWidth = trace.strokeWidth;
					ctx.lineJoin = 'bevel'
					if (!trace.strokeWidth) continue;
					if (trace.overrideData) {
						const overrideSlice = trace.overrideData.slice(
							Math.max(0, Math.min(this.startFrame, this.endFrame)),
							Math.max(0, Math.max(this.startFrame, this.endFrame)) + 1
						);
						if (everyNth > 2 && allowShortening) {
							const len = Math.ceil(overrideSlice.length / everyNth);
							trace.overrideSliceAndSkip = new Float32Array(len);
							let j = 0;
							for (let i = 0; i < len; i += 1, j += everyNth) {
								trace.overrideSliceAndSkip[i] = overrideSlice[j];
							}
						} else trace.overrideSliceAndSkip = overrideSlice;
					}
					ctx.beginPath();
					const array: LogDataType | Float32Array = trace.overrideData
						? trace.overrideSliceAndSkip
						//@ts-expect-error
						: this.sliceAndSkip[trace.path];
					if (!array) continue; // nothing properly selected
					const min = Math.min(trace.minValue, trace.maxValue);
					const max = Math.max(trace.minValue, trace.maxValue);
					let pointY = heightOffset + heightPerGraph - (constrain(array[0], min, max) - trace.minValue) * scale;
					ctx.moveTo(0, pointY);
					for (let k = 1; k < length; k++) {
						pointY = heightOffset + heightPerGraph - (constrain(array[k], min, max) - trace.minValue) * scale;
						ctx.lineTo(k * frameWidth, pointY);
					}
					ctx.stroke();
				}
				heightOffset += heightPerGraph + 0.02 * this.dataViewerWrapper.clientHeight;
			}
			for (const h of this.loadedLog.highlights) {
				if (h < this.startFrame || h > this.endFrame) continue;
				const highlightX = (h - this.startFrame) * width / (this.endFrame - this.startFrame);
				ctx.strokeStyle = 'rgba(255, 255, 255, 0.4)';
				ctx.lineWidth = 4;
				ctx.beginPath();
				ctx.moveTo(highlightX, 0);
				ctx.lineTo(highlightX, height);
				ctx.stroke();
			}
			const visibleFms: { from?: number, to: number, x: number }[] = [];
			let currentFm: number | undefined = undefined;
			let exitFm: number | undefined = undefined;
			for (const f of this.loadedLog.flightModes) {
				const frame = f.frame;
				const prev = currentFm
				currentFm = f.fm;
				if (frame <= this.endFrame) exitFm = currentFm;
				if (frame < this.startFrame || frame > this.endFrame) continue;
				const x = (frame - this.startFrame) * width / (this.endFrame - this.startFrame);
				visibleFms.push({ from: prev, to: currentFm, x });
			}
			for (const f in visibleFms) {
				// green line
				const fm = visibleFms[f];
				ctx.strokeStyle = 'rgba(0, 255, 0, 1)';
				ctx.lineWidth = 1.5;
				ctx.beginPath();
				ctx.moveTo(fm.x, 0);
				ctx.lineTo(fm.x, height);
				ctx.stroke();

				// draw text
				const n = parseInt(f);
				const prev = visibleFms[n - 1];
				const prevX = prev ? prev.x : -10000;
				const textR = FLIGHT_MODES[fm.to] || 'flight mode';
				const textL = fm.from !== undefined ? FLIGHT_MODES[fm.from] : 'flight mode';
				const widthR = ctx.measureText(textR).width;
				const widthL = ctx.measureText(textL).width;
				ctx.fillStyle = 'rgba(0, 255, 0, 0.2)';
				ctx.fillRect(fm.x, height * 0.99 - 25, widthR + 20, 25);
				ctx.textBaseline = 'middle';
				ctx.textAlign = 'left';
				ctx.font = '16px sans-serif';
				ctx.fillStyle = 'white';
				ctx.fillText(textR, fm.x + 10, height * 0.99 - 12.5);
				const space = fm.x - prevX;
				// draw prev mode if space is enough
				if (space > (widthL + 20) * 3) {
					ctx.fillStyle = 'rgba(0, 255, 0, 0.2)';
					ctx.fillRect(fm.x - widthL - 20, height * 0.99 - 25, widthL + 20, 25);
					ctx.textAlign = 'right';
					ctx.fillStyle = 'white';
					ctx.fillText(textL, fm.x - 10, height * 0.99 - 12.5);
				}
			}
			if (visibleFms.length === 0) {
				// just draw the exitFm in the bottom left corner
				if (exitFm !== undefined) {
					ctx.fillStyle = 'rgba(0, 255, 0, 0.2)';
					const text = FLIGHT_MODES[exitFm] || 'flight mode';
					const w = ctx.measureText(text).width
					ctx.fillRect(0, height * 0.99 - 25, w + 20, 25);
					ctx.fillStyle = 'white';
					ctx.textBaseline = 'middle';
					ctx.textAlign = 'left';
					ctx.font = '16px sans-serif';
					ctx.fillText(text, 10, height * 0.99 - 12.5);
				}
			}
			this.domCanvas.getContext('2d')?.clearRect(0, 0, this.dataViewerWrapper.clientWidth, this.dataViewerWrapper.clientHeight);
			this.domCanvas.getContext('2d')?.drawImage(this.canvas, 0, 0);
		},
		decodeDuration(duration: string): number {
			let seconds = parseFloat(duration.replace(/[a-zA-Z]/g, ''));
			if (duration.endsWith('h')) seconds *= 3600;
			else if (duration.endsWith('min')) seconds *= 60;
			else if (duration.endsWith('ms')) seconds *= 0.001;
			else if (duration.endsWith('us')) seconds *= 0.000001;
			return seconds;
		},
		redrawSameCanvas() {
			const domCtx = this.domCanvas.getContext('2d') as CanvasRenderingContext2D;
			domCtx.clearRect(0, 0, this.domCanvas.width, this.domCanvas.height);
			domCtx.drawImage(this.canvas, 0, 0);
		},
		prepareFileDownload(data: Uint8Array) {
			if (data.length < 10) {
				this.rejectWrongFile('Wrong init frame size: ' + data.length);
				return;
			}
			this.binFileNumber = leBytesToInt(data, 0, 2);
			this.binFile = new Uint8Array(leBytesToInt(data, 2, 4));
			this.receivedChunks = [];
			this.graphs = [[]];
			this.loadedLog = undefined;
			this.chunkSize = leBytesToInt(data, 6, 4);
			this.totalChunks = Math.ceil(this.binFile.length / this.chunkSize);
			sendCommand(MspFn.BB_FILE_DOWNLOAD, [
				...intToLeBytes(this.binFileNumber, 2)
			]);
		},
		getChunkTimeoutFn() {
			if (this.binFileNumber === -1) {
				clearInterval(this.getChunkInterval);
				return;
			}
			// first get the last chunk so that there's no timeout for the others
			if (!this.receivedChunks[this.totalChunks - 1]) {
				console.log('Timeouted, requesting last chunk: ' + (this.totalChunks - 1));
				sendCommand(MspFn.BB_FILE_DOWNLOAD, [
					...intToLeBytes(this.binFileNumber, 2),
					...intToLeBytes(this.totalChunks - 1, 4),
				]);
				return;
			}
			// then check for all the other chunks
			for (let i = 0; i < this.totalChunks; i++) {
				if (!this.receivedChunks[i]) {
					console.log('Timeouted, requesting missing chunk: ' + i);
					sendCommand(MspFn.BB_FILE_DOWNLOAD, [
						...intToLeBytes(this.binFileNumber, 2),
						...intToLeBytes(i, 4),
					]);
					return;
				}
			}
		},
		handleFileChunk(data: Uint8Array) {
			//typically 1030 data bytes per packet
			//bytes 0-1 are the file number
			//bytes 2-5 are the chunk number
			//typically 1024 bytes of binary file data, and less for the last packet

			if (this.binFileNumber !== leBytesToInt(data, 0, 2)) {
				this.rejectWrongFile('Wrong file number: ' + this.binFileNumber);
				this.binFileNumber = -1;
				return;
			}

			const chunkNum = leBytesToInt(data, 2, 4);
			const chunkData = data.slice(6);
			this.receivedChunks[chunkNum] = true;

			// if there was no chunk received for 500ms, request the missing chunk
			clearInterval(this.getChunkInterval);
			this.getChunkInterval = setInterval(this.getChunkTimeoutFn, 500);

			this.binFile.set(chunkData, chunkNum * this.chunkSize);
			if (this.receivedChunks.length === this.totalChunks) {
				for (let i = 0; i < this.totalChunks; i++) {
					if (!this.receivedChunks[i]) {
						console.log('Missing chunk: ' + i);
						sendCommand(MspFn.BB_FILE_DOWNLOAD, [
							...intToLeBytes(this.binFileNumber, 2),
							...intToLeBytes(i, 4),
						]);
						return;
					}
				}
				clearInterval(this.getChunkInterval);
				this.decodeBinFile();
			}
		},
		decodeBinFile() {
			const l = parseBlackbox(this.binFile)
			if (typeof l === 'string') {
				this.rejectWrongFile(
					'Wrong magic number: 0x' + l + ' instead of 0x99a12720, receivedChunks.length: ' + this.receivedChunks.length + ', totalChunks: ' + this.totalChunks
				);
				return;
			}
			fillLogWithGenFlags(l);
			this.loadedLog = l;
			this.resolveWhenReady(l);
		},
		getLogInfo() {
			const infoNums: number[] = [];
			for (let i = 0; i < 10; i++) {
				if (this.logInfoPosition >= this.logNums.length) {
					clearInterval(this.logInfoInterval);
					break;
				}
				infoNums[i * 2] = this.logNums[this.logInfoPosition].num & 0xFF;
				infoNums[i * 2 + 1] = (this.logNums[this.logInfoPosition++].num >> 8) & 0xFF;
			}
			if (infoNums.length == 0) return;
			sendCommand(MspFn.BB_FILE_INFO, infoNums);
		},
		processLogInfo(data: Uint8Array) {
			/* data of response (repeat 17 bytes for each log file):
			 * 0-1: file number
			 * 2-5: file size in bytes
			 * 6-8: version of bb file format
			 * 9-12: time of recording start
			 * 13-16: duration in ms
			 */
			for (let i = 0; i < data.length; i += 17) {
				const fileNum = leBytesToInt(data, i, 2);
				// const fileSize = leBytesToInt(data, i + 2, 4);
				const bbVersion = leBytesToInt(data.slice(i + 6, i + 9).reverse(), 0, 3);
				const startTime = new Date(leBytesToInt(data, i + 9, 4) * 1000);
				if (bbVersion !== 1) continue;
				//append duration of log file to logNums
				const index = this.logNums.findIndex(n => n.num == fileNum);
				if (index == -1) continue;
				const duration = Math.round(leBytesToInt(data, i + 13, 4) / 1000);
				this.logNums[index].text = `${this.logNums[index].num} - ${duration}s - ${startTime.toLocaleString()}`;
				this.selected = fileNum;
			}
		},
	},
	mounted() {
		this.getFileList()
		addOnConnectHandler(this.getFileList);
		window.addEventListener('resize', this.onResize);
		this.domCanvas.addEventListener('touchstart', this.onTouchDown, { passive: false });
		this.domCanvas.addEventListener('touchmove', this.onTouchMove, { passive: false });
		this.domCanvas.addEventListener('touchend', this.onTouchUp, { passive: false });
		this.onResize()
		addOnCommandHandler(this.onCommand)

		this.loadedLog = getSavedLog()
		if (this.loadedLog) {
			const { min, max } = getFrameRange();
			this.startFrame = min;
			this.endFrame = max;
			const d = getGraphs();
			for (const i in d) {
				if (!this.graphs[i]) this.graphs[i] = [];
				if (!this.traceInternalData[i]) this.traceInternalData[i] = [];
				for (const j in d[i]) {
					const g = d[i][j];
					this.graphs[i][j] = g.t
					this.traceInternalData[i][j] = g.s as TraceInternalData;
				}
			}
		} else {
			this.startFrame = 0;
			this.endFrame = 0;
			this.graphs = [[]];
		}
	},
	beforeUnmount() {
		clearTimeout(this.drawFullCanvasTimeout);
		clearInterval(this.getChunkInterval);
		removeOnConnectHandler(this.getFileList);
		removeOnCommandHandler(this.onCommand)
		window.removeEventListener('resize', this.onResize);

		saveLog(this.loadedLog);
		if (this.loadedLog) {
			setFrameRange(this.startFrame, this.endFrame);
			const g = [] as { t: TraceInGraph, s: TraceInternalData | undefined }[][]
			for (const i in this.graphs) {
				g[i] = [];
				for (const j in this.graphs[i]) {
					const s = this.traceInternalBackupFn[i][j]() as TraceInternalData | undefined;
					const t = this.graphs[i][j];
					t.hasSetData = true;
					g[i][j] = { t, s }
				}
			}
			setGraphs(g);
		} else {
			setFrameRange(0, 0);
			setGraphs([[]]);
		}
	}
})
</script>
<template>
	<div class="blackbox">
		<div class="selector">
			<select v-model="selected">
				<option v-for="log in logNums" :value="log.num">{{ log.text || log.num }}</option>
			</select>
			<button @click="openLog" :disabled="selected === -1">Open</button>
			<button @click="deleteLog" :disabled="selected === -1">Delete</button>
			<button @click="formatBB">Format</button>
			<button @click="openLogFromFile">Open from file</button>
			<button @click="() => { exportLog() }" :disabled="!loadedLog">Export KBB</button>
			<button @click="() => { exportLog('json') }" :disabled="!loadedLog">Export JSON</button>
			<button @click="() => { showSettings = true }">Settings</button>
		</div>
		<Settings v-if="showSettings" :flags="BB_ALL_FLAGS" @close="() => { showSettings = false; }" />
		<div class="dataViewerWrapper" ref="dataViewerWrapper">
			<canvas id="bbDataViewer" ref="bbDataViewer" @mousedown="onMouseDown" @mouseup="onMouseUp"
				@mousemove="onMouseMove" @mouseleave="onMouseLeave" @wheel="onMouseWheel" @dblclick="() => {
					startFrame = 0;
					endFrame = (loadedLog?.frameCount || 1) - 1;
				}">
			</canvas>
		</div>
		<div class="flagSelector">
			<div v-for="(graph, graphIndex) in graphs" v-if="loadedLog" class="graphSelector">
				<TracePlacer v-for="(trace, traceIndex) in graph" :key="trace.id" :loadedLog="loadedLog"
					:flagProps="BB_ALL_FLAGS" :genFlagProps="BB_GEN_FLAGS" :trace="trace"
					v-model="graphs[graphIndex][traceIndex]" @update="(t: TraceInGraph) => {
						graphs[graphIndex][traceIndex] = t;
						drawCanvas()
					}" @delete="() => {
						deleteTrace(graphIndex, traceIndex);
					}" :set-data="traceInternalData[graphIndex][traceIndex]" @backupfn="(f: () => TraceInternalData | undefined) => {
						if (!traceInternalBackupFn[graphIndex]) traceInternalBackupFn[graphIndex] = [];
						traceInternalBackupFn[graphIndex][traceIndex] = f;
					}" />
				<button class="addTraceButton" :disabled="!loadedLog?.flags?.length" @click="() => {
					addTrace(graphIndex);
				}">Add Trace</button>
				<button class="deleteGraph" :disabled="!loadedLog?.flags?.length || graphs.length == 1" @click="() => {
					deleteGraph(graphIndex);
				}">Del Graph</button>
			</div>
			<button class="addGraphButton" :disabled="!loadedLog" @click="addGraph">Add Graph</button>
			<div v-if="loadedLog" class="fileInfo" style="margin-top: .8rem">
				<div>Blackbox Version: {{ loadedLog.version.join('.') }}</div>
				<div>Start Time: {{ loadedLog.startTime.toLocaleString() }}</div>
				<div>Duration: {{ loadedLog.duration.toFixed(1) }} s</div>
				<div>Frame Count: {{ loadedLog.frameCount }}</div>
				<div>PID Frequency: {{ loadedLog.pidFrequency }} Hz</div>
				<div>Frames per Second: {{ loadedLog.framesPerSecond }} Hz</div>
				<div style="white-space: pre">Flags: {{'\n - ' +
					loadedLog.flags
						.filter(n => n.startsWith('LOG_'))
						.map(el => BB_ALL_FLAGS[el].name)
						.join('\n - ')}}
				</div>
				<div>File Size: {{ (loadedLog.rawFile.length / 1000).toFixed(1) }} KB</div>
				<div>
					IMU Range: {{ loadedLog.ranges.gyro }}°/sec, ±{{ loadedLog.ranges.accel }}g
				</div>
				<div>
					PID Gains:
					<div>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;ROLL PITCH&nbsp;&nbsp;&nbsp;YAW</div>
					<div>
						&nbsp;&nbsp;P:&nbsp;&nbsp;{{ prefixZeros(loadedLog.pidConstantsNice[0][0], 5, ' ') }}
						{{ prefixZeros(loadedLog.pidConstantsNice[1][0], 5, ' ') }}
						{{ prefixZeros(loadedLog.pidConstantsNice[2][0], 5, ' ') }}
					</div>
					<div>
						&nbsp;&nbsp;I:&nbsp;&nbsp;{{ prefixZeros(loadedLog.pidConstantsNice[0][1], 5, ' ') }}
						{{ prefixZeros(loadedLog.pidConstantsNice[1][1], 5, ' ') }}
						{{ prefixZeros(loadedLog.pidConstantsNice[2][1], 5, ' ') }}
					</div>
					<div>
						&nbsp;&nbsp;D:&nbsp;&nbsp;{{ prefixZeros(loadedLog.pidConstantsNice[0][2], 5, ' ') }}
						{{ prefixZeros(loadedLog.pidConstantsNice[1][2], 5, ' ') }}
						{{ prefixZeros(loadedLog.pidConstantsNice[2][2], 5, ' ') }}
					</div>
					<div>
						&nbsp;&nbsp;FF:&nbsp;{{ prefixZeros(loadedLog.pidConstantsNice[0][3], 5, ' ') }}
						{{ prefixZeros(loadedLog.pidConstantsNice[1][3], 5, ' ') }}
						{{ prefixZeros(loadedLog.pidConstantsNice[2][3], 5, ' ') }}
					</div>
					<div>
						&nbsp;&nbsp;S:&nbsp;&nbsp;{{ prefixZeros(loadedLog.pidConstantsNice[0][4], 5, ' ') }}
						{{ prefixZeros(loadedLog.pidConstantsNice[1][4], 5, ' ') }}
						{{ prefixZeros(loadedLog.pidConstantsNice[2][4], 5, ' ') }}
					</div>
				</div>
				<div class="rateCoeffs">
					Rate Coefficients:
					<div>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;ROLL
						PITCH&nbsp;&nbsp;&nbsp;YAW
					</div>
					<div>
						&nbsp;&nbsp;Center:&nbsp;{{ prefixZeros(loadedLog.rateCoeffs[0].center, 5, ' ') }}
						{{ prefixZeros(loadedLog.rateCoeffs[1].center, 5, ' ') }}
						{{ prefixZeros(loadedLog.rateCoeffs[2].center, 5, ' ') }}
					</div>
					<div>
						&nbsp;&nbsp;Max:&nbsp;&nbsp;&nbsp;&nbsp;{{ prefixZeros(loadedLog.rateCoeffs[0].max, 5, ' ') }}
						{{ prefixZeros(loadedLog.rateCoeffs[1].max, 5, ' ') }}
						{{ prefixZeros(loadedLog.rateCoeffs[2].max, 5, ' ') }}
					</div>
					<div>
						&nbsp;&nbsp;Expo:&nbsp;&nbsp;&nbsp;{{ prefixZeros(loadedLog.rateCoeffs[0].expo.toFixed(2), 5,
							' ') }}
						{{ prefixZeros(loadedLog.rateCoeffs[1].expo.toFixed(2), 5, ' ') }}
						{{ prefixZeros(loadedLog.rateCoeffs[2].expo.toFixed(2), 5, ' ') }}
					</div>
				</div>
			</div>
		</div>
		<div class="timelineWrapper">
			<Timeline :loadedLog="loadedLog" :flagProps="BB_ALL_FLAGS" :genFlagProps="BB_GEN_FLAGS"
				:startFrame="startFrame" :endFrame="endFrame"
				@update="(sf, ef) => { startFrame = Math.min(sf, ef); endFrame = Math.max(sf, ef) }" />
		</div>
	</div>

</template>

<style scoped>
.blackbox {
	width: 100%;
	height: 100%;
	box-sizing: border-box;
	display: grid;
	grid-template-rows: 0fr 1fr 0fr;
	grid-template-columns: 1fr 0fr;
	padding-bottom: 0.7rem;
}

.selector {
	padding: 0.5rem;
	grid-column: span 2;
	display: flex;
	flex-direction: row;
	align-items: center;
}

.selector select {
	width: 14rem;
	appearance: none;
	background-color: transparent;
	border: 1px solid var(--border-color);
	border-radius: 4px;
	padding: 4px 16px 4px 8px;
	background: transparent url('data:image/gif;base64,R0lGODlhBgAGAKEDAFVVVX9/f9TU1CgmNyH5BAEKAAMALAAAAAAGAAYAAAIODA4hCDKWxlhNvmCnGwUAOw==') right center no-repeat !important;
	background-position: calc(100% - 5px) center !important;
	color: var(--text-color);
	outline: none;
}

.selector select option {
	color: #444;
}

button {
	background-color: transparent;
	border: 1px solid var(--border-color);
	border-radius: 4px;
	padding: 4px 8px;
	color: var(--text-color);
	outline: none;
	margin-left: 0.8rem;
}

button[disabled] {
	opacity: 0.5;
	cursor: not-allowed;
}

canvas {
	height: 100%;
	width: 100%;
	display: block;
}

.dataViewerWrapper {
	position: relative;
	overflow: hidden;
	background-color: black;
}

.timelineWrapper {
	grid-column: span 2;
}

.flagSelector {
	width: 23rem;
	overflow: auto;
}

.graphSelector {
	padding: 0.8rem;
	border-bottom: var(--border-color) 1px solid;
}

.addGraphButton {
	margin-top: 0.8rem;
}

.deleteGraph {
	float: right;
}

.fileInfo {
	margin-left: 0.5rem;
}

.fileInfo div {
	font-family: monospace;
	font-size: medium;
}
</style>
