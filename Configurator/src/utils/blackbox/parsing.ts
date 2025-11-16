import { BBLog, LogData, TypedArray } from "@utils/types"
import { leBytesToBigInt, leBytesToInt } from "@utils/utils"
import { BB_ALL_FLAGS } from "@utils/blackbox/bbFlags"

const ACC_RANGES = [2, 4, 8, 16]
const GYRO_RANGES = [2000, 1000, 500, 250, 125]
const PID_SHIFTS = [11, 3, 16, 8, 8]

export function resizeTypedArrays(obj: { [key: string]: TypedArray }, newLength: number): void {
	for (const key in obj) {
		const arr = obj[key]
		const ctor = arr.constructor as { new (length: number): TypedArray }
		obj[key] = new ctor(newLength)
	}
}

export function parseBlackboxHeader(header: Uint8Array): BBLog | string {
	const bbLog: BBLog = {
		disarmReason: 0,
		duration: 0,
		fileSize: 0,
		flags: [],
		flightModes: [],
		frameCount: 0,
		frameLoadingStatus: new Uint8Array(0),
		frameSize: 0,
		framesPerSecond: 0,
		frequencyDivider: 0,
		highlights: [],
		isExact: true,
		logData: {},
		motorPoles: 0,
		offsets: {},
		pidConstants: [[], [], []],
		pidConstantsNice: [[], [], []],
		pidFrequency: 0,
		ranges: { gyro: 0, accel: 0 },
		rateCoeffs: [],
		startTime: new Date(),
		syncFrequency: 0,
		syncs: [],
		version: new Uint8Array(0),
	}

	const magic = leBytesToBigInt(header, 0, 8)
	if (magic !== 0x0001494c4f4bdfdcn) {
		return magic.toString(16)
	}
	bbLog.version = header.slice(8, 11)
	bbLog.startTime = new Date(leBytesToInt(header, 11, 4) * 1000)
	bbLog.duration = leBytesToInt(header, 15, 4) / 1000 // in seconds
	bbLog.pidFrequency = [3200][header[19]]
	bbLog.frequencyDivider = header[20]
	const rangeByte = header[21]
	bbLog.ranges = {
		gyro: GYRO_RANGES[(rangeByte >> 2) & 0b111],
		accel: ACC_RANGES[rangeByte & 0b11],
	}
	const rfBytes = header.slice(22, 58)
	for (let i = 0; i < 3; i++) {
		bbLog.rateCoeffs[i] = {
			center: leBytesToInt(rfBytes, i * 12, 4) / 65536,
			max: leBytesToInt(rfBytes, i * 12 + 4, 4) / 65536,
			expo: leBytesToInt(rfBytes, i * 12 + 8, 4) / 65536,
		}
	}

	const pcBytes = header.slice(82, 112)
	for (let i = 0; i < 3; i++) {
		for (let j = 0; j < 5; j++) {
			bbLog.pidConstantsNice[i][j] = leBytesToInt(pcBytes, i * 10 + j * 2, 2)
			bbLog.pidConstants[i][j] = (bbLog.pidConstantsNice[i][0] << PID_SHIFTS[j]) / 65536
		}
	}

	const flagSlice = header.slice(142, 150)
	let offset = 0
	const flags = bbLog.flags
	for (let j = 0; j < 64; j++) {
		const byteNum = Math.floor(j / 8)
		const bitNum = j % 8
		const flagIsSet = flagSlice[byteNum] & (1 << bitNum)
		if (!flagIsSet || !Object.keys(BB_ALL_FLAGS)[j]) continue
		flags.push(Object.keys(BB_ALL_FLAGS)[j])
		bbLog.offsets[Object.keys(BB_ALL_FLAGS)[j]] = offset
		switch (j) {
			case 23:
			case 31:
			case 32:
			case 33:
			case 44:
				offset += 6
				break
			case 38:
			case 40:
			case 41:
				offset += 4
				break
			case 39:
				offset += 3
				break
			case 0: // ELRS_RAW
			case 27: // GPS
			case 45: // VBAT
			case 46: // LINK_STATS
				break
			default:
				offset += 2
				break
		}
	}

	bbLog.motorPoles = header[150]
	bbLog.disarmReason = header[151]
	bbLog.syncFrequency = header[152]
	bbLog.frameSize = header[153]
	bbLog.framesPerSecond = bbLog.pidFrequency / bbLog.frequencyDivider

	const logData = bbLog.logData
	if (flags.includes("LOG_ROLL_SETPOINT")) {
		logData.setpointRoll = new Float32Array(0)
	}
	if (flags.includes("LOG_PITCH_SETPOINT")) {
		logData.setpointPitch = new Float32Array(0)
	}
	if (flags.includes("LOG_THROTTLE_SETPOINT")) {
		logData.setpointThrottle = new Float32Array(0)
	}
	if (flags.includes("LOG_YAW_SETPOINT")) {
		logData.setpointYaw = new Float32Array(0)
	}
	if (flags.includes("LOG_ROLL_GYRO_RAW")) {
		logData.gyroRawRoll = new Float32Array(0)
	}
	if (flags.includes("LOG_PITCH_GYRO_RAW")) {
		logData.gyroRawPitch = new Float32Array(0)
	}
	if (flags.includes("LOG_YAW_GYRO_RAW")) {
		logData.gyroRawYaw = new Float32Array(0)
	}
	if (flags.includes("LOG_ROLL_PID_P")) {
		logData.pidRollP = new Float32Array(0)
	}
	if (flags.includes("LOG_ROLL_PID_I")) {
		logData.pidRollI = new Float32Array(0)
	}
	if (flags.includes("LOG_ROLL_PID_D")) {
		logData.pidRollD = new Float32Array(0)
	}
	if (flags.includes("LOG_ROLL_PID_FF")) {
		logData.pidRollFF = new Float32Array(0)
	}
	if (flags.includes("LOG_ROLL_PID_S")) {
		logData.pidRollS = new Float32Array(0)
	}
	if (flags.includes("LOG_PITCH_PID_P")) {
		logData.pidPitchP = new Float32Array(0)
	}
	if (flags.includes("LOG_PITCH_PID_I")) {
		logData.pidPitchI = new Float32Array(0)
	}
	if (flags.includes("LOG_PITCH_PID_D")) {
		logData.pidPitchD = new Float32Array(0)
	}
	if (flags.includes("LOG_PITCH_PID_FF")) {
		logData.pidPitchFF = new Float32Array(0)
	}
	if (flags.includes("LOG_PITCH_PID_S")) {
		logData.pidPitchS = new Float32Array(0)
	}
	if (flags.includes("LOG_YAW_PID_P")) {
		logData.pidYawP = new Float32Array(0)
	}
	if (flags.includes("LOG_YAW_PID_I")) {
		logData.pidYawI = new Float32Array(0)
	}
	if (flags.includes("LOG_YAW_PID_D")) {
		logData.pidYawD = new Float32Array(0)
	}
	if (flags.includes("LOG_YAW_PID_FF")) {
		logData.pidYawFF = new Float32Array(0)
	}
	if (flags.includes("LOG_YAW_PID_S")) {
		logData.pidYawS = new Float32Array(0)
	}
	if (flags.includes("LOG_MOTOR_OUTPUTS")) {
		logData.motorOutRR = new Uint16Array(0)
		logData.motorOutFR = new Uint16Array(0)
		logData.motorOutRL = new Uint16Array(0)
		logData.motorOutFL = new Uint16Array(0)
	}
	if (flags.includes("LOG_FRAMETIME")) {
		logData.frametime = new Uint16Array(0)
		logData.timestamp = new Uint32Array(0)
	}
	if (flags.includes("LOG_ALTITUDE")) {
		logData.altitude = new Float32Array(0)
	}
	if (flags.includes("LOG_VVEL")) {
		logData.vvel = new Float32Array(0)
	}
	if (flags.includes("LOG_ATT_ROLL")) {
		logData.rollAngle = new Float32Array(0)
	}
	if (flags.includes("LOG_ATT_PITCH")) {
		logData.pitchAngle = new Float32Array(0)
	}
	if (flags.includes("LOG_ATT_YAW")) {
		logData.yawAngle = new Float32Array(0)
	}
	if (flags.includes("LOG_MOTOR_RPM")) {
		logData.rpmRR = new Float32Array(0)
		logData.rpmFR = new Float32Array(0)
		logData.rpmRL = new Float32Array(0)
		logData.rpmFL = new Float32Array(0)
	}
	if (flags.includes("LOG_ACCEL_RAW")) {
		logData.accelRawX = new Float32Array(0)
		logData.accelRawY = new Float32Array(0)
		logData.accelRawZ = new Float32Array(0)
	}
	if (flags.includes("LOG_ACCEL_FILTERED")) {
		logData.accelFilteredX = new Float32Array(0)
		logData.accelFilteredY = new Float32Array(0)
		logData.accelFilteredZ = new Float32Array(0)
	}
	if (flags.includes("LOG_VERTICAL_ACCEL")) {
		logData.accelVertical = new Float32Array(0)
	}
	if (flags.includes("LOG_VVEL_SETPOINT")) {
		logData.setpointVvel = new Float32Array(0)
	}
	if (flags.includes("LOG_MAG_HEADING")) {
		logData.magHeading = new Float32Array(0)
	}
	if (flags.includes("LOG_COMBINED_HEADING")) {
		logData.combinedHeading = new Float32Array(0)
	}
	if (flags.includes("LOG_HVEL")) {
		logData.hvelN = new Float32Array(0)
		logData.hvelE = new Float32Array(0)
	}
	if (flags.includes("LOG_BARO")) {
		logData.baroRaw = new Uint32Array(0)
		logData.baroHpa = new Float32Array(0)
		logData.baroAlt = new Float32Array(0)
		logData.baroUpVel = new Float32Array(0) // TODO remove
		logData.baroUpAccel = new Float32Array(0) // TODO remove
	}
	if (flags.includes("LOG_DEBUG_1")) {
		logData.debug1 = new Int32Array(0)
	}
	if (flags.includes("LOG_DEBUG_2")) {
		logData.debug2 = new Int32Array(0)
	}
	if (flags.includes("LOG_DEBUG_3")) {
		logData.debug3 = new Int16Array(0)
	}
	if (flags.includes("LOG_DEBUG_4")) {
		logData.debug4 = new Int16Array(0)
	}
	if (flags.includes("LOG_PID_SUM")) {
		logData.pidSumRoll = new Int16Array(0)
		logData.pidSumPitch = new Int16Array(0)
		logData.pidSumYaw = new Int16Array(0)
	}

	if (flags.includes("LOG_ELRS_RAW")) {
		logData.elrsRoll = new Uint16Array(0)
		logData.elrsPitch = new Uint16Array(0)
		logData.elrsThrottle = new Uint16Array(0)
		logData.elrsYaw = new Uint16Array(0)
	}
	if (flags.includes("LOG_GPS")) {
		logData.gpsYear = new Uint16Array(0)
		logData.gpsMonth = new Uint8Array(0)
		logData.gpsDay = new Uint8Array(0)
		logData.gpsHour = new Uint8Array(0)
		logData.gpsMinute = new Uint8Array(0)
		logData.gpsSecond = new Uint8Array(0)
		logData.gpsTimeValidityFlags = new Uint8Array(0)
		logData.gpsTAcc = new Uint32Array(0)
		logData.gpsNs = new Uint32Array(0)
		logData.gpsFixType = new Uint8Array(0)
		logData.gpsFlags = new Uint8Array(0)
		logData.gpsFlags2 = new Uint8Array(0)
		logData.gpsSatCount = new Uint8Array(0)
		logData.gpsLon = new Float64Array(0)
		logData.gpsLat = new Float64Array(0)
		logData.gpsAlt = new Float32Array(0)
		logData.gpsHAcc = new Float32Array(0)
		logData.gpsVAcc = new Float32Array(0)
		logData.gpsVelN = new Float32Array(0)
		logData.gpsVelE = new Float32Array(0)
		logData.gpsVelD = new Float32Array(0)
		logData.gpsGSpeed = new Float32Array(0)
		logData.gpsHeadMot = new Float32Array(0)
		logData.gpsSAcc = new Float32Array(0)
		logData.gpsHeadAcc = new Float32Array(0)
		logData.gpsPDop = new Float32Array(0)
		logData.gpsFlags3 = new Uint16Array(0)
	}
	if (flags.includes("LOG_VBAT")) {
		logData.vbat = new Float32Array(0)
	}
	if (flags.includes("LOG_LINK_STATS")) {
		logData.linkRssiA = new Int16Array(0)
		logData.linkRssiB = new Int16Array(0)
		logData.linkLqi = new Uint8Array(0)
		logData.linkSnr = new Int8Array(0)
		logData.linkAntennaSel = new Uint8Array(0)
		logData.linkTargetHz = new Uint16Array(0)
		logData.linkActualHz = new Uint16Array(0)
		logData.linkTxPow = new Uint16Array(0)
	}

	return bbLog
}

