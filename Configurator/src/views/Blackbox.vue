<script lang="ts">
import { defineComponent } from "vue";
import Timeline from "@components/blackbox/Timeline.vue";
import Settings from "@components/blackbox/Settings.vue";
import { BBLog, LogFrame, TraceInGraph, FlagProps, GenFlagProps, Command } from "@/utils/types";
import { constrain, getNestedProperty, leBytesToInt, map, prefixZeros, roundToDecimal } from "@/utils/utils";
import { MspFn, MspVersion } from "@/utils/msp";
import { useLogStore } from "@/stores/logStore";
import { addOnCommandHandler, addOnConnectHandler, removeOnCommandHandler, removeOnConnectHandler, sendCommand } from "@/communication/serial";
import TracePlacer from "@/components/blackbox/TracePlacer.vue";

const DURATION_BAR_RASTER = [
	'100us',
	'200us',
	'500us',
	'1ms',
	'2ms',
	'5ms',
	'10ms',
	'20ms',
	'50ms',
	'100ms',
	'200ms',
	'0.5s',
	'1s',
	'2s',
	'5s',
	'10s',
	'20s',
	'30s',
	'1min',
	'2min',
	'5min',
	'10min',
	'20min',
	'30min',
	'1h'
];

const getGyroBBRange = (file: BBLog | undefined) => {
	if (!file) return { max: -2000, min: 2000 };
	let maxSetpoints = [0, 0, 0];
	for (let exp = 0; exp < 5; exp++)
		for (let ax = 0; ax < 3; ax++) maxSetpoints[ax] += file.rateFactors[exp][ax];
	//Math.max leads to a stack overflow, thus using a for loop now
	let max = 0,
		min = 0;
	for (let i = 0; i < maxSetpoints.length; i++) {
		if (maxSetpoints[i] > max) max = maxSetpoints[i];
		if (maxSetpoints[i] < min) min = maxSetpoints[i];
	}
	file.frames.forEach(f => {
		if (f.gyro.roll! > max) max = f.gyro.roll!;
		if (f.gyro.pitch! > max) max = f.gyro.pitch!;
		if (f.gyro.yaw! > max) max = f.gyro.yaw!;
		if (f.gyro.roll! < min) min = f.gyro.roll!;
		if (f.gyro.pitch! < min) min = f.gyro.pitch!;
		if (f.gyro.yaw! < min) min = f.gyro.yaw!;
	});
	const fullRange = Math.round(Math.max(max, -min));
	return { max: fullRange, min: -fullRange };
};
const getAltitudeRange = (file: BBLog | undefined) => {
	if (!file) return { max: 300, min: 0 };
	let max = -20000,
		min = 20000;
	file.frames.forEach(f => {
		if (f.motion.altitude! > max) max = f.motion.altitude!;
		if (f.motion.altitude! < min) min = f.motion.altitude!;
	});
	max = 10 * Math.ceil(max / 10);
	min = 10 * Math.floor(min / 10);
	return { max, min };
};

