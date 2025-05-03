import type { BBLog, GenFlagProps, FlagProps } from "./types"

const getGyroBBRange = (file: BBLog | undefined) => {
	if (!file) return { max: -2000, min: 2000 }
	let maxSetpoints = [0, 0, 0]
	for (let exp = 0; exp < 5; exp++) for (let ax = 0; ax < 3; ax++) maxSetpoints[ax] += file.rateFactors[exp][ax]
	//Math.max leads to a stack overflow, thus using a for loop now
	let max = 0,
		min = 0
	for (let i = 0; i < maxSetpoints.length; i++) {
		if (maxSetpoints[i] > max) max = maxSetpoints[i]
		if (maxSetpoints[i] < min) min = maxSetpoints[i]
	}
	file.frames.forEach(f => {
		if (f.gyro.roll! > max) max = f.gyro.roll!
		if (f.gyro.pitch! > max) max = f.gyro.pitch!
		if (f.gyro.yaw! > max) max = f.gyro.yaw!
		if (f.gyro.roll! < min) min = f.gyro.roll!
		if (f.gyro.pitch! < min) min = f.gyro.pitch!
		if (f.gyro.yaw! < min) min = f.gyro.yaw!
	})
	const fullRange = Math.round(Math.max(max, -min))
	return { max: fullRange, min: -fullRange }
}
const getAltitudeRange = (file: BBLog | undefined) => {
	if (!file) return { max: 300, min: 0 }
	let max = -20000,
		min = 20000
	file.frames.forEach(f => {
		if (f.motion.altitude! > max) max = f.motion.altitude!
		if (f.motion.altitude! < min) min = f.motion.altitude!
	})
	max = 10 * Math.ceil(max / 10)
	min = 10 * Math.floor(min / 10)
	return { max, min }
}
const getLatLonRange = (file: BBLog) => {
	const latRange = { min: 90, max: -90 }
	const lonRange = { min: 180, max: -180 }
	file.frames.forEach(f => {
		if (f.motion.gps.lat! > latRange.max) latRange.max = f.motion.gps.lat!
		if (f.motion.gps.lat! < latRange.min) latRange.min = f.motion.gps.lat!
		if (f.motion.gps.lon! > lonRange.max) lonRange.max = f.motion.gps.lon!
		if (f.motion.gps.lon! < lonRange.min) lonRange.min = f.motion.gps.lon!
	})
	const latRangeDiff = Math.abs(latRange.max - latRange.min)
	let lonRangeDiff = Math.abs(lonRange.max - lonRange.min)
	const latAvg = (latRange.max + latRange.min) / 2
	const lonAvg = (lonRange.max + lonRange.min) / 2
	const lonScale = Math.cos((latAvg * Math.PI) / 180) || 1
	lonRangeDiff *= lonScale // effective diff, where 1deg = 111km
	const maxDiff = Math.max(latRangeDiff, lonRangeDiff)
	latRange.min = latAvg - maxDiff / 2
	latRange.max = latAvg + maxDiff / 2
	lonRange.min = lonAvg - maxDiff / lonScale / 2
	lonRange.max = lonAvg + maxDiff / lonScale / 2
	return { latRange, lonRange }
}
const getLongitudeRange = (file: BBLog | undefined) => {
	if (!file) return { max: 180, min: -180 }
	return getLatLonRange(file).lonRange
}
const getLatitudeRange = (file: BBLog | undefined) => {
	if (!file) return { max: 90, min: -90 }
	return getLatLonRange(file).latRange
}

