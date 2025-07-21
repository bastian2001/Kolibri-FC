#include "global.h"
#include <math.h>

#ifdef BARO_SPL006
enum BARO_COEFFS {
	c0,
	c1,
	c00,
	c10,
	c01,
	c11,
	c20,
	c21,
	c30,
};
constexpr i32 baroScaleFactor = 7864320;
#elifdef BARO_LPS22
#endif

enum class BaroState {
	NOT_INIT = 0, // not inited / no baro detected yet
	INITIALIZING, // initializing (baro detected but depending on model maybe needs more initialization steps than can be done at once)
	MEASURING, // measuring
	CHECK_READY, // check if baro is ready (status register) -> send back to measuring or forward to reading
	READ_DATA, // reading (sync or async)
	EVAL_DATA, // evaluation (calculate hPa and altitude)
};
BaroState baroState = BaroState::NOT_INIT;

f32 baroASL = 0; // above sea level
f32 baroPres = 0;
volatile i32 blackboxPres = 0;
f32 baroUpVel = 0;
u8 baroTemp = 0;
i32 baroCalibration[9];
fix32 gpsBaroAlt;
static u8 baroBuffer[32] = {0};
elapsedMicros baroTimer = 0;
u32 baroTimerTimeout = 0;
i32 pressureRaw, baroTempRaw;
f32 lastBaroASL = 0, gpsBaroOffset = 0;

