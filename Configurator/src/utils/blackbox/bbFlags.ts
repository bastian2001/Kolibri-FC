import { BBLog, GenFlagProps, FlagProps } from "../types"

const getGyroBBRange = (file: BBLog | undefined) => {
	if (!file) return { max: -2000, min: 2000 }
	let maxSetpoints = [0, 0, 0]
	for (let ax = 0; ax < 3; ax++) maxSetpoints[ax] = file.rateCoeffs[ax].max
	const max = Math.max(...maxSetpoints) * 1.2
	return { max: max, min: -max }
}
const getAltitudeRange = (file: BBLog | undefined) => {
	if (!file || !file.logData.altitude) return { max: 300, min: 0 }
	let max = -20000,
		min = 20000
	file.logData.altitude.forEach(f => {
		if (f > max) max = f
		if (f < min) min = f
	})
	max = 10 * Math.ceil(max / 10)
	min = 10 * Math.floor(min / 10)
	return { max, min }
}
const getLatLonRange = (file: BBLog) => {
	const latRange = { min: 90, max: -90 }
	const lonRange = { min: 180, max: -180 }
	if (!file || !file.logData.gpsLat || !file.logData.gpsLon) return { latRange, lonRange }
	file.logData.gpsLat.forEach(f => {
		if (f > latRange.max) latRange.max = f
		if (f < latRange.min) latRange.min = f
	})
	file.logData.gpsLon.forEach(f => {
		if (f > lonRange.max) lonRange.max = f
		if (f < lonRange.min) lonRange.min = f
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
	LOG_ELRS_RAW: {
		name: "ELRS Raw",
		path: "",
		minValue: 988,
		maxValue: 2012,
		unit: "µs",
		modifier: [
			{
				displayNameShort: "Roll",
				displayName: "Roll",
				path: "elrsRoll",
			},
			{
				displayNameShort: "Pitch",
				displayName: "Pitch",
				path: "elrsPitch",
			},
			{
				displayNameShort: "Throttle",
				displayName: "Throttle",
				path: "elrsThrottle",
			},
			{
				displayNameShort: "Yaw",
				displayName: "Yaw",
				path: "elrsYaw",
			},
		],
	},
	LOG_ROLL_SETPOINT: {
		name: "Roll Setpoint",
		path: "setpointRoll",
		rangeFn: getGyroBBRange,
		unit: "°/sec",
	},
	LOG_PITCH_SETPOINT: {
		name: "Pitch Setpoint",
		path: "setpointPitch",
		rangeFn: getGyroBBRange,
		unit: "°/sec",
	},
	LOG_THROTTLE_SETPOINT: {
		name: "Throttle Setpoint",
		path: "setpointThrottle",
		minValue: 1000,
		maxValue: 2000,
		unit: "µs",
	},
	LOG_YAW_SETPOINT: {
		name: "Yaw Setpoint",
		path: "setpointYaw",
		rangeFn: getGyroBBRange,
		unit: "°/sec",
	},
	LOG_ROLL_GYRO_RAW: {
		name: "Roll Gyro Raw",
		path: "gyroRawRoll",
		rangeFn: getGyroBBRange,
		unit: "°/sec",
	},
	LOG_PITCH_GYRO_RAW: {
		name: "Pitch Gyro Raw",
		path: "gyroRawPitch",
		rangeFn: getGyroBBRange,
		unit: "°/sec",
	},
	LOG_YAW_GYRO_RAW: {
		name: "Yaw Gyro Raw",
		path: "gyroRawYaw",
		rangeFn: getGyroBBRange,
		unit: "°/sec",
	},
	LOG_ROLL_PID_P: {
		name: "Roll PID P",
		path: "pidRollP",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_ROLL_PID_I: {
		name: "Roll PID I",
		path: "pidRollI",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_ROLL_PID_D: {
		name: "Roll PID D",
		path: "pidRollD",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_ROLL_PID_FF: {
		name: "Roll PID FF",
		path: "pidRollFF",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_ROLL_PID_S: {
		name: "Roll PID S",
		path: "pidRollS",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_PITCH_PID_P: {
		name: "Pitch PID P",
		path: "pidPitchP",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_PITCH_PID_I: {
		name: "Pitch PID I",
		path: "pidPitchI",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_PITCH_PID_D: {
		name: "Pitch PID D",
		path: "pidPitchD",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_PITCH_PID_FF: {
		name: "Pitch PID FF",
		path: "pidPitchFF",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_PITCH_PID_S: {
		name: "Pitch PID S",
		path: "pidPitchS",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_YAW_PID_P: {
		name: "Yaw PID P",
		path: "pidYawP",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_YAW_PID_I: {
		name: "Yaw PID I",
		path: "pidYawI",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_YAW_PID_D: {
		name: "Yaw PID D",
		path: "pidYawD",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_YAW_PID_FF: {
		name: "Yaw PID FF",
		path: "pidYawFF",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_YAW_PID_S: {
		name: "Yaw PID S",
		path: "pidYawS",
		minValue: -1500,
		maxValue: 1500,
		unit: "",
	},
	LOG_MOTOR_OUTPUTS: {
		name: "Motor Outputs",
		path: "",
		minValue: 0,
		maxValue: 2000,
		unit: "",
		modifier: [
			{ displayNameShort: "RR", displayName: "Rear Right", path: "motorOutRR" },
			{ displayNameShort: "FR", displayName: "Front Right", path: "motorOutFR" },
			{ displayNameShort: "RL", displayName: "Rear Left", path: "motorOutRL" },
			{ displayNameShort: "FL", displayName: "Front Left", path: "motorOutFL" },
		],
	},
	LOG_FRAMETIME: {
		name: "Frametime",
		path: "frametime",
		minValue: 0,
		maxValue: 1000,
		unit: "µs",
	},
	LOG_ALTITUDE: {
		name: "Altitude",
		path: "altitude",
		rangeFn: getAltitudeRange,
		decimals: 2,
		unit: "m",
	},
	LOG_VVEL: {
		name: "Vertical Velocity",
		path: "vvel",
		minValue: -10,
		maxValue: 10,
		decimals: 2,
		unit: "m/s",
	},
	LOG_GPS: {
		name: "GPS",
		path: "",
		minValue: 0,
		maxValue: 100,
		unit: "",
		modifier: [
			{ displayNameShort: "Year", displayName: "Year", min: 2020, max: 2030, path: "gpsYear" },
			{
				displayNameShort: "Month",
				displayName: "Month",
				min: 1,
				max: 12,
				path: "gpsMonth",
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
			{ displayNameShort: "Day", displayName: "Day", min: 1, max: 31, path: "gpsDay" },
			{ displayNameShort: "Hour", displayName: "Hour", min: 0, max: 23, path: "gpsHour" },
			{ displayNameShort: "Minute", displayName: "Minute", min: 0, max: 59, path: "gpsMinute" },
			{ displayNameShort: "Second", displayName: "Second", min: 0, max: 59, path: "gpsSecond" },
			{
				displayNameShort: "Valid",
				displayName: "Validity Flags",
				min: 0,
				max: 255,
				path: "gpsTimeValidityFlags",
			},
			{
				displayNameShort: "T Acc",
				displayName: "Time Accuracy",
				min: 0,
				max: 100,
				path: "gpsTAcc",
				unit: "ns",
			},
			{
				displayNameShort: "Nanosec",
				displayName: "Nanoseconds",
				min: 0,
				max: 1e9,
				path: "gpsNs",
			},
			{
				displayNameShort: "Fix",
				displayName: "Fix Type",
				min: 0,
				max: 5,
				path: "gpsFixType",
				states: ["No Fix", "Dead reckoning only", "2D Fix", "3D Fix", "GPS + Dead reckoning", "Time only fix"],
			},
			{ displayNameShort: "Flags", displayName: "Flags", min: 0, max: 255, path: "gpsFlags" },
			{ displayNameShort: "Flags2", displayName: "Flags2", min: 0, max: 255, path: "gpsFlags2" },
			{
				displayNameShort: "Sats",
				displayName: "Satellite Count",
				min: 0,
				max: 30,
				path: "gpsSatCount",
			},
			{
				displayNameShort: "Lon",
				displayName: "Longitude",
				rangeFn: getLongitudeRange,
				path: "gpsLon",
				unit: "°",
				decimals: 7,
			},
			{
				displayNameShort: "Lat",
				displayName: "Latitude",
				rangeFn: getLatitudeRange,
				path: "gpsLat",
				unit: "°",
				decimals: 7,
			},
			{
				displayNameShort: "Alt",
				displayName: "Altitude",
				rangeFn: getAltitudeRange,
				path: "gpsAlt",
				unit: "m",
				decimals: 2,
			},
			{
				displayNameShort: "Hor Acc",
				displayName: "Horizontal Accuracy",
				min: 0,
				max: 20,
				path: "gpsHAcc",
				unit: "m",
				decimals: 2,
			},
			{
				displayNameShort: "Ver Acc",
				displayName: "Vertical Accuracy",
				min: 0,
				max: 20,
				path: "gpsVAcc",
				unit: "m",
				decimals: 2,
			},
			{
				displayNameShort: "Vel N",
				displayName: "Velocity North",
				min: -15,
				max: 15,
				path: "gpsVelN",
				unit: "m/s",
				decimals: 2,
			},
			{
				displayNameShort: "Vel E",
				displayName: "Velocity East",
				min: -15,
				max: 15,
				path: "gpsVelE",
				unit: "m/s",
				decimals: 2,
			},
			{
				displayNameShort: "Vel D",
				displayName: "Velocity Down",
				min: 10,
				max: -10, //down is positive, invert by default
				path: "gpsVelD",
				unit: "m/s",
				decimals: 2,
			},
			{
				displayNameShort: "G Speed",
				displayName: "Ground Speed",
				min: 0,
				max: 50,
				path: "gpsGSpeed",
				unit: "m/s",
				decimals: 2,
			},
			{
				displayNameShort: "Head Mot",
				displayName: "Heading of Motion",
				min: -180,
				max: 180,
				path: "gpsHeadMot",
				unit: "°",
			},
			{
				displayNameShort: "S Acc",
				displayName: "Speed Accuracy",
				min: 0,
				max: 10,
				path: "gpsSAcc",
				unit: "m/s",
				decimals: 2,
			},
			{
				displayNameShort: "Head Acc",
				displayName: "Heading Accuracy",
				min: 0,
				max: 20,
				path: "gpsHeadAcc",
				unit: "°",
				decimals: 2,
			},
			{
				displayNameShort: "pDop",
				displayName: "pDop",
				min: 0,
				max: 100,
				path: "gpsPDop",
				decimals: 2,
			},
			{ displayNameShort: "Flags3", displayName: "Flags3", min: 0, max: 31, path: "gpsFlags3" },
		],
	},
	LOG_ATT_ROLL: {
		name: "Roll Angle",
		path: "rollAngle",
		minValue: -180,
		maxValue: 180,
		unit: "°",
	},
	LOG_ATT_PITCH: {
		name: "Pitch Angle",
		path: "pitchAngle",
		minValue: -180,
		maxValue: 180,
		unit: "°",
	},
	LOG_ATT_YAW: {
		name: "Yaw Angle",
		path: "yawAngle",
		minValue: -180,
		maxValue: 180,
		unit: "°",
	},
	LOG_MOTOR_RPM: {
		name: "Motor RPM",
		path: "",
		minValue: 0,
		maxValue: 50000,
		unit: "rpm",
		modifier: [
			{ displayNameShort: "RR", displayName: "Rear Right", path: "rpmRR" },
			{ displayNameShort: "FR", displayName: "Front Right", path: "rpmFR" },
			{ displayNameShort: "RL", displayName: "Rear Left", path: "rpmRL" },
			{ displayNameShort: "FL", displayName: "Front Left", path: "rpmFL" },
		],
	},
	LOG_ACCEL_RAW: {
		name: "Accel Raw",
		path: "",
		minValue: -40,
		maxValue: 40,
		unit: "m/s²",
		decimals: 3,
		modifier: [
			{ displayNameShort: "X", displayName: "X", path: "accelRawX" },
			{ displayNameShort: "Y", displayName: "Y", path: "accelRawY" },
			{ displayNameShort: "Z", displayName: "Z", path: "accelRawZ" },
		],
	},
	LOG_ACCEL_FILTERED: {
		name: "Accel Filtered",
		path: "",
		minValue: -40,
		maxValue: 40,
		unit: "m/s²",
		decimals: 3,
		modifier: [
			{ displayNameShort: "X", displayName: "X", path: "accelFilteredX" },
			{ displayNameShort: "Y", displayName: "Y", path: "accelFilteredY" },
			{ displayNameShort: "Z", displayName: "Z", path: "accelFilteredZ" },
		],
	},
	LOG_VERTICAL_ACCEL: {
		name: "Vertical Accel",
		path: "accelVertical",
		minValue: -40,
		maxValue: 40,
		unit: "m/s²",
		decimals: 3,
	},
	LOG_VVEL_SETPOINT: {
		name: "vVel Setpoint",
		path: "setpointVvel",
		minValue: -10,
		maxValue: 10,
		decimals: 2,
		unit: "m/s",
	},
	LOG_MAG_HEADING: {
		name: "Mag Heading",
		path: "magHeading",
		minValue: -180,
		maxValue: 180,
		unit: "°",
	},
	LOG_COMBINED_HEADING: {
		name: "Combined Heading",
		path: "combinedHeading",
		minValue: -180,
		maxValue: 180,
		unit: "°",
	},
	LOG_HVEL: {
		name: "Hor. Velocity",
		path: "",
		minValue: -15,
		maxValue: 15,
		unit: "m/s",
		decimals: 2,
		modifier: [
			{
				displayNameShort: "North",
				displayName: "North",
				path: "hvelN",
			},
			{
				displayNameShort: "East",
				displayName: "East",
				path: "hvelE",
			},
		],
	},
	LOG_BARO: {
		name: "Barometer",
		path: "",
		unit: "",
		modifier: [
			{
				displayNameShort: "Raw",
				displayName: "Pressure Raw",
				path: "baroRaw",
				max: 4150000,
				min: 4025000,
			},
			{
				displayNameShort: "hPa",
				displayName: "Pressure hPa",
				path: "baroHpa",
				max: 1013.25,
				min: 983,
				decimals: 3,
			},
			{
				displayNameShort: "Alt",
				displayName: "Baro Altitude",
				path: "baroAlt",
				rangeFn: getAltitudeRange,
				decimals: 2,
			},
			{
				displayNameShort: "Up Vel", // TODO remove
				displayName: "Baro Up Velocity",
				path: "baroUpVel",
				max: 10,
				min: -10,
				decimals: 2,
			},
			{
				displayNameShort: "Up Accel", // TODO remove
				displayName: "Baro Up Acceleration",
				path: "baroUpAccel",
				max: 10,
				min: -10,
				decimals: 2,
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
		requires: ["LOG_ELRS_RAW"],
		exact: false,
	},
	GEN_PITCH_SETPOINT: {
		replaces: "LOG_PITCH_SETPOINT",
		requires: ["LOG_ELRS_RAW"],
		exact: false,
	},
	GEN_THROTTLE_SETPOINT: {
		replaces: "LOG_THROTTLE_SETPOINT",
		requires: ["LOG_ELRS_RAW"],
		exact: false,
	},
	GEN_YAW_SETPOINT: {
		replaces: "LOG_YAW_SETPOINT",
		requires: ["LOG_ELRS_RAW"],
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
		requires: ["LOG_ELRS_RAW"],
		exact: false,
	},
}
