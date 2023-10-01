#pragma once
#include <Arduino.h>
#include "fixedPointInt.h"
#define AXIS_ROLL 1
#define AXIS_PITCH 0
#define AXIS_YAW 2

#define P_SHIFT 11
#define I_SHIFT 3
#define D_SHIFT 10
#define FF_SHIFT D_SHIFT
#define S_SHIFT 8 // setpoint follow

extern int16_t bmiDataRaw[6];
extern int16_t *gyroDataRaw;
extern int16_t *accelDataRaw;
extern fixedPointInt32 imuData[6];
extern fixedPointInt32 rateFactors[5][3];
enum class PID_GAINS
{
	P,
	I,
	D,
	FF,
	S,
	iFalloff
};
extern fixedPointInt32 pidGains[3][7];
extern fixedPointInt32 rollSetpoint, pitchSetpoint, yawSetpoint, rollError, pitchError, yawError, rollLast, pitchLast, yawLast, rollLastSetpoint, pitchLastSetpoint, yawLastSetpoint;
extern fixedPointInt32 rollP, pitchP, yawP, rollI, pitchI, yawI, rollD, pitchD, yawD, rollFF, pitchFF, yawFF, rollS, pitchS, yawS;
extern fixedPointInt64 rollErrorSum, pitchErrorSum, yawErrorSum;
extern uint16_t smoothChannels[4];
extern int16_t throttles[4];
extern uint32_t pidLoopCounter;

void pidLoop();

void initPID();