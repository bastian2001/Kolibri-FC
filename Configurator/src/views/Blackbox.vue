<script lang="ts">
import { defineComponent } from "vue";
import Timeline from "@components/blackbox/Timeline.vue";
import Settings from "@components/blackbox/Settings.vue";
import { BBLog, TraceInGraph, Command, LogData, LogDataType } from "@utils/types";
import { constrain, getNestedProperty, intToLeBytes, leBytesToInt, prefixZeros, roundToDecimal, skipValues } from "@utils/utils";
import { MspFn, MspVersion } from "@utils/msp";
import { useLogStore } from "@stores/logStore";
import { addOnCommandHandler, addOnConnectHandler, removeOnCommandHandler, removeOnConnectHandler, sendCommand } from "@/communication/serial";
import TracePlacer from "@components/blackbox/TracePlacer.vue";
import { BB_ALL_FLAGS, BB_GEN_FLAGS } from "@utils/bbFlags";

const DURATION_BAR_RASTER = ['100us', '200us', '500us', '1ms', '2ms', '5ms', '10ms', '20ms', '50ms', '100ms', '200ms', '0.5s', '1s', '2s', '5s', '10s', '20s', '30s', '1min', '2min', '5min', '10min', '20min', '30min', '1h'
];
const ACC_RANGES = [2, 4, 8, 16];
const GYRO_RANGES = [2000, 1000, 500, 250, 125];


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
						this.logNums = Array.from(command.data).map(n => ({ text: '', num: n }));
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
			sendCommand('request', MspFn.BB_FILE_LIST).catch(console.error);
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
		},
		deleteGraph(g: number) {
			this.graphs = this.graphs.filter((_, i) => i !== g);
		},
		deleteTrace(g: number, t: number) {
			this.graphs[g] = this.graphs[g].filter((_, i) => i !== t);
		},
		addTrace(graphIndex: number) {
			const defaultTrace = {
				color: 'transparent',
				maxValue: 10,
				minValue: 0,
				strokeWidth: 1,
				path: '',
				modifier: '',
				decimals: 0,
				unit: '',
				displayName: '',
				id: Math.random()
			};
			this.graphs[graphIndex].push(defaultTrace)
		},
		formatBB() {
			sendCommand('request', MspFn.BB_FORMAT);
		},
		getLog(num: number): Promise<BBLog> {
			this.binFileNumber = -1;

			sendCommand('request', MspFn.BB_FILE_INIT, MspVersion.V2, [...intToLeBytes(num, 2)]);
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
			sendCommand('request', MspFn.BB_FILE_DELETE, MspVersion.V2, [this.selected]);
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
						ctx.lineWidth = trace.strokeWidth * 2;
						// @ts-expect-error
						const data = constrain(trace.overrideData ? trace.overrideSliceAndSkip![closestFrameSliceSkip] : frame[trace.path][0], trace.minValue, trace.maxValue);
						const pointY = heightOffset + heightPerGraph - (data - trace.minValue) * scale;
						ctx.beginPath();
						ctx.arc(frameX, pointY, trace.strokeWidth * 4, 0, Math.PI * 2);
						ctx.stroke();
					}
					heightOffset += heightPerGraph + 0.02 * this.dataViewerWrapper.clientHeight;
				}
				//write down frame number, time in s after start and values next to the cursor at the top
				const timeText =
					(closestFrameNum / this.loadedLog!.framesPerSecond).toFixed(3) + 's, Frame ' + closestFrameNum;
				const valueTexts: string[] = [];
				for (let i = 0; i < numGraphs; i++) {
					const graph = this.graphs[i];
					const numTraces = graph.length;
					for (let j = 0; j < numTraces; j++) {
						const trace = graph[j];
						if (!trace.path) continue;
						let value = trace.overrideData
							? trace.overrideSliceAndSkip![closestFrameSliceSkip]
							: getNestedProperty(frame, trace.path);
						value = roundToDecimal(value, trace.decimals);
						if (trace.states) value = trace.states[value] || value;
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
			const height = this.dataViewerWrapper.clientHeight * 0.98; //1% free space top and bottom
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
			const heightPerGraph = (height - this.dataViewerWrapper.clientHeight * 0.02 * (numGraphs - 1)) / numGraphs;
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
					trace.overrideSliceAndSkip = [];
					if (trace.overrideData) {
						const overrideSlice = trace.overrideData.slice(
							Math.max(0, Math.min(this.startFrame, this.endFrame)),
							Math.max(0, Math.max(this.startFrame, this.endFrame)) + 1
						);
						if (everyNth > 2 && allowShortening) {
							const len = overrideSlice.length;
							for (let i = 0; i < len; i += everyNth) {
								trace.overrideSliceAndSkip.push(overrideSlice[i]);
							}
						} else trace.overrideSliceAndSkip = overrideSlice;
					}
					ctx.beginPath();
					const array: LogDataType | number[] = trace.overrideData
						? trace.overrideSliceAndSkip
						//@ts-expect-error
						: this.sliceAndSkip[trace.path];
					if (!array) continue; // nothing properly selected
					let pointY = heightOffset + heightPerGraph - (constrain(array[0], trace.minValue, trace.maxValue) - trace.minValue) * scale;
					ctx.moveTo(0, pointY);
					for (let k = 1; k < length; k++) {
						pointY = heightOffset + heightPerGraph - (constrain(array[k], trace.minValue, trace.maxValue) - trace.minValue) * scale;
						ctx.lineTo(k * frameWidth, pointY);
					}
					ctx.stroke();
				}
				heightOffset += heightPerGraph + 0.02 * this.dataViewerWrapper.clientHeight;
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
			this.binFileNumber = leBytesToInt(data.slice(0, 2));
			this.binFile = new Uint8Array(leBytesToInt(data.slice(2, 6)));
			this.receivedChunks = [];
			this.graphs = [[]];
			this.loadedLog = undefined;
			this.chunkSize = leBytesToInt(data.slice(6, 10));
			this.totalChunks = Math.ceil(this.binFile.length / this.chunkSize);
			sendCommand('request', MspFn.BB_FILE_DOWNLOAD, MspVersion.V2, [
				...intToLeBytes(this.binFileNumber, 2)
			]);
		},
		handleFileChunk(data: Uint8Array) {
			//typically 1030 data bytes per packet
			//bytes 0-1 are the file number
			//bytes 2-5 are the chunk number
			//typically 1024 bytes of binary file data, and less for the last packet

			if (this.binFileNumber !== leBytesToInt(data.slice(0, 2))) {
				this.rejectWrongFile('Wrong file number: ' + this.binFileNumber);
				this.binFileNumber = -1;
				return;
			}

			const chunkNum = leBytesToInt(data.slice(2, 6));
			const chunkData = data.slice(6);
			this.receivedChunks[chunkNum] = true;

			this.binFile.set(chunkData, chunkNum * this.chunkSize);
			if (this.receivedChunks.length === this.totalChunks) {
				for (let i = 0; i < this.totalChunks; i++) {
					if (!this.receivedChunks[i]) {
						console.log('Missing chunk: ' + i);
						sendCommand('request', MspFn.BB_FILE_DOWNLOAD, MspVersion.V2, [
							...intToLeBytes(this.binFileNumber, 2),
							...intToLeBytes(i, 4),
						]);
						return;
					}
				}
				this.decodeBinFile();
			}
		},
		decodeBinFile() {
			const header = this.binFile.slice(0, 256);
			const data = this.binFile.slice(256);
			const magic = leBytesToInt(header.slice(0, 4));
			if (magic !== 0x99a12720) {
				this.rejectWrongFile(
					'Wrong magic number: 0x' +
					magic.toString(16) +
					' instead of 0x99a12720, receivedChunks.length: ' +
					this.receivedChunks.length +
					', totalChunks: ' +
					this.totalChunks
				);
				return;
			}
			const version = header.slice(4, 7);
			const sTime = leBytesToInt(header.slice(7, 11)); // unix timestamp in seconds (UTC)
			const startTime = new Date(sTime * 1000);
			const pidFreq = 3200 / (1 + header[11]);
			const freqDiv = header[12];
			const rangeByte = header[13];
			const ranges = {
				gyro: GYRO_RANGES[(rangeByte >> 2) & 0b111],
				accel: ACC_RANGES[rangeByte & 0b11]
			};
			const rateFactors: number[][] = [[], [], [], [], []];
			const rfBytes = header.slice(14, 74);
			for (let i = 0; i < 5; i++)
				for (let j = 0; j < 3; j++)
					rateFactors[i][j] = leBytesToInt(rfBytes.slice(i * 12 + j * 4, i * 12 + j * 4 + 4)) / 65536;
			const pidConstants: number[][] = [[], [], []];
			const pidConstantsNice: number[][] = [[], [], []];
			const pcBytes = header.slice(74, 134);
			for (let i = 0; i < 3; i++) {
				pidConstants[i][0] = leBytesToInt(pcBytes.slice(i * 20, i * 20 + 4));
				pidConstantsNice[i][0] = pidConstants[i][0] >> 11;
				pidConstants[i][0] /= 65536;
				pidConstants[i][1] = leBytesToInt(pcBytes.slice(i * 20 + 4, i * 20 + 8));
				pidConstantsNice[i][1] = pidConstants[i][1] >> 3;
				pidConstants[i][1] /= 65536;
				pidConstants[i][2] = leBytesToInt(pcBytes.slice(i * 20 + 8, i * 20 + 12));
				pidConstantsNice[i][2] = pidConstants[i][2] >> 10;
				pidConstants[i][2] /= 65536;
				pidConstants[i][3] = leBytesToInt(pcBytes.slice(i * 20 + 12, i * 20 + 16));
				pidConstantsNice[i][3] = pidConstants[i][3] >> 8;
				pidConstants[i][3] /= 65536;
				pidConstants[i][4] = leBytesToInt(pcBytes.slice(i * 20 + 16, i * 20 + 20));
				pidConstantsNice[i][4] = pidConstants[i][4] >> 8;
				pidConstants[i][4] /= 65536;
			}
			const flags: string[] = [];
			const flagSlice = header.slice(134, 142)
			let frameSize = 0;
			const offsets: { [key: string]: number } = {};
			for (let j = 0; j < 64; j++) {
				const byteNum = Math.floor(j / 8);
				const bitNum = j % 8;
				const flagIsSet = flagSlice[byteNum] & (1 << bitNum);
				if (!flagIsSet || !Object.keys(BB_ALL_FLAGS)[j]) continue;
				flags.push(Object.keys(BB_ALL_FLAGS)[j]);
				offsets[Object.keys(BB_ALL_FLAGS)[j]] = frameSize;
				switch (j) {
					case 26:
					case 35:
					case 36:
					case 37:
						frameSize += 6;
						break;
					case 28:
						frameSize += 1;
						break;
					case 42:
					case 44:
					case 45:
						frameSize += 4;
						break;
					case 43:
						frameSize += 3;
						break;
					default:
						frameSize += 2;
						break;
				}
			}
			const motorPoles = header[142];
			const framesPerSecond = pidFreq / freqDiv;
			const frames = data.length / frameSize;
			const frameCount = Math.floor(frames);
			const logData: LogData = {}
			if (flags.includes('LOG_ROLL_ELRS_RAW')) {
				logData.elrsRoll = new Uint16Array(frameCount);
				const o = offsets['LOG_ROLL_ELRS_RAW'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.elrsRoll[f] = leBytesToInt(data.slice(p, p + 2));
				}
			}
			if (flags.includes('LOG_PITCH_ELRS_RAW')) {
				logData.elrsPitch = new Uint16Array(frameCount);
				const o = offsets['LOG_PITCH_ELRS_RAW'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.elrsPitch[f] = leBytesToInt(data.slice(p, p + 2));
				}
			}
			if (flags.includes('LOG_THROTTLE_ELRS_RAW')) {
				logData.elrsThrottle = new Uint16Array(frameCount);
				const o = offsets['LOG_THROTTLE_ELRS_RAW'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.elrsThrottle[f] = leBytesToInt(data.slice(p, p + 2));
				}
			}
			if (flags.includes('LOG_YAW_ELRS_RAW')) {
				logData.elrsYaw = new Uint16Array(frameCount);
				const o = offsets['LOG_YAW_ELRS_RAW'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.elrsYaw[f] = leBytesToInt(data.slice(p, p + 2));
				}
			}
			if (flags.includes('LOG_ROLL_SETPOINT')) {
				logData.setpointRoll = new Float32Array(frameCount);
				const o = offsets['LOG_ROLL_SETPOINT'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.setpointRoll[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_PITCH_SETPOINT')) {
				logData.setpointPitch = new Float32Array(frameCount);
				const o = offsets['LOG_PITCH_SETPOINT'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.setpointPitch[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_THROTTLE_SETPOINT')) {
				logData.setpointThrottle = new Float32Array(frameCount);
				const o = offsets['LOG_THROTTLE_SETPOINT'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.setpointThrottle[f] = leBytesToInt(data.slice(p, p + 2)) / 32 + 1000; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_YAW_SETPOINT')) {
				logData.setpointYaw = new Float32Array(frameCount);
				const o = offsets['LOG_YAW_SETPOINT'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.setpointYaw[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_ROLL_GYRO_RAW')) {
				logData.gyroRawRoll = new Float32Array(frameCount);
				const o = offsets['LOG_ROLL_GYRO_RAW'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.gyroRawRoll[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_PITCH_GYRO_RAW')) {
				logData.gyroRawPitch = new Float32Array(frameCount);
				const o = offsets['LOG_PITCH_GYRO_RAW'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.gyroRawPitch[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_YAW_GYRO_RAW')) {
				logData.gyroRawYaw = new Float32Array(frameCount);
				const o = offsets['LOG_YAW_GYRO_RAW'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.gyroRawYaw[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_ROLL_PID_P')) {
				logData.pidRollP = new Float32Array(frameCount);
				const o = offsets['LOG_ROLL_PID_P'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.pidRollP[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_ROLL_PID_I')) {
				logData.pidRollI = new Float32Array(frameCount);
				const o = offsets['LOG_ROLL_PID_I'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.pidRollI[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_ROLL_PID_D')) {
				logData.pidRollD = new Float32Array(frameCount);
				const o = offsets['LOG_ROLL_PID_D'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.pidRollD[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_ROLL_PID_FF')) {
				logData.pidRollFF = new Float32Array(frameCount);
				const o = offsets['LOG_ROLL_PID_FF'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.pidRollFF[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_ROLL_PID_S')) {
				logData.pidRollS = new Float32Array(frameCount);
				const o = offsets['LOG_ROLL_PID_S'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.pidRollS[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_PITCH_PID_P')) {
				logData.pidPitchP = new Float32Array(frameCount);
				const o = offsets['LOG_PITCH_PID_P'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.pidPitchP[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_PITCH_PID_I')) {
				logData.pidPitchI = new Float32Array(frameCount);
				const o = offsets['LOG_PITCH_PID_I'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.pidPitchI[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_PITCH_PID_D')) {
				logData.pidPitchD = new Float32Array(frameCount);
				const o = offsets['LOG_PITCH_PID_D'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.pidPitchD[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_PITCH_PID_FF')) {
				logData.pidPitchFF = new Float32Array(frameCount);
				const o = offsets['LOG_PITCH_PID_FF'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.pidPitchFF[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_PITCH_PID_S')) {
				logData.pidPitchS = new Float32Array(frameCount);
				const o = offsets['LOG_PITCH_PID_S'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.pidPitchS[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_YAW_PID_P')) {
				logData.pidYawP = new Float32Array(frameCount);
				const o = offsets['LOG_YAW_PID_P'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.pidYawP[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_YAW_PID_I')) {
				logData.pidYawI = new Float32Array(frameCount);
				const o = offsets['LOG_YAW_PID_I'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.pidYawI[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_YAW_PID_D')) {
				logData.pidYawD = new Float32Array(frameCount);
				const o = offsets['LOG_YAW_PID_D'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.pidYawD[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_YAW_PID_FF')) {
				logData.pidYawFF = new Float32Array(frameCount);
				const o = offsets['LOG_YAW_PID_FF'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.pidYawFF[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_YAW_PID_S')) {
				logData.pidYawS = new Float32Array(frameCount);
				const o = offsets['LOG_YAW_PID_S'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.pidYawS[f] = leBytesToInt(data.slice(p, p + 2), true) / 16; // data is 12.4 fixed point
				}
			}
			if (flags.includes('LOG_MOTOR_OUTPUTS')) {
				logData.motorOutRR = new Uint16Array(frameCount);
				logData.motorOutFR = new Uint16Array(frameCount);
				logData.motorOutRL = new Uint16Array(frameCount);
				logData.motorOutFL = new Uint16Array(frameCount);
				const o = offsets['LOG_MOTOR_OUTPUTS'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					const throttleBytes = data.slice(p, p + 6);
					const motors01 = leBytesToInt(throttleBytes.slice(0, 3));
					const motors23 = leBytesToInt(throttleBytes.slice(3, 6));
					logData.motorOutRR[f] = motors01 & 0xfff;
					logData.motorOutFR[f] = motors01 >> 12;
					logData.motorOutRL[f] = motors23 & 0xfff;
					logData.motorOutFL[f] = motors23 >> 12;
				}
			}
			if (flags.includes('LOG_FRAMETIME')) {
				logData.frametime = new Uint16Array(frameCount);
				const o = offsets['LOG_FRAMETIME'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.frametime[f] = leBytesToInt(data.slice(p, p + 2));
				}
			}
			if (flags.includes('LOG_FLIGHT_MODE')) {
				logData.flightMode = new Uint8Array(frameCount);
				const o = offsets['LOG_FLIGHT_MODE'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.flightMode[f] = data[p];
				}
			}
			if (flags.includes('LOG_ALTITUDE')) {
				logData.altitude = new Float32Array(frameCount);
				const o = offsets['LOG_ALTITUDE'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.altitude[f] = leBytesToInt(data.slice(p, p + 2), true) / 16;
				}
			}
			if (flags.includes('LOG_VVEL')) {
				logData.vvel = new Float32Array(frameCount);
				const o = offsets['LOG_VVEL'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.vvel[f] = leBytesToInt(data.slice(p, p + 2), true) / 256;
				}
			}
			if (flags.includes('LOG_GPS')) {
				logData.gpsYear = new Uint16Array(frameCount)
				logData.gpsMonth = new Uint8Array(frameCount)
				logData.gpsDay = new Uint8Array(frameCount)
				logData.gpsHour = new Uint8Array(frameCount)
				logData.gpsMinute = new Uint8Array(frameCount)
				logData.gpsSecond = new Uint8Array(frameCount)
				logData.gpsTimeValidityFlags = new Uint8Array(frameCount)
				logData.gpsTAcc = new Uint32Array(frameCount)
				logData.gpsNs = new Uint32Array(frameCount)
				logData.gpsFixType = new Uint8Array(frameCount)
				logData.gpsFlags = new Uint8Array(frameCount)
				logData.gpsFlags2 = new Uint8Array(frameCount)
				logData.gpsSatCount = new Uint8Array(frameCount)
				logData.gpsLon = new Float64Array(frameCount)
				logData.gpsLat = new Float64Array(frameCount)
				logData.gpsAlt = new Float32Array(frameCount)
				logData.gpsHAcc = new Float32Array(frameCount)
				logData.gpsVAcc = new Float32Array(frameCount)
				logData.gpsVelN = new Float32Array(frameCount)
				logData.gpsVelE = new Float32Array(frameCount)
				logData.gpsVelD = new Float32Array(frameCount)
				logData.gpsGSpeed = new Float32Array(frameCount)
				logData.gpsHeadMot = new Float32Array(frameCount)
				logData.gpsSAcc = new Float32Array(frameCount)
				logData.gpsHeadAcc = new Float32Array(frameCount)
				logData.gpsPDop = new Float32Array(frameCount)
				logData.gpsFlags3 = new Uint16Array(frameCount)
				const gpsData = new Uint8Array(92);
				const o = offsets['LOG_GPS'];
				for (let f = 0; f < frameCount; f++) {
					const dataPos = f * frameSize + o
					if (frameCount < f + 3) break; // 3 init frames
					if (
						leBytesToInt(data.slice(dataPos, dataPos + 2)) === 20551 && // if this frame has 'G' * 256 + 'P'
						leBytesToInt(data.slice(dataPos + frameSize, dataPos + 2 + frameSize)) === 20563 && // if the next frame has 'S' * 256 + 'P'
						leBytesToInt(data.slice(dataPos + frameSize * 2, dataPos + 2 + frameSize * 2)) === 21590 // if the 2nd next frame has 'V' * 256 + 'T'
					) {
						if (frameCount < f + 49) break; // 3 init frames + 46 frames of data
						for (let j = 0; j < 46; j++) {
							const gpsBytes = data.slice(
								dataPos + frameSize * (3 + j),
								dataPos + frameSize * (3 + j) + 2
							);
							gpsData.set(gpsBytes, j * 2);
						}
						logData.gpsYear[f] = leBytesToInt(gpsData.slice(4, 6))
						logData.gpsMonth[f] = gpsData[6]
						logData.gpsDay[f] = gpsData[7]
						logData.gpsHour[f] = gpsData[8]
						logData.gpsMinute[f] = gpsData[9]
						logData.gpsSecond[f] = gpsData[10]
						logData.gpsTimeValidityFlags[f] = gpsData[11]
						logData.gpsTAcc[f] = leBytesToInt(gpsData.slice(12, 16))
						logData.gpsNs[f] = leBytesToInt(gpsData.slice(16, 20), true)
						logData.gpsFixType[f] = gpsData[20]
						logData.gpsFlags[f] = gpsData[21]
						logData.gpsFlags2[f] = gpsData[22]
						logData.gpsSatCount[f] = gpsData[23]
						logData.gpsLon[f] = leBytesToInt(gpsData.slice(24, 28), true) / 10000000
						logData.gpsLat[f] = leBytesToInt(gpsData.slice(28, 32), true) / 10000000
						logData.gpsAlt[f] = leBytesToInt(gpsData.slice(36, 40), true) / 1000
						logData.gpsHAcc[f] = leBytesToInt(gpsData.slice(40, 44)) / 1000
						logData.gpsVAcc[f] = leBytesToInt(gpsData.slice(44, 48)) / 1000
						logData.gpsVelN[f] = leBytesToInt(gpsData.slice(48, 52), true) / 1000
						logData.gpsVelE[f] = leBytesToInt(gpsData.slice(52, 56), true) / 1000
						logData.gpsVelD[f] = leBytesToInt(gpsData.slice(56, 60), true) / 1000
						logData.gpsGSpeed[f] = leBytesToInt(gpsData.slice(60, 64), true) / 1000
						logData.gpsHeadMot[f] = leBytesToInt(gpsData.slice(64, 68), true) / 100000
						logData.gpsSAcc[f] = leBytesToInt(gpsData.slice(68, 72)) / 1000
						logData.gpsHeadAcc[f] = leBytesToInt(gpsData.slice(72, 76)) / 100000
						logData.gpsPDop[f] = leBytesToInt(gpsData.slice(76, 78)) / 100
						logData.gpsFlags3[f] = leBytesToInt(gpsData.slice(78, 80))
					} else if (f > 0) {
						logData.gpsYear[f] = logData.gpsYear[f - 1]
						logData.gpsMonth[f] = logData.gpsMonth[f - 1]
						logData.gpsDay[f] = logData.gpsDay[f - 1]
						logData.gpsHour[f] = logData.gpsHour[f - 1]
						logData.gpsMinute[f] = logData.gpsMinute[f - 1]
						logData.gpsSecond[f] = logData.gpsSecond[f - 1]
						logData.gpsTimeValidityFlags[f] = logData.gpsTimeValidityFlags[f - 1]
						logData.gpsTAcc[f] = logData.gpsTAcc[f - 1]
						logData.gpsNs[f] = logData.gpsNs[f - 1]
						logData.gpsFixType[f] = logData.gpsFixType[f - 1]
						logData.gpsFlags[f] = logData.gpsFlags[f - 1]
						logData.gpsFlags2[f] = logData.gpsFlags2[f - 1]
						logData.gpsSatCount[f] = logData.gpsSatCount[f - 1]
						logData.gpsLon[f] = logData.gpsLon[f - 1]
						logData.gpsLat[f] = logData.gpsLat[f - 1]
						logData.gpsAlt[f] = logData.gpsAlt[f - 1]
						logData.gpsHAcc[f] = logData.gpsHAcc[f - 1]
						logData.gpsVAcc[f] = logData.gpsVAcc[f - 1]
						logData.gpsVelN[f] = logData.gpsVelN[f - 1]
						logData.gpsVelE[f] = logData.gpsVelE[f - 1]
						logData.gpsVelD[f] = logData.gpsVelD[f - 1]
						logData.gpsGSpeed[f] = logData.gpsGSpeed[f - 1]
						logData.gpsHeadMot[f] = logData.gpsHeadMot[f - 1]
						logData.gpsSAcc[f] = logData.gpsSAcc[f - 1]
						logData.gpsHeadAcc[f] = logData.gpsHeadAcc[f - 1]
						logData.gpsPDop[f] = logData.gpsPDop[f - 1]
						logData.gpsFlags3[f] = logData.gpsFlags3[f - 1]
					}
				}
			}
			if (flags.includes('LOG_ATT_ROLL')) {
				logData.rollAngle = new Float32Array(frameCount);
				const o = offsets['LOG_ATT_ROLL'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.rollAngle[f] =
						((leBytesToInt(data.slice(p, p + 2), true) / 10000) * 180) / Math.PI;
				}
			}
			if (flags.includes('LOG_ATT_PITCH')) {
				logData.pitchAngle = new Float32Array(frameCount);
				const o = offsets['LOG_ATT_PITCH'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.pitchAngle[f] =
						((leBytesToInt(data.slice(p, p + 2), true) / 10000) * 180) / Math.PI;
				}
			}
			if (flags.includes('LOG_ATT_YAW')) {
				logData.yawAngle = new Float32Array(frameCount);
				const o = offsets['LOG_ATT_YAW'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.yawAngle[f] =
						((leBytesToInt(data.slice(p, p + 2), true) / 10000) * 180) / Math.PI;
				}
			}
			if (flags.includes('LOG_MOTOR_RPM')) {
				logData.rpmRR = new Float32Array(frameCount);
				logData.rpmFR = new Float32Array(frameCount);
				logData.rpmRL = new Float32Array(frameCount);
				logData.rpmFL = new Float32Array(frameCount);
				const o = offsets['LOG_MOTOR_RPM'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					const motors01 = leBytesToInt(data.slice(p, p + 3));
					const motors23 = leBytesToInt(data.slice(p + 3, p + 6));
					let rr = motors01 & 0xfff;
					let fr = motors01 >> 12;
					let rl = motors23 & 0xfff;
					let fl = motors23 >> 12;
					if (rr === 0xfff) {
						logData.rpmRR[f] = 0;
					} else {
						rr = (rr & 0x1ff) << (rr >> 9);
						logData.rpmRR[f] = (60000000 + 50 * rr) / rr / (motorPoles / 2);
					}
					if (fr === 0xfff) {
						logData.rpmFR[f] = 0;
					} else {
						fr = (fr & 0x1ff) << (fr >> 9);
						logData.rpmFR[f] = (60000000 + 50 * fr) / fr / (motorPoles / 2);
					}
					if (rl === 0xfff) {
						logData.rpmRL[f] = 0;
					} else {
						rl = (rl & 0x1ff) << (rl >> 9);
						logData.rpmRL[f] = (60000000 + 50 * rl) / rl / (motorPoles / 2);
					}
					if (fl === 0xfff) {
						logData.rpmFL[f] = 0;
					} else {
						fl = (fl & 0x1ff) << (fl >> 9);
						logData.rpmFL[f] = (60000000 + 50 * fl) / fl / (motorPoles / 2);
					}
				}
			}
			if (flags.includes('LOG_ACCEL_RAW')) {
				logData.accelRawX = new Float32Array(frameCount);
				logData.accelRawY = new Float32Array(frameCount);
				logData.accelRawZ = new Float32Array(frameCount);
				const o = offsets['LOG_ACCEL_RAW'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.accelRawX[f] = (leBytesToInt(data.slice(p, p + 2), true) * 9.81) / 2048;
					logData.accelRawY[f] = (leBytesToInt(data.slice(p + 2, p + 4), true) * 9.81) / 2048;
					logData.accelRawZ[f] = (leBytesToInt(data.slice(p + 4, p + 6), true) * 9.81) / 2048;
				}
			}
			if (flags.includes('LOG_ACCEL_FILTERED')) {
				logData.accelFilteredX = new Float32Array(frameCount);
				logData.accelFilteredY = new Float32Array(frameCount);
				logData.accelFilteredZ = new Float32Array(frameCount);
				const o = offsets['LOG_ACCEL_FILTERED'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.accelFilteredX[f] = (leBytesToInt(data.slice(p, p + 2), true) * 9.81) / 2048;
					logData.accelFilteredY[f] = (leBytesToInt(data.slice(p + 2, p + 4), true) * 9.81) / 2048;
					logData.accelFilteredZ[f] = (leBytesToInt(data.slice(p + 4, p + 6), true) * 9.81) / 2048;
				}
			}
			if (flags.includes('LOG_VERTICAL_ACCEL')) {
				logData.accelVertical = new Float32Array(frameCount);
				const o = offsets['LOG_VERTICAL_ACCEL'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.accelVertical[f] = leBytesToInt(data.slice(p, p + 2), true) / 128;
				}
			}
			if (flags.includes('LOG_VVEL_SETPOINT')) {
				logData.setpointVvel = new Float32Array(frameCount);
				const o = offsets['LOG_VVEL_SETPOINT'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.setpointVvel[f] = leBytesToInt(data.slice(p, p + 2), true) / 4096;
				}
			}
			if (flags.includes('LOG_MAG_HEADING')) {
				logData.magHeading = new Float32Array(frameCount);
				const o = offsets['LOG_MAG_HEADING'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.magHeading[f] =
						((leBytesToInt(data.slice(p, p + 2), true) / 8192) * 180) / Math.PI;
				}
			}
			if (flags.includes('LOG_COMBINED_HEADING')) {
				logData.combinedHeading = new Float32Array(frameCount);
				const o = offsets['LOG_COMBINED_HEADING'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.combinedHeading[f] =
						((leBytesToInt(data.slice(p, p + 2), true) / 8192) * 180) / Math.PI;
				}
			}
			if (flags.includes('LOG_HVEL')) {
				logData.hvelN = new Float32Array(frameCount);
				logData.hvelE = new Float32Array(frameCount);
				const o = offsets['LOG_HVEL'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.hvelN[f] = leBytesToInt(data.slice(p, p + 2), true) / 256;
					logData.hvelE[f] = leBytesToInt(data.slice(p + 2, p + 4), true) / 256;
				}
			}
			if (flags.includes('LOG_BARO')) {
				logData.baroRaw = new Uint32Array(frameCount);
				logData.baroHpa = new Float32Array(frameCount);
				logData.baroAlt = new Float32Array(frameCount);
				const o = offsets['LOG_BARO'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.baroRaw[f] = leBytesToInt(data.slice(p, p + 3));
					logData.baroHpa[f] = logData.baroRaw[f] / 4096;
					logData.baroAlt[f] =
						44330 * (1 - Math.pow(logData.baroHpa[f] / 1013.25, 1 / 5.255));
				}
			}
			if (flags.includes('LOG_DEBUG_1')) {
				logData.debug1 = new Int32Array(frameCount);
				const o = offsets['LOG_DEBUG_1'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.debug1[f] = leBytesToInt(data.slice(p, p + 4), true);
				}
			}
			if (flags.includes('LOG_DEBUG_2')) {
				logData.debug2 = new Int32Array(frameCount);
				const o = offsets['LOG_DEBUG_2'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.debug2[f] = leBytesToInt(data.slice(p, p + 4), true);
				}
			}
			if (flags.includes('LOG_DEBUG_3')) {
				logData.debug3 = new Int16Array(frameCount);
				const o = offsets['LOG_DEBUG_3'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.debug3[f] = leBytesToInt(data.slice(p, p + 2), true);
				}
			}
			if (flags.includes('LOG_DEBUG_4')) {
				logData.debug4 = new Int16Array(frameCount);
				const o = offsets['LOG_DEBUG_4'];
				for (let f = 0; f < frameCount; f++) {
					const p = f * frameSize + o
					logData.debug4[f] = leBytesToInt(data.slice(p, p + 2), true);
				}
			}
			this.loadedLog = {
				frameCount: frames,
				flags,
				logData,
				version,
				startTime,
				ranges,
				pidFrequency: pidFreq,
				frequencyDivider: freqDiv,
				rateFactors,
				pidConstants,
				framesPerSecond,
				rawFile: this.binFile,
				isExact: true,
				pidConstantsNice,
				motorPoles
			};
			// this.fillLogWithGenFlags(this.loadedLog);
			this.resolveWhenReady(this.loadedLog);
		},
		getLogInfo() {
			const infoNums: number[] = [];
			for (let i = 0; i < 10; i++) {
				if (this.logInfoPosition >= this.logNums.length) {
					clearInterval(this.logInfoInterval);
					break;
				}
				infoNums[i] = this.logNums[this.logInfoPosition++].num;
			}
			if (infoNums.length == 0) return;
			let checksum = 0;
			for (let i = 0; i < infoNums.length; i++) checksum ^= infoNums[i];
			checksum ^= 0x06;
			checksum ^= infoNums.length;
			sendCommand('request', MspFn.BB_FILE_INFO, MspVersion.V2, infoNums);
		},
		processLogInfo(data: Uint8Array) {
			/* data of response (repeat 22 bytes for each log file)
			 * 0. file number
			 * 1-4. file size in bytes
			 * 5-7. version of bb file format
			 * 8-11: time of recording start
			 * 12. byte that indicates PID frequency
			 * 13. byte that indicates frequency divider
			 * 14-21: recording flags
			 */
			for (let i = 0; i < data.length; i += 22) {
				const fileNum = data[i];
				const fileSize =
					data[i + 1] + data[i + 2] * 256 + data[i + 3] * 256 * 256 + data[i + 4] * 256 * 256 * 256;
				const bbVersion = data[i + 5] * 256 * 256 + data[i + 6] * 256 + data[i + 7];
				const sTime = leBytesToInt(data.slice(i + 8, i + 12), false); // unix timestamp in seconds (UTC)
				const startTime = new Date(sTime * 1000);
				const pidFreq = 3200 / (data[i + 12] + 1);
				const freqDiv = data[i + 13];
				const flags = data.slice(i + 14, i + 22);
				if (bbVersion !== 1) continue;
				const framesPerSecond = pidFreq / freqDiv;
				const dataBytes = fileSize - 256;
				let frameSize = 0;
				for (let j = 0; j < 64; j++) {
					//check flags
					// flag 26 (motors) has 6 bytes per frame, all others only 2
					// flag 28 (flight mode) has 1 byte per frame
					const byteNum = Math.floor(j / 8);
					const bitNum = j % 8;
					const flagIsSet = flags[byteNum] & (1 << bitNum);
					if (!flagIsSet) continue;
					switch (j) {
						case 26:
						case 35:
						case 36:
						case 37:
							frameSize += 6;
							break;
						case 28:
							frameSize += 1;
							break;
						case 42:
						case 44:
						case 45:
							frameSize += 4;
							break;
						case 43:
							frameSize += 3;
							break;
						default:
							frameSize += 2;
							break;
					}
				}
				const frames = dataBytes / frameSize;
				//append duration of log file to logNums
				const index = this.logNums.findIndex(n => n.num == fileNum);
				if (index == -1) continue;
				const duration = Math.round(frames / framesPerSecond);
				this.logNums[index].text = `${this.logNums[index].num} - ${duration}s - ${startTime.toLocaleString()}`;
				this.selected = fileNum;
			}
		},
		// fillLogWithGenFlags(log: BBLog) {
		// 	log.isExact = true;
		// 	const genFlags = Object.keys(BB_GEN_FLAGS);
		// 	for (let i = 0; i < genFlags.length; i++) {
		// 		const flagName = genFlags[i];
		// 		const flag = BB_GEN_FLAGS[flagName];
		// 		if (log.flags.includes(flag.replaces)) continue;
		// 		if (
		// 			flag.requires.every(r => {
		// 				if (typeof r === 'string') return log.flags.includes(r);
		// 				if (Array.isArray(r)) {
		// 					for (const s of r) if (log.flags.includes(s)) return true;
		// 					return false;
		// 				}
		// 				return false;
		// 			})
		// 		) {
		// 			log.flags.push(flagName);
		// 			//generate entries
		// 			if (!flag.exact) log.isExact = false;
		// 			switch (flagName) {
		// 				case 'GEN_ROLL_SETPOINT':
		// 					log.frames.forEach(f => {
		// 						const polynomials: number[] = [(f.elrs.roll! - 1500) / 512];
		// 						for (let i = 1; i < 5; i++) {
		// 							polynomials[i] = polynomials[0] * polynomials[i - 1];
		// 							if (polynomials[0] < 0) polynomials[i] = -polynomials[i];
		// 						}
		// 						f.setpoint.roll = 0;
		// 						for (let i = 0; i < 5; i++) f.setpoint.roll += polynomials[i] * log.rateFactors[i][0];
		// 					});
		// 					break;
		// 				case 'GEN_PITCH_SETPOINT':
		// 					log.frames.forEach(f => {
		// 						const polynomials: number[] = [(f.elrs.pitch! - 1500) / 512];
		// 						for (let i = 1; i < 5; i++) {
		// 							polynomials[i] = polynomials[0] * polynomials[i - 1];
		// 							if (polynomials[0] < 0) polynomials[i] = -polynomials[i];
		// 						}
		// 						f.setpoint.pitch = 0;
		// 						for (let i = 0; i < 5; i++)
		// 							f.setpoint.pitch += polynomials[i] * log.rateFactors[i][1];
		// 					});
		// 					break;
		// 				case 'GEN_THROTTLE_SETPOINT':
		// 					log.frames.forEach(f => {
		// 						f.setpoint.throttle = f.elrs.throttle;
		// 					});
		// 					break;
		// 				case 'GEN_YAW_SETPOINT':
		// 					log.frames.forEach(f => {
		// 						const polynomials: number[] = [(f.elrs.yaw! - 1500) / 512];
		// 						for (let i = 1; i < 5; i++) {
		// 							polynomials[i] = polynomials[0] * polynomials[i - 1];
		// 							if (polynomials[0] < 0) polynomials[i] = -polynomials[i];
		// 						}
		// 						f.setpoint.yaw = 0;
		// 						for (let i = 0; i < 5; i++) f.setpoint.yaw += polynomials[i] * log.rateFactors[i][2];
		// 					});
		// 					break;
		// 				case 'GEN_ROLL_PID_P':
		// 					log.frames.forEach(f => {
		// 						f.pid.roll.p = (f.setpoint.roll! - f.gyro.roll!) * log.pidConstants[0][0];
		// 					});
		// 					break;
		// 				case 'GEN_PITCH_PID_P':
		// 					log.frames.forEach(f => {
		// 						f.pid.pitch.p = (f.setpoint.pitch! - f.gyro.pitch!) * log.pidConstants[1][0];
		// 					});
		// 					break;
		// 				case 'GEN_YAW_PID_P':
		// 					log.frames.forEach(f => {
		// 						f.pid.yaw.p = (f.setpoint.yaw! - f.gyro.yaw!) * log.pidConstants[2][0];
		// 					});
		// 					break;
		// 				case 'GEN_ROLL_PID_D':
		// 					log.frames.forEach((f, i) => {
		// 						f.pid.roll.d =
		// 							(((log.frames[i - 1]?.gyro.roll || 0) - f.gyro.roll!) * log.pidConstants[0][2]) /
		// 							log.frequencyDivider;
		// 					});
		// 					break;
		// 				case 'GEN_PITCH_PID_D':
		// 					log.frames.forEach((f, i) => {
		// 						f.pid.pitch.d =
		// 							(((log.frames[i - 1]?.gyro.pitch || 0) - f.gyro.pitch!) * log.pidConstants[1][2]) /
		// 							log.frequencyDivider;
		// 					});
		// 					break;
		// 				case 'GEN_YAW_PID_D':
		// 					log.frames.forEach((f, i) => {
		// 						f.pid.yaw.d =
		// 							(((log.frames[i - 1]?.gyro.yaw || 0) - f.gyro.yaw!) * log.pidConstants[2][2]) /
		// 							log.frequencyDivider;
		// 					});
		// 					break;
		// 				case 'GEN_ROLL_PID_FF':
		// 					{
		// 						const step = Math.round(8 / log.frequencyDivider) || 1;
		// 						const divider = (step * log.frequencyDivider) / 8;
		// 						log.frames.forEach((f, i) => {
		// 							f.pid.roll.ff =
		// 								((f.setpoint.roll! - log.frames[i - step]?.setpoint.roll! || 0) *
		// 									log.pidConstants[0][3]) /
		// 								divider;
		// 						});
		// 					}
		// 					break;
		// 				case 'GEN_PITCH_PID_FF':
		// 					{
		// 						const step = Math.round(8 / log.frequencyDivider) || 1;
		// 						const divider = step * log.frequencyDivider;
		// 						log.frames.forEach((f, i) => {
		// 							f.pid.pitch.ff =
		// 								((f.setpoint.pitch! - log.frames[i - step]?.setpoint.pitch! || 0) *
		// 									log.pidConstants[1][3]) /
		// 								divider;
		// 						});
		// 					}
		// 					break;
		// 				case 'GEN_YAW_PID_FF':
		// 					{
		// 						const step = Math.round(8 / log.frequencyDivider) || 1;
		// 						const divider = step * log.frequencyDivider;
		// 						log.frames.forEach((f, i) => {
		// 							f.pid.yaw.ff =
		// 								((f.setpoint.yaw! - log.frames[i - step]?.setpoint.yaw! || 0) *
		// 									log.pidConstants[2][3]) /
		// 								divider;
		// 						});
		// 					}
		// 					break;
		// 				case 'GEN_ROLL_PID_S':
		// 					log.frames.forEach(f => {
		// 						f.pid.roll.s = f.setpoint.roll! * log.pidConstants[0][4];
		// 					});
		// 					break;
		// 				case 'GEN_PITCH_PID_S':
		// 					log.frames.forEach(f => {
		// 						f.pid.pitch.s = f.setpoint.pitch! * log.pidConstants[1][4];
		// 					});
		// 					break;
		// 				case 'GEN_YAW_PID_S':
		// 					log.frames.forEach(f => {
		// 						f.pid.yaw.s = f.setpoint.yaw! * log.pidConstants[2][4];
		// 					});
		// 					break;
		// 				case 'GEN_ROLL_PID_I':
		// 					{
		// 						let rollI = 0;
		// 						let takeoffCounter = 0;
		// 						log.frames.forEach(f => {
		// 							rollI += f.setpoint.roll! - f.gyro.roll!;
		// 							if (f.setpoint.throttle! > 1020) takeoffCounter += log.frequencyDivider;
		// 							else if (takeoffCounter < 1000) takeoffCounter = 0;
		// 							if (takeoffCounter < 1000)
		// 								rollI *= Math.pow(log.pidConstants[0][6], log.frequencyDivider);
		// 							f.pid.roll.i = rollI * log.pidConstants[0][1];
		// 						});
		// 					}
		// 					break;
		// 				case 'GEN_PITCH_PID_I':
		// 					{
		// 						let pitchI = 0;
		// 						let takeoffCounter = 0;
		// 						log.frames.forEach(f => {
		// 							pitchI += f.setpoint.pitch! - f.gyro.pitch!;
		// 							if (f.setpoint.throttle! > 1020) takeoffCounter += log.frequencyDivider;
		// 							else if (takeoffCounter < 1000) takeoffCounter = 0;
		// 							if (takeoffCounter < 1000)
		// 								pitchI *= Math.pow(log.pidConstants[1][6], log.frequencyDivider);
		// 							f.pid.pitch.i = pitchI * log.pidConstants[1][1];
		// 						});
		// 					}
		// 					break;
		// 				case 'GEN_YAW_PID_I':
		// 					{
		// 						let yawI = 0;
		// 						let takeoffCounter = 0;
		// 						log.frames.forEach(f => {
		// 							yawI += f.setpoint.yaw! - f.gyro.yaw!;
		// 							if (f.setpoint.throttle! > 1020) takeoffCounter += log.frequencyDivider;
		// 							else if (takeoffCounter < 1000) takeoffCounter = 0;
		// 							if (takeoffCounter < 1000)
		// 								yawI *= Math.pow(log.pidConstants[2][6], log.frequencyDivider);
		// 							f.pid.yaw.i = yawI * log.pidConstants[2][1];
		// 						});
		// 					}
		// 					break;
		// 				case 'GEN_MOTOR_OUTPUTS':
		// 					log.frames.forEach(f => {
		// 						const rollTerm =
		// 							f.pid.roll.p! + f.pid.roll.i! + f.pid.roll.d! + f.pid.roll.ff! + f.pid.roll.s!;
		// 						const pitchTerm =
		// 							f.pid.pitch.p! + f.pid.pitch.i! + f.pid.pitch.d! + f.pid.pitch.ff! + f.pid.pitch.s!;
		// 						const yawTerm =
		// 							f.pid.yaw.p! + f.pid.yaw.i! + f.pid.yaw.d! + f.pid.yaw.ff! + f.pid.yaw.s!;
		// 						f.motors.out = {
		// 							rr: (f.setpoint.throttle! - 1000) * 2 - rollTerm + pitchTerm + yawTerm,
		// 							fr: (f.setpoint.throttle! - 1000) * 2 - rollTerm - pitchTerm - yawTerm,
		// 							rl: (f.setpoint.throttle! - 1000) * 2 + rollTerm + pitchTerm - yawTerm,
		// 							fl: (f.setpoint.throttle! - 1000) * 2 + rollTerm - pitchTerm + yawTerm
		// 						};
		// 						f.motors.out.rr = map(f.motors.out.rr!, 0, 2000, 50, 2000);
		// 						f.motors.out.fr = map(f.motors.out.fr!, 0, 2000, 50, 2000);
		// 						f.motors.out.rl = map(f.motors.out.rl!, 0, 2000, 50, 2000);
		// 						f.motors.out.fl = map(f.motors.out.fl!, 0, 2000, 50, 2000);
		// 						if (f.motors.out.rr! > 2000) {
		// 							const diff = 2000 - f.motors.out.rr!;
		// 							f.motors.out.rr = 2000;
		// 							f.motors.out.fr! -= diff;
		// 							f.motors.out.rl! -= diff;
		// 							f.motors.out.fl! -= diff;
		// 						}
		// 						if (f.motors.out.fr! > 2000) {
		// 							const diff = 2000 - f.motors.out.fr!;
		// 							f.motors.out.fr = 2000;
		// 							f.motors.out.rr! -= diff;
		// 							f.motors.out.rl! -= diff;
		// 							f.motors.out.fl! -= diff;
		// 						}
		// 						if (f.motors.out.rl! > 2000) {
		// 							const diff = 2000 - f.motors.out.rl!;
		// 							f.motors.out.rl = 2000;
		// 							f.motors.out.rr! -= diff;
		// 							f.motors.out.fr! -= diff;
		// 							f.motors.out.fl! -= diff;
		// 						}
		// 						if (f.motors.out.fl! > 2000) {
		// 							const diff = 2000 - f.motors.out.fl!;
		// 							f.motors.out.fl = 2000;
		// 							f.motors.out.rr! -= diff;
		// 							f.motors.out.fr! -= diff;
		// 							f.motors.out.rl! -= diff;
		// 						}
		// 						if (f.motors.out.rr! < 50) {
		// 							const diff = 50 - f.motors.out.rr!;
		// 							f.motors.out.rr = 50;
		// 							f.motors.out.fr! += diff;
		// 							f.motors.out.rl! += diff;
		// 							f.motors.out.fl! += diff;
		// 						}
		// 						if (f.motors.out.fr! < 50) {
		// 							const diff = 50 - f.motors.out.fr!;
		// 							f.motors.out.fr = 50;
		// 							f.motors.out.rr! += diff;
		// 							f.motors.out.rl! += diff;
		// 							f.motors.out.fl! += diff;
		// 						}
		// 						if (f.motors.out.rl! < 50) {
		// 							const diff = 50 - f.motors.out.rl!;
		// 							f.motors.out.rl = 50;
		// 							f.motors.out.rr! += diff;
		// 							f.motors.out.fr! += diff;
		// 							f.motors.out.fl! += diff;
		// 						}
		// 						if (f.motors.out.fl! < 50) {
		// 							const diff = 50 - f.motors.out.fl!;
		// 							f.motors.out.fl = 50;
		// 							f.motors.out.rr! += diff;
		// 							f.motors.out.fr! += diff;
		// 							f.motors.out.rl! += diff;
		// 						}
		// 						f.motors.out.rr = Math.min(f.motors.out.rr!, 2000);
		// 						f.motors.out.fr = Math.min(f.motors.out.fr!, 2000);
		// 						f.motors.out.rl = Math.min(f.motors.out.rl!, 2000);
		// 						f.motors.out.fl = Math.min(f.motors.out.fl!, 2000);
		// 					});
		// 					break;
		// 				case 'GEN_VVEL_SETPOINT':
		// 					log.frames.forEach(f => {
		// 						let t = (f.elrs.throttle! - 1500) * 2;
		// 						if (t > 0) {
		// 							t -= 100;
		// 							if (t < 0) t = 0;
		// 						} else if (t < 0) {
		// 							t += 100;
		// 							if (t > 0) t = 0;
		// 						}
		// 						f.setpoint.vvel = t / 180;
		// 						if (f.flightMode !== undefined && f.flightMode < 2) f.setpoint.vvel = 0;
		// 					});
		// 					break;
		// 			}
		// 		}
		// 	}
		// }
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
	},
	unmounted() {
		clearTimeout(this.drawFullCanvasTimeout);
		removeOnConnectHandler(this.getFileList);
		removeOnCommandHandler(this.onCommand)
		window.removeEventListener('resize', this.onResize);
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
				<div class="rateFactors">
					Rate Factors:
					<div>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;ROLL PITCH&nbsp;&nbsp;&nbsp;YAW</div>
					<div>
						&nbsp;&nbsp;x^1:&nbsp;{{ prefixZeros(loadedLog.rateFactors[0][0], 5, ' ') }}
						{{ prefixZeros(loadedLog.rateFactors[0][1], 5, ' ') }}
						{{ prefixZeros(loadedLog.rateFactors[0][2], 5, ' ') }}
					</div>
					<div>
						&nbsp;&nbsp;x^2:&nbsp;{{ prefixZeros(loadedLog.rateFactors[1][0], 5, ' ') }}
						{{ prefixZeros(loadedLog.rateFactors[1][1], 5, ' ') }}
						{{ prefixZeros(loadedLog.rateFactors[1][2], 5, ' ') }}
					</div>
					<div>
						&nbsp;&nbsp;x^3:&nbsp;{{ prefixZeros(loadedLog.rateFactors[2][0], 5, ' ') }}
						{{ prefixZeros(loadedLog.rateFactors[2][1], 5, ' ') }}
						{{ prefixZeros(loadedLog.rateFactors[2][2], 5, ' ') }}
					</div>
					<div>
						&nbsp;&nbsp;x^4:&nbsp;{{ prefixZeros(loadedLog.rateFactors[3][0], 5, ' ') }}
						{{ prefixZeros(loadedLog.rateFactors[3][1], 5, ' ') }}
						{{ prefixZeros(loadedLog.rateFactors[3][2], 5, ' ') }}
					</div>
					<div>
						&nbsp;&nbsp;x^5:&nbsp;{{ prefixZeros(loadedLog.rateFactors[4][0], 5, ' ') }}
						{{ prefixZeros(loadedLog.rateFactors[4][1], 5, ' ') }}
						{{ prefixZeros(loadedLog.rateFactors[4][2], 5, ' ') }}
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
}

.selector {
	display: flex;
	flex-direction: row;
	align-items: center;
}

.selector select {
	width: 14rem;
	background-color: transparent;
	border: 1px solid var(--border-color);
	border-radius: 4px;
	padding: 3px 6px;
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