export function parseBlackbox(binFile: Uint8Array): BBLog | string {
	const header = binFile.slice(0, 256)
	const data = unescapeBlackbox(binFile.slice(256))

	const bbLog = parseBlackboxHeader(header)
	if (typeof bbLog === "string") return bbLog
	const frameSize = bbLog.frameSize
	bbLog.fileSize = binFile.length

	// count frames, mark special positions
	let pos = 0
	let frameCount = 0
	let framePos: number[] = []
	let flightModes = bbLog.flightModes
	let highlights = bbLog.highlights
	let gpsPos: { pos: number; frame: number }[] = []
	let elrsPos: { pos: number; frame: number }[] = []
	let batPos: { pos: number; frame: number }[] = []
	let elrsLinkPos: { pos: number; frame: number }[] = []
	while (pos < data.length) {
		switch (data[pos]) {
			case 0: // regular frame
				framePos[frameCount++] = pos + 1
				pos += frameSize + 1
				break
			case 1: // flight mode
				flightModes.push({ fm: data[pos + 1], frame: frameCount })
				pos += 2
				break
			case 2: // highlight
				highlights.push(frameCount)
				pos += 1
				break
			case 3: // GPS
				gpsPos.push({ pos: pos + 1, frame: frameCount })
				pos += 93
				break
			case 4: // ELRS
				elrsPos.push({ pos: pos + 1, frame: frameCount })
				pos += 7
				break
			case 5: // VBAT
				batPos.push({ pos: pos + 1, frame: frameCount })
				pos += 3
				break
			case 6: // ELRS link
				elrsLinkPos.push({ pos: pos + 1, frame: frameCount })
				pos += 12
				break
			case 83: // SYNC
				{
					const frame = leBytesToInt(data, pos + 5, 4, false)
					bbLog.syncs.push({ frame, pos, ctrlByte: data[pos + 4] })
					pos += 13
				}
				break
			default:
				console.log("invalid frame found")
				pos++
				break
		}
	}
	bbLog.frameCount = frameCount

	// use frameCount and resize all data holders
	const logData = bbLog.logData
	resizeTypedArrays(logData, frameCount)
	bbLog.frameLoadingStatus = new Uint8Array(frameCount)
	bbLog.frameLoadingStatus.fill(0)

	// TODO idea for getting highlights and fm changes: one additional byte in the sync frame that has flags for a FM change and a highlight
	// TODO idea for getting ELRS and GPS: configurator requests ELRS for e.g. frame 120. PC sends request with needed sync(s) for frame 100 and FC responds with ELRS and "valid from frame x to frame y" after having searched the next ELRS
	// TODO when configurator requests frame, send bitmap alongside the requested frame number to indicate what data is wanted. Then maybe also send the index of the 2nd last sync, not just the last sync

	const flags = bbLog.flags
	if (flags.includes("LOG_ELRS_RAW")) {
		const elrsData = new Uint8Array(elrsPos.length * 6)
		elrsPos.forEach((v, i) => {
			const p = v.pos
			elrsData.set(data.slice(p, p + 6), i * 6)
		})
		const elrsFromFrame = new Uint32Array(elrsPos.length)
		const elrsToFrame = new Uint32Array(elrsPos.length)
		for (let i = 0; i < elrsPos.length - 1; i++) {
			elrsFromFrame[i] = elrsPos[i].frame
			elrsToFrame[i] = elrsPos[i + 1].frame - 1
		}
		elrsFromFrame[elrsPos.length - 1] = elrsPos[elrsPos.length - 1].frame
		elrsToFrame[elrsPos.length - 1] = frameCount - 1

		parseElrs(logData, bbLog.frameLoadingStatus, elrsData, elrsFromFrame, elrsToFrame)
		bbLog.frameLoadingStatus.forEach((v, i) => {
			bbLog.frameLoadingStatus[i] = v |= 0b10
		})
	}
	if (flags.includes("LOG_GPS")) {
		const gpsData = new Uint8Array(gpsPos.length * 92)
		gpsPos.forEach((v, i) => {
			const p = v.pos
			gpsData.set(data.slice(p, p + 92), i * 92)
		})
		const gpsFrom = new Uint32Array(gpsPos.length)
		const gpsTo = new Uint32Array(gpsPos.length)
		for (let i = 0; i < gpsPos.length - 1; i++) {
			gpsFrom[i] = gpsPos[i].frame
			gpsTo[i] = gpsPos[i + 1].frame - 1
		}
		gpsFrom[gpsPos.length - 1] = gpsPos[gpsPos.length - 1].frame
		gpsTo[gpsPos.length - 1] = frameCount - 1

		parseGps(logData, bbLog.frameLoadingStatus, gpsData, gpsFrom, gpsTo)
		bbLog.frameLoadingStatus.forEach((v, i) => {
			bbLog.frameLoadingStatus[i] = v |= 0b100
		})
	}
	if (flags.includes("LOG_VBAT")) {
		const vData = new Uint8Array(batPos.length * 2)
		batPos.forEach((v, i) => {
			const p = v.pos
			vData.set(data.slice(p, p + 2), i * 2)
		})
		const vFrom = new Uint32Array(batPos.length)
		const vTo = new Uint32Array(batPos.length)
		for (let i = 0; i < batPos.length - 1; i++) {
			vFrom[i] = batPos[i].frame
			vTo[i] = batPos[i + 1].frame - 1
		}
		vFrom[batPos.length - 1] = batPos[batPos.length - 1].frame
		vTo[batPos.length - 1] = frameCount - 1

		parseVbat(logData, bbLog.frameLoadingStatus, vData, vFrom, vTo)
		bbLog.frameLoadingStatus.forEach((v, i) => {
			bbLog.frameLoadingStatus[i] = v |= 0b1000
		})
	}
	if (flags.includes("LOG_LINK_STATS")) {
		const lData = new Uint8Array(elrsLinkPos.length * 11)
		elrsLinkPos.forEach((v, i) => {
			const p = v.pos
			lData.set(data.slice(p, p + 11), i * 11)
		})
		const lFrom = new Uint32Array(elrsLinkPos.length)
		const lTo = new Uint32Array(elrsLinkPos.length)
		for (let i = 0; i < elrsLinkPos.length - 1; i++) {
			lFrom[i] = elrsLinkPos[i].frame
			lTo[i] = elrsLinkPos[i + 1].frame - 1
		}
		lFrom[elrsLinkPos.length - 1] = elrsLinkPos[elrsLinkPos.length - 1].frame
		lTo[elrsLinkPos.length - 1] = frameCount - 1

		parseLinkStats(logData, bbLog.frameLoadingStatus, lData, lFrom, lTo)
		bbLog.frameLoadingStatus.forEach((v, i) => {
			bbLog.frameLoadingStatus[i] = v |= 0b10000
		})
	}

	const frameData = new Uint8Array(frameCount * frameSize)
	framePos.forEach((p, i) => {
		frameData.set(data.slice(p, p + frameSize), i * frameSize)
	})
	const frameNumbers = new Uint32Array(frameCount)
	frameNumbers.forEach((_, i) => {
		frameNumbers[i] = i
	})
	parseFrames(
		logData,
		frameData,
		frameNumbers,
		bbLog.frameLoadingStatus,
		bbLog.offsets,
		frameSize,
		flags,
		bbLog.framesPerSecond,
		bbLog.motorPoles
	)

	bbLog.rawFile = binFile
	return bbLog
}

