<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { port, type Command, ConfigCmd } from '../../stores';
	import TracePlacer from './tracePlacer.svelte';
	type TraceInGraph = {
		flagName: string;
		color: string;
		strokeWidth: number;
		minValue: number;
		maxValue: number;
		modifier: any;
	};

	let graphs = [[]] as TraceInGraph[][];

	const ACC_RANGES = [2, 4, 8, 16];
	const GYRO_RANGES = [2000, 1000, 500, 250, 125];

	let dataViewer: HTMLDivElement;

	let dataSlice = [] as LogFrame[];

	$: dataSlice =
		loadedLog?.frames.slice(
			Math.max(0, Math.min(startFrame, endFrame)),
			Math.max(0, Math.max(startFrame, endFrame))
		) || [];
	$: dataSlice, drawCanvas();

	let logNums = [] as { text: string; num: number }[];
	type BBLog = {
		frameCount: number;
		rawFile: number[];
		flags: string[];
		frames: LogFrame[];
		version: number[];
		startTime: number;
		ranges: {
			gyro: number;
			accel: number;
		};
		pidFrequency: number;
		frequencyDivider: number;
		rateFactors: number[][];
		pidConstants: number[][];
		framesPerSecond: number;
	};

	let loadedLog: BBLog | undefined;

	let binFile = [] as number[];
	let binFileNumber = -1;
	let receivedChunks = [] as boolean[];
	let totalChunks = -1;
	let resolveWhenReady = (log: BBLog) => {};
	let rejectWrongFile = (_: string) => {};
	let startFrame = 0;
	let endFrame = 0;
	let mounted = false;

	type LogFrame = {
		elrs: {
			roll?: number;
			pitch?: number;
			throttle?: number;
			yaw?: number;
		};
		setpoint: {
			roll?: number;
			pitch?: number;
			throttle?: number;
			yaw?: number;
		};
		gyro: {
			roll?: number;
			pitch?: number;
			yaw?: number;
		};
		pid: {
			roll: {
				p?: number;
				i?: number;
				d?: number;
				ff?: number;
				s?: number;
			};
			pitch: {
				p?: number;
				i?: number;
				d?: number;
				ff?: number;
				s?: number;
			};
			yaw: {
				p?: number;
				i?: number;
				d?: number;
				ff?: number;
				s?: number;
			};
		};
		motors: {
			rr?: number;
			fr?: number;
			rl?: number;
			fl?: number;
		};
		altitude?: number;
	};

	const getGyroBBRange = (file: BBLog | undefined) => {
		if (!file) return { max: -2000, min: 2000 };
		let maxSetpoints = [0, 0, 0];
		for (let exp = 0; exp < 5; exp++)
			for (let ax = 0; ax < 3; ax++) maxSetpoints[ax] += file.rateFactors[exp][ax];
		const max = Math.max(
			...maxSetpoints,
			...file.frames.map((f) => Math.max(f.gyro.roll || 0, f.gyro.pitch || 0, f.gyro.yaw || 0))
		);
		const min = Math.min(
			...file.frames.map((f) => Math.min(f.gyro.roll || 0, f.gyro.pitch || 0, f.gyro.yaw || 0))
		);
		const fullRange = Math.max(max, -min);
		return { max: fullRange, min: -fullRange };
	};

	const BB_ALL_FLAGS = {
		LOG_ROLL_ELRS_RAW: {
			name: 'Roll ELRS Raw',
			path: 'elrs.roll',
			minValue: 988,
			maxValue: 2012,
			unit: 'µs'
		},
		LOG_PITCH_ELRS_RAW: {
			name: 'Pitch ELRS Raw',
			path: 'elrs.pitch',
			minValue: 988,
			maxValue: 2012,
			unit: 'µs'
		},
		LOG_THROTTLE_ELRS_RAW: {
			name: 'Throttle ELRS Raw',
			path: 'elrs.throttle',
			minValue: 1000,
			maxValue: 2000,
			unit: 'µs'
		},
		LOG_YAW_ELRS_RAW: {
			name: 'Yaw ELRS Raw',
			path: 'elrs.yaw',
			minValue: 988,
			maxValue: 2012,
			unit: 'µs'
		},
		LOG_ROLL_SETPOINT: {
			name: 'Roll Setpoint',
			path: 'setpoint.roll',
			rangeFn: getGyroBBRange,
			unit: '°/sec'
		},
		LOG_PITCH_SETPOINT: {
			name: 'Pitch Setpoint',
			path: 'setpoint.pitch',
			rangeFn: getGyroBBRange,
			unit: '°/sec'
		},
		LOG_THROTTLE_SETPOINT: {
			name: 'Throttle Setpoint',
			path: 'setpoint.throttle',
			minValue: 1000,
			maxValue: 2000,
			unit: '°/sec'
		},
		LOG_YAW_SETPOINT: {
			name: 'Yaw Setpoint',
			path: 'setpoint.yaw',
			rangeFn: getGyroBBRange,
			unit: '°/sec'
		},
		LOG_ROLL_GYRO_RAW: {
			name: 'Roll Gyro Raw',
			path: 'gyro.roll',
			rangeFn: getGyroBBRange,
			unit: '°/sec'
		},
		LOG_PITCH_GYRO_RAW: {
			name: 'Pitch Gyro Raw',
			path: 'gyro.pitch',
			rangeFn: getGyroBBRange,
			unit: '°/sec'
		},
		LOG_YAW_GYRO_RAW: {
			name: 'Yaw Gyro Raw',
			path: 'gyro.yaw',
			rangeFn: getGyroBBRange,
			unit: '°/sec'
		},
		LOG_ROLL_PID_P: {
			name: 'Roll PID P',
			path: 'pid.roll.p',
			minValue: -1500,
			maxValue: 1500,
			unit: ''
		},
		LOG_ROLL_PID_I: {
			name: 'Roll PID I',
			path: 'pid.roll.i',
			minValue: -1500,
			maxValue: 1500,
			unit: ''
		},
		LOG_ROLL_PID_D: {
			name: 'Roll PID D',
			path: 'pid.roll.d',
			minValue: -1500,
			maxValue: 1500,
			unit: ''
		},
		LOG_ROLL_PID_FF: {
			name: 'Roll PID FF',
			path: 'pid.roll.ff',
			minValue: -1500,
			maxValue: 1500,
			unit: ''
		},
		LOG_ROLL_PID_S: {
			name: 'Roll PID S',
			path: 'pid.roll.s',
			minValue: -1500,
			maxValue: 1500,
			unit: ''
		},
		LOG_PITCH_PID_P: {
			name: 'Pitch PID P',
			path: 'pid.pitch.p',
			minValue: -1500,
			maxValue: 1500,
			unit: ''
		},
		LOG_PITCH_PID_I: {
			name: 'Pitch PID I',
			path: 'pid.pitch.i',
			minValue: -1500,
			maxValue: 1500,
			unit: ''
		},
		LOG_PITCH_PID_D: {
			name: 'Pitch PID D',
			path: 'pid.pitch.d',
			minValue: -1500,
			maxValue: 1500,
			unit: ''
		},
		LOG_PITCH_PID_FF: {
			name: 'Pitch PID FF',
			path: 'pid.pitch.ff',
			minValue: -1500,
			maxValue: 1500,
			unit: ''
		},
		LOG_PITCH_PID_S: {
			name: 'Pitch PID S',
			path: 'pid.pitch.s',
			minValue: -1500,
			maxValue: 1500,
			unit: ''
		},
		LOG_YAW_PID_P: {
			name: 'Yaw PID P',
			path: 'pid.yaw.p',
			minValue: -1500,
			maxValue: 1500,
			unit: ''
		},
		LOG_YAW_PID_I: {
			name: 'Yaw PID I',
			path: 'pid.yaw.i',
			minValue: -1500,
			maxValue: 1500,
			unit: ''
		},
		LOG_YAW_PID_D: {
			name: 'Yaw PID D',
			path: 'pid.yaw.d',
			minValue: -1500,
			maxValue: 1500,
			unit: ''
		},
		LOG_YAW_PID_FF: {
			name: 'Yaw PID FF',
			path: 'pid.yaw.ff',
			minValue: -1500,
			maxValue: 1500,
			unit: ''
		},
		LOG_YAW_PID_S: {
			name: 'Yaw PID S',
			path: 'pid.yaw.s',
			minValue: -1500,
			maxValue: 1500,
			unit: ''
		},
		LOG_MOTOR_OUTPUTS: {
			name: 'Motor Outputs',
			path: 'motors',
			minValue: 0,
			maxValue: 2000,
			unit: '',
			usesModifier: true
		},
		LOG_ALTITUDE: {
			name: 'Altitude',
			path: 'altitude',
			minValue: 0,
			maxValue: 300,
			unit: 'm'
		}
	} as {
		[key: string]: {
			name: string;
			path: string;
			minValue?: number;
			maxValue?: number;
			rangeFn?: (file: BBLog | undefined) => { max: number; min: number };
			unit: string;
			usesModifier?: boolean;
		};
	};

	$: handleCommand($port);

	function handleCommand(command: Command) {
		switch (command.command) {
			case ConfigCmd.BB_FILE_LIST | 0x4000:
				logNums = command.data.map((n) => ({ text: '', num: n }));
				if (!logNums.length) {
					logNums = [{ text: 'No logs found', num: -1 }];
					selected = -1;
					return;
				}
				selected = logNums[0].num;
				logInfoPosition = 0;
				logInfoInterval = setInterval(getLogInfo, 500);
				break;
			case ConfigCmd.BB_FILE_INFO | 0x4000:
				processLogInfo(command.data);
				break;
			case ConfigCmd.BB_FILE_DOWNLOAD | 0x4000:
				handleFileChunk(command.data);
				break;
			case 0xffff: //pre-entered default command, do nothing
				break;
			default:
				console.log({ ...command });
		}
	}

	function leBytesToInt(bytes: number[], signed = false) {
		let value = 0;
		for (let i = 0; i < bytes.length; i++) {
			value += bytes[i] * Math.pow(256, i);
		}
		if (signed && bytes[bytes.length - 1] & 0b10000000) {
			value -= Math.pow(256, bytes.length);
		}
		return value;
	}

	function handleFileChunk(data: number[]) {
		//1027 data bytes per packet
		//first byte is file number
		//second and third are chunk number
		//up to 1024 bytes of binary file data
		//last packet has the chunk number set to 0xFFFF, and then two bytes that indicate the number of total chunks
		if (binFileNumber !== data[0]) {
			binFileNumber = data[0];
			binFile = [];
			receivedChunks = [];
			graphs = [[]];
			loadedLog = undefined;
			totalChunks = -1;
		}
		const chunkNum = leBytesToInt(data.slice(1, 3));
		if (chunkNum === 0xffff) {
			totalChunks = leBytesToInt(data.slice(3, 5));
		} else {
			const chunkSize = data.length - 3;
			const chunk = data.slice(3, 3 + chunkSize);
			receivedChunks[chunkNum] = true;
			for (let i = chunkNum * 1024; i < chunkNum * 1024 + chunkSize; i++) {
				binFile[i] = chunk[i - chunkNum * 1024];
			}
		}
		if (receivedChunks.length === totalChunks) {
			for (let i = 0; i < totalChunks; i++) {
				if (!receivedChunks[i]) return;
			}
			const header = binFile.slice(0, 166);
			const data = binFile.slice(166);
			const magic = leBytesToInt(header.slice(0, 4));
			if (magic !== 0x99a12720) {
				rejectWrongFile(
					'Wrong magic number: 0x' +
						magic.toString(16) +
						' instead of 0x99a12720, receivedChunks.length: ' +
						receivedChunks.length +
						', totalChunks: ' +
						totalChunks
				);
				return;
			}
			const version = header.slice(4, 7);
			const startTime = leBytesToInt(header.slice(7, 11));
			const pidFreq = 3200 / (1 + header[11]);
			const freqDiv = header[12];
			const rangeByte = header[13];
			const ranges = {
				gyro: GYRO_RANGES[(rangeByte >> 2) & 0b111],
				accel: ACC_RANGES[rangeByte & 0b11]
			};
			const rateFactors = [[], [], [], [], []] as number[][];
			const rfBytes = header.slice(14, 74);
			for (let i = 0; i < 5; i++)
				for (let j = 0; j < 3; j++)
					rateFactors[i][j] =
						leBytesToInt(rfBytes.slice(i * 12 + j * 4, i * 12 + j * 4 + 4)) / 65536;
			const pidConstants = [[], [], []] as number[][];
			const pcBytes = header.slice(74, 158);
			for (let i = 0; i < 3; i++)
				for (let j = 0; j < 7; j++)
					pidConstants[i][j] =
						leBytesToInt(pcBytes.slice(i * 28 + j * 4, i * 28 + j * 4 + 4)) / 65536;
			const flagsLow = leBytesToInt(header.slice(158, 162));
			const flagsHigh = leBytesToInt(header.slice(162, 166));
			const flags = [] as string[];
			let frameSize = 0;
			const offsets = {} as { [key: string]: number };
			for (let i = 0; i < 32 && Object.keys(BB_ALL_FLAGS).length > i; i++) {
				const flagIsSet = flagsLow & (1 << i);
				if (flagIsSet) {
					flags.push(Object.keys(BB_ALL_FLAGS)[i]);
					offsets[Object.keys(BB_ALL_FLAGS)[i]] = frameSize;
					frameSize += i == 26 ? 6 : 2;
				}
			}
			for (let i = 0; i < 32 && Object.keys(BB_ALL_FLAGS).length > i + 32; i++) {
				const flagIsSet = flagsHigh & (1 << i);
				if (flagIsSet) flags.push(Object.keys(BB_ALL_FLAGS)[i + 32]);
			}
			const framesPerSecond = pidFreq / freqDiv;
			const frames = data.length / frameSize;
			const log = [] as LogFrame[];
			for (let i = 0; i < data.length; i += frameSize) {
				const frame = {
					elrs: {},
					setpoint: {},
					gyro: {},
					pid: { roll: {}, pitch: {}, yaw: {} },
					motors: {}
				} as LogFrame;
				if (flags.includes('LOG_ROLL_ELRS_RAW'))
					frame.elrs.roll = leBytesToInt(
						data.slice(i + offsets['LOG_ROLL_ELRS_RAW'], i + offsets['LOG_ROLL_ELRS_RAW'] + 2)
					);
				if (flags.includes('LOG_PITCH_ELRS_RAW'))
					frame.elrs.pitch = leBytesToInt(
						data.slice(i + offsets['LOG_PITCH_ELRS_RAW'], i + offsets['LOG_PITCH_ELRS_RAW'] + 2)
					);
				if (flags.includes('LOG_THROTTLE_ELRS_RAW'))
					frame.elrs.throttle = leBytesToInt(
						data.slice(
							i + offsets['LOG_THROTTLE_ELRS_RAW'],
							i + offsets['LOG_THROTTLE_ELRS_RAW'] + 2
						)
					);
				if (flags.includes('LOG_YAW_ELRS_RAW'))
					frame.elrs.yaw = leBytesToInt(
						data.slice(i + offsets['LOG_YAW_ELRS_RAW'], i + offsets['LOG_YAW_ELRS_RAW'] + 2)
					);
				if (flags.includes('LOG_ROLL_SETPOINT'))
					frame.setpoint.roll =
						leBytesToInt(
							data.slice(i + offsets['LOG_ROLL_SETPOINT'], i + offsets['LOG_ROLL_SETPOINT'] + 2),
							true
						) / 16; // data is 12.4 fixed point
				if (flags.includes('LOG_PITCH_SETPOINT'))
					frame.setpoint.pitch =
						leBytesToInt(
							data.slice(i + offsets['LOG_PITCH_SETPOINT'], i + offsets['LOG_PITCH_SETPOINT'] + 2),
							true
						) / 16;
				if (flags.includes('LOG_THROTTLE_SETPOINT'))
					frame.setpoint.throttle = leBytesToInt(
						data.slice(
							i + offsets['LOG_THROTTLE_SETPOINT'],
							i + offsets['LOG_THROTTLE_SETPOINT'] + 2
						)
					);
				if (flags.includes('LOG_YAW_SETPOINT'))
					frame.setpoint.yaw =
						leBytesToInt(
							data.slice(i + offsets['LOG_YAW_SETPOINT'], i + offsets['LOG_YAW_SETPOINT'] + 2),
							true
						) / 16;
				if (flags.includes('LOG_ROLL_GYRO_RAW'))
					frame.gyro.roll =
						leBytesToInt(
							data.slice(i + offsets['LOG_ROLL_GYRO_RAW'], i + offsets['LOG_ROLL_GYRO_RAW'] + 2),
							true
						) / 16;
				if (flags.includes('LOG_PITCH_GYRO_RAW'))
					frame.gyro.pitch =
						leBytesToInt(
							data.slice(i + offsets['LOG_PITCH_GYRO_RAW'], i + offsets['LOG_PITCH_GYRO_RAW'] + 2),
							true
						) / 16;
				if (flags.includes('LOG_YAW_GYRO_RAW'))
					frame.gyro.yaw =
						leBytesToInt(
							data.slice(i + offsets['LOG_YAW_GYRO_RAW'], i + offsets['LOG_YAW_GYRO_RAW'] + 2),
							true
						) / 16;
				if (flags.includes('LOG_ROLL_PID_P'))
					frame.pid.roll.p = leBytesToInt(
						data.slice(i + offsets['LOG_ROLL_PID_P'], i + offsets['LOG_ROLL_PID_P'] + 2),
						true
					);
				if (flags.includes('LOG_ROLL_PID_I'))
					frame.pid.roll.i = leBytesToInt(
						data.slice(i + offsets['LOG_ROLL_PID_I'], i + offsets['LOG_ROLL_PID_I'] + 2),
						true
					);
				if (flags.includes('LOG_ROLL_PID_D'))
					frame.pid.roll.d = leBytesToInt(
						data.slice(i + offsets['LOG_ROLL_PID_D'], i + offsets['LOG_ROLL_PID_D'] + 2),
						true
					);
				if (flags.includes('LOG_ROLL_PID_FF'))
					frame.pid.roll.ff = leBytesToInt(
						data.slice(i + offsets['LOG_ROLL_PID_FF'], i + offsets['LOG_ROLL_PID_FF'] + 2),
						true
					);
				if (flags.includes('LOG_ROLL_PID_S'))
					frame.pid.roll.s = leBytesToInt(
						data.slice(i + offsets['LOG_ROLL_PID_S'], i + offsets['LOG_ROLL_PID_S'] + 2),
						true
					);
				if (flags.includes('LOG_PITCH_PID_P'))
					frame.pid.pitch.p = leBytesToInt(
						data.slice(i + offsets['LOG_PITCH_PID_P'], i + offsets['LOG_PITCH_PID_P'] + 2),
						true
					);
				if (flags.includes('LOG_PITCH_PID_I'))
					frame.pid.pitch.i = leBytesToInt(
						data.slice(i + offsets['LOG_PITCH_PID_I'], i + offsets['LOG_PITCH_PID_I'] + 2),
						true
					);
				if (flags.includes('LOG_PITCH_PID_D'))
					frame.pid.pitch.d = leBytesToInt(
						data.slice(i + offsets['LOG_PITCH_PID_D'], i + offsets['LOG_PITCH_PID_D'] + 2),
						true
					);
				if (flags.includes('LOG_PITCH_PID_FF'))
					frame.pid.pitch.ff = leBytesToInt(
						data.slice(i + offsets['LOG_PITCH_PID_FF'], i + offsets['LOG_PITCH_PID_FF'] + 2),
						true
					);
				if (flags.includes('LOG_PITCH_PID_S'))
					frame.pid.pitch.s = leBytesToInt(
						data.slice(i + offsets['LOG_PITCH_PID_S'], i + offsets['LOG_PITCH_PID_S'] + 2),
						true
					);
				if (flags.includes('LOG_YAW_PID_P'))
					frame.pid.yaw.p = leBytesToInt(
						data.slice(i + offsets['LOG_YAW_PID_P'], i + offsets['LOG_YAW_PID_P'] + 2),
						true
					);
				if (flags.includes('LOG_YAW_PID_I'))
					frame.pid.yaw.i = leBytesToInt(
						data.slice(i + offsets['LOG_YAW_PID_I'], i + offsets['LOG_YAW_PID_I'] + 2),
						true
					);
				if (flags.includes('LOG_YAW_PID_D'))
					frame.pid.yaw.d = leBytesToInt(
						data.slice(i + offsets['LOG_YAW_PID_D'], i + offsets['LOG_YAW_PID_D'] + 2),
						true
					);
				if (flags.includes('LOG_YAW_PID_FF'))
					frame.pid.yaw.ff = leBytesToInt(
						data.slice(i + offsets['LOG_YAW_PID_FF'], i + offsets['LOG_YAW_PID_FF'] + 2),
						true
					);
				if (flags.includes('LOG_YAW_PID_S'))
					frame.pid.yaw.s = leBytesToInt(
						data.slice(i + offsets['LOG_YAW_PID_S'], i + offsets['LOG_YAW_PID_S'] + 2),
						true
					);
				if (flags.includes('LOG_MOTOR_OUTPUTS')) {
					const throttleBytes = data.slice(
						i + offsets['LOG_MOTOR_OUTPUTS'],
						i + offsets['LOG_MOTOR_OUTPUTS'] + 6
					);
					const motors23 = leBytesToInt(throttleBytes.slice(3, 6).reverse());
					const motors01 = leBytesToInt(throttleBytes.slice(0, 3).reverse());
					frame.motors.rr = motors01 >> 12;
					frame.motors.fr = motors01 & 0xfff;
					frame.motors.rl = motors23 >> 12;
					frame.motors.fl = motors23 & 0xfff;
				}
				if (flags.includes('LOG_ALTITUDE'))
					frame.altitude = leBytesToInt(
						data.slice(i + offsets['LOG_ALTITUDE'], i + offsets['LOG_ALTITUDE'] + 2)
					);
				log.push(frame);
			}
			loadedLog = {
				frameCount: frames,
				flags,
				frames: log,
				version,
				startTime,
				ranges,
				pidFrequency: pidFreq,
				frequencyDivider: freqDiv,
				rateFactors,
				pidConstants,
				framesPerSecond,
				rawFile: binFile
			};
			resolveWhenReady(loadedLog);
		}
	}

	let logInfoPosition = 0;
	let logInfoInterval: number | NodeJS.Timer = -1;
	let selected = -1;
	function getLogInfo() {
		const infoNums = [] as number[];
		for (let i = 0; i < 10; i++) {
			if (logInfoPosition >= logNums.length) {
				clearInterval(logInfoInterval);
				break;
			}
			infoNums[i] = logNums[logInfoPosition++].num;
		}
		if (infoNums.length == 0) return;
		let checksum = 0;
		for (let i = 0; i < infoNums.length; i++) checksum ^= infoNums[i];
		checksum ^= 0x06;
		checksum ^= infoNums.length;
		port.sendCommand(0x06, infoNums);
	}

	function processLogInfo(data: number[]) {
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
			const bbVersion = data[i + 5] + data[i + 6] * 256 + data[i + 7] * 256 * 256;
			const startTime =
				data[i + 8] + data[i + 9] * 256 + data[i + 10] * 256 * 256 + data[i + 11] * 256 * 256 * 256;
			const pidFreq = 3200 / (data[i + 12] + 1);
			const freqDiv = data[i + 13];
			const flags = data.slice(i + 14, i + 22);
			if (bbVersion !== 0) continue;
			const framesPerSecond = pidFreq / freqDiv;
			const dataBytes = fileSize - 166;
			let frameSize = 0;
			for (let j = 0; j < 64; j++) {
				//check flags
				// flag 26 (motors) has 6 bytes per frame, all others only 2
				const byteNum = Math.floor(j / 8);
				const bitNum = j % 8;
				const flagIsSet = flags[byteNum] & (1 << bitNum);
				if (!flagIsSet) continue;
				if (j == 26) frameSize += 6;
				else frameSize += 2;
			}
			const frames = dataBytes / frameSize;
			//append duration of log file to logNums
			const index = logNums.findIndex((n) => n.num == fileNum);
			if (index == -1) continue;
			const duration = Math.round(frames / framesPerSecond);
			logNums[index].text = `${logNums[index].num} - ${duration}s`;
			selected = fileNum;
		}
	}

	function getNestedProperty(
		obj: any,
		path: string,
		options: { defaultValue?: any; max?: number; min?: number } = {}
	) {
		const pathParts = path.split('.');
		let current = obj;
		for (let i = 0; i < pathParts.length; i++) {
			if (current[pathParts[i]] === undefined) return options.defaultValue;
			current = current[pathParts[i]];
		}
		if (options.max !== undefined && current > options.max) return options.max;
		if (options.min !== undefined && current < options.min) return options.min;
		return current;
	}
	function drawCanvas() {
		if (!mounted) return;
		const canvas = document.getElementById('bbDataViewer') as HTMLCanvasElement;
		const ctx = canvas.getContext('2d') as CanvasRenderingContext2D;
		/**
		 * the drawing canvas has several graphs in it (in one column)
		 * each graph has a set of traces
		 * the whole drawing board has 1% of the height of free space on the top and bottom, as well as 2% oh the height of free space between each graph, no free space left and right
		 * each trace has a range, which represents the top and bottom on the graph for that trace
		 * a modifier appears for some flags, like motor outputs to define one specific motor for example
		 */
		const height = dataViewer.clientHeight * 0.98; //1% free space top and bottom
		const width = dataViewer.clientWidth;
		let sliceAndSkip = dataSlice;
		if (!sliceAndSkip.length) return;
		while (sliceAndSkip.length > 2 * width) {
			sliceAndSkip = sliceAndSkip.filter((_, i) => i % 2 == 0);
		}
		ctx.clearRect(0, 0, dataViewer.clientWidth, dataViewer.clientHeight);
		const frameWidth = width / (sliceAndSkip.length - 1);
		const numGraphs = graphs.length;
		const heightPerGraph = (height - dataViewer.clientHeight * 0.02 * (numGraphs - 1)) / numGraphs;
		let heightOffset = 0.01 * dataViewer.clientHeight;
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

			const graph = graphs[i];
			const numTraces = graph.length;
			for (let j = 0; j < numTraces; j++) {
				const trace = graph[j];
				const range = trace.maxValue - trace.minValue;
				const scale = heightPerGraph / range;
				ctx.strokeStyle = trace.color;
				ctx.lineWidth = trace.strokeWidth;
				let path = BB_ALL_FLAGS[trace.flagName]?.path || '';
				if (BB_ALL_FLAGS[trace.flagName]?.usesModifier) {
					if (trace.modifier) path += '.' + trace.modifier.toLowerCase();
					else continue;
				}
				ctx.beginPath();
				let pointY =
					heightOffset +
					heightPerGraph -
					(getNestedProperty(sliceAndSkip[0], path, { max: trace.maxValue, min: trace.minValue }) -
						trace.minValue) *
						scale;
				ctx.moveTo(0, pointY);
				for (let k = 1; k < sliceAndSkip.length; k++) {
					pointY =
						heightOffset +
						heightPerGraph -
						(getNestedProperty(sliceAndSkip[k], path, {
							max: trace.maxValue,
							min: trace.minValue
						}) -
							trace.minValue) *
							scale;
					ctx.lineTo(k * frameWidth, pointY);
				}
				ctx.stroke();
			}
			heightOffset += heightPerGraph + 0.02 * dataViewer.clientHeight;
		}
	}

	function deleteLog() {
		const logNum = logNums[0].num as number;
		port.sendCommand(ConfigCmd.BB_FILE_DELETE, [logNum]);
	}
	function openLog() {
		getLog(selected)
			.then((data) => {
				startFrame = 0;
				endFrame = data.frameCount - 1;
				drawCanvas();
			})
			.catch(console.error);
	}
	function prefixZeros(num: number, totalDigits: number) {
		let str = num.toString();
		while (str.length < totalDigits) str = '0' + str;
		return str;
	}
	function downloadLog(type: 'kbb' | 'json' = 'kbb') {
		getLog(selected)
			.then((data) => {
				const file = data.rawFile;
				const blob = new Blob([new Uint8Array(file)], { type: 'application/octet-stream' });
				const blobJSON = new Blob([JSON.stringify(data)], { type: 'application/json' });
				const url = URL.createObjectURL(type === 'json' ? blobJSON : blob);
				//download file
				const a = document.createElement('a');
				a.href = url;
				a.download = `blackbox-${prefixZeros(selected, 2)}-${data.startTime}.${type}`;
				a.click();
				URL.revokeObjectURL(url);
			})
			.catch(console.error);
	}
	function getLog(num: number): Promise<BBLog> {
		binFileNumber = -1;

		port.sendCommand(ConfigCmd.BB_FILE_DOWNLOAD, [num]);
		return new Promise((resolve, reject) => {
			resolveWhenReady = resolve;
			rejectWrongFile = reject;
		});
	}
	function openLogFromPromptJSON() {
		//use a js alert prompt to get the json file
		const json = prompt('Paste the JSON file here');
		if (!json) return;
		try {
			const data = JSON.parse(json);
			if (data.version[0] !== 0) {
				alert('Wrong version number: ' + data.version);
				return;
			}
			loadedLog = data as BBLog;
			startFrame = 0;
			endFrame = data.frameCount - 1;
			drawCanvas();
		} catch (e) {
			alert('Error parsing JSON: ' + e);
		}
	}
	function formatBB() {
		port.sendCommand(ConfigCmd.BB_FORMAT);
	}

	function addTrace(graphIndex: number) {
		const defaultTrace = {
			color: 'transparent',
			maxValue: 10,
			minValue: 0,
			strokeWidth: 1,
			flagName: '',
			modifier: ''
		};
		graphs[graphIndex] = [...graphs[graphIndex], defaultTrace];
		drawCanvas();
	}
	function addGraph() {
		graphs = [...graphs, []];
		drawCanvas();
	}
	function updateTrace(event: any, graphIndex: number, traceIndex: number) {
		graphs[graphIndex][traceIndex] = event.detail;
		drawCanvas();
	}
	function getAutoRangeByFlagName(flagName: string) {
		const flag = BB_ALL_FLAGS[flagName];
		const range = { max: 10, min: 0 };
		if (!flag) return range;
		if (flag.rangeFn) return flag.rangeFn(loadedLog);
		range.max = flag.maxValue || 10;
		range.min = flag.minValue || 0;
		return range;
	}
	function deleteGraph(g: number) {
		graphs = graphs.filter((_, i) => i !== g);
		graphs = [...graphs];
		drawCanvas();
	}
	function deleteTrace(g: number, t: number) {
		graphs[g] = graphs[g].filter((_, i) => i !== t);
		graphs = [...graphs];
		drawCanvas();
	}

	onMount(() => {
		mounted = true;
		port.sendCommand(ConfigCmd.BB_FILE_LIST).catch(console.error);
		dataViewer = document.getElementsByClassName('dataViewerWrapper')[0] as HTMLDivElement;
		function onResize() {
			const canvas = document.getElementById('bbDataViewer') as HTMLCanvasElement;
			canvas.height = dataViewer.clientHeight;
			canvas.width = dataViewer.clientWidth;

			drawCanvas();
		}
		window.addEventListener('resize', onResize);
		onResize();
	});
	onDestroy(() => {});
