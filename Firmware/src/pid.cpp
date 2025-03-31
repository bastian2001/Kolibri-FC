#include "global.h"

i16 bmiDataRaw[6] = {0, 0, 0, 0, 0, 0};
i16 *gyroDataRaw;
i16 *accelDataRaw;
FlightMode flightMode = FlightMode::ACRO;

#define MAX_ANGLE 40 // degrees, applied in angle mode and GPS mode
#define MAX_ANGLE_BURST 60 // degrees, this angle is allowed for a short time, e.g. when accelerating in GPS mode (NOT used in angle mode)
#define BURST_DURATION 3000 // ms
#define BURST_COOLDOWN 5000 // ms

#define HVEL_STICK_DEADBAND 30
#define MAX_TARGET_HVEL 12

/*
 * To avoid a lot of floating point math, fixed point math is used.
 * The gyro data is 16 bit, with a range of +/- 2000 degrees per second.
 * All data is converted to degrees per second (both RC commands as well as gyro data),
 * and calculations will be performed on a 16.16 fixed point number.
 * Additions can be performed like normal, while multiplications require
 * the numbers to be converted to 64 bit before calculation.
 */

i16 throttles[4];

fix32 gyroData[3];

fix32 pidGains[3][7];
fix32 pidGainsVVel[4], pidGainsHVel[4];
fix32 angleModeP = 10, velocityModeP = 10;

fix32 rollSetpoint, pitchSetpoint, yawSetpoint, rollError, pitchError, yawError, rollLast, pitchLast, yawLast, vVelSetpoint, vVelError, vVelLast, eVelSetpoint, eVelError, eVelLast, nVelSetpoint, nVelError, nVelLast, vVelLastSetpoint;
fix64 rollErrorSum, pitchErrorSum, yawErrorSum, vVelErrorSum, eVelErrorSum, nVelErrorSum;
fix32 rollP, pitchP, yawP, rollI, pitchI, yawI, rollD, pitchD, yawD, rollFF, pitchFF, yawFF, rollS, pitchS, yawS, vVelP, vVelI, vVelD, vVelFF, eVelP, eVelI, eVelD, eVelFF, nVelP, nVelI, nVelD, nVelFF;
fix32 altSetpoint;
fix32 throttle;
fix64 targetLat, targetLon;
PT1 dFilterRoll(100, 3200), dFilterPitch(100, 3200), dFilterYaw(100, 3200);

fix32 rollSetpoints[8], pitchSetpoints[8], yawSetpoints[8];

u32 pidLoopCounter = 0;

fix32 rateFactors[5][3];
fix64 vVelMaxErrorSum, vVelMinErrorSum;
constexpr fix32 TO_ANGLE = fix32(MAX_ANGLE) / fix32(512);
constexpr fix32 THROTTLE_SCALE = fix32(2000 - IDLE_PERMILLE * 2) / fix32(1024);
fix32 smoothChannels[4];
elapsedMillis burstTimer;
elapsedMillis burstCooldown;

#define RIGHT_BITS(x, n) ((u32)(-(x)) >> (32 - n))

void initPID() {
	for (int i = 0; i < 3; i++) {
		pidGains[i][P].setRaw(40 << P_SHIFT);
		pidGains[i][I].setRaw(20 << I_SHIFT);
		pidGains[i][D].setRaw(100 << D_SHIFT);
		pidGains[i][FF].setRaw(0 << FF_SHIFT);
		pidGains[i][S].setRaw(0 << S_SHIFT);
		pidGains[i][iFalloff] = .998;
	}
	for (int i = 0; i < 3; i++) {
		rateFactors[0][i] = 100; // first order, center rate
		rateFactors[1][i] = 0;
		rateFactors[2][i] = 200;
		rateFactors[3][i] = 0;
		rateFactors[4][i] = 800;
	}
	pidGainsVVel[P] = 50; // additional throttle if velocity is 1m/s too low
	pidGainsVVel[I] = .015; // increase throttle by 3200x this value, when error is 1m/s
	pidGainsVVel[D] = 10000; // additional throttle, if accelerating by 3200m/s^2
	pidGainsVVel[FF] = 30000;
	pidGainsHVel[P] = 12; // immediate target tilt in degree @ 1m/s too slow/fast
	pidGainsHVel[I] = 1.f / 3200.f; // additional tilt per 1/3200th of a second @ 1m/s too slow/fast
	pidGainsHVel[D] = 0; // tilt in degrees, if changing speed by 3200m/s /s
	pidGainsHVel[FF] = 3200.f * .1f; // tilt in degrees for target acceleration of 3200m/s^2
	vVelMaxErrorSum = 1024 / pidGainsVVel[I].getf32();
	vVelMinErrorSum = IDLE_PERMILLE * 2 / pidGainsVVel[I].getf32();
}