function unescapeBlackbox(withEscapedParts: Uint8Array): Uint8Array {
	const out = new Uint8Array(withEscapedParts.length)
	let outIndex = 0
	let synIndex = 0
	const S = "S".charCodeAt(0)
	const Y = "Y".charCodeAt(0)
	const N = "N".charCodeAt(0)
	const EX = "!".charCodeAt(0)
	const C = "C".charCodeAt(0)
	let syncDeadtime = 0
	for (let i = 0; i < withEscapedParts.length; i++) {
		const b = withEscapedParts[i]
		if (syncDeadtime) {
			out[outIndex++] = b
			continue
		}
		if (synIndex === 0) {
			out[outIndex++] = b
			if (b === S) synIndex = 1
			continue
		}

		switch (synIndex) {
			case 1:
				out[outIndex++] = b
				if (b === Y) synIndex = 2
				else if (b === S) synIndex = 1
				else synIndex = 0
				break
			case 2:
				out[outIndex++] = b
				if (b === N) synIndex = 3
				else if (b === S) synIndex = 1
				else synIndex = 0
				break
			case 3:
				switch (b) {
					case EX:
						// ignore the ! in SYN!
						break
					case C:
						// keep SYNC fully
						out[outIndex++] = b
						syncDeadtime = 9
						break
					default:
						console.log("weird shit's going on")
						out[outIndex++] = b
						break
				}
				synIndex = 0
				break
		}
	}
	return out.slice(0, outIndex)
}