</script>

<div class="blackbox">
	<div class="selector">
		<select bind:value={selected}>
			{#each logNums as log}
				<option value={log?.num}>{log?.text || log?.num}</option>
			{/each}
		</select>
		<button on:click={() => openLog()} disabled={selected === -1}>Open</button>
		<button on:click={() => downloadLog()} disabled={selected === -1}>Download BIN</button>
		<button on:click={() => downloadLog('json')} disabled={selected === -1}>Download JSON</button>
		<button on:click={() => deleteLog()} disabled={selected === -1}>Delete</button>
		<button on:click={() => formatBB()}>Format</button>
		<button on:click={() => openLogFromPromptJSON()}>Open JSON</button>
	</div>
	<div class="dataViewerWrapper">
		<canvas id="bbDataViewer" />
	</div>
	<div class="flagSelector">
		{#each graphs as graph, graphIndex}
			<div class="graphSelector">
				{#each graph as trace, traceIndex}
					<TracePlacer
						flags={loadedLog?.flags || []}
						autoRange={getAutoRangeByFlagName(trace.flagName)}
						flagProps={BB_ALL_FLAGS}
						on:update={(event) => {
							updateTrace(event, graphIndex, traceIndex);
						}}
						on:delete={() => {
							deleteTrace(graphIndex, traceIndex);
						}}
					/>
				{/each}
				<button
					class="addTraceButton"
					disabled={!loadedLog?.flags?.length}
					on:click={() => {
						addTrace(graphIndex);
					}}>Add Trace</button
				>
				<button
					class="deleteGraph"
					disabled={!loadedLog?.flags?.length || graphs.length == 1}
					on:click={() => {
						deleteGraph(graphIndex);
					}}>Del Graph</button
				>
			</div>
		{/each}
		<button class="addGraphButton" disabled={!loadedLog} on:click={addGraph}>Add Graph</button>
	</div>
	<div class="timelineWrapper">
		<input
			type="number"
			class="frameInput frameStart"
			bind:value={startFrame}
			step="1"
			on:input|trusted={() => {
				if (startFrame > (loadedLog?.frameCount || 1) - 1)
					startFrame = (loadedLog?.frameCount || 1) - 1;
			}}
		/>
		<input
			type="number"
			class="frameInput frameEnd"
			bind:value={endFrame}
			step="1"
			on:input|trusted={() => {
				if (endFrame > (loadedLog?.frameCount || 1) - 1)
					endFrame = (loadedLog?.frameCount || 1) - 1;
			}}
		/>
	</div>
</div>

<style>
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
		width: 8rem;
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
		width: 19rem;
		overflow: auto;
	}
	.graphSelector {
		padding: 0.8rem;
		border-bottom: var(--border-color) 1px solid;
	}
	.graphSelector:last-of-type {
		margin-bottom: 0.8rem;
	}
	.deleteGraph {
		float: right;
	}
</style>
