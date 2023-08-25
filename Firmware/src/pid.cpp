#include "global.h"

int16_t bmiDataRaw[6] = {0, 0, 0, 0, 0, 0};
int16_t *gyroDataRaw;
int16_t *accelDataRaw;

#define P_SHIFT 11
#define I_SHIFT 3
#define D_SHIFT 10
#define FF_SHIFT D_SHIFT
#define S_SHIFT 8 // setpoint follow
#define IDLE_PERMILLE 25

/*
 * To avoid a lot of floating point math, fixed point math is used.
 * The gyro data is 16 bit, with a range of +/- 2000 degrees per second.
 * All data is converted to degrees per second (both RC commands as well as gyro data),
 * and calculations will be performed on a 16.16 fixed point number.
 * Additions can be performed like normal, while multiplications require
 * the numbers to be converted to 64 bit before calculation.
 * Accel data is also 16.16 bit fixed point math, just the unit is g.
 */

int16_t throttles[4];

int32_t imuData[6] = {0, 0, 0, 0, 0, 0};

int32_t kP = 0;
int32_t kI = 0;
int32_t kD = 0;
int32_t kFF = 0;
int32_t kS = 0;
int32_t iFalloff = 0;

int32_t rollSetpoint, pitchSetpoint, yawSetpoint, rollError, pitchError, yawError, rollLast, pitchLast, yawLast, rollLastSetpoint, pitchLastSetpoint, yawLastSetpoint;
int64_t rollErrorSum, pitchErrorSum, yawErrorSum;
int32_t rollP, pitchP, yawP, rollI, pitchI, yawI, rollD, pitchD, yawD, rollFF, pitchFF, yawFF, rollS, pitchS, yawS;
int32_t tRR, tRL, tFR, tFL;

uint32_t pidLoopCounter = 0;

int32_t rateFactors[5][3];
uint16_t smoothChannels[4];

int32_t
floatToFixedPoint(float f)
{
	return (int32_t)(f * 65536.0f);
}

void initPID()
{
	kP = 100 << P_SHIFT;
	kI = 80 << I_SHIFT;
	kD = 30 << D_SHIFT;
	kFF = 100 << FF_SHIFT;
	kS = 25 << S_SHIFT;
	iFalloff = floatToFixedPoint(.998);
	for (int i = 0; i < 3; i++)
	{
		rateFactors[0][i] = floatToFixedPoint(100); // first order, center rate
		rateFactors[1][i] = floatToFixedPoint(0);
		rateFactors[2][i] = floatToFixedPoint(200);
		rateFactors[3][i] = floatToFixedPoint(0);
		rateFactors[4][i] = floatToFixedPoint(800);
	}
}