export function parseFrames(
	logData: LogData,
	frameData: Uint8Array,
	frameNumbers: Uint32Array,
	frameLoadingStatus: Uint8Array,
	offsets: { [key: string]: number },
	frameSize: number,
	flags: string[],
	framesPerSecond: number,
	motorPoles: number
) {
	if (flags.includes("LOG_ROLL_SETPOINT")) {
		const o = offsets["LOG_ROLL_SETPOINT"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.setpointRoll![f] = leBytesToInt(frameData, p, 2, true) / 16 // data is 12.4 fixed point
		})
	}
	if (flags.includes("LOG_PITCH_SETPOINT")) {
		const o = offsets["LOG_PITCH_SETPOINT"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.setpointPitch![f] = leBytesToInt(frameData, p, 2, true) / 16 // data is 12.4 fixed point
		})
	}
	if (flags.includes("LOG_THROTTLE_SETPOINT")) {
		const o = offsets["LOG_THROTTLE_SETPOINT"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.setpointThrottle![f] = leBytesToInt(frameData, p, 2) / 32 + 1000 // data is 12.4 fixed point
		})
	}
	if (flags.includes("LOG_YAW_SETPOINT")) {
		const o = offsets["LOG_YAW_SETPOINT"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.setpointYaw![f] = leBytesToInt(frameData, p, 2, true) / 16 // data is 12.4 fixed point
		})
	}
	if (flags.includes("LOG_ROLL_GYRO_RAW")) {
		const o = offsets["LOG_ROLL_GYRO_RAW"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.gyroRawRoll![f] = leBytesToInt(frameData, p, 2, true) / 16 // data is 12.4 fixed point
		})
	}
	if (flags.includes("LOG_PITCH_GYRO_RAW")) {
		const o = offsets["LOG_PITCH_GYRO_RAW"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.gyroRawPitch![f] = leBytesToInt(frameData, p, 2, true) / 16 // data is 12.4 fixed point
		})
	}
	if (flags.includes("LOG_YAW_GYRO_RAW")) {
		const o = offsets["LOG_YAW_GYRO_RAW"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.gyroRawYaw![f] = leBytesToInt(frameData, p, 2, true) / 16 // data is 12.4 fixed point
		})
	}
	if (flags.includes("LOG_ROLL_PID_P")) {
		const o = offsets["LOG_ROLL_PID_P"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.pidRollP![f] = leBytesToInt(frameData, p, 2, true)
		})
	}
	if (flags.includes("LOG_ROLL_PID_I")) {
		const o = offsets["LOG_ROLL_PID_I"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.pidRollI![f] = leBytesToInt(frameData, p, 2, true)
		})
	}
	if (flags.includes("LOG_ROLL_PID_D")) {
		const o = offsets["LOG_ROLL_PID_D"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.pidRollD![f] = leBytesToInt(frameData, p, 2, true)
		})
	}
	if (flags.includes("LOG_ROLL_PID_FF")) {
		const o = offsets["LOG_ROLL_PID_FF"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.pidRollFF![f] = leBytesToInt(frameData, p, 2, true)
		})
	}
	if (flags.includes("LOG_ROLL_PID_S")) {
		const o = offsets["LOG_ROLL_PID_S"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.pidRollS![f] = leBytesToInt(frameData, p, 2, true)
		})
	}
	if (flags.includes("LOG_PITCH_PID_P")) {
		const o = offsets["LOG_PITCH_PID_P"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.pidPitchP![f] = leBytesToInt(frameData, p, 2, true)
		})
	}
	if (flags.includes("LOG_PITCH_PID_I")) {
		const o = offsets["LOG_PITCH_PID_I"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.pidPitchI![f] = leBytesToInt(frameData, p, 2, true)
		})
	}
	if (flags.includes("LOG_PITCH_PID_D")) {
		const o = offsets["LOG_PITCH_PID_D"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.pidPitchD![f] = leBytesToInt(frameData, p, 2, true)
		})
	}
	if (flags.includes("LOG_PITCH_PID_FF")) {
		const o = offsets["LOG_PITCH_PID_FF"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.pidPitchFF![f] = leBytesToInt(frameData, p, 2, true)
		})
	}
	if (flags.includes("LOG_PITCH_PID_S")) {
		const o = offsets["LOG_PITCH_PID_S"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.pidPitchS![f] = leBytesToInt(frameData, p, 2, true)
		})
	}
	if (flags.includes("LOG_YAW_PID_P")) {
		const o = offsets["LOG_YAW_PID_P"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.pidYawP![f] = leBytesToInt(frameData, p, 2, true)
		})
	}
	if (flags.includes("LOG_YAW_PID_I")) {
		const o = offsets["LOG_YAW_PID_I"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.pidYawI![f] = leBytesToInt(frameData, p, 2, true)
		})
	}
	if (flags.includes("LOG_YAW_PID_D")) {
		const o = offsets["LOG_YAW_PID_D"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.pidYawD![f] = leBytesToInt(frameData, p, 2, true)
		})
	}
	if (flags.includes("LOG_YAW_PID_FF")) {
		const o = offsets["LOG_YAW_PID_FF"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.pidYawFF![f] = leBytesToInt(frameData, p, 2, true)
		})
	}
	if (flags.includes("LOG_YAW_PID_S")) {
		const o = offsets["LOG_YAW_PID_S"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.pidYawS![f] = leBytesToInt(frameData, p, 2, true)
		})
	}
	if (flags.includes("LOG_MOTOR_OUTPUTS")) {
		const o = offsets["LOG_MOTOR_OUTPUTS"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			const motors = leBytesToBigInt(frameData, p, 6)
			logData.motorOutRR![f] = Number(motors & 0xfffn)
			logData.motorOutFR![f] = Number((motors >> 12n) & 0xfffn)
			logData.motorOutRL![f] = Number((motors >> 24n) & 0xfffn)
			logData.motorOutFL![f] = Number((motors >> 36n) & 0xfffn)
		})
	}
	if (flags.includes("LOG_FRAMETIME")) {
		const o = offsets["LOG_FRAMETIME"]
		let t = 0
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.frametime![f] = leBytesToInt(frameData, p, 2)
			t += logData.frametime![f]
			logData.timestamp![f] = t
		})
	}
	if (flags.includes("LOG_ALTITUDE")) {
		const o = offsets["LOG_ALTITUDE"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.altitude![f] = leBytesToInt(frameData, p, 2, true) / 64
		})
	}
	if (flags.includes("LOG_VVEL")) {
		const o = offsets["LOG_VVEL"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.vvel![f] = leBytesToInt(frameData, p, 2, true) / 256
		})
	}
	if (flags.includes("LOG_ATT_ROLL")) {
		const o = offsets["LOG_ATT_ROLL"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.rollAngle![f] = ((leBytesToInt(frameData, p, 2, true) / 10000) * 180) / Math.PI
		})
	}
	if (flags.includes("LOG_ATT_PITCH")) {
		const o = offsets["LOG_ATT_PITCH"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.pitchAngle![f] = ((leBytesToInt(frameData, p, 2, true) / 10000) * 180) / Math.PI
		})
	}
	if (flags.includes("LOG_ATT_YAW")) {
		const o = offsets["LOG_ATT_YAW"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.yawAngle![f] = ((leBytesToInt(frameData, p, 2, true) / 10000) * 180) / Math.PI
		})
	}
	if (flags.includes("LOG_MOTOR_RPM")) {
		const o = offsets["LOG_MOTOR_RPM"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			const motors = leBytesToBigInt(frameData, p, 6)
			let rr = Number(motors & 0xfffn)
			let fr = Number((motors >> 12n) & 0xfffn)
			let rl = Number((motors >> 24n) & 0xfffn)
			let fl = Number((motors >> 36n) & 0xfffn)
			if (rr === 0xfff) {
				logData.rpmRR![f] = 0
			} else {
				rr = (rr & 0x1ff) << (rr >> 9)
				logData.rpmRR![f] = (60000000 + 50 * rr) / rr / (motorPoles / 2)
			}
			if (fr === 0xfff) {
				logData.rpmFR![f] = 0
			} else {
				fr = (fr & 0x1ff) << (fr >> 9)
				logData.rpmFR![f] = (60000000 + 50 * fr) / fr / (motorPoles / 2)
			}
			if (rl === 0xfff) {
				logData.rpmRL![f] = 0
			} else {
				rl = (rl & 0x1ff) << (rl >> 9)
				logData.rpmRL![f] = (60000000 + 50 * rl) / rl / (motorPoles / 2)
			}
			if (fl === 0xfff) {
				logData.rpmFL![f] = 0
			} else {
				fl = (fl & 0x1ff) << (fl >> 9)
				logData.rpmFL![f] = (60000000 + 50 * fl) / fl / (motorPoles / 2)
			}
		})
	}
	if (flags.includes("LOG_ACCEL_RAW")) {
		const o = offsets["LOG_ACCEL_RAW"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.accelRawX![f] = (leBytesToInt(frameData, p, 2, true) * 9.81) / 2048
			logData.accelRawY![f] = (leBytesToInt(frameData, p + 2, 2, true) * 9.81) / 2048
			logData.accelRawZ![f] = (leBytesToInt(frameData, p + 4, 2, true) * 9.81) / 2048
		})
	}
	if (flags.includes("LOG_ACCEL_FILTERED")) {
		const o = offsets["LOG_ACCEL_FILTERED"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.accelFilteredX![f] = (leBytesToInt(frameData, p, 2, true) * 9.81) / 2048
			logData.accelFilteredY![f] = (leBytesToInt(frameData, p + 2, 2, true) * 9.81) / 2048
			logData.accelFilteredZ![f] = (leBytesToInt(frameData, p + 4, 2, true) * 9.81) / 2048
		})
	}
	if (flags.includes("LOG_VERTICAL_ACCEL")) {
		const o = offsets["LOG_VERTICAL_ACCEL"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.accelVertical![f] = leBytesToInt(frameData, p, 2, true) / 128
		})
	}
	if (flags.includes("LOG_VVEL_SETPOINT")) {
		const o = offsets["LOG_VVEL_SETPOINT"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.setpointVvel![f] = leBytesToInt(frameData, p, 2, true) / 4096
		})
	}
	if (flags.includes("LOG_MAG_HEADING")) {
		const o = offsets["LOG_MAG_HEADING"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.magHeading![f] = ((leBytesToInt(frameData, p, 2, true) / 8192) * 180) / Math.PI
		})
	}
	if (flags.includes("LOG_COMBINED_HEADING")) {
		const o = offsets["LOG_COMBINED_HEADING"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.combinedHeading![f] = ((leBytesToInt(frameData, p, 2, true) / 8192) * 180) / Math.PI
		})
	}
	if (flags.includes("LOG_HVEL")) {
		const o = offsets["LOG_HVEL"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.hvelN![f] = leBytesToInt(frameData, p, 2, true) / 256
			logData.hvelE![f] = leBytesToInt(frameData, p + 2, 2, true) / 256
		})
	}
	if (flags.includes("LOG_BARO")) {
		const fOffset = framesPerSecond / 50
		const o = offsets["LOG_BARO"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.baroRaw![f] = leBytesToInt(frameData, p, 3)
			logData.baroHpa![f] = logData.baroRaw![f] / 4096
			logData.baroAlt![f] = 44330 * (1 - Math.pow(logData.baroHpa![f] / 1013.25, 1 / 5.255))
			logData.baroUpVel![f] = (logData.baroAlt![f] - logData.baroAlt![Math.max(0, f - fOffset)]) * 50 // TODO remove
			logData.baroUpAccel![f] = (logData.baroUpVel![f] - logData.baroUpVel![Math.max(0, f - fOffset)]) * 50 // TODO remove
		})
	}
	if (flags.includes("LOG_DEBUG_1")) {
		const o = offsets["LOG_DEBUG_1"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.debug1![f] = leBytesToInt(frameData, p, 4, true)
		})
	}
	if (flags.includes("LOG_DEBUG_2")) {
		const o = offsets["LOG_DEBUG_2"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.debug2![f] = leBytesToInt(frameData, p, 4, true)
		})
	}
	if (flags.includes("LOG_DEBUG_3")) {
		const o = offsets["LOG_DEBUG_3"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.debug3![f] = leBytesToInt(frameData, p, 2, true)
		})
	}
	if (flags.includes("LOG_DEBUG_4")) {
		const o = offsets["LOG_DEBUG_4"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.debug4![f] = leBytesToInt(frameData, p, 2, true)
		})
	}
	if (flags.includes("LOG_PID_SUM")) {
		const o = offsets["LOG_PID_SUM"]
		frameNumbers.forEach((f, i) => {
			const p = i * frameSize + o
			logData.pidSumRoll![f] = leBytesToInt(frameData, p, 2, true)
			logData.pidSumPitch![f] = leBytesToInt(frameData, p + 2, 2, true)
			logData.pidSumYaw![f] = leBytesToInt(frameData, p + 4, 2, true)
		})
	}

	for (const f of frameNumbers) {
		frameLoadingStatus[f] |= 0b1
	}
}

