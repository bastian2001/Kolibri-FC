#pragma once
#include "PIO_DShot.h"
#include <Arduino.h>

// get dshot beep command for beeps 1-5
#define DSHOT_BEEP_CMD(i) (DSHOT_CMD_BEACON1 + i - 1)

#define MOTOR_POLES 14

extern u32 escRawTelemetry[4]; // raw telemetry values from the ESCs
extern u32 escRpm[4]; // current ESC RPM
extern u32 escTemp[4]; // last reported temperature from the ESCs
extern fix32 escVoltage[4]; // last reported voltage from the ESCs
extern u32 escCurrent[4]; // last reported current from the ESCs
extern u8 escErpmFail; // flags for failed RPM decoding
extern u32 escErpmFailCounter; // consecutive frames with at least one broken RPM measurement, reset to 0 when all RPMs are measured correctly
extern u16 dshotBeepTone; // tone for the ESC beeps (1-5)

/// @brief Initializes the ESC communication
void initESCs();

/// @brief Deinitializes the ESC communication, frees all resources
void deinitESCs();

/**
 * @brief set a new set of motor pins
 *
 * @param newPins array of new pins
 * @return true if everything worked out
 * @return false if there was an error (same pin twice)
 */
bool updateMotorPins(const u8 newPins[4]);

/// @brief writes the current motor pins into the provided buffer
void getMotorPins(u8 pins[4]);

/**
 * @brief Sends throttles to all four ESCs
 *
 * @details Telemetry bit is not set
 *
 * @param throttles Array of four throttle values (0-2000)
 */
void sendThrottles(const i16 throttles[4]);

/**
 * @brief Sends raw values to all four ESCs (useful for special commands)
 *
 * @details Telemetry bit always set
 *
 * @param raw Array of four raw values (0-2047, with 1-47 being the special commands, and the others being the throttle values)
 */
void sendRaw11Bit(const u16 raw[4]);

/**
 * @brief Sends raw values to all four ESCs (useful for special commands)
 *
 * @param raw Array of four raw values, including the telemetry bits and checksum
 */
void sendRaw16Bit(const u16 raw[4]);

/**
 * @brief Decodes the RPM values from the ESCs
 *
 * Stores them in escRpm array and sets or clears the corresponding bit in escErpmFail depending on whether the decoded value is valid (checksum correct)
 */
void decodeErpm();
