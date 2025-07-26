#include "global.h"

// on drone: x = right, y = backward, z = down

static MagState magState = MagState::NOT_INIT;
static u8 magSubState = 0;
elapsedMicros magTimer;
u32 magTimerTimeout = 0;
u8 magBuffer[6] = {0};
i32 magData[3] = {0};

i16 magOffset[3] = {0};
fix32 magRight = 0, magFront = 0;

fix32 magHeading = 0;

void initMag() {
	addArraySetting(SETTING_MAG_CAL_HARD, magOffset);

	i2c_init(I2C_MAG, 400000);
	gpio_set_function(PIN_SDA0, GPIO_FUNC_I2C);
	gpio_set_function(PIN_SCL0, GPIO_FUNC_I2C);
	gpio_pull_up(PIN_SDA0);
	gpio_pull_up(PIN_SCL0);
}

MagState magStateAfterRead = MagState::PROCESS_DATA;
float xtxMatrix[4][4] = {0};
float xtyVector[4] = {0};
u32 calibrationCycle = 0;

float cofactor(float matrix[4][4], i32 row, i32 col) {
	i32 rows[3] = {0};
	i32 cols[3] = {0};
	for (int i = 0; i < 3; i++) {
		rows[i] = i >= row ? i + 1 : i;
		cols[i] = i >= col ? i + 1 : i;
	}
	float temp = 0;
	temp += matrix[rows[0]][cols[0]] * matrix[rows[1]][cols[1]] * matrix[rows[2]][cols[2]];
	temp += matrix[rows[0]][cols[1]] * matrix[rows[1]][cols[2]] * matrix[rows[2]][cols[0]];
	temp += matrix[rows[0]][cols[2]] * matrix[rows[1]][cols[0]] * matrix[rows[2]][cols[1]];
	temp -= matrix[rows[0]][cols[2]] * matrix[rows[1]][cols[1]] * matrix[rows[2]][cols[0]];
	temp -= matrix[rows[0]][cols[1]] * matrix[rows[1]][cols[0]] * matrix[rows[2]][cols[2]];
	temp -= matrix[rows[0]][cols[0]] * matrix[rows[1]][cols[2]] * matrix[rows[2]][cols[1]];
	temp *= (row + col) % 2 == 0 ? 1 : -1;
	return temp;
}

