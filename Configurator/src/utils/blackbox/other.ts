import { BBLog, LogData, TypedArray } from "@utils/types"
import { bigIntToLeBytes, intToLeBytes, uint8ArrayEquals } from "../utils"
import { escapeBlackbox } from "./parsing"
import { BB_ALL_FLAGS } from "./bbFlags"

export function skipValues(slice: LogData, everyNth: number): LogData {
	if (!everyNth) everyNth = 1
	const skipped: LogData = {}
	for (const key in slice) {
		// @ts-expect-error
		const input = slice[key] as TypedArray | number[]
		let output
		if (input instanceof Array) {
			output = input.filter((_, i) => i % everyNth === 0)
		} else if (input instanceof Uint8Array) {
			output = new Uint8Array(input.length / everyNth)
			for (let i = 0; i < output.length; i++) {
				output[i] = input[i * everyNth]
			}
		} else if (input instanceof Int8Array) {
			output = new Int8Array(input.length / everyNth)
			for (let i = 0; i < output.length; i++) {
				output[i] = input[i * everyNth]
			}
		} else if (input instanceof Uint16Array) {
			output = new Uint16Array(input.length / everyNth)
			for (let i = 0; i < output.length; i++) {
				output[i] = input[i * everyNth]
			}
		} else if (input instanceof Int16Array) {
			output = new Int16Array(input.length / everyNth)
			for (let i = 0; i < output.length; i++) {
				output[i] = input[i * everyNth]
			}
		} else if (input instanceof Uint32Array) {
			output = new Uint32Array(input.length / everyNth)
			for (let i = 0; i < output.length; i++) {
				output[i] = input[i * everyNth]
			}
		} else if (input instanceof Int32Array) {
			output = new Int32Array(input.length / everyNth)
			for (let i = 0; i < output.length; i++) {
				output[i] = input[i * everyNth]
			}
		} else if (input instanceof Float32Array) {
			output = new Float32Array(input.length / everyNth)
			for (let i = 0; i < output.length; i++) {
				output[i] = input[i * everyNth]
			}
		} else if (input instanceof Float64Array) {
			output = new Float64Array(input.length / everyNth)
			for (let i = 0; i < output.length; i++) {
				output[i] = input[i * everyNth]
			}
		} else {
			output = input
		}
		// @ts-expect-error
		skipped[key] = output
	}
	return skipped
}

export function interpolateForExport(values: TypedArray, loaded: Uint8Array, bitmask: number) {
	const n = values.length
	if (n === 0) return values
	if ((loaded[0] & bitmask) !== bitmask) {
		return
	}
	if ((loaded[n - 1] & bitmask) !== bitmask) {
		return
	}

	let lastGoodIndex = 0

	// first pass: record positions of good values
	// second pass: fill gaps
	for (let i = 1; i < n; i++) {
		const isGood = (loaded[i] & bitmask) === bitmask

		if (isGood) {
			// if previous good is immediately before, nothing to fill
			if (i > lastGoodIndex + 1) {
				// interpolate from lastGoodIndex → i
				const startValue = values[lastGoodIndex]
				const endValue = values[i]
				const gap = i - lastGoodIndex

				for (let j = 1; j < gap; j++) {
					const t = j / gap
					values[lastGoodIndex + j] = startValue + (endValue - startValue) * t
				}
			}
			lastGoodIndex = i
		}
	}

	return values
}

