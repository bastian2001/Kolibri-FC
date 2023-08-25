#pragma once

#include <Arduino.h>

enum class GyroReg : uint8_t
{
	CHIP_ID = 0x00,
	ERROR = 0x02,
	STATUS = 0x03,
	ACC_X_LSB = 0x0C,
	ACC_X_MSB,
	ACC_Y_LSB,
	ACC_Y_MSB,
	ACC_Z_LSB,
	ACC_Z_MSB,
	GYR_X_LSB = 0x12,
	GYR_X_MSB,
	GYR_Y_LSB,
	GYR_Y_MSB,
	GYR_Z_LSB,
	GYR_Z_MSB,
	SENSOR_TIME_LSB = 0x18,
	EVENT = 0x1B,
	INT_STATUS_0 = 0x1C,
	INT_STATUS_1 = 0x1D,
	SX_OUT_0 = 0x1E,
	SX_OUT_1 = 0x1F,
	INTERNAL_STATUS = 0x21,
	TEMP_LSB = 0x22,
	TEMP_MSB,
	FEATURES = 0x30,
	ACC_CONF = 0x40,
	ACC_RANGE = 0x41,
	GYR_CONF = 0x42,
	GYR_RANGE = 0x43,
	SATURATION = 0x4A,
	INT1_IO_CTRL = 0x53,
	INT2_IO_CTRL = 0x54,
	INT_LATCH = 0x55,
	INT1_MAP = 0x56,
	INT2_MAP = 0x57,
	INT_MAP_DATA = 0x58,
	INIT_CTRL = 0x59,
	INIT_DATA = 0x5E,
	NVM_CONF = 0x6A,
	DRV = 0x6C,
	ACC_SELF_TEST = 0x6D,
	GYR_SELF_TEST_AXES = 0x6E,
	NV_CONF = 0x70,
	OFFSET_0 = 0x71,
	OFFSET_1 = 0x72,
	OFFSET_2 = 0x73,
	OFFSET_3 = 0x74,
	OFFSET_4 = 0x75,
	OFFSET_5 = 0x76,
	OFFSET_6 = 0x77,
	PWR_CONF = 0x7C,
	PWR_CTRL = 0x7D,
	CMD = 0x7E,
};

// gyro data, and by extension the PID loop, is the most time-sensitive task.
// it gets priority, and once the data is read, the flag is set to FFFFFFFF so that
// other tasks know it's safe to run without impacting the gyro data or PID loop.
extern uint32_t gyroUpdateFlag;

int gyroInit();

extern const uint8_t bmi270_config_file[8192];

void gyroGetData(int16_t *buf);

void gyroLoop();