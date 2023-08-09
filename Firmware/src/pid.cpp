#include "global.h"

uint16_t bmiData[6] = {0, 0, 0, 0, 0, 0};
uint16_t *gyroData;
uint16_t *accelData;

/*
 * To avoid a lot of floating point math, fixed point math is used.
 * The gyro data is 16 bit, with a range of +/- 2000 degrees per second.
 * All data is converted to degrees per second (both RC commands as well as gyro data),
 * thus 11 bits + sign bit = 12 bit fixed point is used (12.20 fixed point int).
 */

uint16_t throttles[4] = {0, 0, 0, 0};

void pidLoop()
{
	gyroGetData(bmiData);
	if (ELRS->armed && ELRS->sinceLastRCMessage < 500000)
	{
		// Quad armed and RC connected
		// for now just send the RC throttle commands to the motors
		for (int i = 0; i < 4; i++)
			throttles[i] = map(ELRS->channels[2], 988, 2012, 0, 2000);
		sendThrottles(throttles);
	}
	else
	{
		// Quad disarmed or RC disconnected
		// all motors off
		for (int i = 0; i < 4; i++)
			throttles[i] = 0;
		sendThrottles(throttles);
		ELRS->armed = false;
	}
}