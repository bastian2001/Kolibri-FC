#pragma once

#include <Arduino.h>

// a total of 2000 good samples are required, the first 1000 are ignored
#define CALIBRATION_SAMPLES 1000
#define QUIET_SAMPLES 1000
#define GYRO_CALIBRATION_TOLERANCE 128 // (4deg/s)
#define IMU_ALIGNMENT_TOLERANCE 600 // does not need to be exact, therefore rather big at 3m/s²
#define ACCEL_CALIBRATION_TOLERANCE 200 // 1m/s²

extern volatile bool accelCalDone; // accel calibration flag to save the data
extern volatile u8 accelCalState; // 0 = normal operation, 1 = quiet, 2 = measuring
extern volatile bool imuAlignmentDone; // imu orientation done flag to save the data
extern volatile u8 imuAlignmentStep; // 0 = normal operation, 1 = waiting for normal placement, 2 = waiting for nose down
extern volatile i16 imuAlignmentCounter;

#ifdef GYRO_BMI270
enum class GyroReg : u8 {
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
	FEAT_PAGE = 0x2F,
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
	IF_CONF = 0x6B,
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
#elifdef GYRO_ICM42688P

enum class GyroReg : u8 {
	REG_BANK_SEL = 0x76,

	// user bank 0
	DEVICE_CONFIG = 0x11,
	DRIVE_CONFIG = 0x13,
	INT_CONFIG = 0x14,
	FIFO_CONFIG = 0x16,
	TEMP_MSB = 0x1D, // actually LSB, endianness swapped
	TEMP_LSB, // actually MSB, endianness swapped
	ACC_X_MSB = 0x1F, // actually LSB, endianness swapped
	ACC_X_LSB, // actually MSB, endianness swapped
	ACC_Y_MSB, // actually LSB, endianness swapped
	ACC_Y_LSB, // actually MSB, endianness swapped
	ACC_Z_MSB, // actually LSB, endianness swapped
	ACC_Z_LSB, // actually MSB, endianness swapped
	GYR_X_MSB = 0x25, // actually LSB, endianness swapped
	GYR_X_LSB, // actually MSB, endianness swapped
	GYR_Y_MSB, // actually LSB, endianness swapped
	GYR_Y_LSB, // actually MSB, endianness swapped
	GYR_Z_MSB, // actually LSB, endianness swapped
	GYR_Z_LSB, // actually MSB, endianness swapped
	INT_STATUS = 0x2D,
	SIGNAL_PATH_RESET = 0x4B,
	INTF_CONFIG0 = 0x4C,
	INTF_CONFIG1 = 0x4D,
	PWR_MGMT0 = 0x4E,
	GYRO_CONFIG0 = 0x4F,
	ACCEL_CONFIG0 = 0x50,
	GYRO_CONFIG1 = 0x51,
	GYRO_ACCEL_CONFIG0 = 0x52,
	ACCEL_CONFIG1 = 0x53,
	TMST_CONFIG = 0x54,
	INT_CONFIG0 = 0x63,
	INT_CONFIG1 = 0x64,
	INT_SOURCE0 = 0x65,
	INT_SOURCE1 = 0x66,
	INT_SOURCE3 = 0x68,
	INT_SOURCE4 = 0x69,
	SELF_TEST_CONFIG = 0x70,
	WHO_AM_I = 0x75,

	// user bank 1
	SENSOR_CONFIG0 = 0x03,
	GYRO_CONFIG_STATIC3 = 0x0C,
	GYRO_CONFIG_STATIC4,
	GYRO_CONFIG_STATIC5,
	TMSTVAL0 = 0x62,
	TMSTVAL1,
	TMSTVAL2,
	INTF_CONFIG5 = 0x7B,

	// user bank 2
	ACCEL_CONFIG_STATIC2 = 0x03,
	ACCEL_CONFIG_STATIC3,
	ACCEL_CONFIG_STATIC4,

	// user bank 4
	OFFSET_USER0 = 0x77,
	OFFSET_USER1,
	OFFSET_USER2,
	OFFSET_USER3,
	OFFSET_USER4,
	OFFSET_USER5,
	OFFSET_USER6,
	OFFSET_USER7,
	OFFSET_USER8,
};
#endif

/**
 * @brief provides flags for tasks that depend on the gyro data
 *
 * @details gyro data, and by extension the PID loop, is the most time-sensitive task. it gets priority, and once the data is read, the flag is set to FFFFFFFF so that other tasks know it's safe to run without impacting the gyro data or PID loop.
 */
extern u32 gyroUpdateFlag;
extern fix32 gyroScaled[3]; // gyro data in deg/s
extern fix32 accelScaled[3]; // accel data in m/s^2
extern i32 gyroAligned[3]; // after alignment, no scaling or filtering
extern i32 accelAligned[3]; // after alignment, no scaling or filtering
extern const fix32 *const gyroFiltered[3]; // gyro filter (currently PT1) deg/s
extern const fix32 *const accelFiltered[3]; // PT1 filters for the accelerometer data, m/s^2

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

void startGyroCalibration();

void getGyroCalibration(i16 cal[3]);

void startAccelCalibration();

void startImuAlignment();

void getImuAlignment(u8 axes[3]);
