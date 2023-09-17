<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { port, type Command, ConfigCmd } from '../../stores';
	import TracePlacer from './tracePlacer.svelte';
	import { leBytesToInt } from '../../utils';

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

	let drawFullCanvasTimeout = -1;

	$: dataSlice =
		loadedLog?.frames.slice(
			Math.max(0, Math.min(startFrame, endFrame)),
			Math.max(0, Math.max(startFrame, endFrame)) + 1
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
		pidConstantsNice: number[][];
		framesPerSecond: number;
		isExact: boolean;
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
		frametime?: number;
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
		const fullRange = Math.round(Math.max(max, -min));
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
		},
		LOG_FRAMETIME: {
			name: 'Frametime',
			path: 'frametime',
			minValue: 0,
			maxValue: 1000,
			unit: 'µs'
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

	const BB_GEN_FLAGS = {
		GEN_ROLL_SETPOINT: {
			name: 'Roll Setpoint',
			replaces: 'LOG_ROLL_SETPOINT',
			requires: ['LOG_ROLL_ELRS_RAW'],
			unit: '°/sec',
			exact: false
		},
		GEN_PITCH_SETPOINT: {
			name: 'Pitch Setpoint',
			replaces: 'LOG_PITCH_SETPOINT',
			requires: ['LOG_PITCH_ELRS_RAW'],
			unit: '°/sec',
			exact: false
		},
		GEN_THROTTLE_SETPOINT: {
			name: 'Throttle Setpoint',
			replaces: 'LOG_THROTTLE_SETPOINT',
			requires: ['LOG_THROTTLE_ELRS_RAW'],
			unit: '°/sec',
			exact: false
		},
		GEN_YAW_SETPOINT: {
			name: 'Yaw Setpoint',
			replaces: 'LOG_YAW_SETPOINT',
			requires: ['LOG_YAW_ELRS_RAW'],
			unit: '°/sec',
			exact: false
		},
		GEN_ROLL_PID_P: {
			name: 'Roll PID P',
			replaces: 'LOG_ROLL_PID_P',
			requires: [['LOG_ROLL_SETPOINT', 'GEN_ROLL_SETPOINT'], 'LOG_ROLL_GYRO_RAW'],
			unit: '',
			exact: true
		},
		GEN_ROLL_PID_I: {
			name: 'Roll PID I',
			replaces: 'LOG_ROLL_PID_I',
			requires: [['LOG_ROLL_SETPOINT', 'GEN_ROLL_SETPOINT'], 'LOG_ROLL_GYRO_RAW'],
			unit: '',
			exact: false
		},
		GEN_ROLL_PID_D: {
			name: 'Roll PID D',
			replaces: 'LOG_ROLL_PID_D',
			requires: ['LOG_ROLL_GYRO_RAW'],
			unit: '',
			exact: false
		},
		GEN_ROLL_PID_FF: {
			name: 'Roll PID FF',
			replaces: 'LOG_ROLL_PID_FF',
			requires: [['LOG_ROLL_SETPOINT', 'GEN_ROLL_SETPOINT']],
			unit: '',
			exact: false
		},
		GEN_ROLL_PID_S: {
			name: 'Roll PID S',
			replaces: 'LOG_ROLL_PID_S',
			requires: [['LOG_ROLL_SETPOINT', 'GEN_ROLL_SETPOINT']],
			unit: '',
			exact: true
		},
		GEN_PITCH_PID_P: {
			name: 'Pitch PID P',
			replaces: 'LOG_PITCH_PID_P',
			requires: [['LOG_PITCH_SETPOINT', 'GEN_PITCH_SETPOINT'], 'LOG_PITCH_GYRO_RAW'],
			unit: '',
			exact: true
		},
		GEN_PITCH_PID_I: {
			name: 'Pitch PID I',
			replaces: 'LOG_PITCH_PID_I',
			requires: [['LOG_PITCH_SETPOINT', 'GEN_PITCH_SETPOINT'], 'LOG_PITCH_GYRO_RAW'],
			unit: '',
			exact: false
		},
		GEN_PITCH_PID_D: {
			name: 'Pitch PID D',
			replaces: 'LOG_PITCH_PID_D',
			requires: ['LOG_PITCH_GYRO_RAW'],
			unit: '',
			exact: false
		},
		GEN_PITCH_PID_FF: {
			name: 'Pitch PID FF',
			replaces: 'LOG_PITCH_PID_FF',
			requires: [['LOG_PITCH_SETPOINT', 'GEN_PITCH_SETPOINT']],
			unit: '',
			exact: false
		},
		GEN_PITCH_PID_S: {
			name: 'Pitch PID S',
			replaces: 'LOG_PITCH_PID_S',
			requires: [['LOG_PITCH_SETPOINT', 'GEN_PITCH_SETPOINT']],
			unit: '',
			exact: true
		},
		GEN_YAW_PID_P: {
			name: 'Yaw PID P',
			replaces: 'LOG_YAW_PID_P',
			requires: [['LOG_YAW_SETPOINT', 'GEN_YAW_SETPOINT'], 'LOG_YAW_GYRO_RAW'],
			unit: '',
			exact: true
		},
		GEN_YAW_PID_I: {
			name: 'Yaw PID I',
			replaces: 'LOG_YAW_PID_I',
			requires: [['LOG_YAW_SETPOINT', 'GEN_YAW_SETPOINT'], 'LOG_YAW_GYRO_RAW'],
			unit: '',
			exact: false
		},
		GEN_YAW_PID_D: {
			name: 'Yaw PID D',
			replaces: 'LOG_YAW_PID_D',
			requires: ['LOG_YAW_GYRO_RAW'],
			unit: '',
			exact: false
		},
		GEN_YAW_PID_FF: {
			name: 'Yaw PID FF',
			replaces: 'LOG_YAW_PID_FF',
			requires: [['LOG_YAW_SETPOINT', 'GEN_YAW_SETPOINT']],
			unit: '',
			exact: false
		},
		GEN_YAW_PID_S: {
			name: 'Yaw PID S',
			replaces: 'LOG_YAW_PID_S',
			requires: [['LOG_YAW_SETPOINT', 'GEN_YAW_SETPOINT']],
			unit: '',
			exact: true
		},
		GEN_MOTOR_OUTPUTS: {
			name: 'Motor Outputs',
			replaces: 'LOG_MOTOR_OUTPUTS',
			requires: [
				['GEN_THROTTLE_SETPOINT', 'LOG_THROTTLE_SETPOINT'],
				['LOG_ROLL_PID_P', 'GEN_ROLL_PID_P'],
				['LOG_ROLL_PID_I', 'GEN_ROLL_PID_I'],
				['LOG_ROLL_PID_D', 'GEN_ROLL_PID_D'],
				['LOG_ROLL_PID_FF', 'GEN_ROLL_PID_FF'],
				['LOG_ROLL_PID_S', 'GEN_ROLL_PID_S'],
				['LOG_PITCH_PID_P', 'GEN_PITCH_PID_P'],
				['LOG_PITCH_PID_I', 'GEN_PITCH_PID_I'],
				['LOG_PITCH_PID_D', 'GEN_PITCH_PID_D'],
				['LOG_PITCH_PID_FF', 'GEN_PITCH_PID_FF'],
				['LOG_PITCH_PID_S', 'GEN_PITCH_PID_S'],
				['LOG_YAW_PID_P', 'GEN_YAW_PID_P'],
				['LOG_YAW_PID_I', 'GEN_YAW_PID_I'],
				['LOG_YAW_PID_D', 'GEN_YAW_PID_D'],
				['LOG_YAW_PID_FF', 'GEN_YAW_PID_FF'],
				['LOG_YAW_PID_S', 'GEN_YAW_PID_S']
			],
			unit: '',
			exact: true
		}
	} as {
		[key: string]: {
			name: string;
			replaces: string;
			requires: (string | string[])[]; // if its a string, that has to be in there. If its an array, one of the mentioned ones has to be in there
			unit: string;
			exact: boolean;
		};
	};

	function fillLogWithGenFlags(log: BBLog) {
		log.isExact = true;
		const genFlags = Object.keys(BB_GEN_FLAGS);
		for (let i = 0; i < genFlags.length; i++) {
			const flagName = genFlags[i];
			const flag = BB_GEN_FLAGS[flagName];
			if (log.flags.includes(flag.replaces)) continue;
			if (
				flag.requires.every((r) => {
					if (typeof r === 'string') return log.flags.includes(r);
					if (Array.isArray(r)) {
						for (const s of r) if (log.flags.includes(s)) return true;

						return false;
					}
					return false;
				})
			) {
				log.flags.push(flagName);
				//generate entries
				if (!flag.exact) log.isExact = false;
				const path = BB_ALL_FLAGS[flag.replaces].path;
				switch (flagName) {
					case 'GEN_ROLL_SETPOINT':
						log.frames.forEach((f) => {
							const polynomials: number[] = [(f.elrs.roll! - 1500) / 512];
							for (let i = 1; i < 5; i++) {
								polynomials[i] = polynomials[0] * polynomials[i - 1];
								if (polynomials[0] < 0) polynomials[i] = -polynomials[i];
							}
							f.setpoint.roll = 0;
							for (let i = 0; i < 5; i++)
								f.setpoint.roll! += polynomials[i] * log.rateFactors[i][0];
						});
						break;
					case 'GEN_PITCH_SETPOINT':
						log.frames.forEach((f) => {
							const polynomials: number[] = [(f.elrs.pitch! - 1500) / 512];
							for (let i = 1; i < 5; i++) {
								polynomials[i] = polynomials[0] * polynomials[i - 1];
								if (polynomials[0] < 0) polynomials[i] = -polynomials[i];
							}
							f.setpoint.pitch = 0;
							for (let i = 0; i < 5; i++)
								f.setpoint.pitch! += polynomials[i] * log.rateFactors[i][1];
						});
						break;
					case 'GEN_THROTTLE_SETPOINT':
						log.frames.forEach((f) => {
							f.setpoint.throttle = f.elrs.throttle;
						});
						break;
					case 'GEN_YAW_SETPOINT':
						log.frames.forEach((f) => {
							const polynomials: number[] = [(f.elrs.yaw! - 1500) / 512];
							for (let i = 1; i < 5; i++) {
								polynomials[i] = polynomials[0] * polynomials[i - 1];
								if (polynomials[0] < 0) polynomials[i] = -polynomials[i];
							}
							f.setpoint.yaw = 0;
							for (let i = 0; i < 5; i++) f.setpoint.yaw! += polynomials[i] * log.rateFactors[i][2];
						});
						break;
					case 'GEN_ROLL_PID_P':
						log.frames.forEach((f) => {
							f.pid.roll.p = (f.setpoint.roll! - f.gyro.roll!) * log.pidConstants[0][0];
						});
						break;
					case 'GEN_PITCH_PID_P':
						log.frames.forEach((f) => {
							f.pid.pitch.p = (f.setpoint.pitch! - f.gyro.pitch!) * log.pidConstants[1][0];
						});
						break;
					case 'GEN_YAW_PID_P':
						log.frames.forEach((f) => {
							f.pid.yaw.p = (f.setpoint.yaw! - f.gyro.yaw!) * log.pidConstants[2][0];
						});
						break;
					case 'GEN_ROLL_PID_D':
						log.frames.forEach((f, i) => {
							f.pid.roll.d =
								(f.gyro.roll! - (log.frames[i - 1]?.gyro.roll || 0)) * log.pidConstants[0][2];
						});
						break;
					case 'GEN_PITCH_PID_D':
						log.frames.forEach((f, i) => {
							f.pid.pitch.d =
								(f.gyro.pitch! - (log.frames[i - 1]?.gyro.pitch || 0)) * log.pidConstants[1][2];
						});
						break;
					case 'GEN_YAW_PID_D':
						log.frames.forEach((f, i) => {
							f.pid.yaw.d =
								(f.gyro.yaw! - (log.frames[i - 1]?.gyro.yaw || 0)) * log.pidConstants[2][2];
						});
						break;
					case 'GEN_ROLL_PID_FF':
						log.frames.forEach((f) => {
							f.pid.roll.ff =
								(f.setpoint.roll! - log.frames[i - 1]?.setpoint.roll! || 0) *
								log.pidConstants[0][3];
						});
						break;
					case 'GEN_PITCH_PID_FF':
						log.frames.forEach((f) => {
							f.pid.pitch.ff =
								(f.setpoint.pitch! - log.frames[i - 1]?.setpoint.pitch! || 0) *
								log.pidConstants[1][3];
						});
						break;
					case 'GEN_YAW_PID_FF':
						log.frames.forEach((f) => {
							f.pid.yaw.ff =
								(f.setpoint.yaw! - log.frames[i - 1]?.setpoint.yaw! || 0) * log.pidConstants[2][3];
						});
						break;
					case 'GEN_ROLL_PID_S':
						log.frames.forEach((f) => {
							f.pid.roll.s = f.setpoint.roll! * log.pidConstants[0][4];
						});
						break;
					case 'GEN_PITCH_PID_S':
						log.frames.forEach((f) => {
							f.pid.pitch.s = f.setpoint.pitch! * log.pidConstants[1][4];
						});
						break;
					case 'GEN_YAW_PID_S':
						log.frames.forEach((f) => {
							f.pid.yaw.s = f.setpoint.yaw! * log.pidConstants[2][4];
						});
						break;
					case 'GEN_ROLL_PID_I':
						{
							let rollI = 0;
							let takeoffCounter = 0;
							log.frames.forEach((f) => {
								rollI += f.setpoint.roll! - f.gyro.roll!;
								if (f.setpoint.throttle! > 1020) takeoffCounter += log.frequencyDivider;
								else if (takeoffCounter < 1000) takeoffCounter = 0;
								if (takeoffCounter < 1000)
									rollI *= Math.pow(log.pidConstants[0][6], log.frequencyDivider);
								f.pid.roll.i = rollI * log.pidConstants[0][1];
							});
						}
						break;
					case 'GEN_PITCH_PID_I':
						{
							let pitchI = 0;
							let takeoffCounter = 0;
							log.frames.forEach((f) => {
								pitchI += f.setpoint.pitch! - f.gyro.pitch!;
								if (f.setpoint.throttle! > 1020) takeoffCounter += log.frequencyDivider;
								else if (takeoffCounter < 1000) takeoffCounter = 0;
								if (takeoffCounter < 1000)
									pitchI *= Math.pow(log.pidConstants[1][6], log.frequencyDivider);
								f.pid.pitch.i = pitchI * log.pidConstants[1][1];
							});
						}
						break;
					case 'GEN_YAW_PID_I':
						{
							let yawI = 0;
							let takeoffCounter = 0;
							log.frames.forEach((f) => {
								yawI += f.setpoint.yaw! - f.gyro.yaw!;
								if (f.setpoint.throttle! > 1020) takeoffCounter += log.frequencyDivider;
								else if (takeoffCounter < 1000) takeoffCounter = 0;
								if (takeoffCounter < 1000)
									yawI *= Math.pow(log.pidConstants[2][6], log.frequencyDivider);
								f.pid.yaw.i = yawI * log.pidConstants[2][1];
							});
						}
						break;
					case 'GEN_MOTOR_OUTPUTS':
						log.frames.forEach((f) => {
							const rollTerm =
								f.pid.roll.p! + f.pid.roll.i! + f.pid.roll.d! + f.pid.roll.ff! + f.pid.roll.s!;
							const pitchTerm =
								f.pid.pitch.p! + f.pid.pitch.i! + f.pid.pitch.d! + f.pid.pitch.ff! + f.pid.pitch.s!;
							const yawTerm =
								f.pid.yaw.p! + f.pid.yaw.i! + f.pid.yaw.d! + f.pid.yaw.ff! + f.pid.yaw.s!;
							f.motors = {
								rr: (f.setpoint.throttle! - 1000) * 2 - rollTerm + pitchTerm + yawTerm,
								fr: (f.setpoint.throttle! - 1000) * 2 - rollTerm - pitchTerm - yawTerm,
								rl: (f.setpoint.throttle! - 1000) * 2 + rollTerm + pitchTerm - yawTerm,
								fl: (f.setpoint.throttle! - 1000) * 2 + rollTerm - pitchTerm + yawTerm
							};
							f.motors.rr = map(f.motors.rr!, 0, 2000, 50, 2000);
							f.motors.fr = map(f.motors.fr!, 0, 2000, 50, 2000);
							f.motors.rl = map(f.motors.rl!, 0, 2000, 50, 2000);
							f.motors.fl = map(f.motors.fl!, 0, 2000, 50, 2000);
							if (f.motors.rr! > 2000) {
								const diff = 2000 - f.motors.rr!;
								f.motors.rr = 2000;
								f.motors.fr! -= diff;
								f.motors.rl! -= diff;
								f.motors.fl! -= diff;
							}
							if (f.motors.fr! > 2000) {
								const diff = 2000 - f.motors.fr!;
								f.motors.fr = 2000;
								f.motors.rr! -= diff;
								f.motors.rl! -= diff;
								f.motors.fl! -= diff;
							}
							if (f.motors.rl! > 2000) {
								const diff = 2000 - f.motors.rl!;
								f.motors.rl = 2000;
								f.motors.rr! -= diff;
								f.motors.fr! -= diff;
								f.motors.fl! -= diff;
							}
							if (f.motors.fl! > 2000) {
								const diff = 2000 - f.motors.fl!;
								f.motors.fl = 2000;
								f.motors.rr! -= diff;
								f.motors.fr! -= diff;
								f.motors.rl! -= diff;
							}
							if (f.motors.rr! < 50) {
								const diff = 50 - f.motors.rr!;
								f.motors.rr = 50;
								f.motors.fr! += diff;
								f.motors.rl! += diff;
								f.motors.fl! += diff;
							}
							if (f.motors.fr! < 50) {
								const diff = 50 - f.motors.fr!;
								f.motors.fr = 50;
								f.motors.rr! += diff;
								f.motors.rl! += diff;
								f.motors.fl! += diff;
							}
							if (f.motors.rl! < 50) {
								const diff = 50 - f.motors.rl!;
								f.motors.rl = 50;
								f.motors.rr! += diff;
								f.motors.fr! += diff;
								f.motors.fl! += diff;
							}
							if (f.motors.fl! < 50) {
								const diff = 50 - f.motors.fl!;
								f.motors.fl = 50;
								f.motors.rr! += diff;
								f.motors.fr! += diff;
								f.motors.rl! += diff;
							}
							f.motors.rr = Math.min(f.motors.rr!, 2000);
							f.motors.fr = Math.min(f.motors.fr!, 2000);
							f.motors.rl = Math.min(f.motors.rl!, 2000);
							f.motors.fl = Math.min(f.motors.fl!, 2000);
						});
						break;
				}
			}
		}
	}
	function map(value: number, inMin: number, inMax: number, outMin: number, outMax: number) {
		return ((value - inMin) * (outMax - outMin)) / (inMax - inMin) + outMin;
	}

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
				//@ts-ignore
				logInfoInterval = setInterval(getLogInfo, 100);
				break;
			case ConfigCmd.BB_FILE_INFO | 0x4000:
				processLogInfo(command.data);
				break;
			case ConfigCmd.BB_FILE_DOWNLOAD | 0x4000:
				handleFileChunk(command.data);
				break;
			case ConfigCmd.CONFIGURATOR_PING | 0x4000:
			case ConfigCmd.STATUS | 0x4000:
				break;
			case 0xffff: //pre-entered default command, do nothing
				break;
			default:
				console.log({ ...command });
		}
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
			console.log('final chunk');
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
				if (!receivedChunks[i]) {
					console.log('Missing chunk: ' + i);
					port.sendCommand(ConfigCmd.BB_FILE_DOWNLOAD, [binFileNumber, i & 0xff, (i >> 8) & 0xff]);
					return;
				}
			}
			decodeBinFile();
		}
	}

	function decodeBinFile() {
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
				rateFactors[i][j] = leBytesToInt(rfBytes.slice(i * 12 + j * 4, i * 12 + j * 4 + 4)) / 65536;
		const pidConstants = [[], [], []] as number[][];
		const pidConstantsNice = [[], [], []] as number[][];
		const pcBytes = header.slice(74, 158);
		for (let i = 0; i < 3; i++) {
			pidConstants[i][0] = leBytesToInt(pcBytes.slice(i * 28, i * 28 + 4));
			pidConstantsNice[i][0] = pidConstants[i][0] >> 11;
			pidConstants[i][0] /= 65536;
			pidConstants[i][1] = leBytesToInt(pcBytes.slice(i * 28 + 4, i * 28 + 8));
			pidConstantsNice[i][1] = pidConstants[i][1] >> 3;
			pidConstants[i][1] /= 65536;
			pidConstants[i][2] = leBytesToInt(pcBytes.slice(i * 28 + 8, i * 28 + 12));
			pidConstantsNice[i][2] = pidConstants[i][2] >> 10;
			pidConstants[i][2] /= 65536;
			pidConstants[i][3] = leBytesToInt(pcBytes.slice(i * 28 + 12, i * 28 + 16));
			pidConstantsNice[i][3] = pidConstants[i][3] >> 10;
			pidConstants[i][3] /= 65536;
			pidConstants[i][4] = leBytesToInt(pcBytes.slice(i * 28 + 16, i * 28 + 20));
			pidConstantsNice[i][4] = pidConstants[i][4] >> 8;
			pidConstants[i][4] /= 65536;
			for (let j = 5; j < 7; j++)
				pidConstants[i][j] =
					leBytesToInt(pcBytes.slice(i * 28 + j * 4, i * 28 + j * 4 + 4)) / 65536;
		}
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
					data.slice(i + offsets['LOG_THROTTLE_ELRS_RAW'], i + offsets['LOG_THROTTLE_ELRS_RAW'] + 2)
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
					data.slice(i + offsets['LOG_THROTTLE_SETPOINT'], i + offsets['LOG_THROTTLE_SETPOINT'] + 2)
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
			if (flags.includes('LOG_FRAMETIME'))
				frame.frametime = leBytesToInt(
					data.slice(i + offsets['LOG_FRAMETIME'], i + offsets['LOG_FRAMETIME'] + 2)
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
			rawFile: binFile,
			isExact: true,
			pidConstantsNice
		};
		fillLogWithGenFlags(loadedLog);
		resolveWhenReady(loadedLog);
	}

	let logInfoPosition = 0;
	let logInfoInterval: number = -1;
	let selected = -1;
	function getLogInfo() {
		const infoNums = [] as number[];
		for (let i = 0; i < 10; i++) {
			if (logInfoPosition >= logNums.length) {
				//Depending on the NodeJS version this may be a NodeJS.Timer rather than a number
				//@ts-ignore
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
	const canvas = document.createElement('canvas');
	function drawCanvas(allowShortening = true) {
		if (!mounted) return;
		if (allowShortening) {
			clearTimeout(drawFullCanvasTimeout);
			drawFullCanvasTimeout = setTimeout(() => drawCanvas(false), 250);
		}
		const domCanvas = document.getElementById('bbDataViewer') as HTMLCanvasElement;
		canvas.width = domCanvas.width;
		canvas.height = domCanvas.height;
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
		const everyNth = Math.floor(sliceAndSkip.length / width);
		if (everyNth > 2 && allowShortening) {
			const len = sliceAndSkip.length;
			sliceAndSkip = [] as LogFrame[];
			for (let i = 0; i < len; i += everyNth) {
				sliceAndSkip.push(dataSlice[i]);
			}
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
				let bbFlag = BB_ALL_FLAGS[trace.flagName];
				if (trace.flagName.startsWith('GEN_'))
					bbFlag = BB_ALL_FLAGS[BB_GEN_FLAGS[trace.flagName].replaces];
				let path = bbFlag?.path || '';
				if (bbFlag?.usesModifier) {
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
		domCanvas.getContext('2d')?.clearRect(0, 0, dataViewer.clientWidth, dataViewer.clientHeight);
		domCanvas.getContext('2d')?.drawImage(canvas, 0, 0);
	}
	let trackingStartX = -1;
	let trackingEndX = 0;
	let firstX = 0;
	let startStartFrame = 0;
	let startEndFrame = 0;
	const selectionCanvas = document.createElement('canvas');
	function onMouseMove(e: MouseEvent) {
		if (trackingStartX === -1) return;
		if (trackingStartX === -2) {
			const domCanvas = document.getElementById('bbDataViewer') as HTMLCanvasElement;
			const ratio = (startStartFrame - startEndFrame) / domCanvas.width;
			const diff = e.offsetX - firstX;
			let deltaFrames = Math.floor(diff * ratio);
			if (startEndFrame + deltaFrames > loadedLog!.frameCount - 1)
				deltaFrames = loadedLog!.frameCount - 1 - startEndFrame;
			if (startStartFrame + deltaFrames < 0) deltaFrames = -startStartFrame;
			startFrame = startStartFrame + deltaFrames;
			endFrame = startEndFrame + deltaFrames;
			return;
		}
		trackingEndX = e.clientX;
		const ctx = selectionCanvas.getContext('2d') as CanvasRenderingContext2D;
		ctx.clearRect(0, 0, selectionCanvas.width, selectionCanvas.height);
		ctx.fillStyle = 'rgba(0,0,0,0.2)';
		ctx.fillRect(0, 0, selectionCanvas.width, selectionCanvas.height);
		ctx.clearRect(trackingStartX, 0, trackingEndX - trackingStartX, selectionCanvas.height);
		ctx.strokeStyle = 'white';
		ctx.lineWidth = 1;
		ctx.beginPath();
		ctx.moveTo(trackingStartX, 0);
		ctx.lineTo(trackingStartX, selectionCanvas.height);
		ctx.stroke();
		ctx.beginPath();
		ctx.moveTo(trackingEndX, 0);
		ctx.lineTo(trackingEndX, selectionCanvas.height);
		ctx.stroke();
		const domCanvas = document.getElementById('bbDataViewer') as HTMLCanvasElement;
		const domCtx = domCanvas.getContext('2d') as CanvasRenderingContext2D;
		domCtx.clearRect(0, 0, domCanvas.width, domCanvas.height);
		domCtx.drawImage(canvas, 0, 0);
		domCtx.drawImage(selectionCanvas, 0, 0);
	}
	function onMouseDown(e: MouseEvent) {
		if (!loadedLog || e.button !== 0) return;
		if (!e.shiftKey && (startFrame !== 0 || endFrame !== loadedLog.frameCount - 1)) {
			trackingStartX = -2;
			firstX = e.offsetX;
			startStartFrame = startFrame;
			startEndFrame = endFrame;
			return;
		}
		trackingStartX = e.offsetX;
		const domCanvas = document.getElementById('bbDataViewer') as HTMLCanvasElement;
		selectionCanvas.width = domCanvas.width;
		selectionCanvas.height = domCanvas.height;
		const ctx = selectionCanvas.getContext('2d') as CanvasRenderingContext2D;
		ctx.clearRect(0, 0, selectionCanvas.width, selectionCanvas.height);
		ctx.fillStyle = 'rgba(0,0,0,0.2)';
		ctx.fillRect(0, 0, selectionCanvas.width, selectionCanvas.height);
		ctx.strokeStyle = 'white';
		ctx.lineWidth = 1;
		ctx.beginPath();
		ctx.moveTo(trackingStartX, 0);
		ctx.lineTo(trackingStartX, selectionCanvas.height);
		ctx.stroke();
		const domCtx = domCanvas.getContext('2d') as CanvasRenderingContext2D;
		domCtx.clearRect(0, 0, domCanvas.width, domCanvas.height);
		domCtx.drawImage(canvas, 0, 0);
		domCtx.drawImage(selectionCanvas, 0, 0);
	}
	function onMouseUpOrLeave() {
		if (trackingStartX === -1) return;
		if (trackingStartX === -2) {
			trackingStartX = -1;
			return;
		}
		if (trackingStartX > trackingEndX) {
			const p = trackingStartX;
			trackingStartX = trackingEndX;
			trackingEndX = p;
		}
		const domCanvas = document.getElementById('bbDataViewer') as HTMLCanvasElement;
		const pStart = startFrame;
		startFrame =
			startFrame + Math.floor((endFrame - startFrame) * (trackingStartX / domCanvas.width));
		endFrame = pStart + Math.floor((endFrame - pStart) * (trackingEndX / domCanvas.width));
		trackingStartX = -1;
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
	function prefixZeros(num: number = 0, totalDigits: number, char: string = '0') {
		let str = num.toString();
		while (str.length < totalDigits) str = char + str;
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
	function openLogFromKbb() {
		const file = prompt('Paste the KBB file or JSON Array here');
		if (!file) return;
		if (file.startsWith('[') && file.endsWith(']')) {
			//JSON array
			try {
				binFile = JSON.parse(file);
			} catch (e) {
				alert('Error parsing JSON: ' + e);
			}
		} else {
			//KBB file
			binFile = file.split('').map((c) => c.charCodeAt(0));
		}
		decodeBinFile();
		startFrame = 0;
		endFrame = (loadedLog?.frameCount || 1) - 1;
		drawCanvas();
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
			console.log(loadedLog);
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
	onDestroy(() => {
		clearTimeout(drawFullCanvasTimeout);
	});
</script>

<div class="blackbox">
	<div class="selector">
		<select bind:value={selected}>
			{#each logNums as log}
				<option value={log?.num}>{log?.text || log?.num}</option>
			{/each}
		</select>
		<button on:click={() => openLog()} disabled={selected === -1}>Open</button>
		<button on:click={() => downloadLog()} disabled={selected === -1}>Download KBB</button>
		<button on:click={() => downloadLog('json')} disabled={selected === -1}>Download JSON</button>
		<button on:click={() => deleteLog()} disabled={selected === -1}>Delete</button>
		<button on:click={() => formatBB()}>Format</button>
		<button on:click={() => openLogFromPromptJSON()}>Open JSON</button>
		<button on:click={() => openLogFromKbb()}>Open KBB</button>
	</div>
	<div class="dataViewerWrapper">
		<canvas
			id="bbDataViewer"
			on:mousedown={onMouseDown}
			on:mouseup={onMouseUpOrLeave}
			on:mousemove={onMouseMove}
			on:mouseleave={onMouseUpOrLeave}
			on:dblclick={() => {
				startFrame = 0;
				endFrame = (loadedLog?.frameCount || 1) - 1;
				drawCanvas();
			}}
		/>
	</div>
	<div class="flagSelector">
		{#each graphs as graph, graphIndex}
			<div class="graphSelector">
				{#each graph as trace, traceIndex}
					<TracePlacer
						flags={loadedLog?.flags || []}
						autoRange={getAutoRangeByFlagName(
							trace.flagName.startsWith('GEN_')
								? BB_GEN_FLAGS[trace.flagName].replaces
								: trace.flagName
						)}
						flagProps={BB_ALL_FLAGS}
						genFlagProps={BB_GEN_FLAGS}
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
		{#if loadedLog}
			<div class="fileInfo" style="margin-top: .8rem">
				<div>Blackbox Version: {loadedLog.version.join('.')}</div>
				<div>Start Time: {loadedLog.startTime} ms</div>
				<div>Frame Count: {loadedLog.frameCount}</div>
				<div>PID Frequency: {loadedLog.pidFrequency} Hz</div>
				<div>Frames per Second: {loadedLog.framesPerSecond} Hz</div>
				<div style="white-space: preserve">
					Flags: {'\n  - ' + loadedLog.flags.filter((n) => n.startsWith('LOG_')).join('\n  - ')}
				</div>
				<div>File Size: {(loadedLog.rawFile.length / 1000).toFixed(1)} KB</div>
				<div>
					IMU Range: {loadedLog.ranges.gyro}°/sec, ±{loadedLog.ranges.accel}g
				</div>
				<div>
					PID Gains:
					<div>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;ROLL PITCH&nbsp;&nbsp;&nbsp;YAW</div>
					<div style="white-space:pre-wrap">
						&nbsp;&nbsp;P:&nbsp;&nbsp;{prefixZeros(loadedLog.pidConstantsNice[0][0], 5, ' ')}
						{prefixZeros(loadedLog.pidConstantsNice[1][0], 5, ' ')}
						{prefixZeros(loadedLog.pidConstantsNice[2][0], 5, ' ')}
					</div>
					<div style="white-space:pre-wrap">
						&nbsp;&nbsp;I:&nbsp;&nbsp;{prefixZeros(loadedLog.pidConstantsNice[0][1], 5, ' ')}
						{prefixZeros(loadedLog.pidConstantsNice[1][1], 5, ' ')}
						{prefixZeros(loadedLog.pidConstantsNice[2][1], 5, ' ')}
					</div>
					<div style="white-space:pre-wrap">
						&nbsp;&nbsp;D:&nbsp;&nbsp;{prefixZeros(loadedLog.pidConstantsNice[0][2], 5, ' ')}
						{prefixZeros(loadedLog.pidConstantsNice[1][2], 5, ' ')}
						{prefixZeros(loadedLog.pidConstantsNice[2][2], 5, ' ')}
					</div>
					<div style="white-space:pre-wrap">
						&nbsp;&nbsp;FF:&nbsp;{prefixZeros(loadedLog.pidConstantsNice[0][3], 5, ' ')}
						{prefixZeros(loadedLog.pidConstantsNice[1][3], 5, ' ')}
						{prefixZeros(loadedLog.pidConstantsNice[2][3], 5, ' ')}
					</div>
					<div style="white-space:pre-wrap">
						&nbsp;&nbsp;S:&nbsp;&nbsp;{prefixZeros(loadedLog.pidConstantsNice[0][4], 5, ' ')}
						{prefixZeros(loadedLog.pidConstantsNice[1][4], 5, ' ')}
						{prefixZeros(loadedLog.pidConstantsNice[2][4], 5, ' ')}
					</div>
				</div>
				<div class="rateFactors">
					Rate Factors:
					<div>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;ROLL PITCH&nbsp;&nbsp;&nbsp;YAW</div>
					<div style="white-space:pre-wrap">
						&nbsp;&nbsp;x^1:&nbsp;{prefixZeros(loadedLog.rateFactors[0][0], 5, ' ')}
						{prefixZeros(loadedLog.rateFactors[0][1], 5, ' ')}
						{prefixZeros(loadedLog.rateFactors[0][2], 5, ' ')}
					</div>
					<div style="white-space:pre-wrap">
						&nbsp;&nbsp;x^2:&nbsp;{prefixZeros(loadedLog.rateFactors[1][0], 5, ' ')}
						{prefixZeros(loadedLog.rateFactors[1][1], 5, ' ')}
						{prefixZeros(loadedLog.rateFactors[1][2], 5, ' ')}
					</div>
					<div style="white-space:pre-wrap">
						&nbsp;&nbsp;x^3:&nbsp;{prefixZeros(loadedLog.rateFactors[2][0], 5, ' ')}
						{prefixZeros(loadedLog.rateFactors[2][1], 5, ' ')}
						{prefixZeros(loadedLog.rateFactors[2][2], 5, ' ')}
					</div>
					<div style="white-space:pre-wrap">
						&nbsp;&nbsp;x^4:&nbsp;{prefixZeros(loadedLog.rateFactors[3][0], 5, ' ')}
						{prefixZeros(loadedLog.rateFactors[3][1], 5, ' ')}
						{prefixZeros(loadedLog.rateFactors[3][2], 5, ' ')}
					</div>
					<div style="white-space:pre-wrap">
						&nbsp;&nbsp;x^5:&nbsp;{prefixZeros(loadedLog.rateFactors[4][0], 5, ' ')}
						{prefixZeros(loadedLog.rateFactors[4][1], 5, ' ')}
						{prefixZeros(loadedLog.rateFactors[4][2], 5, ' ')}
					</div>
				</div>
			</div>
		{/if}
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
