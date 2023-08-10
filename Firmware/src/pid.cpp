#include "global.h"

int16_t bmiDataRaw[6] = {0, 0, 0, 0, 0, 0};
int16_t *gyroDataRaw;
int16_t *accelDataRaw;

#define AXIS_ROLL 1
#define AXIS_PITCH 0
#define AXIS_YAW 2

/*
 * To avoid a lot of floating point math, fixed point math is used.
 * The gyro data is 16 bit, with a range of +/- 2000 degrees per second.
 * All data is converted to degrees per second (both RC commands as well as gyro data),
 * and calculations will be performed on a 16.16 fixed point number.
 * Additions can be performed like normal, while multiplications require
 * the numbers to be converted to 64 bit before calculation.
 * Accel data is also 16.16 bit fixed point math, just the unit is g.
 */

uint16_t throttles[4] = {0, 0, 0, 0};

int32_t imuData[6] = {0, 0, 0, 0, 0, 0};

int32_t kP = 0;
int32_t kI = 0;
int32_t kD = 0;

int32_t rollSetpoint, pitchSetpoint, yawSetpoint, rollError, pitchError, yawError, rollLast, pitchLast, yawLast;
int64_t rollErrorSum, pitchErrorSum, yawErrorSum;

int32_t floatToFixedPoint(float f)
{
	return (int32_t)(f * 65536.0f);
}

void initPID()
{
	kP = floatToFixedPoint(.01f);
	kI = floatToFixedPoint(.04f);
	kD = floatToFixedPoint(.1f);
}

int32_t multiply64(int64_t a, int64_t b) // 48.16 signed multiplication
{
	return (int32_t)((a * b) >> 16);
}
int32_t multiply(int32_t a, int32_t b) // 16.16 signed multiplication
{
	return (int32_t)(((int64_t)a * (int64_t)b) >> 16);
}
uint32_t multiply(uint32_t a, uint32_t b) // and unsigned version
{
	return (uint32_t)(((uint64_t)a * (uint64_t)b) >> 16);
}

elapsedMillis printPidTimer;
bool printPid = false;
void pidLoop()
{
	printPid = false;
	if (printPidTimer > 200)
	{
		printPidTimer = 0;
		printPid = true;
	}
	gyroGetData(bmiDataRaw);
	for (int i = 0; i < 3; i++)
	{
		imuData[i] = (int32_t)gyroDataRaw[i]; // gyro data in range of -.5 ... +.5 due to fixed point math
		imuData[i] *= 4000;					  // gyro data in range of -2000 ... +2000 (degrees per second)
		imuData[i + 3] = (int32_t)accelDataRaw[i + 3];
		imuData[i + 3] *= 32;
	}
	imuData[AXIS_ROLL] = -imuData[AXIS_ROLL];
	imuData[AXIS_YAW] = -imuData[AXIS_YAW];
	if (ELRS->armed)
	{
		// Quad armed
		rollSetpoint = ((int32_t)ELRS->channels[0] - 1500) << 16; // 500 deg per second linear range
		pitchSetpoint = ((int32_t)ELRS->channels[1] - 1500) << 16;
		yawSetpoint = ((int32_t)ELRS->channels[3] - 1500) << 16;
		rollError = rollSetpoint - imuData[AXIS_ROLL];
		pitchError = pitchSetpoint - imuData[AXIS_PITCH];
		yawError = yawSetpoint - imuData[AXIS_YAW];
		rollErrorSum += rollError;
		pitchErrorSum += pitchError;
		yawErrorSum += yawError;
		int32_t rollTerm = multiply(kP, rollError) + multiply64(kI, rollErrorSum) + multiply(kD, imuData[AXIS_ROLL] - rollLast);
		int32_t pitchTerm = multiply(kP, pitchError) + multiply64(kI, pitchErrorSum) + multiply(kD, imuData[AXIS_PITCH] - pitchLast);
		int32_t yawTerm = multiply(kP, yawError) + multiply64(kI, yawErrorSum) + multiply(kD, imuData[AXIS_YAW] - yawLast);
		if (printPid)
		{
			Serial.printf("\tP: %d\tI: %d\tD: %d\tTot: %d", multiply(kP, rollError) >> 16, multiply64(kI, rollErrorSum) >> 16, multiply(kD, imuData[AXIS_ROLL] - rollLast) >> 16, rollTerm >> 16);
		}
#ifdef PROPS_OUT
#else
		if (printPid)
		{
			Serial.printf("\t%d\t%d\t%d\t%d\t%d", rollTerm >> 16, pitchTerm >> 16, yawTerm >> 16, (ELRS->channels[2] - 1000) * 2, (ELRS->channels[2] - 1000) * 2 - (rollTerm >> 16) + (pitchTerm >> 16) - (yawTerm >> 16));
		}
		static int32_t tRR; // always recreating variables is slow, but exposing is bad, hence static
		static int32_t tRL;
		static int32_t tFR;
		static int32_t tFL;
		tRR = (ELRS->channels[2] - 1000) * 2 - (rollTerm >> 16) + (pitchTerm >> 16) - (yawTerm >> 16);
		tFR = (ELRS->channels[2] - 1000) * 2 - (rollTerm >> 16) - (pitchTerm >> 16) + (yawTerm >> 16);
		tRL = (ELRS->channels[2] - 1000) * 2 + (rollTerm >> 16) + (pitchTerm >> 16) + (yawTerm >> 16);
		tFL = (ELRS->channels[2] - 1000) * 2 + (rollTerm >> 16) - (pitchTerm >> 16) - (yawTerm >> 16);
		if (printPid)
		{
			Serial.printf("\t%d\t%d\t%d\t%d", tRR, tRL, tFR, tFL);
		}
#endif
		throttles[(uint8_t)MOTOR::RR] = constrain(tRR, 50, 2000);
		throttles[(uint8_t)MOTOR::RL] = constrain(tRL, 50, 2000);
		throttles[(uint8_t)MOTOR::FR] = constrain(tFR, 50, 2000);
		throttles[(uint8_t)MOTOR::FL] = constrain(tFL, 50, 2000);
		sendThrottles(throttles);
		rollLast = imuData[AXIS_ROLL];
		pitchLast = imuData[AXIS_PITCH];
		yawLast = imuData[AXIS_YAW];
		if (printPid)
		{
			Serial.println();
		}
	}
	else
	{
		// Quad disarmed or RC disconnected
		// all motors off
		for (int i = 0; i < 4; i++)
			throttles[i] = 0;
		sendThrottles(throttles);
		ELRS->armed = false;
		rollErrorSum = 0;
		pitchErrorSum = 0;
		yawErrorSum = 0;
		rollLast = 0;
		pitchLast = 0;
		yawLast = 0;
	}
}