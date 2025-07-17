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

fix32 baroASL = 0; // above sea level
fix32 lastBaroASL = 0, gpsBaroOffset = 0;
PT2 baroAslFiltered(1, 50);
f32 baroPres = 0;
volatile i32 blackboxPres = 0;
fix32 baroUpVel = 0;
u8 baroTemp = 0;
i32 baroCalibration[9];
fix32 gpsBaroAlt;
u8 baroBuffer[2] = {0};
elapsedMillis baroTimer = 0;
i32 pressureRaw, baroTempRaw;
bool newBaroData = false;

void getRawPressureTemperature(volatile i32 *pressureRaw, volatile i32 *temperature) {
#ifdef BARO_SPL006
	u8 data[6];
	regRead(SPI_BARO, PIN_BARO_CS, 0x00, data, 6, 0, false);
	// preserve the sign bit
	*pressureRaw = ((i32)data[0]) << 24 | ((i32)data[1]) << 16 | data[2] << 8;
	*temperature = ((i32)data[3]) << 24 | ((i32)data[4]) << 16 | data[5] << 8;
	*pressureRaw >>= 8;
	*temperature >>= 8;
#elifdef BARO_LPS22
	baroBuffer[0] = (u8)BaroRegs::PRESS_OUT_XL;
	i2c_write_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
	i2c_read_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
	*pressureRaw = (i32)baroBuffer[0] << 8;
	baroBuffer[0] = (u8)BaroRegs::PRESS_OUT_L;
	i2c_write_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
	i2c_read_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
	*pressureRaw |= ((i32)baroBuffer[0]) << 16;
	baroBuffer[0] = (u8)BaroRegs::PRESS_OUT_H;
	i2c_write_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
	i2c_read_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
	*pressureRaw |= ((i32)baroBuffer[0]) << 24;
	// baroBuffer[0] = (u8)BaroRegs::TEMP_OUT_L;
	// i2c_write_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
	// i2c_read_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
	// *temperature = (i32)baroBuffer[0] << 16;
	// baroBuffer[0] = (u8)BaroRegs::TEMP_OUT_H;
	// i2c_write_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
	// i2c_read_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
	// *temperature |= ((i32)baroBuffer[0]) << 24;
	*pressureRaw >>= 8;
	// *temperature >>= 16
#endif
}

void initBaro() {
#ifdef BARO_SPL006
	gpio_put(PIN_BARO_CS, 0); // enable SPI by pulling CS low (datasheet page 10)
	sleep_us(10);
	gpio_put(PIN_BARO_CS, 1);
	u8 data[18];
	regRead(SPI_BARO, PIN_BARO_CS, 0x0D, data, 1, 0, false); // read ID register
	if (data[0] != 0x10) {
		Serial.println("Baro not found");
		return;
	}
	regRead(SPI_BARO, PIN_BARO_CS, 0x10, data, 18, 0, false); // read calibration data
	baroCalibration[c0] = (((u32)data[0]) << 4) + (data[1] >> 4);
	if (baroCalibration[c0] > 2047) baroCalibration[c0] -= 4096;
	baroCalibration[c1] = (((u32)(data[1] & 0xF)) << 8) + data[2];
	if (baroCalibration[c1] > 2047) baroCalibration[c1] -= 4096;
	baroCalibration[c00] = (((u32)data[3]) << 12) + (((u32)data[4]) << 4) + (data[5] >> 4);
	if (baroCalibration[c00] > ((1 << 19) - 1)) baroCalibration[c00] -= 1 << 20;
	baroCalibration[c10] = (((u32)(data[5] & 0xF)) << 16) + (((u32)data[6]) << 8) + data[7];
	if (baroCalibration[c10] > ((1 << 19) - 1)) baroCalibration[c10] -= 1 << 20;
	baroCalibration[c01] = (((u32)data[8]) << 8) + data[9];
	if (baroCalibration[c01] > 32767) baroCalibration[c01] -= 65536;
	baroCalibration[c11] = (((u32)data[10]) << 8) + data[11];
	if (baroCalibration[c11] > 32767) baroCalibration[c11] -= 65536;
	baroCalibration[c20] = (((u32)data[12]) << 8) + data[13];
	if (baroCalibration[c20] > 32767) baroCalibration[c20] -= 65536;
	baroCalibration[c21] = (((u32)data[14]) << 8) + data[15];
	if (baroCalibration[c21] > 32767) baroCalibration[c21] -= 65536;
	baroCalibration[c30] = (((u32)data[16]) << 8) + data[17];
	if (baroCalibration[c30] > 32767) baroCalibration[c30] -= 65536;
	data[0] = 0b01100011; // 64 measurements per second, 8x oversampling
	regWrite(SPI_BARO, PIN_BARO_CS, 0x06, data, 1, 0); // set PRS_CFG register
	data[0] = 0b01100011; // 64 measurements per second, 8x oversampling
	regWrite(SPI_BARO, PIN_BARO_CS, 0x07, data, 1, 0); // set TMP_CFG register
	data[0] = 0b00000111; // enable pressureRaw and temperature measurement
	regWrite(SPI_BARO, PIN_BARO_CS, 0x08, data, 1, 0); // set MEAS_CFG register
#elifdef BARO_LPS22
	for (u32 tries = 0; tries < 10; tries++) {
		baroBuffer[0] = (u8)BaroRegs::WHO_AM_I;
		i2c_write_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
		i2c_read_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 1, false);
		if (baroBuffer[0] == 0xB1)
			break;
		if (tries == 9) {
			Serial.println("Baro not found");
			return;
		}
		sleep_ms(2);
	}
	baroBuffer[0] = (u8)BaroRegs::CTRL_REG1;
	baroBuffer[1] = 0b01001010; // 50Hz, Low-pass, block data update (from now on, only single byte reads allowed)
	i2c_write_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 2, false);
	baroBuffer[0] = (u8)BaroRegs::CTRL_REG2;
	baroBuffer[1] = 0b00000000; // clear register increment (needs to be unset when using block data update)
	i2c_write_blocking(I2C_BARO, I2C_BARO_ADDR, baroBuffer, 2, false);
