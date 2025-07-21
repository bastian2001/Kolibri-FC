#include "typedefs.h"

#define MAG_HMC5883L 0
#define MAG_QMC5883L 1
#if MAG_HARDWARE == MAG_QMC5883L
#define MAG_ADDRESS (0x0D)

enum class MAG_REG {
	DATA_X_L = 0x00,
	DATA_X_H,
	DATA_Y_L,
	DATA_Y_H,
	DATA_Z_L,
	DATA_Z_H,
	STATUS,
	TEMP_L,
	TEMP_H,
	CONTROL_1,
	CONTROL_2,
	SET_RESET,
	ID = 0x0D
};

#define MAG_OSR_512 (0b00 << 6) // configure mag oversampling rate, write into CONTROL_1
#define MAG_OSR_256 (0b01 << 6) // configure mag oversampling rate, write into CONTROL_1
#define MAG_OSR_128 (0b10 << 6) // configure mag oversampling rate, write into CONTROL_1
#define MAG_OSR_64 (0b11 << 6) // configure mag oversampling rate, write into CONTROL_1
#define MAG_RANGE_2 (0b00 << 4) // configure mag range, write into CONTROL_1
#define MAG_RANGE_8 (0b01 << 4) // configure mag range, write into CONTROL_1
#define MAG_ODR_10HZ (0b00 << 2) // configure mag output data rate, write into CONTROL_1
#define MAG_ODR_50HZ (0b01 << 2) // configure mag output data rate, write into CONTROL_1
#define MAG_ODR_100HZ (0b10 << 2) // configure mag output data rate, write into CONTROL_1
#define MAG_ODR_200HZ (0b11 << 2) // configure mag output data rate, write into CONTROL_1
#define MAG_MODE_CONTINUOUS 0b01 // configure mag for continuous or single measurement mode, write into CONTROL_1
#define MAG_MODE_SINGLE 0b00 // configure mag for continuous or single measurement mode, write into CONTROL_1
#define MAG_SOFT_RST (0b1 << 7) // write into CONTROL_2 to reset the sensor
#define MAG_ROL_PNT (0b1 << 6) // write into CONTROL_2 to roll over pointer from Z to X
#define MAG_INT_EN (0b1 << 0) // write into CONTROL_2 to enable interrupt
#elif MAG_HARDWARE == MAG_HMC5883L
// driver for HMC5883L magnetometer

#define MAG_ADDRESS (0x1E)

enum class MAG_REG {
	CONF_REGA = 0x00,
	CONF_REGB,
	MODE,
	DATA_X_H,
	DATA_X_L,
	DATA_Z_H,
	DATA_Z_L,
	DATA_Y_H,
	DATA_Y_L,
	STATUS,
	ID_A,
	ID_B,
	ID_C
};

#define MAG_AVG_1 (0b00 << 5) // configure mag averaging, write into CONF_REG_A
#define MAG_AVG_2 (0b01 << 5) // configure mag averaging, write into CONF_REG_A
#define MAG_AVG_4 (0b10 << 5) // configure mag averaging, write into CONF_REG_A
#define MAG_AVG_8 (0b11 << 5) // configure mag averaging, write into CONF_REG_A
#define MAG_ODR_0_75HZ (0b000 << 2) // configure mag output data rate, write into CONF_REG_A
#define MAG_ODR_1_5HZ (0b001 << 2) // configure mag output data rate, write into CONF_REG_A
#define MAG_ODR_3HZ (0b010 << 2) // configure mag output data rate, write into CONF_REG_A
#define MAG_ODR_7_5HZ (0b011 << 2) // configure mag output data rate, write into CONF_REG_A
#define MAG_ODR_15HZ (0b100 << 2) // configure mag output data rate, write into CONF_REG_A
#define MAG_ODR_30HZ (0b101 << 2) // configure mag output data rate, write into CONF_REG_A
#define MAG_ODR_75HZ (0b110 << 2) // configure mag output data rate, write into CONF_REG_A
#define MAG_LOAD_FLOAT 0b00 // configure mag testing with artificial magnetic field, write into CONF_REG_A
#define MAG_LOAD_POSITIVE 0b01 // configure mag testing with artificial magnetic field, write into CONF_REG_A
#define MAG_LOAD_NEGATIVE 0b10 // configure mag testing with artificial magnetic field, write into CONF_REG_A
#define MAG_RANGE_0_88 (0b000 << 5) // configure mag range, write into CONF_REG_B
#define MAG_RANGE_1_3 (0b001 << 5) // configure mag range, write into CONF_REG_B
#define MAG_RANGE_1_9 (0b010 << 5) // configure mag range, write into CONF_REG_B
#define MAG_RANGE_2_5 (0b011 << 5) // configure mag range, write into CONF_REG_B
#define MAG_RANGE_4 (0b100 << 5) // configure mag range, write into CONF_REG_B
#define MAG_RANGE_4_7 (0b101 << 5) // configure mag range, write into CONF_REG_B
#define MAG_RANGE_5_6 (0b110 << 5) // configure mag range, write into CONF_REG_B
#define MAG_RANGE_8_1 (0b111 << 5) // configure mag range, write into CONF_REG_B
#define MAG_MODE_CONTINUOUS 0b00 // configure mag for continuous or single measurement mode, write into MODE
#define MAG_MODE_SINGLE 0b01 // configure mag for continuous or single measurement mode, write into MODE
#define MAG_MODE_IDLE 0b10 // configure mag for continuous or single measurement mode, write into MODE
#define MAG_MODE_HS_I2C (0b1 << 7) // datasheet is unclear: "does not support high speed, only 400kHz", but then this bit can allegedly enable 3400kHz I2C? Even worse: 3400kHz seems to work regardless of this bit

#endif
enum class MagState {
	NOT_INIT = 0,
	INITIALIZING,
	MEASURING,
	CHECK_DATA_READY,
	READ_DATA,
	PROCESS_DATA,
	CALIBRATE,
	PROCESS_CALIBRATION
};

/// @brief Initializes the magnetometer
void initMag();

/**
 * @brief Magnetometer state machine
 *
 * @details Called periodically, this function checks if new data is available from the magnetometer, then reads and processes it
 */
void magLoop();

extern MagState magStateAfterRead; // set to MagState::CALIBRATE or MagState::PROCESS_DATA to start calibration or run the normal operation
extern i16 magOffset[3]; // offset that gets subtracted from the magnetometer values
extern fix32 magHeading; // heading in radians
extern i32 magData[3]; // raw magnetometer data after subtracting offset
extern fix32 magRight, magFront; // magnetometer in earth's right and rearward direction