export function generateFullBinFile(log: BBLog): Uint8Array {
	const { flags, offsets } = log
	const data = log.logData
	const fc = log.frameCount
	// max size of all frames, gps, etc
	let maxLength = (log.frameSize + 1 + 2 + 1 + 93 + 7 + 3 + 12) * fc
	// add syncs
	maxLength += 13 * (log.syncFrequency ? 13 * Math.floor(fc / log.syncFrequency) : 0)
	// escaping worst case
	maxLength *= 4 / 3
	// add header
	maxLength = Math.ceil(maxLength + 256)
	const file = new Uint8Array(maxLength)
	let lastGpsPacket = new Uint8Array(92)
	let lastElrsPacket = 0n
	let lastVbat = -1
	let lastSyncPos = 0
	let hl = false
	let fm = false

	flags.forEach(flag => {
		if (flag.startsWith("GEN_")) return
		const f = BB_ALL_FLAGS[flag]
		if (f.modifier) {
			f.modifier.forEach(m => {
				// @ts-expect-error
				interpolateForExport(data[m.path] as TypedArray, log.frameLoadingStatus, f.loadedBitmask)
			})
		} else {
			// @ts-expect-error
			interpolateForExport(data[f.path] as TypedArray, log.frameLoadingStatus, f.loadedBitmask)
		}
	})

	let pos = 0
	file.set(log.rawFile!)
	pos = 256

	for (let i = 0; i < fc; i++) {
		const fmIndex = log.flightModes.findIndex(fm => i === fm.frame)
		if (fmIndex !== -1) {
			fm = true
			file[pos++] = 1
			file[pos++] = log.flightModes[fmIndex].fm
		}
		if (log.highlights.indexOf(i) !== -1) {
			hl = true
			file[pos++] = 2
		}
		if (flags.indexOf("LOG_GPS") !== -1) {
			let gps = new Uint8Array(92)
			gps.set(intToLeBytes(data.gpsYear![i], 2), 4)
			gps[6] = data.gpsMonth![i]
			gps[7] = data.gpsDay![i]
			gps[8] = data.gpsHour![i]
			gps[9] = data.gpsMinute![i]
			gps[10] = data.gpsSecond![i]
			gps[11] = data.gpsTimeValidityFlags![i]
			gps.set(intToLeBytes(data.gpsTAcc![i], 4), 12)
			gps.set(intToLeBytes(data.gpsNs![i], 4), 16)
			gps[20] = data.gpsFixType![i]
			gps[21] = data.gpsFlags![i]
			gps[22] = data.gpsFlags2![i]
			gps[23] = data.gpsSatCount![i]
			gps.set(intToLeBytes(data.gpsLon![i] * 10000000, 4), 24)
			gps.set(intToLeBytes(data.gpsLat![i] * 10000000, 4), 28)
			gps.set(intToLeBytes(data.gpsAlt![i] * 1000, 4), 36)
			gps.set(intToLeBytes(data.gpsHAcc![i] * 1000, 4), 40)
			gps.set(intToLeBytes(data.gpsVAcc![i] * 1000, 4), 44)
			gps.set(intToLeBytes(data.gpsVelN![i] * 1000, 4), 48)
			gps.set(intToLeBytes(data.gpsVelE![i] * 1000, 4), 52)
			gps.set(intToLeBytes(data.gpsVelD![i] * 1000, 4), 56)
			gps.set(intToLeBytes(data.gpsGSpeed![i] * 1000, 4), 60)
			gps.set(intToLeBytes(data.gpsHeadMot![i] * 100000, 4), 64)
			gps.set(intToLeBytes(data.gpsSAcc![i] * 1000, 4), 68)
			gps.set(intToLeBytes(data.gpsHeadAcc![i] * 100000, 4), 72)
			gps.set(intToLeBytes(data.gpsPDop![i] * 100, 2), 76)
			gps.set(intToLeBytes(data.gpsFlags3![i], 2), 78)
			if (!uint8ArrayEquals(gps, lastGpsPacket)) {
				file[pos++] = 3
				lastGpsPacket = gps
				gps = new Uint8Array(escapeBlackbox(gps))
				file.set(gps, pos)
				pos += gps.length
			}
		}
		if (flags.indexOf("LOG_ELRS_RAW") !== -1) {
			let elrs = 0n
			elrs |= BigInt(data.elrsYaw![i]) & 0xfffn
			elrs <<= 12n
			elrs |= BigInt(data.elrsThrottle![i]) & 0xfffn
			elrs <<= 12n
			elrs |= BigInt(data.elrsPitch![i]) & 0xfffn
			elrs <<= 12n
			elrs |= BigInt(data.elrsRoll![i]) & 0xfffn
			if (elrs != lastElrsPacket) {
				lastElrsPacket = elrs
				file[pos++] = 4
				let buf = new Uint8Array(6)
				for (let j = 0; j < 6; j++) {
					const byte = Number((elrs >> (BigInt(j) * 8n)) & 0xffn)
					buf[j] = byte
				}
				buf = new Uint8Array(escapeBlackbox(buf))
				file.set(buf, pos)
				pos += buf.length
			}
		}

		if (flags.indexOf("LOG_VBAT") !== -1) {
			if (data.vbat![i] !== lastVbat) {
				lastVbat = data.vbat![i]
				file[pos++] = 5
				file.set(intToLeBytes(lastVbat * 100, 2), pos)
				pos += 2
			}
		}

		if (flags.indexOf("LOG_LINK_STATS") !== -1) {
			let ls = new Uint8Array(11)
			ls[0] = data.linkRssiA![i]
			ls[1] = data.linkRssiB![i]
			ls[2] = data.linkLqi![i]
			ls[3] = data.linkSnr![i]
			ls[4] = data.linkAntennaSel![i]
			ls.set(intToLeBytes(data.linkTargetHz![i], 2), 5)
			ls.set(intToLeBytes(data.linkActualHz![i], 2), 7)
			ls.set(intToLeBytes(data.linkTxPow![i], 2), 9)
			ls = new Uint8Array(escapeBlackbox(ls))
			file[pos++] = 6
			file.set(ls, pos)
			pos += ls.length
		}

		if (log.syncFrequency && i % log.syncFrequency === 0) {
			let s = new Uint8Array(9)
			s[0] = (hl ? 1 : 0) + (fm ? 2 : 0)
			s.set(intToLeBytes(i, 4), 1)
			s.set(intToLeBytes(lastSyncPos, 4), 1)
			hl = false
			fm = false
			lastSyncPos = pos
			file[pos++] = "S".charCodeAt(0)
			file[pos++] = "Y".charCodeAt(0)
			file[pos++] = "N".charCodeAt(0)
			file[pos++] = "C".charCodeAt(0)
			s = new Uint8Array(escapeBlackbox(s))
			file.set(s, pos)
			pos += s.length
		}

		let f = new Uint8Array(log.frameSize)
		file[pos++] = 0
		if (flags.indexOf("LOG_ROLL_SETPOINT") !== -1) {
			const o = offsets["LOG_ROLL_SETPOINT"]
			f.set(intToLeBytes(data.setpointRoll![i] * 16, 2), o)
		}

		if (flags.indexOf("LOG_PITCH_SETPOINT") !== -1) {
			const o = offsets["LOG_PITCH_SETPOINT"]
			f.set(intToLeBytes(data.setpointPitch![i] * 16, 2), o)
		}

		if (flags.indexOf("LOG_THROTTLE_SETPOINT") !== -1) {
			const o = offsets["LOG_THROTTLE_SETPOINT"]
			f.set(intToLeBytes((data.setpointThrottle![i] - 1000) * 32, 2), o)
		}

		if (flags.indexOf("LOG_YAW_SETPOINT") !== -1) {
			const o = offsets["LOG_YAW_SETPOINT"]
			f.set(intToLeBytes(data.setpointYaw![i] * 16, 2), o)
		}

		if (flags.indexOf("LOG_ROLL_GYRO_RAW") !== -1) {
			const o = offsets["LOG_ROLL_GYRO_RAW"]
			f.set(intToLeBytes(data.gyroRawRoll![i] * 16, 2), o)
		}

		if (flags.indexOf("LOG_PITCH_GYRO_RAW") !== -1) {
			const o = offsets["LOG_PITCH_GYRO_RAW"]
			f.set(intToLeBytes(data.gyroRawPitch![i] * 16, 2), o)
		}

		if (flags.indexOf("LOG_YAW_GYRO_RAW") !== -1) {
			const o = offsets["LOG_YAW_GYRO_RAW"]
			f.set(intToLeBytes(data.gyroRawYaw![i] * 16, 2), o)
		}

		if (flags.indexOf("LOG_ROLL_PID_P") !== -1) {
			const o = offsets["LOG_ROLL_PID_P"]
			f.set(intToLeBytes(data.pidRollP![i], 2), o)
		}

		if (flags.indexOf("LOG_ROLL_PID_I") !== -1) {
			const o = offsets["LOG_ROLL_PID_I"]
			f.set(intToLeBytes(data.pidRollI![i], 2), o)
		}

		if (flags.indexOf("LOG_ROLL_PID_D") !== -1) {
			const o = offsets["LOG_ROLL_PID_D"]
			f.set(intToLeBytes(data.pidRollD![i], 2), o)
		}

		if (flags.indexOf("LOG_ROLL_PID_FF") !== -1) {
			const o = offsets["LOG_ROLL_PID_FF"]
			f.set(intToLeBytes(data.pidRollFF![i], 2), o)
		}

		if (flags.indexOf("LOG_ROLL_PID_S") !== -1) {
			const o = offsets["LOG_ROLL_PID_S"]
			f.set(intToLeBytes(data.pidRollS![i], 2), o)
		}

		if (flags.indexOf("LOG_PITCH_PID_P") !== -1) {
			const o = offsets["LOG_PITCH_PID_P"]
			f.set(intToLeBytes(data.pidPitchP![i], 2), o)
		}

		if (flags.indexOf("LOG_PITCH_PID_I") !== -1) {
			const o = offsets["LOG_PITCH_PID_I"]
			f.set(intToLeBytes(data.pidPitchI![i], 2), o)
		}

		if (flags.indexOf("LOG_PITCH_PID_D") !== -1) {
			const o = offsets["LOG_PITCH_PID_D"]
			f.set(intToLeBytes(data.pidPitchD![i], 2), o)
		}

		if (flags.indexOf("LOG_PITCH_PID_FF") !== -1) {
			const o = offsets["LOG_PITCH_PID_FF"]
			f.set(intToLeBytes(data.pidPitchFF![i], 2), o)
		}

		if (flags.indexOf("LOG_PITCH_PID_S") !== -1) {
			const o = offsets["LOG_PITCH_PID_S"]
			f.set(intToLeBytes(data.pidPitchS![i], 2), o)
		}

		if (flags.indexOf("LOG_YAW_PID_P") !== -1) {
			const o = offsets["LOG_YAW_PID_P"]
			f.set(intToLeBytes(data.pidYawP![i], 2), o)
		}

		if (flags.indexOf("LOG_YAW_PID_I") !== -1) {
			const o = offsets["LOG_YAW_PID_I"]
			f.set(intToLeBytes(data.pidYawI![i], 2), o)
		}

		if (flags.indexOf("LOG_YAW_PID_D") !== -1) {
			const o = offsets["LOG_YAW_PID_D"]
			f.set(intToLeBytes(data.pidYawD![i], 2), o)
		}

		if (flags.indexOf("LOG_YAW_PID_FF") !== -1) {
			const o = offsets["LOG_YAW_PID_FF"]
			f.set(intToLeBytes(data.pidYawFF![i], 2), o)
		}

		if (flags.indexOf("LOG_YAW_PID_S") !== -1) {
			const o = offsets["LOG_YAW_PID_S"]
			f.set(intToLeBytes(data.pidYawS![i], 2), o)
		}

		if (flags.indexOf("LOG_FRAMETIME") !== -1) {
			const o = offsets["LOG_FRAMETIME"]
			f.set(intToLeBytes(data.frametime![i], 2), o)
		}

		if (flags.indexOf("LOG_ALTITUDE") !== -1) {
			const o = offsets["LOG_ALTITUDE"]
			f.set(intToLeBytes(data.altitude![i] * 64, 2), o)
		}

		if (flags.indexOf("LOG_VVEL") !== -1) {
			const o = offsets["LOG_VVEL"]
			f.set(intToLeBytes(data.vvel![i] * 256, 2), o)
		}

		if (flags.indexOf("LOG_ATT_ROLL") !== -1) {
			const o = offsets["LOG_ATT_ROLL"]
			const rad = (data.rollAngle![i] * Math.PI) / 180
			f.set(intToLeBytes(rad * 10000, 2), o)
		}

		if (flags.indexOf("LOG_ATT_PITCH") !== -1) {
			const o = offsets["LOG_ATT_PITCH"]
			const rad = (data.pitchAngle![i] * Math.PI) / 180
			f.set(intToLeBytes(rad * 10000, 2), o)
		}

		if (flags.indexOf("LOG_ATT_YAW") !== -1) {
			const o = offsets["LOG_ATT_YAW"]
			const rad = (data.yawAngle![i] * Math.PI) / 180
			f.set(intToLeBytes(rad * 10000, 2), o)
		}

		if (flags.indexOf("LOG_ACCEL_RAW") !== -1) {
			const o = offsets["LOG_ACCEL_RAW"]
			f.set(intToLeBytes((data.accelRawX![i] * 2048) / 9.81, 2), o)
			f.set(intToLeBytes((data.accelRawY![i] * 2048) / 9.81, 2), o + 2)
			f.set(intToLeBytes((data.accelRawZ![i] * 2048) / 9.81, 2), o + 4)
		}

		if (flags.indexOf("LOG_ACCEL_FILTERED") !== -1) {
			const o = offsets["LOG_ACCEL_FILTERED"]
			f.set(intToLeBytes((data.accelFilteredX![i] * 2048) / 9.81, 2), o)
			f.set(intToLeBytes((data.accelFilteredY![i] * 2048) / 9.81, 2), o + 2)
			f.set(intToLeBytes((data.accelFilteredZ![i] * 2048) / 9.81, 2), o + 4)
		}

		if (flags.indexOf("LOG_VERTICAL_ACCEL") !== -1) {
			const o = offsets["LOG_VERTICAL_ACCEL"]
			f.set(intToLeBytes(data.accelVertical![i] * 128, 2), o)
		}

		if (flags.indexOf("LOG_VVEL_SETPOINT") !== -1) {
			const o = offsets["LOG_VVEL_SETPOINT"]
			f.set(intToLeBytes(data.setpointVvel![i] * 4096, 2), o)
		}

		if (flags.indexOf("LOG_MAG_HEADING") !== -1) {
			const o = offsets["LOG_MAG_HEADING"]
			const rad = (data.magHeading![i] * Math.PI) / 180
			f.set(intToLeBytes(rad * 8192, 2), o)
		}

		if (flags.indexOf("LOG_COMBINED_HEADING") !== -1) {
			const o = offsets["LOG_COMBINED_HEADING"]
			const rad = (data.combinedHeading![i] * Math.PI) / 180
			f.set(intToLeBytes(rad * 8192, 2), o)
		}

		if (flags.indexOf("LOG_HVEL") !== -1) {
			const o = offsets["LOG_HVEL"]
			f.set(intToLeBytes(data.hvelN![i] * 256, 2), o)
			f.set(intToLeBytes(data.hvelE![i] * 256, 2), o + 2)
		}

		if (flags.indexOf("LOG_BARO") !== -1) {
			const o = offsets["LOG_BARO"]
			f.set(intToLeBytes(data.baroRaw![i], 3), o)
		}

		if (flags.indexOf("LOG_DEBUG_1") !== -1) {
			const o = offsets["LOG_DEBUG_1"]
			f.set(intToLeBytes(data.debug1![i], 4), o)
		}

		if (flags.indexOf("LOG_DEBUG_2") !== -1) {
			const o = offsets["LOG_DEBUG_2"]
			f.set(intToLeBytes(data.debug2![i], 4), o)
		}

		if (flags.indexOf("LOG_DEBUG_3") !== -1) {
			const o = offsets["LOG_DEBUG_3"]
			f.set(intToLeBytes(data.debug3![i], 2), o)
		}

		if (flags.indexOf("LOG_DEBUG_4") !== -1) {
			const o = offsets["LOG_DEBUG_4"]
			f.set(intToLeBytes(data.debug4![i], 2), o)
		}

		if (flags.indexOf("LOG_PID_SUM") !== -1) {
			const o = offsets["LOG_PID_SUM"]
			f.set(intToLeBytes(data.pidSumRoll![i], 2), o)
			f.set(intToLeBytes(data.pidSumPitch![i], 2), o + 2)
			f.set(intToLeBytes(data.pidSumYaw![i], 2), o + 4)
		}

		if (flags.indexOf("LOG_MOTOR_OUTPUTS") !== -1) {
			const o = offsets["LOG_MOTOR_OUTPUTS"]
			let motors = 0n
			motors |= BigInt(data.motorOutFL![i])
			motors <<= 12n
			motors |= BigInt(data.motorOutRL![i])
			motors <<= 12n
			motors |= BigInt(data.motorOutFR![i])
			motors <<= 12n
			motors |= BigInt(data.motorOutRR![i])
			f.set(bigIntToLeBytes(motors, 6), o)
		}

		if (flags.indexOf("LOG_MOTOR_RPM") !== -1) {
			const o = offsets["LOG_MOTOR_RPM"]

			let rpm = 0n
			const k = log.motorPoles / 2

			const ar = [data.rpmFL![i], data.rpmRL![i], data.rpmFR![i], data.rpmRR![i]]

			for (let j = 0; j < 4; j++) {
				const r = ar[j]
				if (r === 0) {
					rpm <<= 12n
					rpm |= 0xfffn
					continue
				}

				// invert rpm -> x
				const denom = r * k - 50
				let x = denom > 0 ? 60000000 / denom : 1
				if (x < 1) x = 1
				let xi = Math.floor(x)

				// convert x -> mantissa + exponent
				let exponent = 0
				while (xi > 0x1ff && exponent < 7) {
					xi >>>= 1
					exponent++
				}
				const mantissa = xi & 0x1ff
				const field = BigInt(mantissa | (exponent << 9))
				rpm <<= 12n
				rpm |= field & 0xfffn
			}
			f.set(bigIntToLeBytes(rpm, 6), o)
		}

		f = new Uint8Array(escapeBlackbox(f))
		file.set(f, pos)
		pos += f.length
	}
	return file.slice(0, pos)
}
