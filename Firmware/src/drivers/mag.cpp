#include "global.h"

// on drone: x = right, y = backward, z = down

u32 magState = 0;
elapsedMicros magTimer;
u8 magBuffer[6]  = {0};
fix32 magData[3] = {0};

i32 minMag[3] = {2047};
i32 maxMag[3] = {-2048};

i32 magOffset[3]  = {0};
fix32 magScale[3] = {1, 1, 1};

fix32 magHeading = 0;

u32 magOutOfRange = 0;

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
	Serial.println(i2c_set_baudrate(I2C_MAG, 3400000));
	magState = 2;
}

enum MAG_STATES {
	MAG_NOT_INIT = 0,
	MAG_INIT,
	MAG_MEASURING,
	MAG_SOON_READY,
	MAG_CHECK_DATA_READY,
	MAG_READ_DATA,
	MAG_PROCESS_DATA,
};

u32 magStateAfterRead = MAG_PROCESS_DATA;

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
		magDataRaw[0] = (i16)(magBuffer[1] + (magBuffer[0] << 8)) - magOffset[0];
		magDataRaw[1] = (i16)(magBuffer[5] + (magBuffer[4] << 8)) - magOffset[1];
		magDataRaw[2] = (i16)(magBuffer[3] + (magBuffer[2] << 8)) - magOffset[2];
		magData[0]    = magScale[0] * (int)magDataRaw[0]; // scale to normalised vector
		magData[1]    = magScale[1] * (int)magDataRaw[1]; // scale to normalised vector
		magData[2]    = magScale[2] * (int)magDataRaw[2]; // scale to normalised vector
		fix32 normSq  = magData[0] * magData[0] + magData[1] * magData[1] + magData[2] * magData[2];
		if (normSq > 2 || normSq < .5f) {
			magOutOfRange++;
		} else
			magOutOfRange = 0;
		magHeading = atan2f(magData[1].getf32(), magData[0].getf32());
		magState   = MAG_MEASURING;
	}
	}
}