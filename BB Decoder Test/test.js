const fs = require("fs").promises
const path = require("path")
//read file from variable fileName, and decode
const fileName = path.resolve(__dirname, "36.kbb.txt")

const logFlags = [
	["LOG_ROLL_ELRS_RAW", 2],
	["LOG_PITCH_ELRS_RAW", 2],
	["LOG_THROTTLE_ELRS_RAW", 2],
	["LOG_YAW_ELRS_RAW", 2],
	["LOG_ROLL_SETPOINT", 2],
	["LOG_PITCH_SETPOINT", 2],
	["LOG_THROTTLE_SETPOINT", 2],
	["LOG_YAW_SETPOINT", 2],
	["LOG_ROLL_GYRO_RAW", 2],
	["LOG_THROTTLE_GYRO_RAW", 2],
	["LOG_YAW_GYRO_RAW", 2],
	["LOG_ROLL_PID_P", 2],
	["LOG_ROLL_PID_I", 2],
	["LOG_ROLL_PID_D", 2],
	["LOG_ROLL_PID_FF", 2],
	["LOG_ROLL_PID_S", 2],
	["LOG_PITCH_PID_P", 2],
	["LOG_PITCH_PID_I", 2],
	["LOG_PITCH_PID_D", 2],
	["LOG_PITCH_PID_FF", 2],
	["LOG_PITCH_PID_S", 2],
	["LOG_YAW_PID_P", 2],
	["LOG_YAW_PID_I", 2],
	["LOG_YAW_PID_D", 2],
	["LOG_YAW_PID_FF", 2],
	["LOG_YAW_PID_S", 2],
	["LOG_MOTOR_OUTPUTS", 6],
	["LOG_ALTITUDE", 2],
]

function convert1616IntToFloat(int) {
	//converts a 16.16 fixed point number to a float
	return int / 65536
}

