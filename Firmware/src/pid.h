#pragma once
#include <Arduino.h>
#define AXIS_ROLL 1
#define AXIS_PITCH 0
#define AXIS_YAW 2

extern int16_t bmiDataRaw[6];
extern int16_t *gyroDataRaw;
extern int16_t *accelDataRaw;
extern int32_t imuData[6];
extern int32_t rateFactors[5][3];
extern int32_t kP, kI, kD, kFF, kS, iFalloff;
extern int32_t rollSetpoint, pitchSetpoint, yawSetpoint, rollError, pitchError, yawError, rollLast, pitchLast, yawLast, rollLastSetpoint, pitchLastSetpoint, yawLastSetpoint;
extern int32_t rollP, pitchP, yawP, rollI, pitchI, yawI, rollD, pitchD, yawD, rollFF, pitchFF, yawFF, rollS, pitchS, yawS;
extern int64_t rollErrorSum, pitchErrorSum, yawErrorSum;
extern uint16_t smoothChannels[4];
extern int32_t tRR, tRL, tFR, tFL;
extern int16_t throttles[4];
extern uint32_t pidLoopCounter;

void pidLoop();

void initPID();