void baroLoop() {
	elapsedMicros taskTimer = 0;
	switch (baroState) {
	case BaroState::NOT_INIT: {
		if (baroTimer < 5000) break;
		baroTimer = 0;
		// no baro detected yet
#ifdef BARO_SPL006
		gpio_put(PIN_BARO_CS, 0); // enable SPI by pulling CS low (datasheet page 10)
		sleep_us(10);
		gpio_put(PIN_BARO_CS, 1);
		regRead(SPI_BARO, PIN_BARO_CS, 0x0D, baroBuffer, 1, 0, false); // read ID register
		if (baroBuffer[0] != 0x10) {
			Serial.println("Baro not found");
			return;
		}
#elifdef BARO_LPS22
		baroBuffer[0] = (u8)BaroRegs::WHO_AM_I;
		i2c_write_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
		i2c_read_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
		if (baroBuffer[0] == 0xB1) {
			baroState = BaroState::INITIALIZING;
			baroTimerTimeout = 19000;
		}
#endif
	} break;
	case BaroState::INITIALIZING: {
		// baro detected
#ifdef BARO_SPL006
		regRead(SPI_BARO, PIN_BARO_CS, 0x10, baroBuffer, 18, 0, false); // read calibration data
		baroCalibration[c0] = (((u32)baroBuffer[0]) << 4) + (baroBuffer[1] >> 4);
		if (baroCalibration[c0] > 2047) baroCalibration[c0] -= 4096;
		baroCalibration[c1] = (((u32)(baroBuffer[1] & 0xF)) << 8) + baroBuffer[2];
		if (baroCalibration[c1] > 2047) baroCalibration[c1] -= 4096;
		baroCalibration[c00] = (((u32)baroBuffer[3]) << 12) + (((u32)baroBuffer[4]) << 4) + (baroBuffer[5] >> 4);
		if (baroCalibration[c00] > ((1 << 19) - 1)) baroCalibration[c00] -= 1 << 20;
		baroCalibration[c10] = (((u32)(baroBuffer[5] & 0xF)) << 16) + (((u32)baroBuffer[6]) << 8) + baroBuffer[7];
		if (baroCalibration[c10] > ((1 << 19) - 1)) baroCalibration[c10] -= 1 << 20;
		baroCalibration[c01] = (((u32)baroBuffer[8]) << 8) + baroBuffer[9];
		if (baroCalibration[c01] > 32767) baroCalibration[c01] -= 65536;
		baroCalibration[c11] = (((u32)baroBuffer[10]) << 8) + baroBuffer[11];
		if (baroCalibration[c11] > 32767) baroCalibration[c11] -= 65536;
		baroCalibration[c20] = (((u32)baroBuffer[12]) << 8) + baroBuffer[13];
		if (baroCalibration[c20] > 32767) baroCalibration[c20] -= 65536;
		baroCalibration[c21] = (((u32)baroBuffer[14]) << 8) + baroBuffer[15];
		if (baroCalibration[c21] > 32767) baroCalibration[c21] -= 65536;
		baroCalibration[c30] = (((u32)baroBuffer[16]) << 8) + baroBuffer[17];
		if (baroCalibration[c30] > 32767) baroCalibration[c30] -= 65536;
		baroBuffer[0] = 0b01100011; // 64 measurements per second, 8x oversampling
		regWrite(SPI_BARO, PIN_BARO_CS, 0x06, baroBuffer, 1, 0); // set PRS_CFG register
		baroBuffer[0] = 0b01100011; // 64 measurements per second, 8x oversampling
		regWrite(SPI_BARO, PIN_BARO_CS, 0x07, baroBuffer, 1, 0); // set TMP_CFG register
		baroBuffer[0] = 0b00000111; // enable pressureRaw and temperature measurement
		regWrite(SPI_BARO, PIN_BARO_CS, 0x08, baroBuffer, 1, 0); // set MEAS_CFG register
#elifdef BARO_LPS22
		baroBuffer[0] = (u8)BaroRegs::CTRL_REG1;
		baroBuffer[1] = 0b01001010; // 50Hz, Low-pass, block data update (from now on, only single byte reads allowed)
		i2c_write_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 2, false);
		baroBuffer[0] = (u8)BaroRegs::CTRL_REG2;
		baroBuffer[1] = 0b00000000; // clear register increment (needs to be unset when using block data update)
		i2c_write_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 2, false);
#endif
		baroState = BaroState::MEASURING;
	} break;
	case BaroState::MEASURING:
		// baro is measuring, send to state 3 to check if data is ready
		if (baroTimer >= baroTimerTimeout) {
			baroTimer = 0;
			baroState = BaroState::CHECK_READY;
		}
		break;
	case BaroState::CHECK_READY: {
		elapsedMicros taskTimer = 0;

#ifdef BARO_SPL006
		baroState = 4;
#elifdef BARO_LPS22
		baroBuffer[0] = (u8)BaroRegs::STATUS;
		i2c_write_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
		i2c_read_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
		if (baroBuffer[0] & (1 << 0)) {
			baroState = BaroState::READ_DATA;
			baroTimerTimeout = 19000; // baro data is available, check after a total of 19ms (slightly faster than 50Hz to allow for slight clock deviations)
		} else {
			baroState = BaroState::MEASURING;
			baroTimerTimeout = 2000; // check for new data again after 2ms
		}
#endif

		tasks[TASK_BAROCHECK].runCounter++;
		u32 duration = taskTimer;
		tasks[TASK_BAROCHECK].totalDuration += duration;
		if (duration < tasks[TASK_BAROCHECK].minDuration) {
			tasks[TASK_BAROCHECK].minDuration = duration;
		}
		if (duration > tasks[TASK_BAROCHECK].maxDuration) {
			tasks[TASK_BAROCHECK].maxDuration = duration;
		}
	} break;
	case BaroState::READ_DATA: {
		elapsedMicros taskTimer = 0;

#ifdef BARO_SPL006
		regRead(SPI_BARO, PIN_BARO_CS, 0x00, baroBuffer, 6, 0, false);
		// preserve the sign bit
		pressureRaw = ((i32)baroBuffer[0]) << 24 | ((i32)baroBuffer[1]) << 16 | baroBuffer[2] << 8;
		temperature = ((i32)baroBuffer[3]) << 24 | ((i32)baroBuffer[4]) << 16 | baroBuffer[5] << 8;
		pressureRaw >>= 8;
		temperature >>= 8;
#elifdef BARO_LPS22
		baroBuffer[0] = (u8)BaroRegs::PRESS_OUT_XL;
		i2c_write_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
		i2c_read_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
		pressureRaw = (i32)baroBuffer[0] << 8;
		baroBuffer[0] = (u8)BaroRegs::PRESS_OUT_L;
		i2c_write_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
		i2c_read_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
		pressureRaw |= ((i32)baroBuffer[0]) << 16;
		baroBuffer[0] = (u8)BaroRegs::PRESS_OUT_H;
		i2c_write_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
		i2c_read_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
		pressureRaw |= ((i32)baroBuffer[0]) << 24;
		// baroBuffer[0] = (u8)BaroRegs::TEMP_OUT_L;
		// i2c_write_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
		// i2c_read_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
		// temperature = (i32)baroBuffer[0] << 16;
		// baroBuffer[0] = (u8)BaroRegs::TEMP_OUT_H;
		// i2c_write_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
		// i2c_read_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
		// temperature |= ((i32)baroBuffer[0]) << 24;
		pressureRaw >>= 8;
		// temperature >>= 16
#endif
		baroState = BaroState::EVAL_DATA;

		tasks[TASK_BAROREAD].runCounter++;
		u32 duration = taskTimer;
		tasks[TASK_BAROREAD].totalDuration += duration;
		if (duration < tasks[TASK_BAROREAD].minDuration) {
			tasks[TASK_BAROREAD].minDuration = duration;
		}
		if (duration > tasks[TASK_BAROREAD].maxDuration) {
			tasks[TASK_BAROREAD].maxDuration = duration;
		}
	} break;
	case BaroState::EVAL_DATA: {
		elapsedMicros taskTimer = 0;

#ifdef BARO_SPL006
		baroTemp = 201 * .5f - baroTempRaw / 7864320.f * 260;
		f32 pressureScaled = pressureRaw / 7864320.f;
		f32 temperatureScaled = baroTempRaw / 7864320.f;
		baroPres = baroCalibration[c00] + pressureScaled * (baroCalibration[c10] + pressureScaled * (baroCalibration[c20] + pressureScaled * baroCalibration[c30])) + temperatureScaled * baroCalibration[c01] + temperatureScaled * pressureScaled * (baroCalibration[c11] + pressureScaled * baroCalibration[c21]);
#elifdef BARO_LPS22
		blackboxPres = pressureRaw;
		baroPres = pressureRaw / 40.96f;
#endif
		lastBaroASL = baroASL;
		baroASL = 44330 * (1 - powf(baroPres / 101325.f, 1 / 5.255f));
		if (gpsStatus.fixType != FIX_3D || gpsStatus.satCount < 6)
			gpsBaroAlt = baroASL - gpsBaroOffset;
		else
			gpsBaroOffset = baroASL - gpsMotion.alt / 1000.f;
		baroUpVel = (baroASL - lastBaroASL) * 50;
		baroState = BaroState::MEASURING;

		tasks[TASK_BAROEVAL].runCounter++;
		u32 duration = taskTimer;
		tasks[TASK_BAROEVAL].totalDuration += duration;
		if (duration < tasks[TASK_BAROEVAL].minDuration) {
			tasks[TASK_BAROEVAL].minDuration = duration;
		}
		if (duration > tasks[TASK_BAROEVAL].maxDuration) {
			tasks[TASK_BAROEVAL].maxDuration = duration;
		}
	} break;
	}
	tasks[TASK_BARO].runCounter++;
	tasks[TASK_BARO].debugInfo = (u8)baroState;
	u32 duration = taskTimer;
	tasks[TASK_BARO].totalDuration += duration;
	if (duration < tasks[TASK_BARO].minDuration) {
		tasks[TASK_BARO].minDuration = duration;
	}
	if (duration > tasks[TASK_BARO].maxDuration) {
		tasks[TASK_BARO].maxDuration = duration;
	}
}