export function parseElrs(
	logData: LogData,
	frameLoadingStatus: Uint8Array,
	elrsData: Uint8Array,
	elrsFromFrame: Uint32Array,
	elrsToFrame: Uint32Array
) {
	for (let i = 0; i < elrsFromFrame.length; i++) {
		let f = elrsFromFrame[i]
		let d = leBytesToBigInt(elrsData, i * 6, 6, false)
		const r = Number(d & 0xfffn)
		d >>= 12n
		const p = Number(d & 0xfffn)
		d >>= 12n
		const t = Number(d & 0xfffn)
		d >>= 12n
		const y = Number(d & 0xfffn)

		for (; f <= elrsToFrame[i]; f++) {
			logData.elrsRoll![f] = r
			logData.elrsPitch![f] = p
			logData.elrsThrottle![f] = t
			logData.elrsYaw![f] = y
			frameLoadingStatus[f] |= 0b10
		}
	}
}

export function parseVbat(
	logData: LogData,
	frameLoadingStatus: Uint8Array,
	vData: Uint8Array,
	vFromFrame: Uint32Array,
	vToFrame: Uint32Array
) {
	for (let i = 0; i < vFromFrame.length; i++) {
		let f = vFromFrame[i]
		const v = leBytesToInt(vData, i * 2, 2, false) / 100

		for (; f <= vToFrame[i]; f++) {
			logData.vbat![f] = v
			frameLoadingStatus[f] |= 0b1000
		}
	}
}

