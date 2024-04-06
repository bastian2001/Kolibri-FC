#include "global.h"

// on drone: x = right, y = backward, z = down

u32 magState = 0;
elapsedMicros magTimer;
u8 magBuffer[6] = {0};
i32 magData[3]  = {0};

i16 magOffset[3] = {0};
fix32 magX = 0, magY = 0;

fix32 magHeading = 0;

void initMag() {
	i2c_init(I2C_MAG, 400000);
	gpio_set_function(PIN_SDA0, GPIO_FUNC_I2C);
	gpio_set_function(PIN_SCL0, GPIO_FUNC_I2C);
	gpio_pull_up(PIN_SDA0);
	gpio_pull_up(PIN_SCL0);
	for (u32 tries = 0; tries < 10; tries++) {
		magBuffer[0] = (u8)MAG_REG::ID_A;
		i2c_write_blocking(I2C_MAG, MAG_ADDRESS, magBuffer, 1, false);
		i2c_read_blocking(I2C_MAG, MAG_ADDRESS, magBuffer, 3, false);
		if (strncmp((char *)magBuffer, "H43", 3) == 0) {
			Serial.println("Success!");
			break;
		}
		if (tries == 9) {
			Serial.println("Failed to find magnetometer");
			return;
		}
		delay(2);
	}
	magState     = 1;
	magBuffer[0] = (u8)MAG_REG::CONF_REGA;
	magBuffer[1] = MAG_AVG_8 | MAG_ODR_75HZ | MAG_LOAD_FLOAT;
	magBuffer[2] = MAG_RANGE_2_5;
	magBuffer[3] = MAG_MODE_CONTINUOUS | MAG_MODE_HS_I2C;
	i2c_write_blocking(I2C_MAG, MAG_ADDRESS, magBuffer, 4, false);
	i2c_set_baudrate(I2C_MAG, 3400000);
	magState = 2;
}

u32 magStateAfterRead = MAG_PROCESS_DATA;
float xtxMatrix[4][4] = {0};
float xtyVector[4]    = {0};
u32 calibrationCycle  = 0;

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
	switch (magState) {
	case MAG_NOT_INIT: // not initialized
	case MAG_INIT:
		break;
	case MAG_MEASURING:
		if (magTimer > 13000) {
			magState = MAG_READ_DATA;
			magTimer = 0;
		}
		break;
	case MAG_SOON_READY:
		if (magTimer > 100) {
			magState = MAG_CHECK_DATA_READY;
			magTimer = 0;
		}
		break;
	case MAG_CHECK_DATA_READY: {
		// check every ms if data is ready
		magBuffer[0] = (u8)MAG_REG::STATUS;
		i2c_write_blocking(I2C_MAG, MAG_ADDRESS, magBuffer, 1, false);
		i2c_read_blocking(I2C_MAG, MAG_ADDRESS, magBuffer, 1, false);
		if (magBuffer[0] & 0x01) // data ready
			magState = MAG_READ_DATA;
		else // data not ready, check again in 1ms
			magState = MAG_SOON_READY;
	} break;
	case MAG_READ_DATA: {
		magBuffer[0] = (u8)MAG_REG::DATA_X_H;
		i2c_write_blocking(I2C_MAG, MAG_ADDRESS, (u8 *)magBuffer, 1, false);
		i2c_read_blocking(I2C_MAG, MAG_ADDRESS, (u8 *)magBuffer, 6, false);
		magState = magStateAfterRead;
	} break;
	case MAG_PROCESS_DATA: {
		static i32 magDataRaw[3];
		magDataRaw[0] = (i16)(magBuffer[1] + (magBuffer[0] << 8));
		magDataRaw[1] = (i16)(magBuffer[5] + (magBuffer[4] << 8));
		magDataRaw[2] = (i16)(magBuffer[3] + (magBuffer[2] << 8));
		magData[0]    = magDataRaw[0] - magOffset[0];
		magData[1]    = magDataRaw[1] - magOffset[1];
		magData[2]    = magDataRaw[2] - magOffset[2];
		startFixTrig();
		fix32 cosRoll  = cosFix(roll);
		fix32 sinRoll  = sinFix(roll);
		fix32 cosPitch = cosFix(pitch);
		fix32 sinPitch = sinFix(pitch);
		// x: right, y: backward, z: down
		// roll: left, pitch: front
		magX            = cosRoll * magData[0] + sinRoll * magData[2];
		magY            = cosPitch * magData[1] - sinPitch * sinRoll * magData[0] + sinPitch * cosRoll * magData[2];
		magHeading      = atan2f(-magX.getf32(), -magY.getf32()) + 0.05643f; // 3.25° magnetic declination in radians
		fix32 updateVal = magHeading - fix32(yaw);
		if (updateVal - (fix32)magHeadingCorrection > fix32(PI)) {
			updateVal -= fix32(2 * PI);
		} else if (updateVal - (fix32)magHeadingCorrection < fix32(-PI)) {
			updateVal += fix32(2 * PI);
		}
		magHeadingCorrection.update(updateVal);
		magHeadingCorrection.rollover();
		magState = MAG_MEASURING;
	} break;
	case MAG_CALIBRATE: {
		i16 val[4];
		val[0] = magBuffer[1] + (magBuffer[0] << 8); // x
		val[1] = magBuffer[5] + (magBuffer[4] << 8); // y
		val[2] = magBuffer[3] + (magBuffer[2] << 8); // z
		val[3] = 1;
		for (int row = 0; row < 4; row++) {
			for (int col = 0; col < 4; col++) {
				xtxMatrix[row][col] += val[row] * val[col];
			}
			xtyVector[row] += val[row] * (val[0] * val[0] + val[1] * val[1] + val[2] * val[2]);
		}
		if (++calibrationCycle == 1000) {
			magState          = MAG_PROCESS_CALIBRATION;
			calibrationCycle  = 0;
			magStateAfterRead = MAG_PROCESS_DATA;
		} else {
			magState = MAG_MEASURING;
		}
	} break;
	case MAG_PROCESS_CALIBRATION: {
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
				xtxMatrixInv[col][row] = (float)cofactor(xtxMatrix, row, col) / det;
			}
		}
		float calibration[4];
		for (int row = 0; row < 4; row++) {
			calibration[row] = 0;
			for (int col = 0; col < 4; col++) {
				calibration[row] += (float)xtxMatrixInv[row][col] * xtyVector[col];
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
		EEPROM.put((u16)EEPROM_POS::MAG_CALIBRATION_HARD, magOffset[0]);
		EEPROM.put((u16)EEPROM_POS::MAG_CALIBRATION_HARD + 2, magOffset[1]);
		EEPROM.put((u16)EEPROM_POS::MAG_CALIBRATION_HARD + 4, magOffset[2]);
		EEPROM.commit();
		magState = MAG_MEASURING;
		char calString[128];
		snprintf(calString, 128, "Offsets: %d %d %d, det: %f", magOffset[0], magOffset[1], magOffset[2], det);
		sendCommand((u16)ConfigCmd::IND_MESSAGE, (char *)calString, strlen(calString));
	} break;
	}
}