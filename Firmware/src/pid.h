#pragma once
#include <Arduino.h>
#include <fixedPointInt.h>
#define AXIS_ROLL 1
#define AXIS_PITCH 0
#define AXIS_YAW 2

#define P_SHIFT 11
#define I_SHIFT 3
#define D_SHIFT 16
#define FF_SHIFT 8
#define S_SHIFT 8 // setpoint follow

extern fix32 gyroScaled[3]; // gyro data in deg/s
enum {
	ACTUAL_CENTER_SENSITIVITY, // center sensitivity for ACTUAL rates => linear part, in deg/s
	ACTUAL_MAX_RATE, // maximum rate for ACTUAL rates => including exponential part, in deg/s
	ACTUAL_EXPO // expo factor for ACTUAL rates, between 0 and 1
};
extern fix32 rateCoeffs[3][3]; // rate coefficients for the PID controller [axis][number type ACTUAL_xxx]
enum {
	P,
	I,
	D,
	FF,
	S,
};
extern u16 pidGainsNice[3][5]; // PID gains as configured for the acro PID controller, 0 = roll, 1 = pitch, 2 = yaw
extern fix32 pidGains[3][5]; // PID gains (raw, calculated) for the acro PID controller, 0 = roll, 1 = pitch, 2 = yaw
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
extern PT1 gyroFiltered[3]; // gyro filter (currently PT1) deg/s
extern i16 throttles[4]; // throttle values for the motors (0-2000)
extern u32 pidLoopCounter; // counter of PID controller loops
extern fix64 targetLat, targetLon; // target latitude and longitude for GPS mode => (position lock)
extern u16 dFilterCutoff; // cutoff frequency for the D filter (Hz)
extern u16 gyroFilterCutoff; // cutoff frequency for the gyro filter (Hz)
extern fix32 setpointDiffCutoff; // used for feedforward and I term relaxation (Hz)
extern u8 idlePermille; // idle throttle in permille (0-1000)
extern fix32 hvelFfFilterCutoff; // cutoff frequency for the horizontal velocity feedforward filter (Hz)
extern fix32 hvelIRelaxFilterCutoff; // cutoff frequency for the horizontal velocity I term relax filter (Hz)
extern fix32 hvelPushFilterCutoff; // cutoff frequency for the horizontal velocity push filter (Hz)
extern fix32 vvelDFilterCutoff; // cutoff frequency for the vertical velocity D filter (Hz)
extern fix32 vvelFFFilterCutoff; // cutoff frequency for the vertical velocity feedforward filter (Hz)
extern bool useDynamicIdle; // whether to use dynamic (fix RPM) or static (fix throttle) idle
extern u16 dynamicIdleRpm; // RPM the FC should target / not go below when the throttle is low

enum class FlightMode {
	ACRO,
	ANGLE,
	ALT_HOLD,
	GPS, // control the velocity of the drone
	GPS_WP, // waypoint mode, i.e. GPS mode with automatic control, for now it just goes to the homepoint and hovers there
	LENGTH // place behind all other modes, acts as a limit for loops etc.
};
extern FlightMode flightMode; // currently selected flight mode (NOT whether the drone is armed)

/// @brief converts nice PIDs to raw (fix32)
void convertPidsFromNice();

/**
 * @brief PID controller loop
 *
 * @details 1. read the gyro data and convert to deg/s, 2. update attitude, 3. decode ERPM, 4. run pid controllers for the selected mode, 5. air mode, 6. send motor values, 7. blackbox logging
 */
void pidLoop();

/// @brief intialize PID terms and gains
void initPid();