u32 takeoffCounter = 0;
elapsedMicros taskTimerGyro, taskTimerPid;
void __not_in_flash_func(pidLoop)() {
	u32 duration = taskTimerGyro;
	if (tasks[TASK_GYROREAD].maxGap < duration)
		tasks[TASK_GYROREAD].maxGap = duration;
	taskTimerGyro = 0;
	tasks[TASK_GYROREAD].runCounter++;
	gyroGetData(bmiDataRaw);
	for (int i = 0; i < 3; i++) {
		gyroData[i].setRaw((i32)gyroDataRaw[i] * 4000); // gyro data in range of -.5 ... +.5 due to fixed point math,gyro data in range of -2000 ... +2000 (degrees per second)
	}
	gyroData[AXIS_PITCH] = -gyroData[AXIS_PITCH];
	gyroData[AXIS_YAW] = -gyroData[AXIS_YAW];
	duration = taskTimerGyro;
	tasks[TASK_GYROREAD].totalDuration += duration;
	if (duration > tasks[TASK_GYROREAD].maxDuration)
		tasks[TASK_GYROREAD].maxDuration = duration;
	if (duration < tasks[TASK_GYROREAD].minDuration)
		tasks[TASK_GYROREAD].minDuration = duration;
	taskTimerGyro = 0;

	imuUpdate();
	duration = taskTimerPid;
	if (tasks[TASK_PID_MOTORS].maxGap < duration)
		tasks[TASK_PID_MOTORS].maxGap = duration;
	taskTimerPid = 0;
	tasks[TASK_PID_MOTORS].runCounter++;

	decodeErpm();

	if (armed) {
		// Quad armed
		static u32 ffBufPos = 0;
		static fix32 polynomials[5][3];
		ELRS->getSmoothChannels(smoothChannels);
		// calculate setpoints
		polynomials[0][0] = (smoothChannels[0] - 1500) >> 9; //-1...+1
		polynomials[0][1] = (smoothChannels[1] - 1500) >> 9;
		polynomials[0][2] = (smoothChannels[3] - 1500) >> 9;
		throttle = (smoothChannels[2] - 988); // 0...1024
		rollSetpoint = 0;
		pitchSetpoint = 0;
		yawSetpoint = 0;
		if (flightMode == FlightMode::ANGLE || flightMode == FlightMode::ALT_HOLD || flightMode == FlightMode::GPS_VEL) {
			fix32 dRoll;
			fix32 dPitch;
			if (flightMode < FlightMode::GPS_VEL) {
				dRoll = (smoothChannels[0] - 1500) * TO_ANGLE + (FIX_RAD_TO_DEG * roll);
				dPitch = (smoothChannels[1] - 1500) * TO_ANGLE - (FIX_RAD_TO_DEG * pitch);
				rollSetpoint = dRoll * angleModeP;
				pitchSetpoint = dPitch * angleModeP;
			} else if (flightMode == FlightMode::GPS_VEL) {
				static PT1 ffFilterNVel(2, 3200);
				static PT1 ffFilterEVel(2, 3200);
				static DualPT1 iRelaxFilterNVel(0.5, 3200);
				static DualPT1 iRelaxFilterEVel(0.5, 3200);
				static PT1 pushNorth(4, 3200);
				static PT1 pushEast(4, 3200);
				static fix32 lastNVelSetpoint = 0;
				static fix32 lastEVelSetpoint = 0;
				static elapsedMillis locationSetpointTimer = 0;
				fix32 rightCommand = smoothChannels[0] - 1500;
				fix32 fwdCommand = smoothChannels[1] - 1500;
				fix32 pushEastVel = 0;
				fix32 pushNorthVel = 0;
				if (fwdCommand.abs() < HVEL_STICK_DEADBAND)
					fwdCommand = 0;
				else if (fwdCommand > 0)
					fwdCommand -= HVEL_STICK_DEADBAND;
				else
					fwdCommand += HVEL_STICK_DEADBAND;
				if (rightCommand.abs() < HVEL_STICK_DEADBAND)
					rightCommand = 0;
				else if (rightCommand > 0)
					rightCommand -= HVEL_STICK_DEADBAND;
				else
					rightCommand += HVEL_STICK_DEADBAND;
				if (rightCommand || fwdCommand) {
					eVelSetpoint = cosHeading * rightCommand + sinHeading * fwdCommand;
					nVelSetpoint = -sinHeading * rightCommand + cosHeading * fwdCommand;
					eVelSetpoint = eVelSetpoint >> 9; // +-512 => +-1 (slightly less due to deadband)
					nVelSetpoint = nVelSetpoint >> 9; // +-512 => +-1 (slightly less due to deadband)
					eVelSetpoint *= MAX_TARGET_HVEL; // +-1 => +-12m/s
					nVelSetpoint *= MAX_TARGET_HVEL; // +-1 => +-12m/s
					pushNorth.set(0);
					pushEast.set(0);
					locationSetpointTimer = 0;
				} else if (locationSetpointTimer < 2000) {
					eVelSetpoint = 0;
					nVelSetpoint = 0;
					targetLat = gpsLatitudeFiltered;
					targetLon = gpsLongitudeFiltered;
				} else {
					fix64 latDiff = targetLat - fix64(gpsMotion.lat) / 10000000;
					fix64 lonDiff = targetLon - fix64(gpsMotion.lon) / 10000000;
					if (lonDiff > 180)
						lonDiff = lonDiff - 360;
					else if (lonDiff < -180)
						lonDiff = lonDiff + 360;
					fix32 tooFarSouth = latDiff * (40075000 / 360); // in m
					fix32 tooFarWest = lonDiff * (40075000 / 360); // in m
					startFixTrig();
					tooFarWest *= cosFix(targetLat);
					pushNorth.update(tooFarSouth);
					pushEast.update(tooFarWest);
					eVelSetpoint = 0;
					nVelSetpoint = 0;
					pushEastVel = fix32(pushEast) / 4;
					pushEastVel = constrain(pushEastVel, -MAX_TARGET_HVEL, MAX_TARGET_HVEL);
					pushNorthVel = fix32(pushNorth) / 4;
					pushNorthVel = constrain(pushNorthVel, -MAX_TARGET_HVEL, MAX_TARGET_HVEL);
				}
				eVelError = (eVelSetpoint + pushEastVel) - eVel;
				nVelError = (nVelSetpoint + pushNorthVel) - nVel;

				// shift to get more resolution in the filter
				// after shifting, we get a value of 0.08 for 1m/s² commanded acceleration
				// (1m/s / 1s / 3200f/s * 256 = 0.08)
				// assuming full 12m/s in 50ms, this gives us a value of 19.2 (0.08 * 12 * 20)
				ffFilterNVel.update((nVelSetpoint - lastNVelSetpoint) << 8);
				ffFilterEVel.update((eVelSetpoint - lastEVelSetpoint) << 8);
				// same here, just more shifting
				iRelaxFilterNVel.update((nVelSetpoint - lastNVelSetpoint) << 12);
				iRelaxFilterEVel.update((eVelSetpoint - lastEVelSetpoint) << 12);
				fix32 iRelaxTotal = sqrtf(((fix32)iRelaxFilterNVel * (fix32)iRelaxFilterNVel + (fix32)iRelaxFilterEVel * (fix32)iRelaxFilterEVel).getf32());
				if (iRelaxTotal < 1) {
					// low commanded acceleration: normal I gain
					eVelErrorSum = eVelErrorSum + eVelError;
					nVelErrorSum = nVelErrorSum + nVelError;
				} else if (iRelaxTotal > 2) {
					// high commanded acceleration: low I gain
					eVelErrorSum = eVelErrorSum + eVelError / 6;
					nVelErrorSum = nVelErrorSum + nVelError / 6;
				} else {
					// medium commanded acceleration: interpolate I gain
					fix32 iRelaxFactor = (iRelaxTotal - 1) / 1;
					fix32 divider = iRelaxFactor * 5 + 1;
					eVelErrorSum = eVelErrorSum + eVelError / divider;
					nVelErrorSum = nVelErrorSum + nVelError / divider;
				}
				eVelP = pidGainsHVel[P] * eVelError;
				nVelP = pidGainsHVel[P] * nVelError;
				eVelI = pidGainsHVel[I] * eVelErrorSum;
				nVelI = pidGainsHVel[I] * nVelErrorSum;
				eVelD = pidGainsHVel[D] * (eVelLast - eVel);
				nVelD = pidGainsHVel[D] * (nVelLast - nVel);
				eVelFF = (pidGainsHVel[FF] >> 8) * ffFilterEVel;
				nVelFF = (pidGainsHVel[FF] >> 8) * ffFilterNVel;

				fix32 eVelPID = eVelP + eVelI + eVelD + eVelFF;
				fix32 nVelPID = nVelP + nVelI + nVelD + nVelFF;
				fix32 targetRoll = eVelPID * cosHeading - nVelPID * sinHeading;
				fix32 targetPitch = eVelPID * sinHeading + nVelPID * cosHeading;
				if (targetRoll.abs() > MAX_ANGLE || targetPitch.abs() > MAX_ANGLE) {
					// limit the tilt to MAX_ANGLE
					if (burstCooldown > BURST_COOLDOWN) {
						// restart burst timer
						burstTimer = 0;
					}
					if (burstTimer < BURST_DURATION) {
						// allowed to tilt more for a short time
						targetRoll = constrain(targetRoll, -MAX_ANGLE_BURST, MAX_ANGLE_BURST);
						targetPitch = constrain(targetPitch, -MAX_ANGLE_BURST, MAX_ANGLE_BURST);
						burstCooldown = 0;
					} else {
						// limit the tilt to MAX_ANGLE
						targetRoll = constrain(targetRoll, -MAX_ANGLE, MAX_ANGLE);
						targetPitch = constrain(targetPitch, -MAX_ANGLE, MAX_ANGLE);
					}
				}
				dRoll = targetRoll + (FIX_RAD_TO_DEG * roll);
				dPitch = targetPitch - (FIX_RAD_TO_DEG * pitch);
				rollSetpoint = dRoll * velocityModeP;
				pitchSetpoint = dPitch * velocityModeP;
				lastNVelSetpoint = nVelSetpoint;
				lastEVelSetpoint = eVelSetpoint;
				nVelLast = nVel;
				eVelLast = eVel;
			}
			for (int i = 1; i < 5; i++) {
				polynomials[i][2] = polynomials[i - 1][2] * polynomials[0][2];
				if (polynomials[0][2] < 0)
					polynomials[i][2] = -polynomials[i][2];
			}

			yawSetpoint = 0;
			for (int i = 0; i < 5; i++)
				yawSetpoint += rateFactors[i][2] * polynomials[i][2];

			if (flightMode == FlightMode::ALT_HOLD || flightMode == FlightMode::GPS_VEL) {
				fix32 t = throttle - 512;
				static PT1 vVelDFilter(15, 3200);
				static PT1 vVelFFFilter(2, 3200);
				static elapsedMillis setAltSetpointTimer;
				static u32 stickWasCentered = 0;
				// deadband in center of stick
				if (t > 0) {
					t -= 50;
					if (t < 0) t = 0;
				} else {
					t += 50;
					if (t > 0) t = 0;
				}
				// estimate throttle
				vVelSetpoint = t / 90; // +/- 5 m/s
				if (vVelSetpoint == 0) {
					if (!stickWasCentered) {
						setAltSetpointTimer = 0;
						stickWasCentered = 1;
					} else if (setAltSetpointTimer > 1000) {
						if (stickWasCentered == 1) {
							// set altitude setpoint 1s after throttle is centered
							altSetpoint = combinedAltitude;
							stickWasCentered = 2;
						}
						vVelSetpoint += (altSetpoint - combinedAltitude) / 5; // prevent vVel drift slowly
					}
				} else {
					stickWasCentered = 0;
				}
				vVelError = vVelSetpoint - vVel;
				vVelErrorSum = vVelErrorSum + ((vVelFFFilter.update(vVelSetpoint - vVelLastSetpoint).abs() < fix32(0.001f)) ? vVelError : vVelError / 2); // reduce windup during fast changes
				vVelErrorSum = constrain(vVelErrorSum, vVelMinErrorSum, vVelMaxErrorSum);
				vVelP = pidGainsVVel[P] * vVelError;
				vVelI = pidGainsVVel[I] * vVelErrorSum;
				vVelD = pidGainsVVel[D] * vVelDFilter.update(vVelLast - vVel);
				vVelFF = pidGainsVVel[FF] * vVelFFFilter;
				vVelLastSetpoint = vVelSetpoint;
				throttle = vVelP + vVelI + vVelD + vVelFF;
				/* The cos of the thrust angle gives us the thrust "efficiency" (=cos(acos(...))),
				aka how much of the thrust is actually used to lift the quad.
				Dividing by this "efficiency" will give us the actual thrust needed to lift the quad.
				This acts much quicker than the PID would ever increase the throttle when tilting the quad. */
				fix32 throttleFactor = cosRoll * cosPitch;
				if (throttleFactor < 0) // quad is upside down
					throttleFactor = 1;
				throttleFactor = constrain(throttleFactor, fix32(0.33f), 1); // we limit the throttle increase to 3x (ca. 72° tilt), and also prevent division by zero
				throttleFactor = fix32(1) / throttleFactor; // 1/cos(acos(...)) = 1/cos(thrust angle)
				throttle = throttle * throttleFactor;
				throttle = constrain(throttle, 0, 1024);
			}
			vVelLast = vVel;
		} else if (flightMode == FlightMode::ACRO) {
			/*
			 * at full stick deflection, ...Raw values are either +1 or -1. That will make all the
			 * polynomials also +/-1. Thus, the total rate for each axis is equal to the sum of all 5 rateFactors
			 * of that axis. The center rate is the ratefactor[x][0].
			 */
			for (int i = 1; i < 5; i++) {
				for (int j = 0; j < 3; j++) {
					polynomials[i][j] = polynomials[i - 1][j] * polynomials[0][j];
					if (polynomials[0][j] < 0) // on second and fourth order, preserve initial sign
						polynomials[i][j] = -polynomials[i][j];
				}
			}
			for (int i = 0; i < 5; i++) {
				rollSetpoint += rateFactors[i][0] * polynomials[i][0];
				pitchSetpoint += rateFactors[i][1] * polynomials[i][1];
				yawSetpoint += rateFactors[i][2] * polynomials[i][2];
			}
		}
		rollError = rollSetpoint - gyroData[AXIS_ROLL];
		pitchError = pitchSetpoint - gyroData[AXIS_PITCH];
		yawError = yawSetpoint - gyroData[AXIS_YAW];
		if (ELRS->channels[2] > 1020)
			takeoffCounter++;
		else if (takeoffCounter < 1000) // 1000 = ca. 0.3s
			takeoffCounter = 0; // if the quad hasn't "taken off" yet, reset the counter
		if (takeoffCounter < 1000) // enable i term falloff (windup prevention) only before takeoff
		{
			rollErrorSum = rollErrorSum * pidGains[0][iFalloff];
			pitchErrorSum = pitchErrorSum * pidGains[1][iFalloff];
			yawErrorSum = yawErrorSum * pidGains[2][iFalloff];
		}

		rollErrorSum = rollErrorSum + rollError;
		pitchErrorSum = pitchErrorSum + pitchError;
		yawErrorSum = yawErrorSum + yawError;

		rollP = pidGains[0][P] * rollError;
		pitchP = pidGains[1][P] * pitchError;
		yawP = pidGains[2][P] * yawError;
		rollI = pidGains[0][I] * rollErrorSum;
		pitchI = pidGains[1][I] * pitchErrorSum;
		yawI = pidGains[2][I] * yawErrorSum;
		rollD = pidGains[0][D] * dFilterRoll.update(rollLast - gyroData[AXIS_ROLL]);
		pitchD = pidGains[1][D] * dFilterPitch.update(pitchLast - gyroData[AXIS_PITCH]);
		yawD = pidGains[2][D] * dFilterYaw.update(yawLast - gyroData[AXIS_YAW]);
		rollFF = pidGains[0][FF] * (rollSetpoint - rollSetpoints[ffBufPos]);
		pitchFF = pidGains[1][FF] * (pitchSetpoint - pitchSetpoints[ffBufPos]);
		yawFF = pidGains[2][FF] * (yawSetpoint - yawSetpoints[ffBufPos]);
		rollS = pidGains[0][S] * rollSetpoint;
		pitchS = pidGains[1][S] * pitchSetpoint;
		yawS = pidGains[2][S] * yawSetpoint;

		rollSetpoints[ffBufPos] = rollSetpoint;
		pitchSetpoints[ffBufPos] = pitchSetpoint;
		yawSetpoints[ffBufPos] = yawSetpoint;
		ffBufPos++;
		ffBufPos &= 7;

		fix32 rollTerm = rollP + rollI + rollD + rollFF + rollS;
		fix32 pitchTerm = pitchP + pitchI + pitchD + pitchFF + pitchS;
		fix32 yawTerm = yawP + yawI + yawD + yawFF + yawS;
		// scale throttle from 0...1024 to IDLE_PERMILLE*2...2000 (DShot output is 0...2000)
		throttle *= THROTTLE_SCALE; // 0...1024 => 0...2000-IDLE_PERMILLE*2
		throttle += IDLE_PERMILLE * 2; // 0...2000-IDLE_PERMILLE*2 => IDLE_PERMILLE*2...2000
		fix32 tRR, tRL, tFR, tFL;
#ifdef PROPS_OUT
		tRR = throttle - rollTerm + pitchTerm + yawTerm;
		tFR = throttle - rollTerm - pitchTerm - yawTerm;
		tRL = throttle + rollTerm + pitchTerm - yawTerm;
		tFL = throttle + rollTerm - pitchTerm + yawTerm;
#else
		tRR = throttle - rollTerm + pitchTerm - yawTerm;
		tFR = throttle - rollTerm - pitchTerm + yawTerm;
		tRL = throttle + rollTerm + pitchTerm + yawTerm;
		tFL = throttle + rollTerm - pitchTerm - yawTerm;
#endif
		throttles[(u8)MOTOR::RR] = tRR.geti32();
		throttles[(u8)MOTOR::RL] = tRL.geti32();
		throttles[(u8)MOTOR::FR] = tFR.geti32();
		throttles[(u8)MOTOR::FL] = tFL.geti32();
		if (throttles[(u8)MOTOR::RR] > 2000) {
			i16 diff = throttles[(u8)MOTOR::RR] - 2000;
			throttles[(u8)MOTOR::RR] = 2000;
			throttles[(u8)MOTOR::FL] -= diff;
			throttles[(u8)MOTOR::FR] -= diff;
			throttles[(u8)MOTOR::RL] -= diff;
		}
		if (throttles[(u8)MOTOR::RL] > 2000) {
			i16 diff = throttles[(u8)MOTOR::RL] - 2000;
			throttles[(u8)MOTOR::RL] = 2000;
			throttles[(u8)MOTOR::FL] -= diff;
			throttles[(u8)MOTOR::FR] -= diff;
			throttles[(u8)MOTOR::RR] -= diff;
		}
		if (throttles[(u8)MOTOR::FR] > 2000) {
			i16 diff = throttles[(u8)MOTOR::FR] - 2000;
			throttles[(u8)MOTOR::FR] = 2000;
			throttles[(u8)MOTOR::FL] -= diff;
			throttles[(u8)MOTOR::RL] -= diff;
			throttles[(u8)MOTOR::RR] -= diff;
		}
		if (throttles[(u8)MOTOR::FL] > 2000) {
			i16 diff = throttles[(u8)MOTOR::FL] - 2000;
			throttles[(u8)MOTOR::FL] = 2000;
			throttles[(u8)MOTOR::RL] -= diff;
			throttles[(u8)MOTOR::RR] -= diff;
			throttles[(u8)MOTOR::FR] -= diff;
		}
		if (throttles[(u8)MOTOR::RR] < IDLE_PERMILLE * 2) {
			i16 diff = IDLE_PERMILLE * 2 - throttles[(u8)MOTOR::RR];
			throttles[(u8)MOTOR::RR] = IDLE_PERMILLE * 2;
			throttles[(u8)MOTOR::FL] += diff;
			throttles[(u8)MOTOR::FR] += diff;
			throttles[(u8)MOTOR::RL] += diff;
		}
		if (throttles[(u8)MOTOR::RL] < IDLE_PERMILLE * 2) {
			i16 diff = IDLE_PERMILLE * 2 - throttles[(u8)MOTOR::RL];
			throttles[(u8)MOTOR::RL] = IDLE_PERMILLE * 2;
			throttles[(u8)MOTOR::FL] += diff;
			throttles[(u8)MOTOR::FR] += diff;
			throttles[(u8)MOTOR::RR] += diff;
		}
		if (throttles[(u8)MOTOR::FR] < IDLE_PERMILLE * 2) {
			i16 diff = IDLE_PERMILLE * 2 - throttles[(u8)MOTOR::FR];
			throttles[(u8)MOTOR::FR] = IDLE_PERMILLE * 2;
			throttles[(u8)MOTOR::FL] += diff;
			throttles[(u8)MOTOR::RL] += diff;
			throttles[(u8)MOTOR::RR] += diff;
		}
		if (throttles[(u8)MOTOR::FL] < IDLE_PERMILLE * 2) {
			i16 diff = IDLE_PERMILLE * 2 - throttles[(u8)MOTOR::FL];
			throttles[(u8)MOTOR::FL] = IDLE_PERMILLE * 2;
			throttles[(u8)MOTOR::RL] += diff;
			throttles[(u8)MOTOR::RR] += diff;
			throttles[(u8)MOTOR::FR] += diff;
		}
		for (int i = 0; i < 4; i++)
			throttles[i] = throttles[i] > 2000 ? 2000 : throttles[i];
		sendThrottles(throttles);
		rollLast = gyroData[AXIS_ROLL];
		pitchLast = gyroData[AXIS_PITCH];
		yawLast = gyroData[AXIS_YAW];
		if ((pidLoopCounter % bbFreqDivider) == 0 && bbFreqDivider) {
			writeSingleFrame();
		}
		pidLoopCounter++;
	} else {
		// Quad disarmed or RC disconnected
		// all motors off
		if (mspOverrideMotors > 1000)
			for (int i = 0; i < 4; i++)
				throttles[i] = 0;
		if (ELRS->channels[9] < 1500) {
			sendThrottles(throttles);
		} else {
			static elapsedMillis motorBeepTimer = 0;
			if (motorBeepTimer > 500)
				motorBeepTimer = 0;
			if (motorBeepTimer < 50) {
				u16 motors[4] = {DSHOT_CMD_BEACON2, DSHOT_CMD_BEACON2, DSHOT_CMD_BEACON2, DSHOT_CMD_BEACON2};
				sendRaw11Bit(motors);
			} else {
				u16 motors[4] = {0, 0, 0, 0};
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
	duration = taskTimerPid;
	tasks[TASK_PID_MOTORS].totalDuration += duration;
	if (duration < tasks[TASK_PID_MOTORS].minDuration) {
		tasks[TASK_PID_MOTORS].minDuration = duration;
	}
	if (duration > tasks[TASK_PID_MOTORS].maxDuration) {
		tasks[TASK_PID_MOTORS].maxDuration = duration;
	}
	taskTimerPid = 0;
}