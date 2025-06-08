#pragma once
#include "utils/fixedPointInt.h"
#include <Arduino.h>
#define AXIS_ROLL 1
#define AXIS_PITCH 0
#define AXIS_YAW 2

#define P_SHIFT 11
#define I_SHIFT 3
#define D_SHIFT 10
#define FF_SHIFT 8
#define S_SHIFT 8 // setpoint follow

extern fix32 gyroScaled[3]; // gyro data in deg/s
extern fix32 rateFactors[5][3]; // rate factors for the PID controller, 0 = x^1, 1 = x^2... (x normalized to +-1 at full deflection)
enum {
	P,
	I,
	D,
	FF,
	S,
};
extern fix32 pidGains[3][5]; // PID gains for the acro PID controller, 0 = roll, 1 = pitch, 2 = yaw
extern fix32 iFalloff; // I term is reduced by this value per second
extern fix32 pidGainsVVel[4]; // PID gains for the vertical velocity PID controller
extern fix32 pidGainsHVel[4]; // PID gains for the horizontal velocity PID controller
extern fix32 rollSetpoint, pitchSetpoint, yawSetpoint; // acro setpoint (deg/s)
extern fix32 rollError, pitchError, yawError; // acro rate error (deg/s)
extern fix32 rollLast, pitchLast, yawLast; // acro rate of last PID cycle (deg/s)
extern fix32 vVelSetpoint, vVelError, vVelLast; // vertical velocity PID states
extern fix32 eVelSetpoint, eVelError, eVelLast, nVelSetpoint, nVelError, nVelLast; // horizontal velocity PID states
extern fix32 rollP, pitchP, yawP, rollI, pitchI, yawI, rollD, pitchD, yawD, rollFF, pitchFF, yawFF, rollS, pitchS, yawS; // acro PID summands
extern fix32 vVelP, vVelI, vVelD, eVelP, eVelI, eVelD, nVelP, nVelI, nVelD; // velocity PID summands
extern fix64 rollErrorSum, pitchErrorSum, yawErrorSum, vVelErrorSum, eVelErrorSum, nVelErrorSum; // I term sum for the PID controller
extern fix32 altSetpoint; // altitude setpoint (m ASL)
extern fix32 throttle; // current throttle setpoint (idlePermille*2 to 2000)
extern fix32 smoothChannels[4]; // smoothed RC channel values (1000ish to 2000ish)
extern i16 throttles[4]; // throttle values for the motors (0-2000)
extern u32 pidLoopCounter; // counter of PID controller loops
extern fix64 targetLat, targetLon; // target latitude and longitude for GPS mode => (position lock)
extern u16 dFilterCutoff; // cutoff frequency for the D filter (Hz)
extern u16 gyroFilterCutoff; // cutoff frequency for the gyro filter (Hz)
extern fix32 setpointDiffCutoff; // used for feedforward and I term relaxation (Hz)
extern u16 idlePermille; // idle throttle in permille (0-1000)
extern fix32 hvelFfFilterCutoff; // cutoff frequency for the horizontal velocity feedforward filter (Hz)
extern fix32 hvelIRelaxFilterCutoff; // cutoff frequency for the horizontal velocity I term relax filter (Hz)
extern fix32 hvelPushFilterCutoff; // cutoff frequency for the horizontal velocity push filter (Hz)
extern fix32 vvelDFilterCutoff; // cutoff frequency for the vertical velocity D filter (Hz)
extern fix32 vvelFFFilterCutoff; // cutoff frequency for the vertical velocity feedforward filter (Hz)

enum class FlightMode {
	ACRO,
	ANGLE,
	ALT_HOLD,
	GPS, // control the velocity of the drone
	GPS_WP, // waypoint mode, i.e. GPS mode with automatic control, for now it just goes to the homepoint and hovers there
	LENGTH // place behind all other modes, acts as a limit for loops etc.
};
extern FlightMode flightMode; // currently selected flight mode (NOT whether the drone is armed)

/**
 * @brief PID controller loop
 *
 * @details 1. read the gyro data and convert to deg/s, 2. update attitude, 3. decode ERPM, 4. run pid controllers for the selected mode, 5. air mode, 6. send motor values, 7. blackbox logging
 */
void pidLoop();

/// @brief intialize PID terms and gains
void initPid();

/**
 * @brief calculates the distance in meters between two GPS coordinates
 *
 * Takes a trivial approach, which is not very accurate for large distances or when close to the poles.
 * It finds the shortest path on an equirectangular projection of the Earth.
 *
 * @param lat1 latitude of first coordinate in deg
 * @param lon1 longitude of first coordinate in deg
 * @param lat2 latitude of second coordinate in deg
 * @param lon2 longitude of second coordinate in deg
 * @param distNorth distance to go north from first to second coordinate
 * @param distEast distance to go east from first to second coordinate
 */
void distFromCoordinates(fix64 lat1, fix64 lon1, fix64 lat2, fix64 lon2, fix32 *distN, fix32 *distE);

/// @brief set and initialize a flight mode
/// the flight mode is applied without plausibility checks
void setFlightMode(FlightMode mode);