#endif

	sleep_ms(22);
	readBaroLoop();
	baroPres = pressureRaw / 40.96f;
	baroASL = 44330 * (1 - powf(baroPres / 101325.f, 1 / 5.255f));
	lastBaroASL = baroASL;
	baroAslFiltered.set(baroASL);
	baroUpVel = 0;
}

void readBaroLoop() {
	if (baroTimer >= 20) {
		elapsedMicros taskTimer = 0;
		tasks[TASK_BAROREAD].runCounter++;
		baroTimer = 0;
		getRawPressureTemperature(&pressureRaw, &baroTempRaw);
		newBaroData = true;
		u32 duration = taskTimer;
		tasks[TASK_BAROREAD].totalDuration += duration;
		if (duration < tasks[TASK_BAROREAD].minDuration) {
			tasks[TASK_BAROREAD].minDuration = duration;
		}
		if (duration > tasks[TASK_BAROREAD].maxDuration) {
			tasks[TASK_BAROREAD].maxDuration = duration;
		}
	}
}

void evalBaroLoop() {
	if (!newBaroData) return;
	newBaroData = false;
	tasks[TASK_BAROEVAL].runCounter++;
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
	baroASL = baroAslFiltered.update(44330 * (1 - powf(baroPres / 101325.f, 1 / 5.255f)));

	if (gpsStatus.fixType != FIX_3D || gpsStatus.satCount < 6)
		gpsBaroAlt = baroASL - gpsBaroOffset;
	else
		gpsBaroOffset = baroASL - gpsMotion.alt / 1000.f;
	baroUpVel = (baroASL - lastBaroASL) * 50;
	bbDebug3 = baroUpVel.raw >> 4;
	baroImuUpVelFilter.update(FIX_3D ? fix32(-gpsMotion.velD / 10) * 0.01f : baroUpVel);
	mspDebugSensors[1] = (fix32(baroImuUpVelFilter) * 1000).geti32();
	u32 duration = taskTimer;
	tasks[TASK_BAROEVAL].totalDuration += duration;
	if (duration < tasks[TASK_BAROEVAL].minDuration) {
		tasks[TASK_BAROEVAL].minDuration = duration;
	}
	if (duration > tasks[TASK_BAROEVAL].maxDuration) {
		tasks[TASK_BAROEVAL].maxDuration = duration;
	}
}
