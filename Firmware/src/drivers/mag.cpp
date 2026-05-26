/**
 * @file mag.cpp
 * @brief Magnetometer driver implementation (HMC5883L, QMC5883L)
 *
 * Copyright (c) 2026 Kolibri-FC contributors
 *
 * This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 *
 * Kolibri-FC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kolibri-FC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
 */

#include "global.h"

// ======================== MAGNETOMETER DRIVER IMPLEMENTATION =======================
// on drone: x = right, y = backward, z = down
static MagState magState = MagState::NOT_INIT;
static u8 magSubState = 0;
static elapsedMicros magTimer;
static u32 magTimerTimeout = 0;
static u8 magBuffer[6] = {};
i32 magData[3] = {};

i16 magOffset[3] = {};
fix32 magRight = 0, magFront = 0;
fix32 hardMagHeading = 0;

// ========================= SOFT MAGNETOMETER IMPLEMENTATION =======================
static SoftMagState softMagState = SoftMagState::WAITING;
static elapsedMicros softMagTimer;
static PT1 thrustDirection(0.5f, 20); // direction of the thrust in the horizontal plane, calculated from the IMU (roll/pitch/yaw, no heading), between -pi and pi with respect to the non-corrected IMU heading. Therefore this varies with yaw drift, vibration, etc.
static fix32 thrustMagnitude = 0; // strength of the thrust, calculated from the IMU (roll/pitch), between 0 and 1
static PT1 softMagCorrection(0.2f, 20); // correction to apply to the IMU-based heading to get the soft mag heading, calculated from the GPS and the IMU-based thrust direction, between -pi and pi
static fix32 gpsHeadingOfMotion = 0;
static i32 gpsQuality = 0;
static i32 softMagTrust = 0; // how good the data of the soft mag probably is. Fast continuous motion increases this value, standing still decreases it. From 10000 points up, the soft heading is considered valid. Caps at 20000 points.
fix32 softMagHeading = 0;

// ========================= MAGNETOMETER INTERFACE IMPLEMENTATION =======================

static bool enableHardMag = true;
static bool enableSoftMag = false;
static u8 magSource = 0;
fix32 magHeading = 0;

void initMags() {
	addArraySetting(SETTING_MAG_CAL_HARD, magOffset);
	addSetting(SETTING_MAG_ENABLE, &enableHardMag, true);
	addSetting(SETTING_SOFT_MAG_ENABLE, &enableSoftMag, false);
	addSetting(SETTING_MAG_SOURCE, &magSource, 0);

	if (enableHardMag) {
		i2c_init(I2C_MAG, 400000);
		gpio_set_function(PIN_SDA0, GPIO_FUNC_I2C);
		gpio_set_function(PIN_SCL0, GPIO_FUNC_I2C);
		gpio_pull_up(PIN_SDA0);
		gpio_pull_up(PIN_SCL0);
	} else {
		magState = MagState::DISABLED;
	}

	if (!enableSoftMag) {
		softMagState = SoftMagState::DISABLED;
	}
}

MagState magStateAfterRead = MagState::PROCESS_DATA;
static float xtxMatrix[4][4] = {};
static float xtyVector[4] = {};
u32 calibrationCycle = 0;

