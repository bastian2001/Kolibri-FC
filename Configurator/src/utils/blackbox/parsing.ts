import { ActualCoeffs, BBLog, LogData } from "@utils/types"
import { leBytesToBigInt, leBytesToInt } from "@utils/utils"
import { BB_ALL_FLAGS } from "@utils/blackbox/bbFlags"

const ACC_RANGES = [2, 4, 8, 16]
const GYRO_RANGES = [2000, 1000, 500, 250, 125]

export function parseBlackbox(binFile: Uint8Array): BBLog | string {
	const header = binFile.slice(0, 256)
	const data = binFile.slice(256)
	const magic = leBytesToBigInt(header.slice(0, 8))
	if (magic !== 0x0001494c4f4bdfdcn) {
		return magic.toString(16)
	}
	const version = header.slice(8, 11)
	const startTime = new Date(leBytesToInt(header.slice(11, 15)) * 1000)
	const duration = leBytesToInt(header.slice(15, 19)) / 1000 // in seconds
	const pidFreq = [3200][header[19]]
	const freqDiv = header[20]
	const rangeByte = header[21]
	const ranges = {
		gyro: GYRO_RANGES[(rangeByte >> 2) & 0b111],
		accel: ACC_RANGES[rangeByte & 0b11],
	}
	const rateCoeffs: ActualCoeffs[] = []
	const rfBytes = header.slice(22, 58)
	for (let i = 0; i < 3; i++) {
		rateCoeffs[i] = {
			center: leBytesToInt(rfBytes.slice(i * 12, i * 12 + 4)) / 65536,
			max: leBytesToInt(rfBytes.slice(i * 12 + 4, i * 12 + 8)) / 65536,
			expo: leBytesToInt(rfBytes.slice(i * 12 + 8, i * 12 + 12)) / 65536,
		}
	}
	const pidConstants: number[][] = [[], [], []]
	const pidConstantsNice: number[][] = [[], [], []]
	const pcBytes = header.slice(82, 142)
	for (let i = 0; i < 3; i++) {
		pidConstants[i][0] = leBytesToInt(pcBytes.slice(i * 20, i * 20 + 4))
		pidConstantsNice[i][0] = pidConstants[i][0] >> 11
		pidConstants[i][0] /= 65536
		pidConstants[i][1] = leBytesToInt(pcBytes.slice(i * 20 + 4, i * 20 + 8))
		pidConstantsNice[i][1] = pidConstants[i][1] >> 3
		pidConstants[i][1] /= 65536
		pidConstants[i][2] = leBytesToInt(pcBytes.slice(i * 20 + 8, i * 20 + 12))
		pidConstantsNice[i][2] = pidConstants[i][2] >> 16
		pidConstants[i][2] /= 65536
		pidConstants[i][3] = leBytesToInt(pcBytes.slice(i * 20 + 12, i * 20 + 16))
		pidConstantsNice[i][3] = pidConstants[i][3] >> 8
		pidConstants[i][3] /= 65536
		pidConstants[i][4] = leBytesToInt(pcBytes.slice(i * 20 + 16, i * 20 + 20))
		pidConstantsNice[i][4] = pidConstants[i][4] >> 8
		pidConstants[i][4] /= 65536
	}
	const flags: string[] = []
	const flagSlice = header.slice(142, 150)
	let frameSize = 0
	const offsets: { [key: string]: number } = {}
	for (let j = 0; j < 64; j++) {
		const byteNum = Math.floor(j / 8)
		const bitNum = j % 8
		const flagIsSet = flagSlice[byteNum] & (1 << bitNum)
		if (!flagIsSet || !Object.keys(BB_ALL_FLAGS)[j]) continue
		flags.push(Object.keys(BB_ALL_FLAGS)[j])
		offsets[Object.keys(BB_ALL_FLAGS)[j]] = frameSize
		switch (j) {
			case 23:
			case 31:
			case 32:
			case 33:
			case 44:
				frameSize += 6
				break
			case 38:
			case 40:
			case 41:
				frameSize += 4
				break
			case 39:
				frameSize += 3
				break
			case 0: // ELRS_RAW
			case 27: // GPS
				break
			default:
				frameSize += 2
				break
		}
	}
	let pos = 0
	let frameCount = 0
	let framePos: number[] = []
	let flightModes: { fm: number; frame: number }[] = []
	let highlights: number[] = []
	let gpsPos: { pos: number; frame: number }[] = []
	let elrsPos: { pos: number; frame: number }[] = []
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
				gpsPos.push({ pos, frame: frameCount })
				pos += 93
				break
			case 4: // ELRS
				elrsPos.push({ pos, frame: frameCount })
				pos += 7
				break
			default:
				pos++
				break
		}
	}
	const motorPoles = header[150]
	const framesPerSecond = pidFreq / freqDiv
	const logData: LogData = {}
	if (flags.includes("LOG_ELRS_RAW")) {
		logData.elrsRoll = new Uint16Array(frameCount)
		logData.elrsPitch = new Uint16Array(frameCount)
		logData.elrsThrottle = new Uint16Array(frameCount)
		logData.elrsYaw = new Uint16Array(frameCount)
		const a: boolean[] = Array(frameCount).fill(false)
		for (const e of elrsPos) {
			let d = leBytesToBigInt(data.slice(e.pos + 1, e.pos + 7), false)
			const f = e.frame
			logData.elrsRoll[f] = Number(d & 0xfffn)
			d >>= 12n
			logData.elrsPitch[f] = Number(d & 0xfffn)
			d >>= 12n
			logData.elrsThrottle[f] = Number(d & 0xfffn)
			d >>= 12n
			logData.elrsYaw[f] = Number(d & 0xfffn)
			a[f] = true
		}
		for (let i = 1; i < frameCount; i++) {
			if (!a[i]) {
				logData.elrsRoll[i] = logData.elrsRoll[i - 1]
				logData.elrsPitch[i] = logData.elrsPitch[i - 1]
				logData.elrsThrottle[i] = logData.elrsThrottle[i - 1]
				logData.elrsYaw[i] = logData.elrsYaw[i - 1]
			}
		}
	}
	if (flags.includes("LOG_ROLL_SETPOINT")) {
		logData.setpointRoll = new Float32Array(frameCount)
		const o = offsets["LOG_ROLL_SETPOINT"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.setpointRoll[f] = leBytesToInt(data.slice(p, p + 2), true) / 16 // data is 12.4 fixed point
		}
	}
	if (flags.includes("LOG_PITCH_SETPOINT")) {
		logData.setpointPitch = new Float32Array(frameCount)
		const o = offsets["LOG_PITCH_SETPOINT"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.setpointPitch[f] = leBytesToInt(data.slice(p, p + 2), true) / 16 // data is 12.4 fixed point
		}
	}
	if (flags.includes("LOG_THROTTLE_SETPOINT")) {
		logData.setpointThrottle = new Float32Array(frameCount)
		const o = offsets["LOG_THROTTLE_SETPOINT"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.setpointThrottle[f] = leBytesToInt(data.slice(p, p + 2)) / 32 + 1000 // data is 12.4 fixed point
		}
	}
	if (flags.includes("LOG_YAW_SETPOINT")) {
		logData.setpointYaw = new Float32Array(frameCount)
		const o = offsets["LOG_YAW_SETPOINT"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.setpointYaw[f] = leBytesToInt(data.slice(p, p + 2), true) / 16 // data is 12.4 fixed point
		}
	}
	if (flags.includes("LOG_ROLL_GYRO_RAW")) {
		logData.gyroRawRoll = new Float32Array(frameCount)
		const o = offsets["LOG_ROLL_GYRO_RAW"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.gyroRawRoll[f] = leBytesToInt(data.slice(p, p + 2), true) / 16 // data is 12.4 fixed point
		}
	}
	if (flags.includes("LOG_PITCH_GYRO_RAW")) {
		logData.gyroRawPitch = new Float32Array(frameCount)
		const o = offsets["LOG_PITCH_GYRO_RAW"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.gyroRawPitch[f] = leBytesToInt(data.slice(p, p + 2), true) / 16 // data is 12.4 fixed point
		}
	}
	if (flags.includes("LOG_YAW_GYRO_RAW")) {
		logData.gyroRawYaw = new Float32Array(frameCount)
		const o = offsets["LOG_YAW_GYRO_RAW"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.gyroRawYaw[f] = leBytesToInt(data.slice(p, p + 2), true) / 16 // data is 12.4 fixed point
		}
	}
	if (flags.includes("LOG_ROLL_PID_P")) {
		logData.pidRollP = new Float32Array(frameCount)
		const o = offsets["LOG_ROLL_PID_P"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.pidRollP[f] = leBytesToInt(data.slice(p, p + 2), true)
		}
	}
	if (flags.includes("LOG_ROLL_PID_I")) {
		logData.pidRollI = new Float32Array(frameCount)
		const o = offsets["LOG_ROLL_PID_I"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.pidRollI[f] = leBytesToInt(data.slice(p, p + 2), true)
		}
	}
	if (flags.includes("LOG_ROLL_PID_D")) {
		logData.pidRollD = new Float32Array(frameCount)
		const o = offsets["LOG_ROLL_PID_D"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.pidRollD[f] = leBytesToInt(data.slice(p, p + 2), true)
		}
	}
	if (flags.includes("LOG_ROLL_PID_FF")) {
		logData.pidRollFF = new Float32Array(frameCount)
		const o = offsets["LOG_ROLL_PID_FF"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.pidRollFF[f] = leBytesToInt(data.slice(p, p + 2), true)
		}
	}
	if (flags.includes("LOG_ROLL_PID_S")) {
		logData.pidRollS = new Float32Array(frameCount)
		const o = offsets["LOG_ROLL_PID_S"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.pidRollS[f] = leBytesToInt(data.slice(p, p + 2), true)
		}
	}
	if (flags.includes("LOG_PITCH_PID_P")) {
		logData.pidPitchP = new Float32Array(frameCount)
		const o = offsets["LOG_PITCH_PID_P"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.pidPitchP[f] = leBytesToInt(data.slice(p, p + 2), true)
		}
	}
	if (flags.includes("LOG_PITCH_PID_I")) {
		logData.pidPitchI = new Float32Array(frameCount)
		const o = offsets["LOG_PITCH_PID_I"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.pidPitchI[f] = leBytesToInt(data.slice(p, p + 2), true)
		}
	}
	if (flags.includes("LOG_PITCH_PID_D")) {
		logData.pidPitchD = new Float32Array(frameCount)
		const o = offsets["LOG_PITCH_PID_D"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.pidPitchD[f] = leBytesToInt(data.slice(p, p + 2), true)
		}
	}
	if (flags.includes("LOG_PITCH_PID_FF")) {
		logData.pidPitchFF = new Float32Array(frameCount)
		const o = offsets["LOG_PITCH_PID_FF"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.pidPitchFF[f] = leBytesToInt(data.slice(p, p + 2), true)
		}
	}
	if (flags.includes("LOG_PITCH_PID_S")) {
		logData.pidPitchS = new Float32Array(frameCount)
		const o = offsets["LOG_PITCH_PID_S"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.pidPitchS[f] = leBytesToInt(data.slice(p, p + 2), true)
		}
	}
	if (flags.includes("LOG_YAW_PID_P")) {
		logData.pidYawP = new Float32Array(frameCount)
		const o = offsets["LOG_YAW_PID_P"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.pidYawP[f] = leBytesToInt(data.slice(p, p + 2), true)
		}
	}
	if (flags.includes("LOG_YAW_PID_I")) {
		logData.pidYawI = new Float32Array(frameCount)
		const o = offsets["LOG_YAW_PID_I"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.pidYawI[f] = leBytesToInt(data.slice(p, p + 2), true)
		}
	}
	if (flags.includes("LOG_YAW_PID_D")) {
		logData.pidYawD = new Float32Array(frameCount)
		const o = offsets["LOG_YAW_PID_D"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.pidYawD[f] = leBytesToInt(data.slice(p, p + 2), true)
		}
	}
	if (flags.includes("LOG_YAW_PID_FF")) {
		logData.pidYawFF = new Float32Array(frameCount)
		const o = offsets["LOG_YAW_PID_FF"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.pidYawFF[f] = leBytesToInt(data.slice(p, p + 2), true)
		}
	}
	if (flags.includes("LOG_YAW_PID_S")) {
		logData.pidYawS = new Float32Array(frameCount)
		const o = offsets["LOG_YAW_PID_S"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.pidYawS[f] = leBytesToInt(data.slice(p, p + 2), true)
		}
	}
	if (flags.includes("LOG_MOTOR_OUTPUTS")) {
		logData.motorOutRR = new Uint16Array(frameCount)
		logData.motorOutFR = new Uint16Array(frameCount)
		logData.motorOutRL = new Uint16Array(frameCount)
		logData.motorOutFL = new Uint16Array(frameCount)
		const o = offsets["LOG_MOTOR_OUTPUTS"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			const motors = leBytesToBigInt(data.slice(p, p + 6))
			logData.motorOutRR[f] = Number(motors & 0xfffn)
			logData.motorOutFR[f] = Number((motors >> 12n) & 0xfffn)
			logData.motorOutRL[f] = Number((motors >> 24n) & 0xfffn)
			logData.motorOutFL[f] = Number((motors >> 36n) & 0xfffn)
		}
	}
	if (flags.includes("LOG_FRAMETIME")) {
		logData.frametime = new Uint16Array(frameCount)
		logData.timestamp = new Uint32Array(frameCount)
		const o = offsets["LOG_FRAMETIME"]
		let t = 0
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.frametime[f] = leBytesToInt(data.slice(p, p + 2))
			t += logData.frametime[f]
			logData.timestamp[f] = t
		}
	}
	if (flags.includes("LOG_ALTITUDE")) {
		logData.altitude = new Float32Array(frameCount)
		const o = offsets["LOG_ALTITUDE"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.altitude[f] = leBytesToInt(data.slice(p, p + 2), true) / 64
		}
	}
	if (flags.includes("LOG_VVEL")) {
		logData.vvel = new Float32Array(frameCount)
		const o = offsets["LOG_VVEL"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.vvel[f] = leBytesToInt(data.slice(p, p + 2), true) / 256
		}
	}
	if (flags.includes("LOG_GPS")) {
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
		const gpsData = new Uint8Array(92)
		const exclude: number[] = []
		for (const g of gpsPos) {
			gpsData.set(data.slice(g.pos + 1, g.pos + 93), 0)
			const f = g.frame
			exclude.push(f)
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
		}
		for (let i = 1; i < frameCount; i++) {
			if (exclude.includes(i)) continue
			logData.gpsYear[i] = logData.gpsYear[i - 1]
			logData.gpsMonth[i] = logData.gpsMonth[i - 1]
			logData.gpsDay[i] = logData.gpsDay[i - 1]
			logData.gpsHour[i] = logData.gpsHour[i - 1]
			logData.gpsMinute[i] = logData.gpsMinute[i - 1]
			logData.gpsSecond[i] = logData.gpsSecond[i - 1]
			logData.gpsTimeValidityFlags[i] = logData.gpsTimeValidityFlags[i - 1]
			logData.gpsTAcc[i] = logData.gpsTAcc[i - 1]
			logData.gpsNs[i] = logData.gpsNs[i - 1]
			logData.gpsFixType[i] = logData.gpsFixType[i - 1]
			logData.gpsFlags[i] = logData.gpsFlags[i - 1]
			logData.gpsFlags2[i] = logData.gpsFlags2[i - 1]
			logData.gpsSatCount[i] = logData.gpsSatCount[i - 1]
			logData.gpsLon[i] = logData.gpsLon[i - 1]
			logData.gpsLat[i] = logData.gpsLat[i - 1]
			logData.gpsAlt[i] = logData.gpsAlt[i - 1]
			logData.gpsHAcc[i] = logData.gpsHAcc[i - 1]
			logData.gpsVAcc[i] = logData.gpsVAcc[i - 1]
			logData.gpsVelN[i] = logData.gpsVelN[i - 1]
			logData.gpsVelE[i] = logData.gpsVelE[i - 1]
			logData.gpsVelD[i] = logData.gpsVelD[i - 1]
			logData.gpsGSpeed[i] = logData.gpsGSpeed[i - 1]
			logData.gpsHeadMot[i] = logData.gpsHeadMot[i - 1]
			logData.gpsSAcc[i] = logData.gpsSAcc[i - 1]
			logData.gpsHeadAcc[i] = logData.gpsHeadAcc[i - 1]
			logData.gpsPDop[i] = logData.gpsPDop[i - 1]
			logData.gpsFlags3[i] = logData.gpsFlags3[i - 1]
		}
	}
	if (flags.includes("LOG_ATT_ROLL")) {
		logData.rollAngle = new Float32Array(frameCount)
		const o = offsets["LOG_ATT_ROLL"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.rollAngle[f] = ((leBytesToInt(data.slice(p, p + 2), true) / 10000) * 180) / Math.PI
		}
	}
	if (flags.includes("LOG_ATT_PITCH")) {
		logData.pitchAngle = new Float32Array(frameCount)
		const o = offsets["LOG_ATT_PITCH"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.pitchAngle[f] = ((leBytesToInt(data.slice(p, p + 2), true) / 10000) * 180) / Math.PI
		}
	}
	if (flags.includes("LOG_ATT_YAW")) {
		logData.yawAngle = new Float32Array(frameCount)
		const o = offsets["LOG_ATT_YAW"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.yawAngle[f] = ((leBytesToInt(data.slice(p, p + 2), true) / 10000) * 180) / Math.PI
		}
	}
	if (flags.includes("LOG_MOTOR_RPM")) {
		logData.rpmRR = new Float32Array(frameCount)
		logData.rpmFR = new Float32Array(frameCount)
		logData.rpmRL = new Float32Array(frameCount)
		logData.rpmFL = new Float32Array(frameCount)
		const o = offsets["LOG_MOTOR_RPM"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			const motors = leBytesToBigInt(data.slice(p, p + 6))
			let rr = Number(motors & 0xfffn)
			let fr = Number((motors >> 12n) & 0xfffn)
			let rl = Number((motors >> 24n) & 0xfffn)
			let fl = Number((motors >> 36n) & 0xfffn)
			if (rr === 0xfff) {
				logData.rpmRR[f] = 0
			} else {
				rr = (rr & 0x1ff) << (rr >> 9)
				logData.rpmRR[f] = (60000000 + 50 * rr) / rr / (motorPoles / 2)
			}
			if (fr === 0xfff) {
				logData.rpmFR[f] = 0
			} else {
				fr = (fr & 0x1ff) << (fr >> 9)
				logData.rpmFR[f] = (60000000 + 50 * fr) / fr / (motorPoles / 2)
			}
			if (rl === 0xfff) {
				logData.rpmRL[f] = 0
			} else {
				rl = (rl & 0x1ff) << (rl >> 9)
				logData.rpmRL[f] = (60000000 + 50 * rl) / rl / (motorPoles / 2)
			}
			if (fl === 0xfff) {
				logData.rpmFL[f] = 0
			} else {
				fl = (fl & 0x1ff) << (fl >> 9)
				logData.rpmFL[f] = (60000000 + 50 * fl) / fl / (motorPoles / 2)
			}
		}
	}
	if (flags.includes("LOG_ACCEL_RAW")) {
		logData.accelRawX = new Float32Array(frameCount)
		logData.accelRawY = new Float32Array(frameCount)
		logData.accelRawZ = new Float32Array(frameCount)
		const o = offsets["LOG_ACCEL_RAW"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.accelRawX[f] = (leBytesToInt(data.slice(p, p + 2), true) * 9.81) / 2048
			logData.accelRawY[f] = (leBytesToInt(data.slice(p + 2, p + 4), true) * 9.81) / 2048
			logData.accelRawZ[f] = (leBytesToInt(data.slice(p + 4, p + 6), true) * 9.81) / 2048
		}
	}
	if (flags.includes("LOG_ACCEL_FILTERED")) {
		logData.accelFilteredX = new Float32Array(frameCount)
		logData.accelFilteredY = new Float32Array(frameCount)
		logData.accelFilteredZ = new Float32Array(frameCount)
		const o = offsets["LOG_ACCEL_FILTERED"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.accelFilteredX[f] = (leBytesToInt(data.slice(p, p + 2), true) * 9.81) / 2048
			logData.accelFilteredY[f] = (leBytesToInt(data.slice(p + 2, p + 4), true) * 9.81) / 2048
			logData.accelFilteredZ[f] = (leBytesToInt(data.slice(p + 4, p + 6), true) * 9.81) / 2048
		}
	}
	if (flags.includes("LOG_VERTICAL_ACCEL")) {
		logData.accelVertical = new Float32Array(frameCount)
		const o = offsets["LOG_VERTICAL_ACCEL"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.accelVertical[f] = leBytesToInt(data.slice(p, p + 2), true) / 128
		}
	}
	if (flags.includes("LOG_VVEL_SETPOINT")) {
		logData.setpointVvel = new Float32Array(frameCount)
		const o = offsets["LOG_VVEL_SETPOINT"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.setpointVvel[f] = leBytesToInt(data.slice(p, p + 2), true) / 4096
		}
	}
	if (flags.includes("LOG_MAG_HEADING")) {
		logData.magHeading = new Float32Array(frameCount)
		const o = offsets["LOG_MAG_HEADING"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.magHeading[f] = ((leBytesToInt(data.slice(p, p + 2), true) / 8192) * 180) / Math.PI
		}
	}
	if (flags.includes("LOG_COMBINED_HEADING")) {
		logData.combinedHeading = new Float32Array(frameCount)
		const o = offsets["LOG_COMBINED_HEADING"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.combinedHeading[f] = ((leBytesToInt(data.slice(p, p + 2), true) / 8192) * 180) / Math.PI
		}
	}
	if (flags.includes("LOG_HVEL")) {
		logData.hvelN = new Float32Array(frameCount)
		logData.hvelE = new Float32Array(frameCount)
		const o = offsets["LOG_HVEL"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.hvelN[f] = leBytesToInt(data.slice(p, p + 2), true) / 256
			logData.hvelE[f] = leBytesToInt(data.slice(p + 2, p + 4), true) / 256
		}
	}
	if (flags.includes("LOG_BARO")) {
		logData.baroRaw = new Uint32Array(frameCount)
		logData.baroHpa = new Float32Array(frameCount)
		logData.baroAlt = new Float32Array(frameCount)
		logData.baroUpVel = new Float32Array(frameCount) // TODO remove
		logData.baroUpAccel = new Float32Array(frameCount) // TODO remove
		const fOffset = framesPerSecond / 50
		const o = offsets["LOG_BARO"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.baroRaw[f] = leBytesToInt(data.slice(p, p + 3))
			logData.baroHpa[f] = logData.baroRaw[f] / 4096
			logData.baroAlt[f] = 44330 * (1 - Math.pow(logData.baroHpa[f] / 1013.25, 1 / 5.255))
			logData.baroUpVel[f] = (logData.baroAlt[f] - logData.baroAlt[Math.max(0, f - fOffset)]) * 50 // TODO remove
			logData.baroUpAccel[f] = (logData.baroUpVel[f] - logData.baroUpVel[Math.max(0, f - fOffset)]) * 50 // TODO remove
		}
	}
	if (flags.includes("LOG_DEBUG_1")) {
		logData.debug1 = new Int32Array(frameCount)
		const o = offsets["LOG_DEBUG_1"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.debug1[f] = leBytesToInt(data.slice(p, p + 4), true)
		}
	}
	if (flags.includes("LOG_DEBUG_2")) {
		logData.debug2 = new Int32Array(frameCount)
		const o = offsets["LOG_DEBUG_2"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.debug2[f] = leBytesToInt(data.slice(p, p + 4), true)
		}
	}
	if (flags.includes("LOG_DEBUG_3")) {
		logData.debug3 = new Int16Array(frameCount)
		const o = offsets["LOG_DEBUG_3"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.debug3[f] = leBytesToInt(data.slice(p, p + 2), true)
		}
	}
	if (flags.includes("LOG_DEBUG_4")) {
		logData.debug4 = new Int16Array(frameCount)
		const o = offsets["LOG_DEBUG_4"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.debug4[f] = leBytesToInt(data.slice(p, p + 2), true)
		}
	}
	if (flags.includes("LOG_PID_SUM")) {
		logData.pidSumRoll = new Int16Array(frameCount)
		logData.pidSumPitch = new Int16Array(frameCount)
		logData.pidSumYaw = new Int16Array(frameCount)
		const o = offsets["LOG_PID_SUM"]
		for (let f = 0; f < frameCount; f++) {
			const p = framePos[f] + o
			logData.pidSumRoll[f] = leBytesToInt(data.slice(p, p + 2), true)
			logData.pidSumPitch[f] = leBytesToInt(data.slice(p + 2, p + 4), true)
			logData.pidSumYaw[f] = leBytesToInt(data.slice(p + 4, p + 6), true)
		}
	}

	return {
		frameCount,
		flags,
		logData,
		version,
		startTime,
		ranges,
		pidFrequency: pidFreq,
		frequencyDivider: freqDiv,
		rateCoeffs,
		pidConstants,
		framesPerSecond,
		rawFile: binFile,
		isExact: true,
		pidConstantsNice,
		motorPoles,
		flightModes,
		highlights,
		duration,
	}
}