int64_t multiply6464(int64_t a, int64_t b) // 48.16 signed multiplication
{
	return (int64_t)((a * b) >> 16);
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

uint32_t takeoffCounter = 0;
void pidLoop()
{
	gyroGetData(bmiDataRaw);
	for (int i = 0; i < 3; i++)
	{
		imuData[i] = (int32_t)gyroDataRaw[i]; // gyro data in range of -.5 ... +.5 due to fixed point math
		imuData[i] *= 4000;					  // gyro data in range of -2000 ... +2000 (degrees per second)
		imuData[i + 3] = (int32_t)accelDataRaw[i];
		imuData[i + 3] *= 32;
	}
	imuData[AXIS_ROLL] = -imuData[AXIS_ROLL];
	imuData[AXIS_YAW] = -imuData[AXIS_YAW];
	if (ELRS->armed)
	{
		// Quad armed
		static int32_t polynomials[5][3]; // always recreating variables is slow, but exposing is bad, hence static
		ELRS->getSmoothChannels(smoothChannels);
		// calculate setpoints
		polynomials[0][0] = ((int32_t)smoothChannels[0] - 1500) << 7; //-1...+1 in fixed point notation;
		polynomials[0][1] = ((int32_t)smoothChannels[1] - 1500) << 7;
		polynomials[0][2] = ((int32_t)smoothChannels[3] - 1500) << 7;
		/* at full stick deflection, ...Raw values are either +1 or -1. That will make all the
		 * polynomials also +/-1. Thus, the total rate for each axis is equal to the sum of all 5 rateFactors
		 * of that axis. The center rate is the ratefactor[x][0].
		 */
		for (int i = 1; i < 5; i++)
		{
			for (int j = 0; j < 3; j++)
			{
				polynomials[i][j] = multiply64(polynomials[i - 1][j], polynomials[0][j]);
				if ((j & 1) && polynomials[0][j] < 0) // on second and fourth order, preserve initial sign
					polynomials[i][j] *= -1;
			}
		}
		rollSetpoint = 0;
		pitchSetpoint = 0;
		yawSetpoint = 0;
		for (int i = 0; i < 5; i++)
		{
			rollSetpoint += multiply(rateFactors[i][0], polynomials[i][0]);
			pitchSetpoint += multiply(rateFactors[i][1], polynomials[i][1]);
			yawSetpoint += multiply(rateFactors[i][2], polynomials[i][2]);
		}
		// Serial.printf("%d %d %d\n", rollSetpoint >> 16, pitchSetpoint >> 16, yawSetpoint >> 16);
		rollError = rollSetpoint - imuData[AXIS_ROLL];
		pitchError = pitchSetpoint - imuData[AXIS_PITCH];
		yawError = yawSetpoint - imuData[AXIS_YAW];
		if (ELRS->channels[2] > 1020)
			takeoffCounter++;
		else if (takeoffCounter < 1000) // 1000 = ca. 0.6s
			takeoffCounter = 0;			// if the quad hasn't "taken off" yet, reset the counter
		if (takeoffCounter < 1000)		// enable i term falloff (windup prevention) only before takeoff
		{
			rollErrorSum = multiply6464(rollErrorSum, iFalloff);
			pitchErrorSum = multiply6464(pitchErrorSum, iFalloff);
			yawErrorSum = multiply6464(yawErrorSum, iFalloff);
		}

		rollErrorSum += rollError;
		pitchErrorSum += pitchError;
		yawErrorSum += yawError;
		rollP = multiply(kP, rollError);
		pitchP = multiply(kP, pitchError);
		yawP = multiply(kP, yawError);
		rollI = multiply64(kI, rollErrorSum);
		pitchI = multiply64(kI, pitchErrorSum);
		yawI = multiply64(kI, yawErrorSum);
		rollD = multiply(kD, imuData[AXIS_ROLL] - rollLast);
		pitchD = multiply(kD, imuData[AXIS_PITCH] - pitchLast);
		yawD = multiply(kD, imuData[AXIS_YAW] - yawLast);
		rollFF = multiply(kFF, rollSetpoint - rollLastSetpoint);
		pitchFF = multiply(kFF, pitchSetpoint - pitchLastSetpoint);
		yawFF = multiply(kFF, yawSetpoint - yawLastSetpoint);
		rollS = multiply(kS, rollSetpoint);
		pitchS = multiply(kS, pitchSetpoint);
		yawS = multiply(kS, yawSetpoint);
		int32_t rollTerm = rollP + rollI + rollD + rollFF + rollS;
		int32_t pitchTerm = pitchP + pitchI + pitchD + pitchFF + pitchS;
		int32_t yawTerm = yawP + yawI + yawD + yawFF + yawS;
#ifdef PROPS_OUT
		tRR = (smoothChannels[2] - 1000) * 2 - (rollTerm >> 16) + (pitchTerm >> 16) + (yawTerm >> 16);
		tFR = (smoothChannels[2] - 1000) * 2 - (rollTerm >> 16) - (pitchTerm >> 16) - (yawTerm >> 16);
		tRL = (smoothChannels[2] - 1000) * 2 + (rollTerm >> 16) + (pitchTerm >> 16) - (yawTerm >> 16);
		tFL = (smoothChannels[2] - 1000) * 2 + (rollTerm >> 16) - (pitchTerm >> 16) + (yawTerm >> 16);
#else
		tRR = (smoothChannels[2] - 1000) * 2 - (rollTerm >> 16) + (pitchTerm >> 16) - (yawTerm >> 16);
		tFR = (smoothChannels[2] - 1000) * 2 - (rollTerm >> 16) - (pitchTerm >> 16) + (yawTerm >> 16);
		tRL = (smoothChannels[2] - 1000) * 2 + (rollTerm >> 16) + (pitchTerm >> 16) + (yawTerm >> 16);
		tFL = (smoothChannels[2] - 1000) * 2 + (rollTerm >> 16) - (pitchTerm >> 16) - (yawTerm >> 16);
#endif
		throttles[(uint8_t)MOTOR::RR] = map(tRR, 0, 2000, IDLE_PERMILLE * 2, 2000);
		throttles[(uint8_t)MOTOR::RL] = map(tRL, 0, 2000, IDLE_PERMILLE * 2, 2000);
		throttles[(uint8_t)MOTOR::FR] = map(tFR, 0, 2000, IDLE_PERMILLE * 2, 2000);
		throttles[(uint8_t)MOTOR::FL] = map(tFL, 0, 2000, IDLE_PERMILLE * 2, 2000);
		if (throttles[(uint8_t)MOTOR::RR] < IDLE_PERMILLE * 2)
			throttles[(uint8_t)MOTOR::FL] += IDLE_PERMILLE * 2 - throttles[(uint8_t)MOTOR::RR];
		if (throttles[(uint8_t)MOTOR::FR] < IDLE_PERMILLE * 2)
			throttles[(uint8_t)MOTOR::RL] += IDLE_PERMILLE * 2 - throttles[(uint8_t)MOTOR::FR];
		if (throttles[(uint8_t)MOTOR::RL] < IDLE_PERMILLE * 2)
			throttles[(uint8_t)MOTOR::FR] += IDLE_PERMILLE * 2 - throttles[(uint8_t)MOTOR::RL];
		if (throttles[(uint8_t)MOTOR::FL] < IDLE_PERMILLE * 2)
			throttles[(uint8_t)MOTOR::RR] += IDLE_PERMILLE * 2 - throttles[(uint8_t)MOTOR::FL];
		throttles[(uint8_t)MOTOR::RR] = constrain(throttles[(uint8_t)MOTOR::RR], IDLE_PERMILLE * 2, 2000);
		throttles[(uint8_t)MOTOR::FR] = constrain(throttles[(uint8_t)MOTOR::FR], IDLE_PERMILLE * 2, 2000);
		throttles[(uint8_t)MOTOR::RL] = constrain(throttles[(uint8_t)MOTOR::RL], IDLE_PERMILLE * 2, 2000);
		throttles[(uint8_t)MOTOR::FL] = constrain(throttles[(uint8_t)MOTOR::FL], IDLE_PERMILLE * 2, 2000);
		sendThrottles(throttles);
		rollLast = imuData[AXIS_ROLL];
		pitchLast = imuData[AXIS_PITCH];
		yawLast = imuData[AXIS_YAW];
		rollLastSetpoint = rollSetpoint;
		pitchLastSetpoint = pitchSetpoint;
		yawLastSetpoint = yawSetpoint;
		if ((pidLoopCounter & 1) == 0)
			writeSingleFrame();
		pidLoopCounter++;
	}
	else
	{
		// Quad disarmed or RC disconnected
		// all motors off
		for (int i = 0; i < 4; i++)
			throttles[i] = 0;
		sendThrottles(throttles);
		rollErrorSum = 0;
		pitchErrorSum = 0;
		yawErrorSum = 0;
		rollLast = 0;
		pitchLast = 0;
		yawLast = 0;
		takeoffCounter = 0;
	}
}