float cofactor(float matrix[4][4], i32 row, i32 col) {
	i32 rows[3] = {};
	i32 cols[3] = {};
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
	case MagState::DISABLED:
		break;
	case MagState::NOT_INIT:
		if (magTimer < 5000) break;
		if (i2c0blocker) break;
		magTimer = 0;
#if HW_MAG == MAG_HMC5883L
		magBuffer[0] = (u8)MAG_REG::ID_A;
		i2c_write_blocking(I2C_MAG, MAG_ADDRESS, magBuffer, 1, false);
		i2c_read_blocking(I2C_MAG, MAG_ADDRESS, magBuffer, 3, false);
		if (strncmp((char *)magBuffer, "H43", 3) == 0)
			magState = MagState::INITIALIZING;
#elif HW_MAG == MAG_QMC5883L
		magBuffer[0] = (u8)MAG_REG::ID;
		i2c_write_blocking(I2C_MAG, MAG_ADDRESS, magBuffer, 1, false);
		i2c_read_blocking(I2C_MAG, MAG_ADDRESS, magBuffer, 1, false);
		if (magBuffer[0] == 0xFF)
			magState = MagState::INITIALIZING;
#endif
		break;
	case MagState::INITIALIZING:
		if (i2c0blocker) break;
#if HW_MAG == MAG_HMC5883L
		magBuffer[0] = (u8)MAG_REG::CONF_REGA;
		magBuffer[1] = MAG_AVG_8 | MAG_ODR_75HZ | MAG_LOAD_FLOAT;
		magBuffer[2] = MAG_RANGE_2_5;
		magBuffer[3] = MAG_MODE_CONTINUOUS | MAG_MODE_HS_I2C;
		i2c_write_blocking(I2C_MAG, MAG_ADDRESS, magBuffer, 4, false);
		i2c_set_baudrate(I2C_MAG, 3400000);
		magState = MagState::MEASURING;
		magTimerTimeout = 13000;
#elif HW_MAG == MAG_QMC5883L
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
#if HW_MAG == MAG_QMC5883L
		if (magTimer > magTimerTimeout) {
			magTimer = 0;
			magState = MagState::CHECK_DATA_READY;
			magSubState = 0;
		}
#elif HW_MAG == MAG_HMC5883L
		if (magTimer > magTimerTimeout) {
			magState = MagState::READ_DATA;
			magTimer = 0;
		}
#endif
		break;
	case MagState::CHECK_DATA_READY: {
		TASK_START(TASK_MAG_CHECK);
#if HW_MAG == MAG_QMC5883L
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
#if HW_MAG == MAG_HMC5883L
				magBuffer[0] = (u8)MAG_REG::DATA_X_H;
#elif HW_MAG == MAG_QMC5883L
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
#if HW_MAG == MAG_HMC5883L
		// TODO: can't be right: 3 permutations (odd)
		magDataRaw[0] = -(i16)(magBuffer[5] + (magBuffer[4] << 8));
		magDataRaw[1] = (i16)(magBuffer[1] + (magBuffer[0] << 8));
		magDataRaw[2] = (i16)(magBuffer[3] + (magBuffer[2] << 8));
#elif HW_MAG == MAG_QMC5883L
		magDataRaw[0] = -((i16)(magBuffer[0] + (magBuffer[1] << 8))) >> 4;
		magDataRaw[1] = -((i16)(magBuffer[2] + (magBuffer[3] << 8))) >> 4;
		magDataRaw[2] = ((i16)(magBuffer[4] + (magBuffer[5] << 8))) >> 4;
#endif
		magData[0] = magDataRaw[0] - magOffset[0];
		magData[1] = magDataRaw[1] - magOffset[1];
		magData[2] = magDataRaw[2] - magOffset[2];
		// x/0: forward, y/1: right, z/2: down
		// roll: right, pitch: up
		magRight = cosRoll * magData[1] - sinRoll * magData[2];
		magFront = cosPitch * magData[0] + sinPitch * sinRoll * magData[1] + sinPitch * cosRoll * magData[2];
		startFixMath();
		hardMagHeading = atan2Fix(-magRight, magFront) + 0.05643f; // 3.25° magnetic declination in radians
		if (magSource == 0) {
			magHeading = hardMagHeading;
			fix32 updateVal = hardMagHeading - yaw;
			if (updateVal - magHeadingCorrection > FIX_PI) {
				updateVal -= FIX_PI * 2;
			} else if (updateVal - magHeadingCorrection < -FIX_PI) {
				updateVal += FIX_PI * 2;
			}
			magHeadingCorrection.update(updateVal);
			magHeadingCorrection.rollover();
		}
		magState = MagState::MEASURING;
		TASK_END(TASK_MAG_EVAL);
	} break;
	case MagState::CALIBRATE: {
		// https://www.nxp.com/docs/en/application-note/AN4248.pdf
		i16 val[4];
#if HW_MAG == MAG_HMC5883L
		val[0] = -(i16)(magBuffer[5] + (magBuffer[4] << 8));
		val[1] = (i16)(magBuffer[1] + (magBuffer[0] << 8));
		val[2] = (i16)(magBuffer[3] + (magBuffer[2] << 8));
#elif HW_MAG == MAG_QMC5883L
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
#if HW_MAG == MAG_HMC5883L
		if (++calibrationCycle == 1000)
#elif HW_MAG == MAG_QMC5883L
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
			DEBUG_PRINTLN("Determinant is 0");
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
		if (lastMspSerial != nullptr) {
			char data = 1;
			MspMsgSetup s = {
				.serial = *lastMspSerial,
				.fn = MspFn::MAG_CALIBRATION,
				.type = MspMsgType::REQUEST,
				.version = lastMspSerial->lastMspVersion,
			};
			sendMsp(s, &data, 1);
			char calString[64];
			s.fn = MspFn::IND_MESSAGE;
			snprintf(calString, 64, "Offsets: %d %d %d, det: %f", magOffset[0], magOffset[1], magOffset[2], det);
			sendMsp(s, (char *)calString, strlen(calString));
		}
	} break;
	}
	tasks[TASK_MAG].debugInfo = (u32)magState * 10 + magSubState;
	TASK_END(TASK_MAG);
}