export const BB_ALL_FLAGS: { [key: string]: FlagProps } = {
	LOG_ROLL_ELRS_RAW: {
		name: "Roll ELRS Raw",
		path: "elrs.roll",
		minValue: 988,
		maxValue: 2012,
		unit: "µs",
	},
	LOG_PITCH_ELRS_RAW: {
		name: "Pitch ELRS Raw",
		path: "elrs.pitch",
		minValue: 988,
		maxValue: 2012,
		unit: "µs",
	},
	LOG_THROTTLE_ELRS_RAW: {
		name: "Throttle ELRS Raw",
		path: "elrs.throttle",
		minValue: 1000,
		maxValue: 2000,
		unit: "µs",
	},
	LOG_YAW_ELRS_RAW: {
		name: "Yaw ELRS Raw",
		path: "elrs.yaw",
		minValue: 988,
		maxValue: 2012,
		unit: "µs",
	},
	LOG_ROLL_SETPOINT: {
		name: "Roll Setpoint",
		path: "setpoint.roll",
		rangeFn: getGyroBBRange,
		unit: "°/sec",
	},
	LOG_PITCH_SETPOINT: {
		name: "Pitch Setpoint",
		path: "setpoint.pitch",
		rangeFn: getGyroBBRange,
		unit: "°/sec",
	},
	LOG_THROTTLE_SETPOINT: {
		name: "Throttle Setpoint",
		path: "setpoint.throttle",
		minValue: 1000,
		maxValue: 2000,
		unit: "µs",
	},
	LOG_YAW_SETPOINT: {
		name: "Yaw Setpoint",
		path: "setpoint.yaw",
		rangeFn: getGyroBBRange,
		unit: "°/sec",
	},
	LOG_ROLL_GYRO_RAW: {
		name: "Roll Gyro Raw",
		path: "gyro.roll",
		rangeFn: getGyroBBRange,
		unit: "°/sec",
	},
	LOG_PITCH_GYRO_RAW: {
		name: "Pitch Gyro Raw",
		path: "gyro.pitch",
		rangeFn: getGyroBBRange,
		unit: "°/sec",
	},
	LOG_YAW_GYRO_RAW: {
		name: "Yaw Gyro Raw",
		path: "gyro.yaw",
		rangeFn: getGyroBBRange,
		unit: "°/sec",
	},
	LOG_ROLL_PID_P: {
		name: "Roll PID P",
		path: "pid.roll.p",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_ROLL_PID_I: {
		name: "Roll PID I",
		path: "pid.roll.i",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_ROLL_PID_D: {
		name: "Roll PID D",
		path: "pid.roll.d",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_ROLL_PID_FF: {
		name: "Roll PID FF",
		path: "pid.roll.ff",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_ROLL_PID_S: {
		name: "Roll PID S",
		path: "pid.roll.s",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_PITCH_PID_P: {
		name: "Pitch PID P",
		path: "pid.pitch.p",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_PITCH_PID_I: {
		name: "Pitch PID I",
		path: "pid.pitch.i",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_PITCH_PID_D: {
		name: "Pitch PID D",
		path: "pid.pitch.d",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_PITCH_PID_FF: {
		name: "Pitch PID FF",
		path: "pid.pitch.ff",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_PITCH_PID_S: {
		name: "Pitch PID S",
		path: "pid.pitch.s",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_YAW_PID_P: {
		name: "Yaw PID P",
		path: "pid.yaw.p",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_YAW_PID_I: {
		name: "Yaw PID I",
		path: "pid.yaw.i",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_YAW_PID_D: {
		name: "Yaw PID D",
		path: "pid.yaw.d",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_YAW_PID_FF: {
		name: "Yaw PID FF",
		path: "pid.yaw.ff",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_YAW_PID_S: {
		name: "Yaw PID S",
		path: "pid.yaw.s",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_MOTOR_OUTPUTS: {
		name: "Motor Outputs",
		path: "motors.out",
		minValue: 0,
		maxValue: 2000,
		unit: "",
		modifier: [
			{ displayNameShort: "RR", displayName: "Rear Right", path: "rr" },
			{ displayNameShort: "FR", displayName: "Front Right", path: "fr" },
			{ displayNameShort: "FL", displayName: "Front Left", path: "fl" },
			{ displayNameShort: "RL", displayName: "Rear Left", path: "rl" },
		],
	},
	LOG_FRAMETIME: {
		name: "Frametime",
		path: "frametime",
		minValue: 0,
		maxValue: 1000,
		unit: "µs",
	},
	LOG_FLIGHT_MODE: {
		name: "Flight Mode",
		path: "flightMode",
		minValue: 0,
		maxValue: 4,
		states: ["Acro", "Angle", "Altitude Hold", "GPS Velocity", "GPS Position"],
		unit: "",
	},
	LOG_ALTITUDE: {
		name: "Altitude",
		path: "motion.altitude",
		rangeFn: getAltitudeRange,
		decimals: 2,
		unit: "m",
	},
	LOG_VVEL: {
		name: "Vertical Velocity",
		path: "motion.vvel",
		minValue: -10,
		maxValue: 10,
		decimals: 2,
		unit: "m/s",
	},
	LOG_GPS: {
		name: "GPS",
		path: "motion.gps",
		minValue: 0,
		maxValue: 100,
		unit: "",
		modifier: [
			{ displayNameShort: "Year", displayName: "Year", min: 2020, max: 2030, path: "year" },
			{
				displayNameShort: "Month",
				displayName: "Month",
				min: 1,
				max: 12,
				path: "month",
				states: [
					"",
					"January",
					"February",
					"March",
					"April",
					"May",
					"June",
					"July",
					"August",
					"September",
					"October",
					"November",
					"December",
				],
			},
			{ displayNameShort: "Day", displayName: "Day", min: 1, max: 31, path: "day" },
			{ displayNameShort: "Hour", displayName: "Hour", min: 0, max: 23, path: "hour" },
			{ displayNameShort: "Minute", displayName: "Minute", min: 0, max: 59, path: "minute" },
			{ displayNameShort: "Second", displayName: "Second", min: 0, max: 59, path: "second" },
			{
				displayNameShort: "Valid",
				displayName: "Validity Flags",
				min: 0,
				max: 255,
				path: "time_validity_flags",
			},
			{
				displayNameShort: "T Acc",
				displayName: "Time Accuracy",
				min: 0,
				max: 100,
				path: "t_acc",
				unit: "ns",
			},
			{
				displayNameShort: "Nanosec",
				displayName: "Nanoseconds",
				min: 0,
				max: 1e9,
				path: "ns",
				unit: "ns",
			},
			{
				displayNameShort: "Fix",
				displayName: "Fix Type",
				min: 0,
				max: 5,
				path: "fix_type",
				states: ["No Fix", "Dead reckoning only", "2D Fix", "3D Fix", "GPS + Dead reckoning", "Time only fix"],
			},
			{ displayNameShort: "Flags", displayName: "Flags", min: 0, max: 255, path: "flags" },
			{ displayNameShort: "Flags2", displayName: "Flags2", min: 0, max: 255, path: "flags2" },
			{
				displayNameShort: "Sats",
				displayName: "Satellite Count",
				min: 0,
				max: 30,
				path: "sat_count",
			},
			{
				displayNameShort: "Lon",
				displayName: "Longitude",
				rangeFn: getLongitudeRange,
				path: "lon",
				unit: "°",
				decimals: 7,
			},
			{
				displayNameShort: "Lat",
				displayName: "Latitude",
				rangeFn: getLatitudeRange,
				path: "lat",
				unit: "°",
				decimals: 7,
			},
			{
				displayNameShort: "Alt",
				displayName: "Altitude",
				rangeFn: getAltitudeRange,
				path: "alt",
				unit: "m",
				decimals: 2,
			},
			{
				displayNameShort: "Hor Acc",
				displayName: "Horizontal Accuracy",
				min: 0,
				max: 20,
				path: "h_acc",
				unit: "m",
				decimals: 2,
			},
			{
				displayNameShort: "Ver Acc",
				displayName: "Vertical Accuracy",
				min: 0,
				max: 20,
				path: "v_acc",
				unit: "m",
				decimals: 2,
			},
			{
				displayNameShort: "Vel N",
				displayName: "Velocity North",
				min: -15,
				max: 15,
				path: "vel_n",
				unit: "m/s",
				decimals: 2,
			},
			{
				displayNameShort: "Vel E",
				displayName: "Velocity East",
				min: -15,
				max: 15,
				path: "vel_e",
				unit: "m/s",
				decimals: 2,
			},
			{
				displayNameShort: "Vel D",
				displayName: "Velocity Down",
				min: 10,
				max: -10, //down is positive, invert by default
				path: "vel_d",
				unit: "m/s",
				decimals: 2,
			},
			{
				displayNameShort: "G Speed",
				displayName: "Ground Speed",
				min: 0,
				max: 50,
				path: "g_speed",
				unit: "m/s",
				decimals: 2,
			},
			{
				displayNameShort: "Head Mot",
				displayName: "Heading of Motion",
				min: -180,
				max: 180,
				path: "head_mot",
				unit: "°",
			},
			{
				displayNameShort: "S Acc",
				displayName: "Speed Accuracy",
				min: 0,
				max: 10,
				path: "s_acc",
				unit: "m/s",
				decimals: 2,
			},
			{
				displayNameShort: "Head Acc",
				displayName: "Heading Accuracy",
				min: 0,
				max: 20,
				path: "head_acc",
				unit: "°",
				decimals: 2,
			},
			{
				displayNameShort: "pDop",
				displayName: "pDop",
				min: 0,
				max: 100,
				path: "p_dop",
				decimals: 2,
			},
			{ displayNameShort: "Flags3", displayName: "Flags3", min: 0, max: 31, path: "flags3" },
		],
	},
	LOG_ATT_ROLL: {
		name: "Roll Angle",
		path: "attitude.roll",
		minValue: -180,
		maxValue: 180,
		unit: "°",
	},
	LOG_ATT_PITCH: {
		name: "Pitch Angle",
		path: "attitude.pitch",
		minValue: -180,
		maxValue: 180,
		unit: "°",
	},
	LOG_ATT_YAW: {
		name: "Yaw Angle",
		path: "attitude.yaw",
		minValue: -180,
		maxValue: 180,
		unit: "°",
	},
	LOG_MOTOR_RPM: {
		name: "Motor RPM",
		path: "motors.rpm",
		minValue: 0,
		maxValue: 50000,
		unit: "rpm",
		modifier: [
			{ displayNameShort: "RR", displayName: "Rear Right", path: "rr" },
			{ displayNameShort: "FR", displayName: "Front Right", path: "fr" },
			{ displayNameShort: "FL", displayName: "Front Left", path: "fl" },
			{ displayNameShort: "RL", displayName: "Rear Left", path: "rl" },
		],
	},
	LOG_ACCEL_RAW: {
		name: "Accel Raw",
		path: "motion.accelRaw",
		minValue: -40,
		maxValue: 40,
		unit: "m/s²",
		decimals: 3,
		modifier: [
			{ displayNameShort: "X", displayName: "X", path: "x" },
			{ displayNameShort: "Y", displayName: "Y", path: "y" },
			{ displayNameShort: "Z", displayName: "Z", path: "z" },
		],
	},
	LOG_ACCEL_FILTERED: {
		name: "Accel Filtered",
		path: "motion.accelFiltered",
		minValue: -40,
		maxValue: 40,
		unit: "m/s²",
		decimals: 3,
		modifier: [
			{ displayNameShort: "X", displayName: "X", path: "x" },
			{ displayNameShort: "Y", displayName: "Y", path: "y" },
			{ displayNameShort: "Z", displayName: "Z", path: "z" },
		],
	},
	LOG_VERTICAL_ACCEL: {
		name: "Vertical Accel",
		path: "motion.accelVertical",
		minValue: -40,
		maxValue: 40,
		unit: "m/s²",
		decimals: 3,
	},
	LOG_VVEL_SETPOINT: {
		name: "vVel Setpoint",
		path: "setpoint.vvel",
		minValue: -10,
		maxValue: 10,
		decimals: 2,
		unit: "m/s",
	},
	LOG_MAG_HEADING: {
		name: "Mag Heading",
		path: "motion.magHeading",
		minValue: -180,
		maxValue: 180,
		unit: "°",
	},
	LOG_COMBINED_HEADING: {
		name: "Combined Heading",
		path: "motion.combinedHeading",
		minValue: -180,
		maxValue: 180,
		unit: "°",
	},
	LOG_HVEL: {
		name: "Hor. Velocity",
		path: "motion.hvel",
		minValue: -15,
		maxValue: 15,
		unit: "m/s",
		decimals: 2,
		modifier: [
			{
				displayNameShort: "North",
				displayName: "North",
				path: "n",
			},
			{
				displayNameShort: "East",
				displayName: "East",
				path: "e",
			},
		],
	},
	LOG_BARO: {
		name: "Barometer",
		path: "motion.baro",
		unit: "",
		modifier: [
			{
				displayNameShort: "Raw",
				displayName: "Pressure Raw",
				path: "raw",
				max: 4150000,
				min: 4025000,
			},
			{
				displayNameShort: "hPa",
				displayName: "Pressure hPa",
				path: "hpa",
				max: 1013.25,
				min: 983,
			},
			{
				displayNameShort: "Alt",
				displayName: "Baro Altitude",
				path: "alt",
				min: 0,
				max: 255,
			},
		],
	},
	LOG_DEBUG_1: {
		name: "Debug 1",
		path: "debug1",
		unit: "",
		minValue: 0,
		maxValue: 100,
	},
	LOG_DEBUG_2: {
		name: "Debug 2",
		path: "debug2",
		unit: "",
		minValue: 0,
		maxValue: 100,
	},
	LOG_DEBUG_3: {
		name: "Debug 3",
		path: "debug3",
		unit: "",
		minValue: 0,
		maxValue: 100,
	},
	LOG_DEBUG_4: {
		name: "Debug 4",
		path: "debug4",
		unit: "",
		minValue: 0,
		maxValue: 100,
	},
}

export const BB_GEN_FLAGS: { [key: string]: GenFlagProps } = {
	GEN_ROLL_SETPOINT: {
		replaces: "LOG_ROLL_SETPOINT",
		requires: ["LOG_ROLL_ELRS_RAW"],
		exact: false,
	},
	GEN_PITCH_SETPOINT: {
		replaces: "LOG_PITCH_SETPOINT",
		requires: ["LOG_PITCH_ELRS_RAW"],
		exact: false,
	},
	GEN_THROTTLE_SETPOINT: {
		replaces: "LOG_THROTTLE_SETPOINT",
		requires: ["LOG_THROTTLE_ELRS_RAW"],
		exact: false,
	},
	GEN_YAW_SETPOINT: {
		replaces: "LOG_YAW_SETPOINT",
		requires: ["LOG_YAW_ELRS_RAW"],
		exact: false,
	},
	GEN_ROLL_PID_P: {
		replaces: "LOG_ROLL_PID_P",
		requires: [["LOG_ROLL_SETPOINT", "GEN_ROLL_SETPOINT"], "LOG_ROLL_GYRO_RAW"],
		exact: true,
	},
	GEN_ROLL_PID_I: {
		replaces: "LOG_ROLL_PID_I",
		requires: [["LOG_ROLL_SETPOINT", "GEN_ROLL_SETPOINT"], "LOG_ROLL_GYRO_RAW"],
		exact: false,
	},
	GEN_ROLL_PID_D: {
		replaces: "LOG_ROLL_PID_D",
		requires: ["LOG_ROLL_GYRO_RAW"],
		exact: false,
	},
	GEN_ROLL_PID_FF: {
		replaces: "LOG_ROLL_PID_FF",
		requires: [["LOG_ROLL_SETPOINT", "GEN_ROLL_SETPOINT"]],
		exact: false,
	},
	GEN_ROLL_PID_S: {
		replaces: "LOG_ROLL_PID_S",
		requires: [["LOG_ROLL_SETPOINT", "GEN_ROLL_SETPOINT"]],
		exact: true,
	},
	GEN_PITCH_PID_P: {
		replaces: "LOG_PITCH_PID_P",
		requires: [["LOG_PITCH_SETPOINT", "GEN_PITCH_SETPOINT"], "LOG_PITCH_GYRO_RAW"],
		exact: true,
	},
	GEN_PITCH_PID_I: {
		replaces: "LOG_PITCH_PID_I",
		requires: [["LOG_PITCH_SETPOINT", "GEN_PITCH_SETPOINT"], "LOG_PITCH_GYRO_RAW"],
		exact: false,
	},
	GEN_PITCH_PID_D: {
		replaces: "LOG_PITCH_PID_D",
		requires: ["LOG_PITCH_GYRO_RAW"],
		exact: false,
	},
	GEN_PITCH_PID_FF: {
		replaces: "LOG_PITCH_PID_FF",
		requires: [["LOG_PITCH_SETPOINT", "GEN_PITCH_SETPOINT"]],
		exact: false,
	},
	GEN_PITCH_PID_S: {
		replaces: "LOG_PITCH_PID_S",
		requires: [["LOG_PITCH_SETPOINT", "GEN_PITCH_SETPOINT"]],
		exact: true,
	},
	GEN_YAW_PID_P: {
		replaces: "LOG_YAW_PID_P",
		requires: [["LOG_YAW_SETPOINT", "GEN_YAW_SETPOINT"], "LOG_YAW_GYRO_RAW"],
		exact: true,
	},
	GEN_YAW_PID_I: {
		replaces: "LOG_YAW_PID_I",
		requires: [["LOG_YAW_SETPOINT", "GEN_YAW_SETPOINT"], "LOG_YAW_GYRO_RAW"],
		exact: false,
	},
	GEN_YAW_PID_D: {
		replaces: "LOG_YAW_PID_D",
		requires: ["LOG_YAW_GYRO_RAW"],
		exact: false,
	},
	GEN_YAW_PID_FF: {
		replaces: "LOG_YAW_PID_FF",
		requires: [["LOG_YAW_SETPOINT", "GEN_YAW_SETPOINT"]],
		exact: false,
	},
	GEN_YAW_PID_S: {
		replaces: "LOG_YAW_PID_S",
		requires: [["LOG_YAW_SETPOINT", "GEN_YAW_SETPOINT"]],
		exact: true,
	},
	GEN_MOTOR_OUTPUTS: {
		replaces: "LOG_MOTOR_OUTPUTS",
		requires: [
			["GEN_THROTTLE_SETPOINT", "LOG_THROTTLE_SETPOINT"],
			["LOG_ROLL_PID_P", "GEN_ROLL_PID_P"],
			["LOG_ROLL_PID_I", "GEN_ROLL_PID_I"],
			["LOG_ROLL_PID_D", "GEN_ROLL_PID_D"],
			["LOG_ROLL_PID_FF", "GEN_ROLL_PID_FF"],
			["LOG_ROLL_PID_S", "GEN_ROLL_PID_S"],
			["LOG_PITCH_PID_P", "GEN_PITCH_PID_P"],
			["LOG_PITCH_PID_I", "GEN_PITCH_PID_I"],
			["LOG_PITCH_PID_D", "GEN_PITCH_PID_D"],
			["LOG_PITCH_PID_FF", "GEN_PITCH_PID_FF"],
			["LOG_PITCH_PID_S", "GEN_PITCH_PID_S"],
			["LOG_YAW_PID_P", "GEN_YAW_PID_P"],
			["LOG_YAW_PID_I", "GEN_YAW_PID_I"],
			["LOG_YAW_PID_D", "GEN_YAW_PID_D"],
			["LOG_YAW_PID_FF", "GEN_YAW_PID_FF"],
			["LOG_YAW_PID_S", "GEN_YAW_PID_S"],
		],
		exact: true,
	},
	GEN_VVEL_SETPOINT: {
		replaces: "LOG_VVEL_SETPOINT",
		requires: ["LOG_THROTTLE_ELRS_RAW"],
		exact: false,
	},
}
