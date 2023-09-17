#include "global.h"

int16_t bmiDataRaw[6] = {0, 0, 0, 0, 0, 0};
int16_t *gyroDataRaw;
int16_t *accelDataRaw;

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

enum
{
	P,
	I,
	D,
	FF,
	S,
	iFalloff
};
int32_t pidGains[3][7];

int32_t rollSetpoint, pitchSetpoint, yawSetpoint, rollError, pitchError, yawError, rollLast, pitchLast, yawLast, rollLastSetpoint, pitchLastSetpoint, yawLastSetpoint;
int64_t rollErrorSum, pitchErrorSum, yawErrorSum;
int32_t rollP, pitchP, yawP, rollI, pitchI, yawI, rollD, pitchD, yawD, rollFF, pitchFF, yawFF, rollS, pitchS, yawS;
int32_t tRR, tRL, tFR, tFL;

uint32_t pidLoopCounter = 0;

int32_t rateFactors[5][3];
uint16_t smoothChannels[4];

int32_t floatToFixedPoint(float f)
{
	return (int32_t)(f * 65536.0f);
}

void initPID()
{
	for (int i = 0; i < 3; i++)
	{
		pidGains[i][P] = 40 << P_SHIFT;
		pidGains[i][I] = 20 << I_SHIFT;
		pidGains[i][D] = 100 << D_SHIFT;
		pidGains[i][FF] = 0 << FF_SHIFT;
		pidGains[i][S] = 0 << S_SHIFT;
		pidGains[i][iFalloff] = floatToFixedPoint(.998);
	}
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
		if ((pidLoopCounter % BB_FREQ_DIVIDER) == 0)
		{
			// Serial.printf("%8d %8d %8d\n", imuData[AXIS_ROLL] >> 16, imuData[AXIS_PITCH] >> 16, imuData[AXIS_YAW] >> 16);
			// Serial.printf("%04X %04X %04X\n", gyroDataRaw[AXIS_ROLL], gyroDataRaw[AXIS_PITCH], gyroDataRaw[AXIS_YAW]);
		}
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
				if (polynomials[0][j] < 0) // on second and fourth order, preserve initial sign
					polynomials[i][j] = -polynomials[i][j];
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
			rollErrorSum = multiply6464(rollErrorSum, pidGains[0][iFalloff]);
			pitchErrorSum = multiply6464(pitchErrorSum, pidGains[1][iFalloff]);
			yawErrorSum = multiply6464(yawErrorSum, pidGains[2][iFalloff]);
		}

		rollErrorSum += rollError;
		pitchErrorSum += pitchError;
		yawErrorSum += yawError;
		rollP = multiply(pidGains[0][P], rollError);
		pitchP = multiply(pidGains[1][P], pitchError);
		yawP = multiply(pidGains[2][P], yawError);
		rollI = multiply64(pidGains[0][I], rollErrorSum);
		pitchI = multiply64(pidGains[1][I], pitchErrorSum);
		yawI = multiply64(pidGains[2][I], yawErrorSum);
		rollD = multiply(pidGains[0][D], imuData[AXIS_ROLL] - rollLast);
		pitchD = multiply(pidGains[1][D], imuData[AXIS_PITCH] - pitchLast);
		yawD = multiply(pidGains[2][D], imuData[AXIS_YAW] - yawLast);
		rollFF = multiply(pidGains[0][FF], rollSetpoint - rollLastSetpoint);
		pitchFF = multiply(pidGains[1][FF], pitchSetpoint - pitchLastSetpoint);
		yawFF = multiply(pidGains[2][FF], yawSetpoint - yawLastSetpoint);
		rollS = multiply(pidGains[0][S], rollSetpoint);
		pitchS = multiply(pidGains[1][S], pitchSetpoint);
		yawS = multiply(pidGains[2][S], yawSetpoint);
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
		if (throttles[(uint8_t)MOTOR::RR] > 2000)
		{
			int16_t diff = throttles[(uint8_t)MOTOR::RR] - 2000;
			throttles[(uint8_t)MOTOR::RR] = 2000;
			throttles[(uint8_t)MOTOR::FL] -= diff;
			throttles[(uint8_t)MOTOR::FR] -= diff;
			throttles[(uint8_t)MOTOR::RL] -= diff;
		}
		if (throttles[(uint8_t)MOTOR::RL] > 2000)
		{
			int16_t diff = throttles[(uint8_t)MOTOR::RL] - 2000;
			throttles[(uint8_t)MOTOR::RL] = 2000;
			throttles[(uint8_t)MOTOR::FL] -= diff;
			throttles[(uint8_t)MOTOR::FR] -= diff;
			throttles[(uint8_t)MOTOR::RR] -= diff;
		}
		if (throttles[(uint8_t)MOTOR::FR] > 2000)
		{
			int16_t diff = throttles[(uint8_t)MOTOR::FR] - 2000;
			throttles[(uint8_t)MOTOR::FR] = 2000;
			throttles[(uint8_t)MOTOR::FL] -= diff;
			throttles[(uint8_t)MOTOR::RL] -= diff;
			throttles[(uint8_t)MOTOR::RR] -= diff;
		}
		if (throttles[(uint8_t)MOTOR::FL] > 2000)
		{
			int16_t diff = throttles[(uint8_t)MOTOR::FL] - 2000;
			throttles[(uint8_t)MOTOR::FL] = 2000;
			throttles[(uint8_t)MOTOR::RL] -= diff;
			throttles[(uint8_t)MOTOR::RR] -= diff;
			throttles[(uint8_t)MOTOR::FR] -= diff;
		}
		if (throttles[(uint8_t)MOTOR::RR] < IDLE_PERMILLE * 2)
		{
			int16_t diff = IDLE_PERMILLE * 2 - throttles[(uint8_t)MOTOR::RR];
			throttles[(uint8_t)MOTOR::RR] = IDLE_PERMILLE * 2;
			throttles[(uint8_t)MOTOR::FL] += diff;
			throttles[(uint8_t)MOTOR::FR] += diff;
			throttles[(uint8_t)MOTOR::RL] += diff;
		}
		if (throttles[(uint8_t)MOTOR::RL] < IDLE_PERMILLE * 2)
		{
			int16_t diff = IDLE_PERMILLE * 2 - throttles[(uint8_t)MOTOR::RL];
			throttles[(uint8_t)MOTOR::RL] = IDLE_PERMILLE * 2;
			throttles[(uint8_t)MOTOR::FL] += diff;
			throttles[(uint8_t)MOTOR::FR] += diff;
			throttles[(uint8_t)MOTOR::RR] += diff;
		}
		if (throttles[(uint8_t)MOTOR::FR] < IDLE_PERMILLE * 2)
		{
			int16_t diff = IDLE_PERMILLE * 2 - throttles[(uint8_t)MOTOR::FR];
			throttles[(uint8_t)MOTOR::FR] = IDLE_PERMILLE * 2;
			throttles[(uint8_t)MOTOR::FL] += diff;
			throttles[(uint8_t)MOTOR::RL] += diff;
			throttles[(uint8_t)MOTOR::RR] += diff;
		}
		if (throttles[(uint8_t)MOTOR::FL] < IDLE_PERMILLE * 2)
		{
			int16_t diff = IDLE_PERMILLE * 2 - throttles[(uint8_t)MOTOR::FL];
			throttles[(uint8_t)MOTOR::FL] = IDLE_PERMILLE * 2;
			throttles[(uint8_t)MOTOR::RL] += diff;
			throttles[(uint8_t)MOTOR::RR] += diff;
			throttles[(uint8_t)MOTOR::FR] += diff;
		}
		for (int i = 0; i < 4; i++)
			throttles[i] = throttles[i] > 2000 ? 2000 : throttles[i];
		sendThrottles(throttles);
		rollLast = imuData[AXIS_ROLL];
		pitchLast = imuData[AXIS_PITCH];
		yawLast = imuData[AXIS_YAW];
		rollLastSetpoint = rollSetpoint;
		pitchLastSetpoint = pitchSetpoint;
		yawLastSetpoint = yawSetpoint;
		if ((pidLoopCounter % BB_FREQ_DIVIDER) == 0)
			writeSingleFrame();
		pidLoopCounter++;
	}
	else
	{
		// Quad disarmed or RC disconnected
		// all motors off
		if (configOverrideMotors > 1000)
			for (int i = 0; i < 4; i++)
				throttles[i] = 0;
		if (ELRS->channels[9] < 1500)
			sendThrottles(throttles);
		else
		{
			static elapsedMillis motorBeepTimer = 0;
			if (motorBeepTimer > 500)
				motorBeepTimer = 0;
			if (motorBeepTimer < 200)
			{
				uint16_t motors[4] = {DSHOT_CMD_BEACON2, DSHOT_CMD_BEACON2, DSHOT_CMD_BEACON2, DSHOT_CMD_BEACON2};
				sendRaw11Bit(motors);
			}
			else
			{
				uint16_t motors[4] = {0, 0, 0, 0};
				sendRaw11Bit(motors);
			}
		}
		rollErrorSum = 0;
		pitchErrorSum = 0;
		yawErrorSum = 0;
		rollLast = 0;
		pitchLast = 0;
		yawLast = 0;
		takeoffCounter = 0;
	}
}