void softMagLoop() {
	TASK_START(TASK_SOFT_MAG);
	switch (softMagState) {
	case SoftMagState::DISABLED:
		break;
	case SoftMagState::WAITING:
		if (softMagTimer > 50000 && armed) {
			softMagTimer = 0;
			softMagState = SoftMagState::UPDATE_FROM_IMU;
		}
		break;
	case SoftMagState::UPDATE_FROM_IMU: {
		// calculate the thrust direction and magnitude from the IMU data
		fix32 absRoll = roll.abs();
		fix32 absPitch = pitch.abs();
		if (absRoll > 80 || absPitch > 80 || (absRoll < 10 && absPitch < 10)) {
			// if the drone is almost upside down, or almost straight, the thrust direction is at least untrustworthy, so don't update it
			softMagState = SoftMagState::BAD_SAMPLE;
			break;
		}
		fix32 fwdThrust = -sinPitch * cosRoll;
		fix32 rightThrust = sinRoll;
		thrustMagnitude = fwdThrust * fwdThrust + rightThrust * rightThrust;
		startFixMath();
		fix32 thrustUpdate = atan2Fix(rightThrust, fwdThrust);
		if (thrustUpdate - thrustDirection > FIX_PI) {
			thrustUpdate -= FIX_PI * 2;
		} else if (thrustUpdate - thrustDirection < -FIX_PI) {
			thrustUpdate += FIX_PI * 2;
		}
		thrustDirection.update(thrustUpdate);
		thrustDirection.rollover();
		softMagState = SoftMagState::UPDATE_FROM_GPS;
	} break;
	case SoftMagState::UPDATE_FROM_GPS: {
		if (gpsStatus.fixType < 3 || gpsAcc.sAcc > 5000 || gpsMotion.gSpeed < 5000 || gpsStatus.satCount < 6 || gpsAcc.headAcc > 3000000) {
			// if the GPS doesn't have a 3D fix or the speed or speed accuracy is worse than 5 m/s, heading acc > 30deg, or less than 6 sats, the GPS heading is untrustworthy, so don't update it
			softMagState = SoftMagState::BAD_SAMPLE;
			break;
		}
		gpsHeadingOfMotion = gpsMotion.headMot * (0.00001f * (float)DEG_TO_RAD);
		// up to 20 points from sAcc, 5 points per sat, 3 points for every 1m/s above 5m/s (up to 20m/s total), up to 20 points from headAcc, max 100 points
		// these values are a bit arbitrary, I know
		u32 speed = gpsMotion.gSpeed - 5000;
		if (speed > 15000) speed = 15000;
		gpsQuality = (5000 - gpsAcc.sAcc) / 250 + speed / 333 + 5 * (gpsStatus.satCount - 6) + (3000000 - gpsAcc.headAcc) / 150000;
		softMagState = SoftMagState::PROCESS_UPDATE;
	} break;
	case SoftMagState::PROCESS_UPDATE: {
		// calculate an update to the softMagCorrection based on the difference between the GPS heading and the IMU-based thrust direction, weighted by the thrust magnitude and the GPS speed accuracy
		softMagCorrection.update(gpsHeadingOfMotion - thrustDirection);
		if (gpsQuality < 0) gpsQuality = 0;
		if (gpsQuality > 100) gpsQuality = 100;
		if (thrustMagnitude > 1) thrustMagnitude = 1;
		// up to 200 points of trust added per frame => gain up to 4000 trust per second, trust as quick as 2.5 seconds with full trust after min. 5 seconds
		softMagTrust += (gpsQuality * (thrustMagnitude * 100).geti32()) / 50;
		if (softMagTrust > 20000) softMagTrust = 20000;
		softMagState = SoftMagState::APPLY_UPDATE;
	} break;
	case SoftMagState::BAD_SAMPLE:
		// 20 updates per second => lose up to 500 trust points per second => lose trust after 20 seconds of bad data, be at 0 trust after 40 seconds of bad data
		softMagTrust -= 25;
		if (softMagTrust < 0) softMagTrust = 0;
		softMagState = SoftMagState::APPLY_UPDATE;
		break;
	case SoftMagState::APPLY_UPDATE:
		softMagHeading = yaw + softMagCorrection;
		if (softMagHeading > FIX_PI) {
			softMagHeading -= FIX_PI * 2;
		} else if (softMagHeading < -FIX_PI) {
			softMagHeading += FIX_PI * 2;
		}
		if (magSource == 1) {
			magHeading = softMagHeading;
			magHeadingCorrection.set(softMagCorrection);
		}
		break;
	}
	TASK_END(TASK_SOFT_MAG);
}
