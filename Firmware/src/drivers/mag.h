#include "typedefs.h"

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

/// @brief Initializes the magnetometer (HMC5883L)
void initMag();

/**
 * @brief Magnetometer state machine
 *
 * @details Called periodically, this function checks if new data is available from the magnetometer, then reads and processes it
 */
void magLoop();

extern u32 magStateAfterRead; // set to MAG_CALIBRATE or MAG_PROCESS_DATA to start calibration or run the normal operation
extern i16 magOffset[3]; // offset that gets subtracted from the magnetometer values
extern fix32 magHeading; // heading in radians
extern i32 magData[3]; // raw magnetometer data after subtracting offset
extern fix32 magX, magY; // magnetometer in earth's right and rearward direction

enum MAG_STATES {
	MAG_NOT_INIT = 0,
	MAG_INIT,
	MAG_MEASURING,
	MAG_SOON_READY,
	MAG_CHECK_DATA_READY,
	MAG_READ_DATA,
	MAG_PROCESS_DATA,
	MAG_CALIBRATE,
	MAG_PROCESS_CALIBRATION
};