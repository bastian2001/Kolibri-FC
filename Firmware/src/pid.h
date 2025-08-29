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
extern fix32 rateCoeffs[3][3]; // rate coefficients for the PID controller [axis][number type ACTUAL_xxx]
enum {
	P,
	I,
	D,
	FF,
	S,
};
extern u16 pidGainsNice[3][5]; // PID gains as configured for the acro PID controller, 0 = roll, 1 = pitch, 2 = yaw
extern fix32 iFalloff; // I term is reduced by this value per second
extern fix32 rollSetpoint, pitchSetpoint, yawSetpoint; // acro setpoint (deg/s), provided by control.cpp and used by pid.cpp
extern fix32 throttleSetpoint; // throttle value as set by the control functions (acro/angle/...)
extern fix32 rollP, pitchP, yawP, rollI, pitchI, yawI, rollD, pitchD, yawD, rollFF, pitchFF, yawFF, rollS, pitchS, yawS, rollSum, pitchSum, yawSum; // acro PID summands
extern fix32 throttle; // current throttle setpoint (idlePermille*2 to 2000)
extern PT1 gyroFiltered[3]; // gyro filter (currently PT1) deg/s
extern i16 throttles[4]; // throttle values for the motors (0-2000)
extern u16 dFilterCutoff; // cutoff frequency for the D filter (Hz)
extern u16 gyroFilterCutoff; // cutoff frequency for the gyro filter (Hz)
extern fix32 setpointDiffCutoff; // used for feedforward and I term relaxation (Hz)
extern u8 idlePermille; // idle throttle in permille (0-1000)
extern bool useDynamicIdle; // whether to use dynamic (fix RPM) or static (fix throttle) idle
extern u16 dynamicIdleRpm; // RPM the FC should target / not go below when the throttle is low

/// @brief converts nice PIDs to raw (fix32)
void convertPidsFromNice();

/**
 * @brief PID controller loop
 *
 * @details 1. read the gyro data and convert to deg/s, 2. update attitude, 3. decode ERPM, 4. run pid controllers for the selected mode, 5. air mode, 6. send motor values, 7. blackbox logging
 */
void pidLoop();

void pidDisarmedLoop();

/// @brief intialize PID terms and gains
void initPid();