fs.readFile(fileName, { encoding: "ascii" })
	.then(data => {
		let buf = Buffer.from(data)
		buf = buf.subarray(23)
		//check magic number
		if (buf.toString("hex", 0, 4) == "2027a199") {
			console.log("Magic byte sequence correct")
		} else {
			console.log("Magic byte sequence incorrect", buf.toString("hex", 0, 4))
		}
		//check version
		const ver = buf.readUInt8(4) + "." + buf.readUInt8(5) + "." + buf.readUInt8(6)
		console.log("Version: " + ver)
		console.log("Start time: ", buf.readUInt32LE(7), "ms")
		console.log("PID loop frequency: ", 3200 / (1 << buf.readUInt8(11)), "Hz")
		console.log("Loop divider: ", buf.readUInt8(12))
		const gyrAccelRange = buf.readUInt8(13)
		console.log("Gyro/Accel range: ", gyrAccelRange & 0b111, gyrAccelRange >> 3)
		const rateFactors = [[], [], [], [], []]
		for (let i = 0; i < 5; i++) {
			rateFactors[i][0] = convert1616IntToFloat(buf.readInt32LE(14 + i * 12))
			rateFactors[i][1] = convert1616IntToFloat(buf.readInt32LE(18 + i * 12))
			rateFactors[i][2] = convert1616IntToFloat(buf.readInt32LE(22 + i * 12))
		}
		console.log("Rate factors:  x^1\tx^2\tx^3\tx^4\tx^5")
		console.log(
			`\tRoll:  ${rateFactors[0][0]}\t${rateFactors[1][0]}\t${rateFactors[2][0]}\t${rateFactors[3][0]}\t${rateFactors[4][0]}`
		)
		console.log(
			`\tPitch: ${rateFactors[0][1]}\t${rateFactors[1][1]}\t${rateFactors[2][1]}\t${rateFactors[3][1]}\t${rateFactors[4][1]}`
		)
		console.log(
			`\tYaw:   ${rateFactors[0][2]}\t${rateFactors[1][2]}\t${rateFactors[2][2]}\t${rateFactors[3][2]}\t${rateFactors[4][2]}`
		)
		const pidConstants = [[], [], []]
		for (let i = 0; i < 3; i++) {
			pidConstants[i][0] = convert1616IntToFloat(buf.readInt32LE(74 + i * 28))
			pidConstants[i][1] = convert1616IntToFloat(buf.readInt32LE(78 + i * 28))
			pidConstants[i][2] = convert1616IntToFloat(buf.readInt32LE(82 + i * 28))
			pidConstants[i][3] = convert1616IntToFloat(buf.readInt32LE(86 + i * 28))
			pidConstants[i][4] = convert1616IntToFloat(buf.readInt32LE(90 + i * 28))
			pidConstants[i][5] = convert1616IntToFloat(buf.readInt32LE(94 + i * 28))
			pidConstants[i][6] = convert1616IntToFloat(buf.readInt32LE(98 + i * 28))
		}
		console.log("PID constants: kP\tkI\t\tkD\tkFF\tkS\t\tiFall\t\treserved")
		console.log(
			`\tRoll:  ${pidConstants[0][0]}\t${pidConstants[0][1]}\t${pidConstants[0][2]}\t${pidConstants[0][3]}\t${
				pidConstants[0][4]
			}\t${pidConstants[0][5].toFixed(10)}\t${pidConstants[0][6].toFixed(10)}`
		)
		console.log(
			`\tPitch: ${pidConstants[1][0]}\t${pidConstants[1][1]}\t${pidConstants[1][2]}\t${pidConstants[1][3]}\t${
				pidConstants[1][4]
			}\t${pidConstants[1][5].toFixed(10)}\t${pidConstants[1][6].toFixed(10)}`
		)
		console.log(
			`\tYaw:   ${pidConstants[2][0]}\t${pidConstants[2][1]}\t${pidConstants[2][2]}\t${pidConstants[2][3]}\t${
				pidConstants[2][4]
			}\t${pidConstants[2][5].toFixed(10)}\t${pidConstants[2][6].toFixed(10)}`
		)
		console.log("Enabled flags: ")
		let enabledFlags = buf.readUInt32LE(158)
		let dataFrameLength = 0
		for (let i = 0; i < logFlags.length; i++) {
			if (enabledFlags & (1 << i)) {
				console.log("  - " + logFlags[i][0])
				dataFrameLength += logFlags[i][1]
			}
		}
		enabledFlags = buf.readUInt32LE(162) //flags are 64 bits, but only 32 are used for now

		console.log("  => ", dataFrameLength, "bytes per data frame")

		//shrink buffer to only contain data
		buf = buf.subarray(166, buf.length - 30) //lfs_close...
		console.log("  => ", buf.length / dataFrameLength, "data frames")
		if (buf.length % dataFrameLength != 0) {
			console.log("WARNING: data frame length does not match data length")
		}
		//convert data to array of arrays
		// const data = []
		// for (let i = 0; i < buf.length / dataFrameLength; i++) {
		// 	data[i] = []
		// 	for (let j = 0; j < logFlags.length; j++) {
		// 		if (enabledFlags & (1 << j)) {
		// 			if (logFlags[j][1] == 2) {
		// 				data[i][j] = buf.readInt16LE(i * dataFrameLength + logFlags[j][2])
		// 			} else if (logFlags[j][1] == 4) {
		// 				data[i][j] = buf.readInt32LE(i * dataFrameLength + logFlags[j][2])
		// 			} else if (logFlags[j][1] == 6) {
		// 				data[i][j] = buf.readInt16LE(i * dataFrameLength + logFlags[j][2])
		// 				data[i][j] += buf.readInt32LE(i * dataFrameLength + logFlags[j][2] + 2) * 65536
		// 			}
		// 		}
		// 	}
		// }
	})
	.catch(err => {
		console.error(err)
	})