export function parseLinkStats(
	logData: LogData,
	frameLoadingStatus: Uint8Array,
	lData: Uint8Array,
	lFromFrame: Uint32Array,
	lToFrame: Uint32Array
) {
	for (let i = 0; i < lFromFrame.length; i++) {
		let f = lFromFrame[i]
		let d = lData.slice(i * 11, i * 11 + 11)
		const linkRssiA = -leBytesToInt(d, 0, 1)
		const linkRssiB = -leBytesToInt(d, 1, 1)
		const linkLqi = leBytesToInt(d, 2, 1)
		const linkSnr = leBytesToInt(d, 3, 1, true)
		const linkAntennaSel = leBytesToInt(d, 4, 1)
		const linkTargetHz = leBytesToInt(d, 5, 2)
		const linkActualHz = leBytesToInt(d, 7, 2)
		const linkTxPow = leBytesToInt(d, 9, 2)

		for (; f <= lToFrame[i]; f++) {
			logData.linkRssiA![f] = linkRssiA
			logData.linkRssiB![f] = linkRssiB
			logData.linkLqi![f] = linkLqi
			logData.linkSnr![f] = linkSnr
			logData.linkAntennaSel![f] = linkAntennaSel
			logData.linkTargetHz![f] = linkTargetHz
			logData.linkActualHz![f] = linkActualHz
			logData.linkTxPow![f] = linkTxPow
			frameLoadingStatus[f] |= 0b10000
		}
	}
}

