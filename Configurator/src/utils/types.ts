export type TypedArray =
	| Int8Array
	| Uint8Array
	| Uint8ClampedArray
	| Int16Array
	| Uint16Array
	| Int32Array
	| Uint32Array
	| Float32Array
	| Float64Array

export type LogData = {
	elrsRoll?: Uint16Array
	elrsPitch?: Uint16Array
	elrsThrottle?: Uint16Array
	elrsYaw?: Uint16Array
	setpointRoll?: Float32Array
	setpointPitch?: Float32Array
	setpointThrottle?: Float32Array
	setpointYaw?: Float32Array
	setpointVvel?: Float32Array
	gyroRawRoll?: Float32Array
	gyroRawPitch?: Float32Array
	gyroRawYaw?: Float32Array
	pidRollP?: Float32Array
	pidRollI?: Float32Array
	pidRollD?: Float32Array
	pidRollFF?: Float32Array
	pidRollS?: Float32Array
	pidPitchP?: Float32Array
	pidPitchI?: Float32Array
	pidPitchD?: Float32Array
	pidPitchFF?: Float32Array
	pidPitchS?: Float32Array
	pidYawP?: Float32Array
	pidYawI?: Float32Array
	pidYawD?: Float32Array
	pidYawFF?: Float32Array
	pidYawS?: Float32Array
	motorOutRR?: Uint16Array
	motorOutFR?: Uint16Array
	motorOutRL?: Uint16Array
	motorOutFL?: Uint16Array
	rpmRR?: Float32Array
	rpmFR?: Float32Array
	rpmRL?: Float32Array
	rpmFL?: Float32Array
	frametime?: Uint16Array
	rollAngle?: Float32Array
	pitchAngle?: Float32Array
	yawAngle?: Float32Array
	altitude?: Float32Array
	baroRaw?: Uint32Array
	baroHpa?: Float32Array
	baroAlt?: Float32Array
	baroUpVel?: Float32Array // TODO remove
	baroUpAccel?: Float32Array // TODO remove
	vvel?: Float32Array
	hvelN?: Float32Array
	hvelE?: Float32Array
	magHeading?: Float32Array
	combinedHeading?: Float32Array
	accelRawX?: Float32Array
	accelRawY?: Float32Array
	accelRawZ?: Float32Array
	accelFilteredX?: Float32Array
	accelFilteredY?: Float32Array
	accelFilteredZ?: Float32Array
	accelVertical?: Float32Array
	gpsYear?: Uint16Array
	gpsMonth?: Uint8Array
	gpsDay?: Uint8Array
	gpsHour?: Uint8Array
	gpsMinute?: Uint8Array
	gpsSecond?: Uint8Array
	gpsTimeValidityFlags?: Uint8Array
	gpsTAcc?: Uint32Array
	gpsNs?: Uint32Array
	gpsFixType?: Uint8Array
	gpsFlags?: Uint8Array
	gpsFlags2?: Uint8Array
	gpsSatCount?: Uint8Array
	gpsLon?: Float64Array
	gpsLat?: Float64Array
	gpsAlt?: Float32Array
	gpsHAcc?: Float32Array
	gpsVAcc?: Float32Array
	gpsVelN?: Float32Array
	gpsVelE?: Float32Array
	gpsVelD?: Float32Array
	gpsGSpeed?: Float32Array
	gpsHeadMot?: Float32Array
	gpsSAcc?: Float32Array
	gpsHeadAcc?: Float32Array
	gpsPDop?: Float32Array
	gpsFlags3?: Uint16Array
	flightMode?: Uint8Array
	debug1?: Int32Array
	debug2?: Int32Array
	debug3?: Int16Array
	debug4?: Int16Array
	pidSumRoll?: Int16Array
	pidSumPitch?: Int16Array
	pidSumYaw?: Int16Array
	timestamp?: Uint32Array
	vbat?: Float32Array
	linkRssiA?: Int16Array
	linkRssiB?: Int16Array
	linkLqi?: Uint8Array
	linkSnr?: Int8Array
	linkAntennaSel?: Uint8Array
	linkTargetHz?: Uint16Array
	linkActualHz?: Uint16Array
	linkTxPow?: Uint16Array
}

export type ActualCoeffs = {
	center: number
	max: number
	expo: number
}

export type BBLog = {
	frameCount: number
	flightModes: { fm: number; frame: number }[]
	offsets: { [key: string]: number }
	frameLoadingStatus: Uint8Array
	highlights: number[]
	rawFile: Uint8Array
	flags: string[]
	logData: LogData
	version: Uint8Array
	startTime: Date
	ranges: {
		gyro: number
		accel: number
	}
	pidFrequency: number
	frequencyDivider: number
	rateCoeffs: ActualCoeffs[]
	pidConstants: number[][]
	pidConstantsNice: number[][]
	framesPerSecond: number
	isExact: boolean
	motorPoles: number
	duration: number
	disarmReason: number
}

export type TraceInGraph = {
	path: string
	color: string
	strokeWidth: number
	minValue: number
	maxValue: number
	id: number
	unit: string
	loadedBitmask: number
	states?: string[]
	decimals: number
	displayName: string
	overrideData?: Float32Array
	hasSetData: boolean
}

export type TraceInternalData = {
	autoRangeOn: boolean
	currentModifierName: string
	filteringOn: boolean
	filterType: "pt1" | "pt2" | "pt3" | "sma" | "binomial"
	filterValue1: number
	filterValue2: boolean
	flagName: string
	minValue: number
	maxValue: number
}

export type FlagProps = {
	name: string
	path: string
	loadedBitmask: number
	minValue?: number
	maxValue?: number
	rangeFn?: (file: BBLog | undefined) => { max: number; min: number }
	unit: string
	decimals?: number
	states?: string[]
	modifier?: {
		displayNameShort: string
		displayName: string
		min?: number
		max?: number
		rangeFn?: (file: BBLog | undefined) => { max: number; min: number }
		path: string
		unit?: string
		decimals?: number
		states?: string[]
	}[]
}

export type GenFlagProps = {
	replaces: string
	requires: (string | string[])[] // if its a string, that has to be in there. If its an array, one of the mentioned ones has to be in there
	exact: boolean
}

export type Command = {
	command: number
	length: number
	data: Uint8Array
	dataStr: string
	cmdType: "request" | "response" | "error"
	flag: number
	version: number
	callbackData?: any
}