const BB_ALL_FLAGS: { [key: string]: FlagProps } = {
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
		unit: 'µs'
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
		path: 'motors.out',
		minValue: 0,
		maxValue: 2000,
		unit: '',
		modifier: [
			{ displayNameShort: 'RR', displayName: 'Rear Right', path: 'rr' },
			{ displayNameShort: 'FR', displayName: 'Front Right', path: 'fr' },
			{ displayNameShort: 'FL', displayName: 'Front Left', path: 'fl' },
			{ displayNameShort: 'RL', displayName: 'Rear Left', path: 'rl' }
		]
	},
	LOG_FRAMETIME: {
		name: 'Frametime',
		path: 'frametime',
		minValue: 0,
		maxValue: 1000,
		unit: 'µs'
	},
	LOG_FLIGHT_MODE: {
		name: 'Flight Mode',
		path: 'flightMode',
		minValue: 0,
		maxValue: 4,
		states: ['Acro', 'Angle', 'Altitude Hold', 'GPS Velocity', 'GPS Position'],
		unit: ''
	},
	LOG_ALTITUDE: {
		name: 'Altitude',
		path: 'motion.altitude',
		rangeFn: getAltitudeRange,
		decimals: 2,
		unit: 'm'
	},
	LOG_VVEL: {
		name: 'Vertical Velocity',
		path: 'motion.vvel',
		minValue: -10,
		maxValue: 10,
		decimals: 2,
		unit: 'm/s'
	},
	LOG_GPS: {
		name: 'GPS',
		path: 'motion.gps',
		minValue: 0,
		maxValue: 100,
		unit: '',
		modifier: [
			{ displayNameShort: 'Year', displayName: 'Year', min: 2020, max: 2030, path: 'year' },
			{
				displayNameShort: 'Month',
				displayName: 'Month',
				min: 1,
				max: 12,
				path: 'month',
				states: [
					'',
					'January',
					'February',
					'March',
					'April',
					'May',
					'June',
					'July',
					'August',
					'September',
					'October',
					'November',
					'December'
				]
			},
			{ displayNameShort: 'Day', displayName: 'Day', min: 1, max: 31, path: 'day' },
			{ displayNameShort: 'Hour', displayName: 'Hour', min: 0, max: 23, path: 'hour' },
			{ displayNameShort: 'Minute', displayName: 'Minute', min: 0, max: 59, path: 'minute' },
			{ displayNameShort: 'Second', displayName: 'Second', min: 0, max: 59, path: 'second' },
			{
				displayNameShort: 'Valid',
				displayName: 'Validity Flags',
				min: 0,
				max: 255,
				path: 'time_validity_flags'
			},
			{
				displayNameShort: 'T Acc',
				displayName: 'Time Accuracy',
				min: 0,
				max: 100,
				path: 't_acc',
				unit: 'ns'
			},
			{
				displayNameShort: 'Nanosec',
				displayName: 'Nanoseconds',
				min: 0,
				max: 1e9,
				path: 'ns',
				unit: 'ns'
			},
			{
				displayNameShort: 'Fix',
				displayName: 'Fix Type',
				min: 0,
				max: 5,
				path: 'fix_type',
				states: [
					'No Fix',
					'Dead reckoning only',
					'2D Fix',
					'3D Fix',
					'GPS + Dead reckoning',
					'Time only fix'
				]
			},
			{ displayNameShort: 'Flags', displayName: 'Flags', min: 0, max: 255, path: 'flags' },
			{ displayNameShort: 'Flags2', displayName: 'Flags2', min: 0, max: 255, path: 'flags2' },
			{
				displayNameShort: 'Sats',
				displayName: 'Satellite Count',
				min: 0,
				max: 30,
				path: 'sat_count'
			},
			{
				displayNameShort: 'Lon',
				displayName: 'Longitude',
				min: -180,
				max: 180,
				path: 'lon',
				unit: '°',
				decimals: 7
			},
			{
				displayNameShort: 'Lat',
				displayName: 'Latitude',
				min: -90,
				max: 90,
				path: 'lat',
				unit: '°',
				decimals: 7
			},
			{
				displayNameShort: 'Alt',
				displayName: 'Altitude',
				rangeFn: getAltitudeRange,
				path: 'alt',
				unit: 'm',
				decimals: 2
			},
			{
				displayNameShort: 'Hor Acc',
				displayName: 'Horizontal Accuracy',
				min: 0,
				max: 20,
				path: 'h_acc',
				unit: 'm',
				decimals: 2
			},
			{
				displayNameShort: 'Ver Acc',
				displayName: 'Vertical Accuracy',
				min: 0,
				max: 20,
				path: 'v_acc',
				unit: 'm',
				decimals: 2
			},
			{
				displayNameShort: 'Vel N',
				displayName: 'Velocity North',
				min: -10,
				max: 10,
				path: 'vel_n',
				unit: 'm/s',
				decimals: 2
			},
			{
				displayNameShort: 'Vel E',
				displayName: 'Velocity East',
				min: -10,
				max: 10,
				path: 'vel_e',
				unit: 'm/s',
				decimals: 2
			},
			{
				displayNameShort: 'Vel D',
				displayName: 'Velocity Down',
				min: 10,
				max: -10, //down is positive, invert by default
				path: 'vel_d',
				unit: 'm/s',
				decimals: 2
			},
			{
				displayNameShort: 'G Speed',
				displayName: 'Ground Speed',
				min: 0,
				max: 50,
				path: 'g_speed',
				unit: 'm/s',
				decimals: 2
			},
			{
				displayNameShort: 'Head Mot',
				displayName: 'Heading of Motion',
				min: -180,
				max: 180,
				path: 'head_mot',
				unit: '°'
			},
			{
				displayNameShort: 'S Acc',
				displayName: 'Speed Accuracy',
				min: 0,
				max: 10,
				path: 's_acc',
				unit: 'm/s',
				decimals: 2
			},
			{
				displayNameShort: 'Head Acc',
				displayName: 'Heading Accuracy',
				min: 0,
				max: 20,
				path: 'head_acc',
				unit: '°',
				decimals: 2
			},
			{
				displayNameShort: 'pDop',
				displayName: 'pDop',
				min: 0,
				max: 100,
				path: 'p_dop',
				decimals: 2
			},
			{ displayNameShort: 'Flags3', displayName: 'Flags3', min: 0, max: 31, path: 'flags3' }
		]
	},
	LOG_ATT_ROLL: {
		name: 'Roll Angle',
		path: 'attitude.roll',
		minValue: -180,
		maxValue: 180,
		unit: '°'
	},
	LOG_ATT_PITCH: {
		name: 'Pitch Angle',
		path: 'attitude.pitch',
		minValue: -180,
		maxValue: 180,
		unit: '°'
	},
	LOG_ATT_YAW: {
		name: 'Yaw Angle',
		path: 'attitude.yaw',
		minValue: -180,
		maxValue: 180,
		unit: '°'
	},
	LOG_MOTOR_RPM: {
		name: 'Motor RPM',
		path: 'motors.rpm',
		minValue: 0,
		maxValue: 50000,
		unit: 'rpm',
		modifier: [
			{ displayNameShort: 'RR', displayName: 'Rear Right', path: 'rr' },
			{ displayNameShort: 'FR', displayName: 'Front Right', path: 'fr' },
			{ displayNameShort: 'FL', displayName: 'Front Left', path: 'fl' },
			{ displayNameShort: 'RL', displayName: 'Rear Left', path: 'rl' }
		]
	},
	LOG_ACCEL_RAW: {
		name: 'Accel Raw',
		path: 'motion.accelRaw',
		minValue: -40,
		maxValue: 40,
		unit: 'm/s²',
		decimals: 3,
		modifier: [
			{ displayNameShort: 'X', displayName: 'X', path: 'x' },
			{ displayNameShort: 'Y', displayName: 'Y', path: 'y' },
			{ displayNameShort: 'Z', displayName: 'Z', path: 'z' }
		]
	},
	LOG_ACCEL_FILTERED: {
		name: 'Accel Filtered',
		path: 'motion.accelFiltered',
		minValue: -40,
		maxValue: 40,
		unit: 'm/s²',
		decimals: 3,
		modifier: [
			{ displayNameShort: 'X', displayName: 'X', path: 'x' },
			{ displayNameShort: 'Y', displayName: 'Y', path: 'y' },
			{ displayNameShort: 'Z', displayName: 'Z', path: 'z' }
		]
	},
	LOG_VERTICAL_ACCEL: {
		name: 'Vertical Accel',
		path: 'motion.accelVertical',
		minValue: -40,
		maxValue: 40,
		unit: 'm/s²',
		decimals: 3
	},
	LOG_VVEL_SETPOINT: {
		name: 'vVel Setpoint',
		path: 'setpoint.vvel',
		minValue: -10,
		maxValue: 10,
		decimals: 2,
		unit: 'm/s'
	},
	LOG_MAG_HEADING: {
		name: 'Mag Heading',
		path: 'motion.magHeading',
		minValue: -180,
		maxValue: 180,
		unit: '°'
	},
	LOG_COMBINED_HEADING: {
		name: 'Combined Heading',
		path: 'motion.combinedHeading',
		minValue: -180,
		maxValue: 180,
		unit: '°'
	}
};
const ACC_RANGES = [2, 4, 8, 16];
const GYRO_RANGES = [2000, 1000, 500, 250, 125];