export function parseGps(
	logData: LogData,
	frameLoadingStatus: Uint8Array,
	gpsData: Uint8Array,
	gpsFromFrame: Uint32Array,
	gpsToFrame: Uint32Array
) {
	for (let i = 0; i < gpsFromFrame.length; i++) {
		let f = gpsFromFrame[i]
		let d = gpsData.slice(i * 92, i * 92 + 92)
		const gpsYear = leBytesToInt(d, 4, 2)
		const gpsMonth = d[6]
		const gpsDay = d[7]
		const gpsHour = d[8]
		const gpsMinute = d[9]
		const gpsSecond = d[10]
		const gpsTimeValidityFlags = d[11]
		const gpsTAcc = leBytesToInt(d, 12, 4)
		const gpsNs = leBytesToInt(d, 16, 4, true)
		const gpsFixType = d[20]
		const gpsFlags = d[21]
		const gpsFlags2 = d[22]
		const gpsSatCount = d[23]
		const gpsLon = leBytesToInt(d, 24, 4, true) / 10000000
		const gpsLat = leBytesToInt(d, 28, 4, true) / 10000000
		const gpsAlt = leBytesToInt(d, 36, 4, true) / 1000
		const gpsHAcc = leBytesToInt(d, 40, 4) / 1000
		const gpsVAcc = leBytesToInt(d, 44, 4) / 1000
		const gpsVelN = leBytesToInt(d, 48, 4, true) / 1000
		const gpsVelE = leBytesToInt(d, 52, 4, true) / 1000
		const gpsVelD = leBytesToInt(d, 56, 4, true) / 1000
		const gpsGSpeed = leBytesToInt(d, 60, 4, true) / 1000
		const gpsHeadMot = leBytesToInt(d, 64, 4, true) / 100000
		const gpsSAcc = leBytesToInt(d, 68, 4) / 1000
		const gpsHeadAcc = leBytesToInt(d, 72, 4) / 100000
		const gpsPDop = leBytesToInt(d, 76, 2) / 100
		const gpsFlags3 = leBytesToInt(d, 78, 2)
		for (; f <= gpsToFrame[i]; f++) {
			logData.gpsYear![f] = gpsYear
			logData.gpsMonth![f] = gpsMonth
			logData.gpsDay![f] = gpsDay
			logData.gpsHour![f] = gpsHour
			logData.gpsMinute![f] = gpsMinute
			logData.gpsSecond![f] = gpsSecond
			logData.gpsTimeValidityFlags![f] = gpsTimeValidityFlags
			logData.gpsTAcc![f] = gpsTAcc
			logData.gpsNs![f] = gpsNs
			logData.gpsFixType![f] = gpsFixType
			logData.gpsFlags![f] = gpsFlags
			logData.gpsFlags2![f] = gpsFlags2
			logData.gpsSatCount![f] = gpsSatCount
			logData.gpsLon![f] = gpsLon
			logData.gpsLat![f] = gpsLat
			logData.gpsAlt![f] = gpsAlt
			logData.gpsHAcc![f] = gpsHAcc
			logData.gpsVAcc![f] = gpsVAcc
			logData.gpsVelN![f] = gpsVelN
			logData.gpsVelE![f] = gpsVelE
			logData.gpsVelD![f] = gpsVelD
			logData.gpsGSpeed![f] = gpsGSpeed
			logData.gpsHeadMot![f] = gpsHeadMot
			logData.gpsSAcc![f] = gpsSAcc
			logData.gpsHeadAcc![f] = gpsHeadAcc
			logData.gpsPDop![f] = gpsPDop
			logData.gpsFlags3![f] = gpsFlags3
			frameLoadingStatus[f] |= 0b100
		}
	}
}