void magLoop() {
	TASK_START(TASK_MAG);
	switch (magState) {
	case MagState::NOT_INIT:
		if (magTimer < 5000) break;
		if (i2c0blocker) break;
		magTimer = 0;
#if MAG_HARDWARE == MAG_HMC5883L
		magBuffer[0] = (u8)MAG_REG::ID_A;
		i2c_write_blocking(I2C_MAG, MAG_ADDRESS, magBuffer, 1, false);
		i2c_read_blocking(I2C_MAG, MAG_ADDRESS, magBuffer, 3, false);
		if (strncmp((char *)magBuffer, "H43", 3) == 0)
			magState = MagState::INITIALIZING;
#elif MAG_HARDWARE == MAG_QMC5883L
		magBuffer[0] = (u8)MAG_REG::ID;
		i2c_write_blocking(I2C_MAG, MAG_ADDRESS, magBuffer, 1, false);
		i2c_read_blocking(I2C_MAG, MAG_ADDRESS, magBuffer, 1, false);
		if (magBuffer[0] == 0xFF)
			magState = MagState::INITIALIZING;
#endif
		break;
	case MagState::INITIALIZING:
		if (i2c0blocker) break;
#if MAG_HARDWARE == MAG_HMC5883L
		magBuffer[0] = (u8)MAG_REG::CONF_REGA;
		magBuffer[1] = MAG_AVG_8 | MAG_ODR_75HZ | MAG_LOAD_FLOAT;
		magBuffer[2] = MAG_RANGE_2_5;
		magBuffer[3] = MAG_MODE_CONTINUOUS | MAG_MODE_HS_I2C;
		i2c_write_blocking(I2C_MAG, MAG_ADDRESS, magBuffer, 4, false);
		i2c_set_baudrate(I2C_MAG, 3400000);
		magState = MagState::MEASURING;
		magTimerTimeout = 13000;
#elif MAG_HARDWARE == MAG_QMC5883L
		magBuffer[0] = (u8)MAG_REG::SET_RESET;
		magBuffer[1] = 1;
		i2c_write_blocking(I2C_MAG, MAG_ADDRESS, magBuffer, 2, false);
		magBuffer[0] = (u8)MAG_REG::CONTROL_1;
		magBuffer[1] = MAG_OSR_512 | MAG_RANGE_2 | MAG_ODR_200HZ | MAG_MODE_CONTINUOUS;
		i2c_write_blocking(I2C_MAG, MAG_ADDRESS, magBuffer, 2, false);
		magState = MagState::MEASURING;
		magTimerTimeout = 4000;
#endif
		break;
	case MagState::MEASURING:
#if MAG_HARDWARE == MAG_QMC5883L
		if (magTimer > magTimerTimeout) {
			magTimer = 0;
			magState = MagState::CHECK_DATA_READY;
			magSubState = 0;
		}
#elif MAG_HARDWARE == MAG_HMC5883L
		if (magTimer > magTimerTimeout) {
			magState = MagState::READ_DATA;
			magTimer = 0;
		}
#endif
		break;
	case MagState::CHECK_DATA_READY: {
		TASK_START(TASK_MAG_CHECK);
#if MAG_HARDWARE == MAG_QMC5883L
		// check every ms if data is ready
		switch (magSubState) {
		case 0:
			if (i2c0blocker == 0) {
				i2c0blocker = 2;
				magSubState = 1;
				magBuffer[0] = (u8)MAG_REG::STATUS;
				startI2cWrite(MAG_ADDRESS, magBuffer, 1);
			}
			break;
		case 1:
			if (checkI2cWriteDone()) {
				startI2cRead(MAG_ADDRESS, 1);
				magSubState = 2;
			}
			break;
		case 2:
			if (getI2cReadCount()) {
				getI2cReadData(magBuffer, 1);
				if (magBuffer[0] & (1 << 0)) {
					// data ready
					magState = MagState::READ_DATA;
					magSubState = 0;
					magTimerTimeout = 4000;
				} else {
					// data not ready, check again in 1ms
					magState = MagState::MEASURING;
					magSubState = 0;
					magTimerTimeout = 1000;
				}
				i2c0blocker = 0;
			}
			break;
		}
#endif
		TASK_END(TASK_MAG_CHECK);
	} break;
	case MagState::READ_DATA: {
		TASK_START(TASK_MAG_READ);
		switch (magSubState) {
		case 0:
			if (i2c0blocker == 0) {
				i2c0blocker = 2;
				magSubState = 1;
#if MAG_HARDWARE == MAG_HMC5883L
				magBuffer[0] = (u8)MAG_REG::DATA_X_H;
#elif MAG_HARDWARE == MAG_QMC5883L
				magBuffer[0] = (u8)MAG_REG::DATA_X_L;
#endif
				startI2cWrite(MAG_ADDRESS, magBuffer, 1);
			}
			break;
		case 1:
			if (checkI2cWriteDone()) {
				startI2cRead(MAG_ADDRESS, 6);
				magSubState = 2;
			}
			break;
		case 2: {
			i8 rc = getI2cReadCount();
			if (rc == 6) {
				getI2cReadData(magBuffer, 6);
				magState = magStateAfterRead;
				magSubState = 0;
				i2c0blocker = 0;
			} else if (rc < 0) {
				magSubState = 0;
				i2c0blocker = 0;
			}
		} break;
		}
		TASK_END(TASK_MAG_READ);
	} break;
	case MagState::PROCESS_DATA: {
		TASK_START(TASK_MAG_EVAL);
		static i32 magDataRaw[3];
#if MAG_HARDWARE == MAG_HMC5883L
		// TODO: can't be right: 3 permutations (odd)
		magDataRaw[0] = -(i16)(magBuffer[5] + (magBuffer[4] << 8));
		magDataRaw[1] = (i16)(magBuffer[1] + (magBuffer[0] << 8));
		magDataRaw[2] = (i16)(magBuffer[3] + (magBuffer[2] << 8));
#elif MAG_HARDWARE == MAG_QMC5883L
		magDataRaw[0] = -((i16)(magBuffer[0] + (magBuffer[1] << 8))) >> 4;
		magDataRaw[1] = -((i16)(magBuffer[2] + (magBuffer[3] << 8))) >> 4;
		magDataRaw[2] = ((i16)(magBuffer[4] + (magBuffer[5] << 8))) >> 4;
#endif
		magData[0] = magDataRaw[0] - magOffset[0];
		magData[1] = magDataRaw[1] - magOffset[1];
		magData[2] = magDataRaw[2] - magOffset[2];
		// x/0: forward, y/1: right, z/2: down
		// roll: left, pitch: forward
		// 2 -> -2
		// 0 -> 1
		// 1 -> 0
		magRight = cosRoll * magData[1] + sinRoll * magData[2];
		magFront = cosPitch * magData[0] + sinPitch * sinRoll * magData[1] - sinPitch * cosRoll * magData[2];
		startFixMath();
		magHeading = atan2Fix(-magRight, magFront) + 0.05643f; // 3.25° magnetic declination in radians
		fix32 updateVal = magHeading - yaw;
		if (updateVal - magHeadingCorrection > FIX_PI) {
			updateVal -= FIX_PI * 2;
		} else if (updateVal - magHeadingCorrection < -FIX_PI) {
			updateVal += FIX_PI * 2;
		}
		magHeadingCorrection.update(updateVal);
		magHeadingCorrection.rollover();
		magState = MagState::MEASURING;
		TASK_END(TASK_MAG_EVAL);
	} break;
	case MagState::CALIBRATE: {
		// https://www.nxp.com/docs/en/application-note/AN4248.pdf
		i16 val[4];
#if MAG_HARDWARE == MAG_HMC5883L
		val[0] = -(i16)(magBuffer[5] + (magBuffer[4] << 8));
		val[1] = (i16)(magBuffer[1] + (magBuffer[0] << 8));
		val[2] = (i16)(magBuffer[3] + (magBuffer[2] << 8));
#elif MAG_HARDWARE == MAG_QMC5883L
		val[0] = -((i16)(magBuffer[0] + (magBuffer[1] << 8))) >> 4;
		val[1] = -((i16)(magBuffer[2] + (magBuffer[3] << 8))) >> 4;
		val[2] = ((i16)(magBuffer[4] + (magBuffer[5] << 8))) >> 4;
#endif
		val[3] = 1;
		magData[0] = val[0] - magOffset[0];
		magData[1] = val[1] - magOffset[1];
		magData[2] = val[2] - magOffset[2];
		for (int row = 0; row < 4; row++) {
			for (int col = 0; col < 4; col++) {
				xtxMatrix[row][col] += val[row] * val[col];
			}
			xtyVector[row] += val[row] * (val[0] * val[0] + val[1] * val[1] + val[2] * val[2]);
		}
#if MAG_HARDWARE == MAG_HMC5883L
		if (++calibrationCycle == 1000)
#elif MAG_HARDWARE == MAG_QMC5883L
		if (++calibrationCycle == 3000)
#endif
		{
			magState = MagState::PROCESS_CALIBRATION;
			calibrationCycle = 0;
			magStateAfterRead = MagState::PROCESS_DATA;
		} else {
			magState = MagState::MEASURING;
		}
	} break;
	case MagState::PROCESS_CALIBRATION: {
		float xtxMatrixInv[4][4];
		float det = 0;
		for (int step = 0; step < 4; step++) {
			det += xtxMatrix[0][step] * cofactor(xtxMatrix, 0, step);
		}
		if (det == 0) {
			Serial.println("Determinant is 0");
			break;
		}
		for (int row = 0; row < 4; row++) {
			for (int col = 0; col < 4; col++) {
				xtxMatrixInv[col][row] = cofactor(xtxMatrix, row, col) / det;
			}
		}
		float calibration[4];
		for (int row = 0; row < 4; row++) {
			calibration[row] = 0;
			for (int col = 0; col < 4; col++) {
				calibration[row] += xtxMatrixInv[row][col] * xtyVector[col];
			}
		}
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				xtxMatrix[i][j] = 0;
			}
			xtyVector[i] = 0;
		}
		magOffset[0] = calibration[0] / 2;
		magOffset[1] = calibration[1] / 2;
		magOffset[2] = calibration[2] / 2;
		openSettingsFile();
		getSetting(SETTING_MAG_CAL_HARD)->updateSettingInFile();
		closeSettingsFile();
		magState = MagState::MEASURING;
		char data = 1;
		sendMsp(lastMspSerial, MspMsgType::REQUEST, MspFn::MAG_CALIBRATION, lastMspVersion, &data, 1);
		char calString[64];
		snprintf(calString, 64, "Offsets: %d %d %d, det: %f", magOffset[0], magOffset[1], magOffset[2], det);
		sendMsp(lastMspSerial, MspMsgType::REQUEST, MspFn::IND_MESSAGE, lastMspVersion, (char *)calString, strlen(calString));
	} break;
	}
	tasks[TASK_MAG].debugInfo = (u32)magState * 10 + magSubState;
	TASK_END(TASK_MAG);
}