const BB_GEN_FLAGS: { [key: string]: GenFlagProps } = {
	GEN_ROLL_SETPOINT: {
		replaces: 'LOG_ROLL_SETPOINT',
		requires: ['LOG_ROLL_ELRS_RAW'],
		exact: false
	},
	GEN_PITCH_SETPOINT: {
		replaces: 'LOG_PITCH_SETPOINT',
		requires: ['LOG_PITCH_ELRS_RAW'],
		exact: false
	},
	GEN_THROTTLE_SETPOINT: {
		replaces: 'LOG_THROTTLE_SETPOINT',
		requires: ['LOG_THROTTLE_ELRS_RAW'],
		exact: false
	},
	GEN_YAW_SETPOINT: {
		replaces: 'LOG_YAW_SETPOINT',
		requires: ['LOG_YAW_ELRS_RAW'],
		exact: false
	},
	GEN_ROLL_PID_P: {
		replaces: 'LOG_ROLL_PID_P',
		requires: [['LOG_ROLL_SETPOINT', 'GEN_ROLL_SETPOINT'], 'LOG_ROLL_GYRO_RAW'],
		exact: true
	},
	GEN_ROLL_PID_I: {
		replaces: 'LOG_ROLL_PID_I',
		requires: [['LOG_ROLL_SETPOINT', 'GEN_ROLL_SETPOINT'], 'LOG_ROLL_GYRO_RAW'],
		exact: false
	},
	GEN_ROLL_PID_D: {
		replaces: 'LOG_ROLL_PID_D',
		requires: ['LOG_ROLL_GYRO_RAW'],
		exact: false
	},
	GEN_ROLL_PID_FF: {
		replaces: 'LOG_ROLL_PID_FF',
		requires: [['LOG_ROLL_SETPOINT', 'GEN_ROLL_SETPOINT']],
		exact: false
	},
	GEN_ROLL_PID_S: {
		replaces: 'LOG_ROLL_PID_S',
		requires: [['LOG_ROLL_SETPOINT', 'GEN_ROLL_SETPOINT']],
		exact: true
	},
	GEN_PITCH_PID_P: {
		replaces: 'LOG_PITCH_PID_P',
		requires: [['LOG_PITCH_SETPOINT', 'GEN_PITCH_SETPOINT'], 'LOG_PITCH_GYRO_RAW'],
		exact: true
	},
	GEN_PITCH_PID_I: {
		replaces: 'LOG_PITCH_PID_I',
		requires: [['LOG_PITCH_SETPOINT', 'GEN_PITCH_SETPOINT'], 'LOG_PITCH_GYRO_RAW'],
		exact: false
	},
	GEN_PITCH_PID_D: {
		replaces: 'LOG_PITCH_PID_D',
		requires: ['LOG_PITCH_GYRO_RAW'],
		exact: false
	},
	GEN_PITCH_PID_FF: {
		replaces: 'LOG_PITCH_PID_FF',
		requires: [['LOG_PITCH_SETPOINT', 'GEN_PITCH_SETPOINT']],
		exact: false
	},
	GEN_PITCH_PID_S: {
		replaces: 'LOG_PITCH_PID_S',
		requires: [['LOG_PITCH_SETPOINT', 'GEN_PITCH_SETPOINT']],
		exact: true
	},
	GEN_YAW_PID_P: {
		replaces: 'LOG_YAW_PID_P',
		requires: [['LOG_YAW_SETPOINT', 'GEN_YAW_SETPOINT'], 'LOG_YAW_GYRO_RAW'],
		exact: true
	},
	GEN_YAW_PID_I: {
		replaces: 'LOG_YAW_PID_I',
		requires: [['LOG_YAW_SETPOINT', 'GEN_YAW_SETPOINT'], 'LOG_YAW_GYRO_RAW'],
		exact: false
	},
	GEN_YAW_PID_D: {
		replaces: 'LOG_YAW_PID_D',
		requires: ['LOG_YAW_GYRO_RAW'],
		exact: false
	},
	GEN_YAW_PID_FF: {
		replaces: 'LOG_YAW_PID_FF',
		requires: [['LOG_YAW_SETPOINT', 'GEN_YAW_SETPOINT']],
		exact: false
	},
	GEN_YAW_PID_S: {
		replaces: 'LOG_YAW_PID_S',
		requires: [['LOG_YAW_SETPOINT', 'GEN_YAW_SETPOINT']],
		exact: true
	},
	GEN_MOTOR_OUTPUTS: {
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
		exact: true
	},
	GEN_VVEL_SETPOINT: {
		replaces: 'LOG_VVEL_SETPOINT',
		requires: ['LOG_THROTTLE_ELRS_RAW'],
		exact: false
	}
};

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
			binFile: [] as number[],
			binFileNumber: -1,
			receivedChunks: [] as boolean[],
			totalChunks: -1,
			resolveWhenReady: (_log: BBLog) => { },
			rejectWrongFile: (_: string) => { },
			showSettings: false,
			logInfoPosition: 0,
			logInfoInterval: -1,
			selected: -1,
			canvas: document.createElement("canvas"),
			selectionCanvas: document.createElement("canvas"),
			sliceAndSkip: [] as LogFrame[],
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
		dataSlice(): LogFrame[] {
			return this.loadedLog?.frames.slice(this.startFrame, this.endFrame + 1) || []
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
			}
		},
	},
	methods: {
		onCommand(command: Command) {
			if (command.cmdType === 'response') {
				switch (command.command) {
					case MspFn.BB_FILE_LIST:
						this.logNums = command.data.map(n => ({ text: '', num: n }));
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
			this.graphs = [...this.graphs, []];
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
			this.graphs[graphIndex] = [...this.graphs[graphIndex], defaultTrace];
		},
		formatBB() {
			sendCommand('request', MspFn.BB_FORMAT);
		},
		getLog(num: number): Promise<BBLog> {
			this.binFileNumber = -1;

			sendCommand('request', MspFn.BB_FILE_DOWNLOAD, MspVersion.V2, [num]);
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
						this.binFile = [];
						const view = new Uint8Array(data);
						for (let i = 0; i < view.length; i++) {
							this.binFile.push(view[i]);
						}

						this.decodeBinFile();
						this.startFrame = 0;
						this.endFrame = (this.loadedLog?.frameCount || 1) - 1;
					};
					reader.readAsArrayBuffer(file);
				}
			};
			input.click();
		},
		downloadLog(type: 'kbb' | 'json' = 'kbb') {
			this.getLog(this.selected)
				.then(data => {
					const file = data.rawFile;
					const blob = new Blob([new Uint8Array(file)], { type: 'application/octet-stream' });
					const blobJSON = new Blob([JSON.stringify(data)], { type: 'application/json' });
					const url = URL.createObjectURL(type === 'json' ? blobJSON : blob);
					//download file
					const a = document.createElement('a');
					a.href = url;
					a.download = `bb${prefixZeros(this.selected, 2)} ${data.startTime
						.toISOString()
						.replace('T', ' ')
						.replace('.000Z', '')
						.replace(/_/g, '-')}.${type}`;
					a.click();
					URL.revokeObjectURL(url);
				})
				.catch(console.error);
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
			const domCanvas = document.getElementById('bbDataViewer') as HTMLCanvasElement;
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
						this.startFrame + Math.round((this.endFrame - this.startFrame) * (touches[0].clientX / domCanvas.width));
					this.frame1 =
						this.startFrame + Math.round((this.endFrame - this.startFrame) * (touches[1].clientX / domCanvas.width));
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
					this.startFrame + Math.round((this.endFrame - this.startFrame) * (touches[0].clientX / domCanvas.width));
				this.frame1 =
					this.startFrame + Math.round((this.endFrame - this.startFrame) * (touches[1].clientX / domCanvas.width));
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
			const domCanvas = document.getElementById('bbDataViewer') as HTMLCanvasElement;
			const touches = [];
			for (let i = 0; i < e.touches.length; i++)
				if (e.touches[i].identifier < 2) touches.push(e.touches[i]);
			if (this.touchMode === 'move') {
				const diff = touches[0].clientX - this.touchStartX;
				const ratio = (this.startStartFrame - this.startEndFrame) / domCanvas.width;
				let deltaFrames = Math.floor(diff * ratio);
				if (this.startEndFrame + deltaFrames > this.loadedLog!.frameCount - 1)
					deltaFrames = this.loadedLog!.frameCount - 1 - this.startEndFrame;
				if (this.startStartFrame + deltaFrames < 0) deltaFrames = -this.startStartFrame;
				this.startFrame = this.startStartFrame + deltaFrames;
				this.endFrame = this.startEndFrame + deltaFrames;
			}
			if (this.touchMode === 'zoom') {
				let span = Math.round(
					((this.frame1 - this.frame0) * domCanvas.width) / (e.touches[1].clientX - e.touches[0].clientX)
				);
				if (span < 0) span = this.loadedLog.frameCount - 1;
				if (span >= this.loadedLog.frameCount - 1) {
					this.startFrame = 0;
					this.endFrame = this.loadedLog.frameCount - 1;
					return;
				}
				const frameCenter = Math.round((this.frame0 + this.frame1) / 2);
				const centerPos = (e.touches[0].clientX + e.touches[1].clientX) / 2;
				this.startFrame = frameCenter - Math.round(span * (centerPos / domCanvas.width));
				this.endFrame = frameCenter + Math.round(span * (1 - centerPos / domCanvas.width));
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
			const domCanvas = document.getElementById('bbDataViewer') as HTMLCanvasElement;
			const domCtx = domCanvas.getContext('2d') as CanvasRenderingContext2D;
			domCtx.clearRect(0, 0, domCanvas.width, domCanvas.height);
			domCtx.drawImage(this.canvas, 0, 0);
			domCtx.drawImage(this.selectionCanvas, 0, 0);
		},
		onMouseMove(e: MouseEvent) {
			if (!this.loadedLog) return;
			if (e.buttons !== 1) {
				this.onMouseUp();
				// highlight all points on the current frame
				const domCanvas = document.getElementById('bbDataViewer') as HTMLCanvasElement;
				const domCtx = domCanvas.getContext('2d') as CanvasRenderingContext2D;
				domCtx.clearRect(0, 0, domCanvas.width, domCanvas.height);
				domCtx.drawImage(this.canvas, 0, 0);
				const closestFrameSliceSkip = Math.round(
					(e.offsetX / domCanvas.width) * (this.sliceAndSkip.length - 1)
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
				const frameWidth = width / (this.sliceAndSkip.length - 1);
				const frameX = closestFrameSliceSkip * frameWidth;
				ctx.moveTo(frameX, 0);
				ctx.lineTo(frameX, highlightCanvas.height);
				ctx.stroke();
				//iterate over all graphs and draw the points
				const numGraphs = this.graphs.length;
				const heightPerGraph =
					(height - this.dataViewerWrapper.clientHeight * 0.02 * (numGraphs - 1)) / numGraphs;
				let heightOffset = 0.01 * this.dataViewerWrapper.clientHeight;
				const frame = this.sliceAndSkip[closestFrameSliceSkip];
				for (let i = 0; i < numGraphs; i++) {
					const graph = this.graphs[i];
					const numTraces = graph.length;
					for (let j = 0; j < numTraces; j++) {
						const trace = graph[j];
						const range = trace.maxValue - trace.minValue;
						const scale = heightPerGraph / range;
						ctx.strokeStyle = trace.color;
						ctx.lineWidth = trace.strokeWidth * 2;
						const pointY =
							heightOffset +
							heightPerGraph -
							((trace.overrideData
								? constrain(
									trace.overrideSliceAndSkip![closestFrameSliceSkip],
									trace.minValue,
									trace.maxValue
								)
								: getNestedProperty(frame, trace.path, {
									max: Math.max(trace.maxValue, trace.minValue),
									min: Math.min(trace.minValue, trace.maxValue)
								})) -
								trace.minValue) *
							scale;
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
				const domCanvas = document.getElementById('bbDataViewer') as HTMLCanvasElement;
				const ratio = (this.startStartFrame - this.startEndFrame) / domCanvas.width;
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
			const domCanvas = document.getElementById('bbDataViewer') as HTMLCanvasElement;
			const nStart =
				this.startFrame + Math.floor((this.endFrame - this.startFrame) * (this.trackingStartX / domCanvas.width));
			const nEnd =
				this.startFrame + Math.floor((this.endFrame - this.startFrame) * (this.trackingEndX / domCanvas.width));
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
			const domCanvas = document.getElementById('bbDataViewer') as HTMLCanvasElement;
			this.canvas.width = domCanvas.width;
			this.canvas.height = domCanvas.height;
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
			if (!this.dataSlice.length) return;
			this.sliceAndSkip = [];
			this.skipValue = 1;
			const everyNth = Math.floor(this.dataSlice.length / width);
			if (everyNth > 2 && allowShortening) {
				this.skipValue = everyNth;
				const len = this.dataSlice.length;
				for (let i = 0; i < len; i += everyNth) {
					this.sliceAndSkip.push(this.dataSlice[i]);
				}
				clearTimeout(this.drawFullCanvasTimeout);
				this.drawFullCanvasTimeout = setTimeout(() => this.drawCanvas(false), 250);
			} else {
				this.sliceAndSkip = this.dataSlice;
			}
			const pixelsPerSec =
				(this.dataViewerWrapper.clientWidth * this.loadedLog.framesPerSecond) / (this.dataSlice.length - 1);
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

			const frameWidth = width / (this.sliceAndSkip.length - 1);
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
					let pointY =
						heightOffset +
						heightPerGraph -
						((trace.overrideData
							? constrain(trace.overrideSliceAndSkip[0], trace.minValue, trace.maxValue)
							: getNestedProperty(this.sliceAndSkip[0], trace.path, {
								max: Math.max(trace.maxValue, trace.minValue),
								min: Math.min(trace.minValue, trace.maxValue)
							})) -
							trace.minValue) *
						scale;
					ctx.moveTo(0, pointY);
					for (let k = 1; k < this.sliceAndSkip.length; k++) {
						pointY =
							heightOffset +
							heightPerGraph -
							((trace.overrideData
								? constrain(trace.overrideSliceAndSkip[k], trace.minValue, trace.maxValue)
								: getNestedProperty(this.sliceAndSkip[k], trace.path, {
									max: Math.max(trace.maxValue, trace.minValue),
									min: Math.min(trace.minValue, trace.maxValue)
								})) -
								trace.minValue) *
							scale;
						ctx.lineTo(k * frameWidth, pointY);
					}
					ctx.stroke();
				}
				heightOffset += heightPerGraph + 0.02 * this.dataViewerWrapper.clientHeight;
			}
			domCanvas.getContext('2d')?.clearRect(0, 0, this.dataViewerWrapper.clientWidth, this.dataViewerWrapper.clientHeight);
			domCanvas.getContext('2d')?.drawImage(this.canvas, 0, 0);
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
		handleFileChunk(data: number[]) {
			//1027 data bytes per packet
			//first byte is file number
			//second and third are chunk number
			//up to 1024 bytes of binary file data
			//last packet has the chunk number set to 0xFFFF, and then two bytes that indicate the number of total chunks
			if (this.binFileNumber !== data[0]) {
				this.binFileNumber = data[0];
				this.binFile = [];
				this.receivedChunks = [];
				this.graphs = [[]];
				this.loadedLog = undefined;
				this.totalChunks = -1;
			}
			const chunkNum = leBytesToInt(data.slice(1, 3));
			if (chunkNum === 0xffff) {
				this.totalChunks = leBytesToInt(data.slice(3, 5));
			} else {
				const chunkSize = data.length - 3;
				const chunk = data.slice(3, 3 + chunkSize);
				this.receivedChunks[chunkNum] = true;
				for (let i = chunkNum * 1024; i < chunkNum * 1024 + chunkSize; i++) {
					this.binFile[i] = chunk[i - chunkNum * 1024];
				}
			}
			if (this.receivedChunks.length === this.totalChunks) {
				for (let i = 0; i < this.totalChunks; i++) {
					if (!this.receivedChunks[i]) {
						console.log('Missing chunk: ' + i);
						sendCommand('request', MspFn.BB_FILE_DOWNLOAD, MspVersion.V2, [
							this.binFileNumber,
							i & 0xff,
							(i >> 8) & 0xff,
							(i >> 16) & 0xff,
							(i >> 24) & 0xff
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
			const sTime = leBytesToInt(header.slice(7, 11));
			const year = sTime >> 26;
			const month = (sTime >> 22) & 0b1111;
			const day = (sTime >> 17) & 0b11111;
			const hour = (sTime >> 12) & 0b11111;
			const minute = (sTime >> 6) & 0b111111;
			const second = sTime & 0b111111;
			const startTime = new Date(year + 2000, month - 1, day, hour, minute, second);
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
				pidConstantsNice[i][3] = pidConstants[i][3] >> 13;
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
			const motorPoles = header[166];
			const flags: string[] = [];
			let frameSize = 0;
			const offsets: { [key: string]: number } = {};
			for (let i = 0; i < 32 && Object.keys(BB_ALL_FLAGS).length > i; i++) {
				const flagIsSet = flagsLow & (1 << i);
				if (flagIsSet) {
					flags.push(Object.keys(BB_ALL_FLAGS)[i]);
					offsets[Object.keys(BB_ALL_FLAGS)[i]] = frameSize;
					if (i == 26) frameSize += 6;
					else if (i == 28) frameSize++;
					else frameSize += 2;
				}
			}
			for (let i = 0; i < 32 && Object.keys(BB_ALL_FLAGS).length > i + 32; i++) {
				const flagIsSet = flagsHigh & (1 << i);
				if (flagIsSet) {
					flags.push(Object.keys(BB_ALL_FLAGS)[i + 32]);
					offsets[Object.keys(BB_ALL_FLAGS)[i + 32]] = frameSize;
					if ([35, 36, 37].includes(i + 32)) frameSize += 6;
					else frameSize += 2;
				}
			}
			const framesPerSecond = pidFreq / freqDiv;
			const frames = data.length / frameSize;
			const log: LogFrame[] = [];
			for (let i = 0; i < data.length; i += frameSize) {
				const frame: LogFrame = {
					elrs: {},
					setpoint: {},
					gyro: {},
					pid: { roll: {}, pitch: {}, yaw: {} },
					motors: { out: {}, rpm: {} },
					motion: { gps: {}, accelRaw: {}, accelFiltered: {} },
					attitude: {}
				};
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
					frame.setpoint.throttle =
						leBytesToInt(
							data.slice(
								i + offsets['LOG_THROTTLE_SETPOINT'],
								i + offsets['LOG_THROTTLE_SETPOINT'] + 2
							)
						) /
						32 +
						1000;
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
					const motors01 = leBytesToInt(throttleBytes.slice(0, 3));
					const motors23 = leBytesToInt(throttleBytes.slice(3, 6));
					frame.motors.out.rr = motors01 & 0xfff;
					frame.motors.out.fr = motors01 >> 12;
					frame.motors.out.rl = motors23 & 0xfff;
					frame.motors.out.fl = motors23 >> 12;
				}
				if (flags.includes('LOG_FRAMETIME'))
					frame.frametime = leBytesToInt(
						data.slice(i + offsets['LOG_FRAMETIME'], i + offsets['LOG_FRAMETIME'] + 2)
					);
				if (flags.includes('LOG_FLIGHT_MODE')) {
					frame.flightMode = data[i + offsets['LOG_FLIGHT_MODE']];
				}
				if (flags.includes('LOG_ALTITUDE'))
					//12.4 fixed point
					frame.motion.altitude =
						leBytesToInt(
							data.slice(i + offsets['LOG_ALTITUDE'], i + offsets['LOG_ALTITUDE'] + 2),
							true
						) / 16;
				if (flags.includes('LOG_VVEL')) {
					//10.6 fixed point
					frame.motion.vvel =
						leBytesToInt(data.slice(i + offsets['LOG_VVEL'], i + offsets['LOG_VVEL'] + 2), true) /
						256;
				}
				if (flags.includes('LOG_GPS')) {
					frame.motion.gps = log[log.length - 1]?.motion.gps || {};
					if (
						leBytesToInt(data.slice(i + offsets['LOG_GPS'], i + offsets['LOG_GPS'] + 2)) === 20551 && // 'G' * 256 + 'P'
						leBytesToInt(
							data.slice(i + offsets['LOG_GPS'] + frameSize, i + offsets['LOG_GPS'] + 2 + frameSize)
						) === 20563 && // 'G' * 256 + 'P'
						leBytesToInt(
							data.slice(
								i + offsets['LOG_GPS'] + frameSize * 2,
								i + offsets['LOG_GPS'] + 2 + frameSize * 2
							)
						) === 21590 // 'V' * 256 + 'T'
					) {
						const gpsData: number[] = [];
						//copy 92 bytes of GPS data from the next 46 frames (after the 3 frames for GPSPVT)
						for (let j = 0; j < 46; j++) {
							const gpsBytes = data.slice(
								i + offsets['LOG_GPS'] + frameSize * 3 + j * frameSize,
								i + offsets['LOG_GPS'] + frameSize * 3 + j * frameSize + 2
							);
							gpsData.push(...gpsBytes);
						}
						frame.motion.gps = {
							year: leBytesToInt(gpsData.slice(4, 6)),
							month: gpsData[6],
							day: gpsData[7],
							hour: gpsData[8],
							minute: gpsData[9],
							second: gpsData[10],
							time_validity_flags: gpsData[11],
							t_acc: leBytesToInt(gpsData.slice(12, 16)),
							ns: leBytesToInt(gpsData.slice(16, 20), true),
							fix_type: gpsData[20],
							flags: gpsData[21],
							flags2: gpsData[22],
							sat_count: gpsData[23],
							lon: leBytesToInt(gpsData.slice(24, 28), true) / 10000000,
							lat: leBytesToInt(gpsData.slice(28, 32), true) / 10000000,
							alt: leBytesToInt(gpsData.slice(36, 40), true) / 1000,
							h_acc: leBytesToInt(gpsData.slice(40, 44)) / 1000,
							v_acc: leBytesToInt(gpsData.slice(44, 48)) / 1000,
							vel_n: leBytesToInt(gpsData.slice(48, 52), true) / 1000,
							vel_e: leBytesToInt(gpsData.slice(52, 56), true) / 1000,
							vel_d: leBytesToInt(gpsData.slice(56, 60), true) / 1000,
							g_speed: leBytesToInt(gpsData.slice(60, 64), true) / 1000,
							head_mot: leBytesToInt(gpsData.slice(64, 68), true) / 100000,
							s_acc: leBytesToInt(gpsData.slice(68, 72)) / 1000,
							head_acc: leBytesToInt(gpsData.slice(72, 76)) / 100000,
							p_dop: leBytesToInt(gpsData.slice(76, 78)) / 100,
							flags3: leBytesToInt(gpsData.slice(78, 80))
						};
					}
				}
				if (flags.includes('LOG_ATT_ROLL')) {
					// roll = 0.0001 * raw (signed 16 bit int)
					frame.attitude.roll =
						((leBytesToInt(
							data.slice(i + offsets['LOG_ATT_ROLL'], i + offsets['LOG_ATT_ROLL'] + 2),
							true
						) /
							10000) *
							180) /
						Math.PI;
				}
				if (flags.includes('LOG_ATT_PITCH')) {
					// pitch = 0.0001 * raw (signed 16 bit int)
					frame.attitude.pitch =
						((leBytesToInt(
							data.slice(i + offsets['LOG_ATT_PITCH'], i + offsets['LOG_ATT_PITCH'] + 2),
							true
						) /
							10000) *
							180) /
						Math.PI;
				}
				if (flags.includes('LOG_ATT_YAW')) {
					// yaw = 0.0001 * raw (signed 16 bit int)
					frame.attitude.yaw =
						((leBytesToInt(
							data.slice(i + offsets['LOG_ATT_YAW'], i + offsets['LOG_ATT_YAW'] + 2),
							true
						) /
							10000) *
							180) /
						Math.PI;
				}
				if (flags.includes('LOG_MOTOR_RPM')) {
					const rpmBytes = data.slice(i + offsets['LOG_MOTOR_RPM'], i + offsets['LOG_MOTOR_RPM'] + 6);
					const motors01 = leBytesToInt(rpmBytes.slice(0, 3));
					const motors23 = leBytesToInt(rpmBytes.slice(3, 6));
					let rr = motors01 & 0xfff;
					let fr = motors01 >> 12;
					let rl = motors23 & 0xfff;
					let fl = motors23 >> 12;
					if (rr === 0xfff) {
						frame.motors.rpm.rr = 0;
					} else {
						rr = (rr & 0x1ff) << (rr >> 9);
						frame.motors.rpm.rr = (60000000 + 50 * rr) / rr / (motorPoles / 2);
					}
					if (fr === 0xfff) {
						frame.motors.rpm.fr = 0;
					} else {
						fr = (fr & 0x1ff) << (fr >> 9);
						frame.motors.rpm.fr = (60000000 + 50 * fr) / fr / (motorPoles / 2);
					}
					if (rl === 0xfff) {
						frame.motors.rpm.rl = 0;
					} else {
						rl = (rl & 0x1ff) << (rl >> 9);
						frame.motors.rpm.rl = (60000000 + 50 * rl) / rl / (motorPoles / 2);
					}
					if (fl === 0xfff) {
						frame.motors.rpm.fl = 0;
					} else {
						fl = (fl & 0x1ff) << (fl >> 9);
						frame.motors.rpm.fl = (60000000 + 50 * fl) / fl / (motorPoles / 2);
					}
				}
				if (flags.includes('LOG_ACCEL_RAW')) {
					const accelBytes = data.slice(
						i + offsets['LOG_ACCEL_RAW'],
						i + offsets['LOG_ACCEL_RAW'] + 6
					);
					frame.motion.accelRaw.x = (leBytesToInt(accelBytes.slice(0, 2), true) * 9.81) / 2048;
					frame.motion.accelRaw.y = (leBytesToInt(accelBytes.slice(2, 4), true) * 9.81) / 2048;
					frame.motion.accelRaw.z = (leBytesToInt(accelBytes.slice(4, 6), true) * 9.81) / 2048;
				}
				if (flags.includes('LOG_ACCEL_FILTERED')) {
					const accelBytes = data.slice(
						i + offsets['LOG_ACCEL_FILTERED'],
						i + offsets['LOG_ACCEL_FILTERED'] + 6
					);
					frame.motion.accelFiltered.x = (leBytesToInt(accelBytes.slice(0, 2), true) * 9.81) / 2048;
					frame.motion.accelFiltered.y = (leBytesToInt(accelBytes.slice(2, 4), true) * 9.81) / 2048;
					frame.motion.accelFiltered.z = (leBytesToInt(accelBytes.slice(4, 6), true) * 9.81) / 2048;
				}
				if (flags.includes('LOG_VERTICAL_ACCEL')) {
					frame.motion.accelVertical =
						leBytesToInt(
							data.slice(i + offsets['LOG_VERTICAL_ACCEL'], i + offsets['LOG_VERTICAL_ACCEL'] + 2),
							true
						) / 128;
				}
				if (flags.includes('LOG_VVEL_SETPOINT')) {
					frame.setpoint.vvel =
						leBytesToInt(
							data.slice(i + offsets['LOG_VVEL_SETPOINT'], i + offsets['LOG_VVEL_SETPOINT'] + 2),
							true
						) / 4096;
				}
				if (flags.includes('LOG_MAG_HEADING')) {
					frame.motion.magHeading =
						((leBytesToInt(
							data.slice(i + offsets['LOG_MAG_HEADING'], i + offsets['LOG_MAG_HEADING'] + 2),
							true
						) /
							8192) *
							180) /
						Math.PI;
				}
				if (flags.includes('LOG_COMBINED_HEADING')) {
					frame.motion.combinedHeading =
						((leBytesToInt(
							data.slice(
								i + offsets['LOG_COMBINED_HEADING'],
								i + offsets['LOG_COMBINED_HEADING'] + 2
							),
							true
						) /
							8192) *
							180) /
						Math.PI;
				}
				log.push(frame);
			}
			this.loadedLog = {
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
				rawFile: this.binFile,
				isExact: true,
				pidConstantsNice,
				motorPoles
			};
			this.fillLogWithGenFlags(this.loadedLog);
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
		processLogInfo(data: number[]) {
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
				const sTime = leBytesToInt(data.slice(i + 8, i + 12));
				const year = sTime >> 26;
				const month = (sTime >> 22) & 0b1111;
				const day = (sTime >> 17) & 0b11111;
				const hour = (sTime >> 12) & 0b11111;
				const minute = (sTime >> 6) & 0b111111;
				const second = sTime & 0b111111;
				const startTime = new Date(year + 2000, month - 1, day, hour, minute, second);
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
					if (j == 26 || [35, 36, 37].includes(j)) frameSize += 6;
					else if (j == 28) frameSize++;
					else frameSize += 2;
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
		fillLogWithGenFlags(log: BBLog) {
			log.isExact = true;
			const genFlags = Object.keys(BB_GEN_FLAGS);
			for (let i = 0; i < genFlags.length; i++) {
				const flagName = genFlags[i];
				const flag = BB_GEN_FLAGS[flagName];
				if (log.flags.includes(flag.replaces)) continue;
				if (
					flag.requires.every(r => {
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
					switch (flagName) {
						case 'GEN_ROLL_SETPOINT':
							log.frames.forEach(f => {
								const polynomials: number[] = [(f.elrs.roll! - 1500) / 512];
								for (let i = 1; i < 5; i++) {
									polynomials[i] = polynomials[0] * polynomials[i - 1];
									if (polynomials[0] < 0) polynomials[i] = -polynomials[i];
								}
								f.setpoint.roll = 0;
								for (let i = 0; i < 5; i++) f.setpoint.roll += polynomials[i] * log.rateFactors[i][0];
							});
							break;
						case 'GEN_PITCH_SETPOINT':
							log.frames.forEach(f => {
								const polynomials: number[] = [(f.elrs.pitch! - 1500) / 512];
								for (let i = 1; i < 5; i++) {
									polynomials[i] = polynomials[0] * polynomials[i - 1];
									if (polynomials[0] < 0) polynomials[i] = -polynomials[i];
								}
								f.setpoint.pitch = 0;
								for (let i = 0; i < 5; i++)
									f.setpoint.pitch += polynomials[i] * log.rateFactors[i][1];
							});
							break;
						case 'GEN_THROTTLE_SETPOINT':
							log.frames.forEach(f => {
								f.setpoint.throttle = f.elrs.throttle;
							});
							break;
						case 'GEN_YAW_SETPOINT':
							log.frames.forEach(f => {
								const polynomials: number[] = [(f.elrs.yaw! - 1500) / 512];
								for (let i = 1; i < 5; i++) {
									polynomials[i] = polynomials[0] * polynomials[i - 1];
									if (polynomials[0] < 0) polynomials[i] = -polynomials[i];
								}
								f.setpoint.yaw = 0;
								for (let i = 0; i < 5; i++) f.setpoint.yaw += polynomials[i] * log.rateFactors[i][2];
							});
							break;
						case 'GEN_ROLL_PID_P':
							log.frames.forEach(f => {
								f.pid.roll.p = (f.setpoint.roll! - f.gyro.roll!) * log.pidConstants[0][0];
							});
							break;
						case 'GEN_PITCH_PID_P':
							log.frames.forEach(f => {
								f.pid.pitch.p = (f.setpoint.pitch! - f.gyro.pitch!) * log.pidConstants[1][0];
							});
							break;
						case 'GEN_YAW_PID_P':
							log.frames.forEach(f => {
								f.pid.yaw.p = (f.setpoint.yaw! - f.gyro.yaw!) * log.pidConstants[2][0];
							});
							break;
						case 'GEN_ROLL_PID_D':
							log.frames.forEach((f, i) => {
								f.pid.roll.d =
									(((log.frames[i - 1]?.gyro.roll || 0) - f.gyro.roll!) * log.pidConstants[0][2]) /
									log.frequencyDivider;
							});
							break;
						case 'GEN_PITCH_PID_D':
							log.frames.forEach((f, i) => {
								f.pid.pitch.d =
									(((log.frames[i - 1]?.gyro.pitch || 0) - f.gyro.pitch!) * log.pidConstants[1][2]) /
									log.frequencyDivider;
							});
							break;
						case 'GEN_YAW_PID_D':
							log.frames.forEach((f, i) => {
								f.pid.yaw.d =
									(((log.frames[i - 1]?.gyro.yaw || 0) - f.gyro.yaw!) * log.pidConstants[2][2]) /
									log.frequencyDivider;
							});
							break;
						case 'GEN_ROLL_PID_FF':
							{
								const step = Math.round(8 / log.frequencyDivider) || 1;
								const divider = (step * log.frequencyDivider) / 8;
								log.frames.forEach((f, i) => {
									f.pid.roll.ff =
										((f.setpoint.roll! - log.frames[i - step]?.setpoint.roll! || 0) *
											log.pidConstants[0][3]) /
										divider;
								});
							}
							break;
						case 'GEN_PITCH_PID_FF':
							{
								const step = Math.round(8 / log.frequencyDivider) || 1;
								const divider = step * log.frequencyDivider;
								log.frames.forEach((f, i) => {
									f.pid.pitch.ff =
										((f.setpoint.pitch! - log.frames[i - step]?.setpoint.pitch! || 0) *
											log.pidConstants[1][3]) /
										divider;
								});
							}
							break;
						case 'GEN_YAW_PID_FF':
							{
								const step = Math.round(8 / log.frequencyDivider) || 1;
								const divider = step * log.frequencyDivider;
								log.frames.forEach((f, i) => {
									f.pid.yaw.ff =
										((f.setpoint.yaw! - log.frames[i - step]?.setpoint.yaw! || 0) *
											log.pidConstants[2][3]) /
										divider;
								});
							}
							break;
						case 'GEN_ROLL_PID_S':
							log.frames.forEach(f => {
								f.pid.roll.s = f.setpoint.roll! * log.pidConstants[0][4];
							});
							break;
						case 'GEN_PITCH_PID_S':
							log.frames.forEach(f => {
								f.pid.pitch.s = f.setpoint.pitch! * log.pidConstants[1][4];
							});
							break;
						case 'GEN_YAW_PID_S':
							log.frames.forEach(f => {
								f.pid.yaw.s = f.setpoint.yaw! * log.pidConstants[2][4];
							});
							break;
						case 'GEN_ROLL_PID_I':
							{
								let rollI = 0;
								let takeoffCounter = 0;
								log.frames.forEach(f => {
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
								log.frames.forEach(f => {
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
								log.frames.forEach(f => {
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
							log.frames.forEach(f => {
								const rollTerm =
									f.pid.roll.p! + f.pid.roll.i! + f.pid.roll.d! + f.pid.roll.ff! + f.pid.roll.s!;
								const pitchTerm =
									f.pid.pitch.p! + f.pid.pitch.i! + f.pid.pitch.d! + f.pid.pitch.ff! + f.pid.pitch.s!;
								const yawTerm =
									f.pid.yaw.p! + f.pid.yaw.i! + f.pid.yaw.d! + f.pid.yaw.ff! + f.pid.yaw.s!;
								f.motors.out = {
									rr: (f.setpoint.throttle! - 1000) * 2 - rollTerm + pitchTerm + yawTerm,
									fr: (f.setpoint.throttle! - 1000) * 2 - rollTerm - pitchTerm - yawTerm,
									rl: (f.setpoint.throttle! - 1000) * 2 + rollTerm + pitchTerm - yawTerm,
									fl: (f.setpoint.throttle! - 1000) * 2 + rollTerm - pitchTerm + yawTerm
								};
								f.motors.out.rr = map(f.motors.out.rr!, 0, 2000, 50, 2000);
								f.motors.out.fr = map(f.motors.out.fr!, 0, 2000, 50, 2000);
								f.motors.out.rl = map(f.motors.out.rl!, 0, 2000, 50, 2000);
								f.motors.out.fl = map(f.motors.out.fl!, 0, 2000, 50, 2000);
								if (f.motors.out.rr! > 2000) {
									const diff = 2000 - f.motors.out.rr!;
									f.motors.out.rr = 2000;
									f.motors.out.fr! -= diff;
									f.motors.out.rl! -= diff;
									f.motors.out.fl! -= diff;
								}
								if (f.motors.out.fr! > 2000) {
									const diff = 2000 - f.motors.out.fr!;
									f.motors.out.fr = 2000;
									f.motors.out.rr! -= diff;
									f.motors.out.rl! -= diff;
									f.motors.out.fl! -= diff;
								}
								if (f.motors.out.rl! > 2000) {
									const diff = 2000 - f.motors.out.rl!;
									f.motors.out.rl = 2000;
									f.motors.out.rr! -= diff;
									f.motors.out.fr! -= diff;
									f.motors.out.fl! -= diff;
								}
								if (f.motors.out.fl! > 2000) {
									const diff = 2000 - f.motors.out.fl!;
									f.motors.out.fl = 2000;
									f.motors.out.rr! -= diff;
									f.motors.out.fr! -= diff;
									f.motors.out.rl! -= diff;
								}
								if (f.motors.out.rr! < 50) {
									const diff = 50 - f.motors.out.rr!;
									f.motors.out.rr = 50;
									f.motors.out.fr! += diff;
									f.motors.out.rl! += diff;
									f.motors.out.fl! += diff;
								}
								if (f.motors.out.fr! < 50) {
									const diff = 50 - f.motors.out.fr!;
									f.motors.out.fr = 50;
									f.motors.out.rr! += diff;
									f.motors.out.rl! += diff;
									f.motors.out.fl! += diff;
								}
								if (f.motors.out.rl! < 50) {
									const diff = 50 - f.motors.out.rl!;
									f.motors.out.rl = 50;
									f.motors.out.rr! += diff;
									f.motors.out.fr! += diff;
									f.motors.out.fl! += diff;
								}
								if (f.motors.out.fl! < 50) {
									const diff = 50 - f.motors.out.fl!;
									f.motors.out.fl = 50;
									f.motors.out.rr! += diff;
									f.motors.out.fr! += diff;
									f.motors.out.rl! += diff;
								}
								f.motors.out.rr = Math.min(f.motors.out.rr!, 2000);
								f.motors.out.fr = Math.min(f.motors.out.fr!, 2000);
								f.motors.out.rl = Math.min(f.motors.out.rl!, 2000);
								f.motors.out.fl = Math.min(f.motors.out.fl!, 2000);
							});
							break;
						case 'GEN_VVEL_SETPOINT':
							log.frames.forEach(f => {
								let t = (f.elrs.throttle! - 1500) * 2;
								if (t > 0) {
									t -= 100;
									if (t < 0) t = 0;
								} else if (t < 0) {
									t += 100;
									if (t > 0) t = 0;
								}
								f.setpoint.vvel = t / 180;
								if (f.flightMode !== undefined && f.flightMode < 2) f.setpoint.vvel = 0;
							});
							break;
					}
				}
			}
		}
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
			<button @click="() => { downloadLog() }" :disabled="selected === -1">Download KBB</button>
			<button @click="() => { downloadLog('json') }" :disabled="selected === -1">Download JSON</button>
			<button @click="deleteLog" :disabled="selected === -1">Delete</button>
			<button @click="formatBB">Format</button>
			<button @click="openLogFromFile">Open from file</button>
			<button @click="() => { showSettings = true }">Settings</button>
		</div>
		<Settings v-if="showSettings" :f="BB_ALL_FLAGS" @close="() => { showSettings = false; }" />
		<div class="dataViewerWrapper" ref="dataViewerWrapper">
			<canvas id="bbDataViewer" ref="bbDataViewer" @mousedown="onMouseDown" @mouseup="onMouseUp"
				@mousemove="onMouseMove" @mouseleave="onMouseLeave" @wheel="onMouseWheel" @dblclick="() => {
					startFrame = 0;
					endFrame = (loadedLog?.frameCount || 1) - 1;
				}">
			</canvas>
		</div>
		<div class="flagSelector">
			<div v-for="(graph, graphIndex) in graphs" class="graphSelector">
				<TracePlacer v-for="(trace, traceIndex) in graph" :key="trace.id" :ll="loadedLog!" :fp="BB_ALL_FLAGS"
					:gfp="BB_GEN_FLAGS" :t="trace" v-model="graphs[graphIndex][traceIndex]" @update="(t: TraceInGraph) => {
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
				<div style="white-space: pre">
					Flags: {{'\n - ' +
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
			<Timeline :ll="loadedLog" :fp="BB_ALL_FLAGS" :gfp="BB_GEN_FLAGS" :startFrame="startFrame"
				:endFrame="endFrame"
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
