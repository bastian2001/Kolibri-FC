import { BBLog } from "@utils/types"
import { BB_GEN_FLAGS } from "@utils/blackbox/bbFlags"
import { PT1 } from "@utils/filters"
import { getSetpointActual, map } from "@utils/utils"

export function fillLogWithGenFlags(log: BBLog) {
	log.isExact = true
	const genFlags = Object.keys(BB_GEN_FLAGS)
	const frameCount = log.frameCount
	for (let i = 0; i < genFlags.length; i++) {
		const flagName = genFlags[i]
		const flag = BB_GEN_FLAGS[flagName]
		if (log.flags.includes(flag.replaces)) continue
		if (
			flag.requires.every(r => {
				if (typeof r === "string") return log.flags.includes(r)
				if (Array.isArray(r)) {
					for (const s of r) if (log.flags.includes(s)) return true
					return false
				}
				return false
			})
		) {
			log.flags.push(flagName)
			//generate entries
			if (!flag.exact) log.isExact = false
			switch (flagName) {
				case "GEN_ROLL_SETPOINT":
					log.logData.setpointRoll = new Float32Array(frameCount)
					for (let i = 0; i < frameCount; i++) {
						const stick = (log.logData.elrsRoll![i] - 1500) / 512
						log.logData.setpointRoll[i] = getSetpointActual(stick, log.rateCoeffs[0])
					}
					break
				case "GEN_PITCH_SETPOINT":
					log.logData.setpointPitch = new Float32Array(frameCount)
					for (let i = 0; i < frameCount; i++) {
						const stick = (log.logData.elrsPitch![i] - 1500) / 512
						log.logData.setpointPitch[i] = getSetpointActual(stick, log.rateCoeffs[1])
					}
					break
				case "GEN_THROTTLE_SETPOINT":
					log.logData.setpointThrottle = new Float32Array(frameCount)
					for (let i = 0; i < frameCount; i++) {
						log.logData.setpointThrottle[i] = log.logData.elrsThrottle![i]
					}
					break
				case "GEN_YAW_SETPOINT":
					log.logData.setpointYaw = new Float32Array(frameCount)
					for (let i = 0; i < frameCount; i++) {
						const stick = (log.logData.elrsYaw![i] - 1500) / 512
						log.logData.setpointYaw[i] = getSetpointActual(stick, log.rateCoeffs[2])
					}
					break
				case "GEN_ROLL_PID_P":
					log.logData.pidRollP = new Float32Array(frameCount)
					for (let i = 0; i < frameCount; i++) {
						log.logData.pidRollP[i] =
							(log.logData.setpointRoll![i] - log.logData.gyroRawRoll![i]) * log.pidConstants[0][0]
					}
					break
				case "GEN_PITCH_PID_P":
					log.logData.pidPitchP = new Float32Array(frameCount)
					for (let i = 0; i < frameCount; i++) {
						log.logData.pidPitchP[i] =
							(log.logData.setpointPitch![i] - log.logData.gyroRawPitch![i]) * log.pidConstants[1][0]
					}
					break
				case "GEN_YAW_PID_P":
					log.logData.pidYawP = new Float32Array(frameCount)
					for (let i = 0; i < frameCount; i++) {
						log.logData.pidYawP[i] =
							(log.logData.setpointYaw![i] - log.logData.gyroRawYaw![i]) * log.pidConstants[2][0]
					}
					break
				case "GEN_ROLL_PID_D":
					log.logData.pidRollD = new Float32Array(frameCount)
					for (let i = 1; i < frameCount; i++) {
						log.logData.pidRollD[i] =
							((log.logData.gyroRawRoll![i - 1] - log.logData.gyroRawRoll![i]) * log.pidConstants[0][2]) /
							log.frequencyDivider
					}
					break
				case "GEN_PITCH_PID_D":
					log.logData.pidPitchD = new Float32Array(frameCount)
					for (let i = 1; i < frameCount; i++) {
						log.logData.pidPitchD[i] =
							((log.logData.gyroRawPitch![i - 1] - log.logData.gyroRawPitch![i]) *
								log.pidConstants[1][2]) /
							log.frequencyDivider
					}
					break
				case "GEN_YAW_PID_D":
					log.logData.pidYawD = new Float32Array(frameCount)
					for (let i = 1; i < frameCount; i++) {
						log.logData.pidYawD[i] =
							((log.logData.gyroRawYaw![i - 1] - log.logData.gyroRawYaw![i]) * log.pidConstants[2][2]) /
							log.frequencyDivider
					}
					break
				case "GEN_ROLL_PID_FF":
					{
						const filter = new PT1(12, log.framesPerSecond)
						log.logData.pidRollFF = new Float32Array(frameCount)
						for (let i = 1; i < frameCount; i++) {
							log.logData.pidRollFF[i] = filter.update(
								((log.logData.setpointRoll![i] - log.logData.setpointRoll![i - 1]) / 16) *
									log.framesPerSecond *
									log.pidConstants[0][3]
							)
						}
					}
					break
				case "GEN_PITCH_PID_FF":
					{
						const filter = new PT1(12, log.framesPerSecond)
						log.logData.pidPitchFF = new Float32Array(frameCount)
						for (let i = 1; i < frameCount; i++) {
							log.logData.pidPitchFF[i] = filter.update(
								((log.logData.setpointPitch![i] - log.logData.setpointPitch![i - 1]) / 16) *
									log.framesPerSecond *
									log.pidConstants[1][3]
							)
						}
					}
					break
				case "GEN_YAW_PID_FF":
					{
						const filter = new PT1(12, log.framesPerSecond)
						log.logData.pidYawFF = new Float32Array(frameCount)
						for (let i = 1; i < frameCount; i++) {
							log.logData.pidYawFF[i] = filter.update(
								((log.logData.setpointYaw![i] - log.logData.setpointYaw![i - 1]) / 16) *
									log.framesPerSecond *
									log.pidConstants[2][3]
							)
						}
					}
					break
				case "GEN_ROLL_PID_S":
					log.logData.pidRollS = new Float32Array(frameCount)
					for (let i = 0; i < frameCount; i++) {
						log.logData.pidRollS[i] = log.logData.setpointRoll![i] * log.pidConstants[0][4]
					}
					break
				case "GEN_PITCH_PID_S":
					log.logData.pidPitchS = new Float32Array(frameCount)
					for (let i = 0; i < frameCount; i++) {
						log.logData.pidPitchS[i] = log.logData.setpointPitch![i] * log.pidConstants[1][4]
					}
					break
				case "GEN_YAW_PID_S":
					log.logData.pidYawS = new Float32Array(frameCount)
					for (let i = 0; i < frameCount; i++) {
						log.logData.pidYawS[i] = log.logData.setpointYaw![i] * log.pidConstants[2][4]
					}
					break
				case "GEN_ROLL_PID_I":
					{
						let rollI = 0
						let takeoffCounter = 0
						log.logData.pidRollI = new Float32Array(frameCount)
						for (let i = 0; i < frameCount; i++) {
							rollI += log.logData.setpointRoll![i] - log.logData.gyroRawRoll![i]
							if (log.logData.setpointThrottle![i] > 1020) takeoffCounter += log.frequencyDivider
							else if (takeoffCounter < 1000) takeoffCounter = 0
							if (takeoffCounter < 1000) rollI *= Math.pow(log.pidConstants[0][6], log.frequencyDivider)
							log.logData.pidRollI[i] = rollI * log.pidConstants[0][1]
						}
					}
					break
				case "GEN_PITCH_PID_I":
					{
						let pitchI = 0
						let takeoffCounter = 0
						log.logData.pidPitchI = new Float32Array(frameCount)
						for (let i = 0; i < frameCount; i++) {
							pitchI += log.logData.setpointPitch![i] - log.logData.gyroRawPitch![i]
							if (log.logData.setpointThrottle![i] > 1020) takeoffCounter += log.frequencyDivider
							else if (takeoffCounter < 1000) takeoffCounter = 0
							if (takeoffCounter < 1000) pitchI *= Math.pow(log.pidConstants[1][6], log.frequencyDivider)
							log.logData.pidPitchI[i] = pitchI * log.pidConstants[1][1]
						}
					}
					break
				case "GEN_YAW_PID_I":
					{
						let yawI = 0
						let takeoffCounter = 0
						log.logData.pidYawI = new Float32Array(frameCount)
						for (let i = 0; i < frameCount; i++) {
							yawI += log.logData.setpointYaw![i] - log.logData.gyroRawYaw![i]
							if (log.logData.setpointThrottle![i] > 1020) takeoffCounter += log.frequencyDivider
							else if (takeoffCounter < 1000) takeoffCounter = 0
							if (takeoffCounter < 1000) yawI *= Math.pow(log.pidConstants[2][6], log.frequencyDivider)
							log.logData.pidYawI[i] = yawI * log.pidConstants[2][1]
						}
					}
					break
				case "GEN_MOTOR_OUTPUTS":
					log.logData.motorOutRR = new Uint16Array(frameCount)
					log.logData.motorOutFR = new Uint16Array(frameCount)
					log.logData.motorOutRL = new Uint16Array(frameCount)
					log.logData.motorOutFL = new Uint16Array(frameCount)
					for (let i = 0; i < frameCount; i++) {
						const rollTerm =
							log.logData.pidRollP![i] +
							log.logData.pidRollI![i] +
							log.logData.pidRollD![i] +
							log.logData.pidRollFF![i] +
							log.logData.pidRollS![i]
						const pitchTerm =
							log.logData.pidPitchP![i] +
							log.logData.pidPitchI![i] +
							log.logData.pidPitchD![i] +
							log.logData.pidPitchFF![i] +
							log.logData.pidPitchS![i]
						const yawTerm =
							log.logData.pidYawP![i] +
							log.logData.pidYawI![i] +
							log.logData.pidYawD![i] +
							log.logData.pidYawFF![i] +
							log.logData.pidYawS![i]
						log.logData.motorOutRR[i] =
							(log.logData.setpointThrottle![i] - 1000) * 2 - rollTerm + pitchTerm + yawTerm
						log.logData.motorOutFR[i] =
							(log.logData.setpointThrottle![i] - 1000) * 2 - rollTerm - pitchTerm - yawTerm
						log.logData.motorOutRL[i] =
							(log.logData.setpointThrottle![i] - 1000) * 2 + rollTerm + pitchTerm - yawTerm
						log.logData.motorOutFL[i] =
							(log.logData.setpointThrottle![i] - 1000) * 2 + rollTerm - pitchTerm + yawTerm
						log.logData.motorOutRR[i] = map(log.logData.motorOutRR[i], 0, 2000, 50, 2000)
						log.logData.motorOutFR[i] = map(log.logData.motorOutFR[i], 0, 2000, 50, 2000)
						log.logData.motorOutRL[i] = map(log.logData.motorOutRL[i], 0, 2000, 50, 2000)
						log.logData.motorOutFL[i] = map(log.logData.motorOutFL[i], 0, 2000, 50, 2000)
						if (log.logData.motorOutRR[i] > 2000) {
							const diff = 2000 - log.logData.motorOutRR[i]
							log.logData.motorOutRR[i] = 2000
							log.logData.motorOutFR[i] -= diff
							log.logData.motorOutRL[i] -= diff
							log.logData.motorOutFL[i] -= diff
						}
						if (log.logData.motorOutFR[i] > 2000) {
							const diff = 2000 - log.logData.motorOutFR[i]
							log.logData.motorOutFR[i] = 2000
							log.logData.motorOutRR[i] -= diff
							log.logData.motorOutRL[i] -= diff
							log.logData.motorOutFL[i] -= diff
						}
						if (log.logData.motorOutRL[i] > 2000) {
							const diff = 2000 - log.logData.motorOutRL[i]
							log.logData.motorOutRL[i] = 2000
							log.logData.motorOutRR[i] -= diff
							log.logData.motorOutFR[i] -= diff
							log.logData.motorOutFL[i] -= diff
						}
						if (log.logData.motorOutFL[i] > 2000) {
							const diff = 2000 - log.logData.motorOutFL[i]
							log.logData.motorOutFL[i] = 2000
							log.logData.motorOutRR[i] -= diff
							log.logData.motorOutFR[i] -= diff
							log.logData.motorOutRL[i] -= diff
						}
						if (log.logData.motorOutRR[i] < 50) {
							const diff = 50 - log.logData.motorOutRR[i]
							log.logData.motorOutRR[i] = 50
							log.logData.motorOutFR[i] += diff
							log.logData.motorOutRL[i] += diff
							log.logData.motorOutFL[i] += diff
						}
						if (log.logData.motorOutFR[i] < 50) {
							const diff = 50 - log.logData.motorOutFR[i]
							log.logData.motorOutFR[i] = 50
							log.logData.motorOutRR[i] += diff
							log.logData.motorOutRL[i] += diff
							log.logData.motorOutFL[i] += diff
						}
						if (log.logData.motorOutRL[i] < 50) {
							const diff = 50 - log.logData.motorOutRL[i]
							log.logData.motorOutRL[i] = 50
							log.logData.motorOutRR[i] += diff
							log.logData.motorOutFR[i] += diff
							log.logData.motorOutFL[i] += diff
						}
						if (log.logData.motorOutFL[i] < 50) {
							const diff = 50 - log.logData.motorOutFL[i]
							log.logData.motorOutFL[i] = 50
							log.logData.motorOutRR[i] += diff
							log.logData.motorOutFR[i] += diff
							log.logData.motorOutRL[i] += diff
						}
						log.logData.motorOutRR[i] = Math.min(log.logData.motorOutRR[i], 2000)
						log.logData.motorOutFR[i] = Math.min(log.logData.motorOutFR[i], 2000)
						log.logData.motorOutRL[i] = Math.min(log.logData.motorOutRL[i], 2000)
						log.logData.motorOutFL[i] = Math.min(log.logData.motorOutFL[i], 2000)
					}
					break
				case "GEN_VVEL_SETPOINT":
					log.logData.setpointVvel = new Float32Array(frameCount)
					for (let i = 0; i < frameCount; i++) {
						let t = (log.logData.elrsThrottle![i] - 1500) * 2
						if (t > 0) {
							t -= 100
							if (t < 0) t = 0
						} else if (t < 0) {
							t += 100
							if (t > 0) t = 0
						}
						log.logData.setpointVvel[i] = t / 180
						if (log.logData.flightMode !== undefined && log.logData.flightMode[i] < 2)
							log.logData.setpointVvel[i] = 0
					}
					break
			}
		}
	}
}
