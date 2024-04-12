#pragma once

#include <Arduino.h>

// a total of 2000 good samples are required, the first 1000 are ignored
#define CALIBRATION_SAMPLES 1000
#define QUIET_SAMPLES 1000
#define CALIBRATION_TOLERANCE 64 // (4deg/s)

enum class GyroReg : u8 {
	CHIP_ID   = 0x00,
	ERROR     = 0x02,
	STATUS    = 0x03,
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
	EVENT           = 0x1B,
	INT_STATUS_0    = 0x1C,
	INT_STATUS_1    = 0x1D,
	SX_OUT_0        = 0x1E,
	SX_OUT_1        = 0x1F,
	INTERNAL_STATUS = 0x21,
	TEMP_LSB        = 0x22,
	TEMP_MSB,
	FEAT_PAGE          = 0x2F,
	FEATURES           = 0x30,
	ACC_CONF           = 0x40,
	ACC_RANGE          = 0x41,
	GYR_CONF           = 0x42,
	GYR_RANGE          = 0x43,
	SATURATION         = 0x4A,
	INT1_IO_CTRL       = 0x53,
	INT2_IO_CTRL       = 0x54,
	INT_LATCH          = 0x55,
	INT1_MAP           = 0x56,
	INT2_MAP           = 0x57,
	INT_MAP_DATA       = 0x58,
	INIT_CTRL          = 0x59,
	INIT_DATA          = 0x5E,
	NVM_CONF           = 0x6A,
	DRV                = 0x6C,
	ACC_SELF_TEST      = 0x6D,
	GYR_SELF_TEST_AXES = 0x6E,
	NV_CONF            = 0x70,
	OFFSET_0           = 0x71,
	OFFSET_1           = 0x72,
	OFFSET_2           = 0x73,
	OFFSET_3           = 0x74,
	OFFSET_4           = 0x75,
	OFFSET_5           = 0x76,
	OFFSET_6           = 0x77,
	PWR_CONF           = 0x7C,
	PWR_CTRL           = 0x7D,
	CMD                = 0x7E,
};

/**
 * @brief provides flags for tasks that depend on the gyro data
 *
 * @details gyro data, and by extension the PID loop, is the most time-sensitive task. it gets priority, and once the data is read, the flag is set to FFFFFFFF so that other tasks know it's safe to run without impacting the gyro data or PID loop.
 */
extern u32 gyroUpdateFlag;
extern u16 accelCalibrationCycles;    /// counts down the cycles for the accelerometer calibration, calibration is done if the value is 0
extern i32 accelCalibrationOffset[3]; /// offset that gets subtracted from the accelerometer values

/// @brief Initializes the gyro (Bosch BMI270)
/// @return 0 on success, 1 on failure
int gyroInit();

/**
 * @brief reads the gyro data
 *
 * @details also subtracts offsets and sets the gyroUpdateFlag
 *
 * @param buf buffer to store the data in
 */
void gyroGetData(i16 *buf);

/// @brief checks for new gyro data, sets off the read and PID tasks
void gyroLoop();