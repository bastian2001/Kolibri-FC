#include "global.h"

FlightMode flightMode = FlightMode::ACRO;

/*
 * To avoid a lot of floating point math, fixed point math is used.
 * The gyro data is 16 bit, with a range of +/- 2000 degrees per second.
 * All data is converted to degrees per second (both RC commands as well as gyro data),
 * and calculations will be performed on a 16.16 fixed point number.
 * Additions can be performed like normal, while multiplications require
 * the numbers to be converted to 64 bit before calculation.
 */

i16 throttles[4];

fix32 gyroScaled[3];

fix32 pidGains[3][5];
fix32 iFalloff;
fix32 pidGainsVVel[4], pidGainsHVel[4];
fix32 angleModeP = 10, velocityModeP = 10;

fix32 rollSetpoint, pitchSetpoint, yawSetpoint, rollError, pitchError, yawError, rollLast, pitchLast, yawLast, vVelSetpoint, vVelError, vVelLast, eVelSetpoint, eVelError, eVelLast, nVelSetpoint, nVelError, nVelLast, vVelLastSetpoint;
fix64 rollErrorSum, pitchErrorSum, yawErrorSum, vVelErrorSum, eVelErrorSum, nVelErrorSum;
fix32 rollP, pitchP, yawP, rollI, pitchI, yawI, rollD, pitchD, yawD, rollFF, pitchFF, yawFF, rollS, pitchS, yawS, vVelP, vVelI, vVelD, vVelFF, eVelP, eVelI, eVelD, eVelFF, nVelP, nVelI, nVelD, nVelFF;
fix32 altSetpoint;
fix32 throttle;
fix64 targetLat, targetLon;
u16 dFilterCutoff;
PT2 dFilterRoll, dFilterPitch, dFilterYaw;
u16 gyroFilterCutoff;
PT1 gyroFiltered[3];

fix32 setpointDiffCutoff = 12;
PT1 setpointDiff[3];
fix32 lastSetpoints[3];

fix32 pidBoostCutoff = 5; // cutoff frequency for pid boost throttle filter
PT1 pidBoostFilter;
fix32 lastThrottle;
u8 pidBoostAxis = 1; // 0: off, 1: RP only, 2: RPY
fix32 pidBoostP = 2.5; // addition boost factor, e.g. when set to 2 in full effect, P is 3x
fix32 pidBoostI = 2.5; // addition boost factor, e.g. when set to 2 in full effect, I is 3x
fix32 pidBoostD = 0; // addition boost factor, e.g. when set to 2 in full effect, D is 3x
fix32 pidBoostStart; // dThrottle/dt in 1/1024 / s when pidBoost starts
fix32 pidBoostFull; // dThrottle/dt in 1/1024 / s when pidBoost is in full effect

u32 pidLoopCounter = 0;

fix32 rateCoeffs[3][3];
fix32 rateInterp[3][258]; // +-128 (=257) for sticks, index 257 only exists so that high + 1 is valid even if stick = 1
fix64 vVelMaxErrorSum, vVelMinErrorSum;
u8 maxAngle; // degrees, applied in angle mode and GPS mode
u8 maxAngleBurst; // degrees, this angle is allowed for a short time, e.g. when accelerating in GPS mode (NOT used in angle mode)
fix32 stickToAngle;
u16 idlePermille;
fix32 throttleScale;
fix32 maxTargetHvel; // maximum target horizontal velocity (m/s)
fix32 smoothChannels[4];
elapsedMillis burstTimer;
elapsedMillis burstCooldown;
u16 angleBurstTime; // milliseconds, time for which the maxAngleBurst is allowed
u16 angleBurstCooldownTime; // milliseconds, time for which the maxAngleBurst is not allowed after the burst time

fix32 vvelDFilterCutoff;
fix32 vvelFFFilterCutoff;
PT1 vVelDFilter;
PT1 vVelFFFilter;

fix32 hvelFfFilterCutoff;
fix32 hvelIRelaxFilterCutoff;
fix32 hvelPushFilterCutoff;
u16 hvelStickDeadband; // deadband for the horizontal velocity stick input (total stick is -512 to 512)
PT1 ffFilterNVel;
PT1 ffFilterEVel;
DualPT1 iRelaxFilterNVel;
DualPT1 iRelaxFilterEVel;
PT1 pushNorth;
PT1 pushEast;

interp_config rateInterpConfig0, rateInterpConfig1, rateInterpConfig2;

void initPidGains() {
	for (int i = 0; i < 3; i++) {
		pidGains[i][P].setRaw(80 << P_SHIFT);
		pidGains[i][I].setRaw(40 << I_SHIFT);
		pidGains[i][D].setRaw(500 << D_SHIFT);
		pidGains[i][FF].setRaw(40 << FF_SHIFT);
		pidGains[i][S].setRaw(0 << S_SHIFT);
	}
}

void initRateCoeffs() {
	for (int i = 0; i < 3; i++) {
		rateCoeffs[i][ACTUAL_CENTER_SENSITIVITY] = 170;
		rateCoeffs[i][ACTUAL_MAX_RATE] = 900;
		rateCoeffs[i][ACTUAL_EXPO] = 0.57;
	}
}

/**
 * @brief calculates the rate based on ACTUAL rates, does NOT use the LUT
 *
 * @param stick from -1 to 1, where 0 is center and +-1 is full stick
 * @param axis AXIS_ROLL, AXIS_PITCH or AXIS_YAW (0, 1 or 2)
 * @return fix32 the rate in degrees per second
 */
static fix32 calculateActual(fix32 stick, u8 axis) {
	fix32 center = rateCoeffs[axis][ACTUAL_CENTER_SENSITIVITY];
	fix32 maxRate = rateCoeffs[axis][ACTUAL_MAX_RATE];
	fix32 expo = rateCoeffs[axis][ACTUAL_EXPO];
	fix32 stick2 = stick * stick;
	fix32 stick6 = stick2 * stick2 * stick2;
	if (expo < 0) expo = 0;
	if (expo > 1) expo = 1;
	fix32 linPart = stick * center;
	fix32 expoPart = (expo * stick6 + (fix32(1) - expo) * stick2) * (maxRate - center) * stick.sign();
	return linPart + expoPart;
}

static void startRateInterp() {
	interp_set_config(interp0, 0, &rateInterpConfig0);
	interp_set_config(interp0, 1, &rateInterpConfig1);
	interp_set_config(interp1, 0, &rateInterpConfig2);
	interp1->base[0] = -1 << 15;
	interp1->base[1] = 1 << 15;
}

/**
 * @brief Get the target rotational rate for a given stick value and axis.
 *
 * Uses the interpolator for faster access to the pre-calculated rates. Run startRateInterp() before every batch of using this function.
 *
 * @param stick stick position from -1 to 1, where 0 is center and +-1 is full stick
 * @param axis AXIS_ROLL, AXIS_PITCH or AXIS_YAW (0, 1 or 2)
 * @return fix32 the rate in degrees per second
 */
static fix32 getRateInterp(fix32 stick, u8 axis) {
	// we use interp1 lane 0 for shifting and clamping the -1...1 (17 bit) stick value. Shifting so it is a 16 bit number, and clamping so that we do not get out of bounds
	interp1->accum[0] = stick.raw;
	// the peek value is shifted and clamped to -0.5...0.5 (-32768...32768), after shifting it is -128...128, then move that range to 0...256 for the LUT
	i32 high = ((i32)interp1->peek[0] >> 8) + 128; // high byte (from 0-255 for -1 to <1 stick, 256 only for stick = 1)

	interp0->accum[1] = interp1->peek[0]; // interpolation alpha value (only 8 LSBs (the ones we removed in the high byte) are used), this is the low byte
	interp0->base[0] = rateInterp[axis][high].raw; // lower bound from LUT
	interp0->base[1] = rateInterp[axis][high + 1].raw; // upper bound from LUT
	return fix32().setRaw(interp0->peek[1]); // alpha applied between lower and upper
}

static void fillRateInterp() {
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 257; j++) {
			fix32 stick = fix32(j - 128) / 128; // from -1 to 1
			rateInterp[i][j] = calculateActual(stick, i);
		}
	}
	startRateInterp();

	elapsedMicros e;
	i32 time = 0, time2 = 0;
	e = 0;
	fix32 sum = 0;
	fix32 sum2 = 0;
	for (fix32 i = -1; i <= 1; i += fix32(1) / 1024) {
		sum += getRateInterp(i, AXIS_ROLL);
	}
	time = e;
	e = 0;
	for (fix32 i = -1; i <= 1; i += fix32(1) / 1024) {
		sum2 += calculateActual(i, AXIS_ROLL);
	}
	time2 = e;
	Serial.printf("Time for interp: %d us, Time for calculateActual: %d us, diff: %d us\n", time, time2, time - time2);
	Serial.printf("Sum for interp: %.3f, Sum for calculateActual: %.3f, diff: %.3f\n", sum.getf32(), sum2.getf32(), (sum - sum2).getf32());
}

static void initRateInterp() {
	rateInterpConfig0 = interp_default_config();
	interp_config_set_blend(&rateInterpConfig0, true);
	rateInterpConfig1 = interp_default_config();
	interp_config_set_signed(&rateInterpConfig1, true);
	rateInterpConfig2 = interp_default_config();
	interp_config_set_signed(&rateInterpConfig2, true);
	interp_config_set_clamp(&rateInterpConfig2, true);
	interp_config_set_shift(&rateInterpConfig2, 1);
	interp_config_set_mask(&rateInterpConfig2, 0, 30);
	fillRateInterp();
}

void initPidVVel() {
	pidGainsVVel[P] = 50; // additional throttle if velocity is 1m/s too low
	pidGainsVVel[I] = .03; // increase throttle by 3200x this value, when error is 1m/s
	pidGainsVVel[D] = 10000; // additional throttle, if accelerating by 3200m/s^2
	pidGainsVVel[FF] = 30000;
}

void initPidHVel() {
	pidGainsHVel[P] = 12; // immediate target tilt in degree @ 1m/s too slow/fast
	pidGainsHVel[I] = 1.f / 3200.f; // additional tilt per 1/3200th of a second @ 1m/s too slow/fast
	pidGainsHVel[D] = 0; // tilt in degrees, if changing speed by 3200m/s /s
	pidGainsHVel[FF] = 3200.f * .1f; // tilt in degrees for target acceleration of 3200m/s^2
}

void initPid() {
	addArraySetting(SETTING_PID_GAINS, pidGains, &initPidGains);
	addArraySetting(SETTING_RATE_COEFFS, rateCoeffs, &initRateCoeffs);
	addArraySetting(SETTING_PID_VVEL, pidGainsVVel, &initPidVVel);
	addArraySetting(SETTING_PID_HVEL, pidGainsHVel, &initPidHVel);
	addSetting(SETTING_IDLE_PERMILLE, &idlePermille, 35);
	addSetting(SETTING_MAX_ANGLE, &maxAngle, 40);
	addSetting(SETTING_DFILTER_CUTOFF, &dFilterCutoff, 70);
	addSetting(SETTING_GYRO_FILTER_CUTOFF, &gyroFilterCutoff, 100);
	addSetting(SETTING_SETPOINT_DIFF_CUTOFF, &setpointDiffCutoff, 12);
	addSetting(SETTING_PID_BOOST_CUTOFF, &pidBoostCutoff, 5);
	addSetting(SETTING_PID_BOOST_START, &pidBoostStart, 2000);
	addSetting(SETTING_PID_BOOST_FULL, &pidBoostFull, 6000);
	addSetting(SETTING_MAX_TARGET_HVEL, &maxTargetHvel, 12);
	addSetting(SETTING_VVEL_FF_FILTER_CUTOFF, &vvelFFFilterCutoff, 2);
	addSetting(SETTING_VVEL_D_FILTER_CUTOFF, &vvelDFilterCutoff, 15);
	addSetting(SETTING_HVEL_FF_FILTER_CUTOFF, &hvelFfFilterCutoff, 2);
	addSetting(SETTING_HVEL_I_RELAX_FILTER_CUTOFF, &hvelIRelaxFilterCutoff, 0.5f);
	addSetting(SETTING_HVEL_PUSH_FILTER_CUTOFF, &hvelPushFilterCutoff, 4);
	addSetting(SETTING_MAX_ANGLE_BURST, &maxAngleBurst, 60);
	addSetting(SETTING_ANGLE_BURST_TIME, &angleBurstTime, 3000);
	addSetting(SETTING_ANGLE_BURST_COOLDOWN, &angleBurstCooldownTime, 5000);
	addSetting(SETTING_HVEL_STICK_DEADBAND, &hvelStickDeadband, 30);
	addSetting(SETTING_IFALLOFF, &iFalloff, 400);

	initRateInterp();

	vVelMaxErrorSum = 1024 / pidGainsVVel[I].getf32();
	vVelMinErrorSum = idlePermille * 2 / pidGainsVVel[I].getf32();

	stickToAngle = fix32(maxAngle) / fix32(512);
	throttleScale = fix32(2000 - idlePermille * 2) / fix32(1024);

	dFilterRoll = PT2(dFilterCutoff, 3200);
	dFilterPitch = PT2(dFilterCutoff, 3200);
	dFilterYaw = PT2(dFilterCutoff, 3200);

	gyroFiltered[AXIS_ROLL] = PT1(gyroFilterCutoff, 3200);
	gyroFiltered[AXIS_PITCH] = PT1(gyroFilterCutoff, 3200);
	gyroFiltered[AXIS_YAW] = PT1(gyroFilterCutoff, 3200);

	setpointDiff[AXIS_ROLL] = PT1(setpointDiffCutoff, 3200);
	setpointDiff[AXIS_PITCH] = PT1(setpointDiffCutoff, 3200);
	setpointDiff[AXIS_YAW] = PT1(setpointDiffCutoff, 3200);

	pidBoostFilter = PT1(pidBoostCutoff, 3200);

	vVelDFilter = PT1(vvelDFilterCutoff, 3200);
	vVelFFFilter = PT1(vvelFFFilterCutoff, 3200);

	ffFilterNVel = PT1(hvelFfFilterCutoff, 3200);
	ffFilterEVel = PT1(hvelFfFilterCutoff, 3200);
	iRelaxFilterNVel = DualPT1(hvelIRelaxFilterCutoff, 3200);
	iRelaxFilterEVel = DualPT1(hvelIRelaxFilterCutoff, 3200);
	pushNorth = PT1(hvelPushFilterCutoff, 3200);
	pushEast = PT1(hvelPushFilterCutoff, 3200);
}

u32 takeoffCounter = 0;
elapsedMicros taskTimerPid;
void pidLoop() {
	u32 duration = taskTimerPid;
	if (tasks[TASK_PID].maxGap < duration)
		tasks[TASK_PID].maxGap = duration;
	taskTimerPid = 0;
	tasks[TASK_PID].runCounter++;

	gyroFiltered[AXIS_ROLL].update(gyroScaled[AXIS_ROLL]);
	gyroFiltered[AXIS_PITCH].update(gyroScaled[AXIS_PITCH]);
	gyroFiltered[AXIS_YAW].update(gyroScaled[AXIS_YAW]);

	if (armed) {
		// Quad armed
		fix32 stickPos[3]; // RPY stick positions, -1...+1
		ELRS->getSmoothChannels(smoothChannels);
		// calculate setpoints
		stickPos[0] = (smoothChannels[0] - 1500) >> 9; //-1...+1
		stickPos[1] = (smoothChannels[1] - 1500) >> 9;
		stickPos[2] = (smoothChannels[3] - 1500) >> 9;
		throttle = (smoothChannels[2] - 988); // 0...1024
		rollSetpoint = 0;
		pitchSetpoint = 0;
		yawSetpoint = 0;
		if (flightMode == FlightMode::ANGLE || flightMode == FlightMode::ALT_HOLD || flightMode == FlightMode::GPS_VEL) {
			fix32 dRoll;
			fix32 dPitch;
			fix32 newRollSetpoint, newPitchSetpoint, newYawSetpoint;
			if (flightMode < FlightMode::GPS_VEL) {
				dRoll = (smoothChannels[0] - 1500) * stickToAngle + (FIX_RAD_TO_DEG * roll);
				dPitch = (smoothChannels[1] - 1500) * stickToAngle - (FIX_RAD_TO_DEG * pitch);
				newRollSetpoint = dRoll * angleModeP;
				newPitchSetpoint = dPitch * angleModeP;
			} else if (flightMode == FlightMode::GPS_VEL) {
				static fix32 lastNVelSetpoint = 0;
				static fix32 lastEVelSetpoint = 0;
				static elapsedMillis locationSetpointTimer = 0;
				fix32 rightCommand = smoothChannels[0] - 1500;
				fix32 fwdCommand = smoothChannels[1] - 1500;
				fix32 pushEastVel = 0;
				fix32 pushNorthVel = 0;
				if (fwdCommand.abs() < hvelStickDeadband)
					fwdCommand = 0;
				else if (fwdCommand > 0)
					fwdCommand -= hvelStickDeadband;
				else
					fwdCommand += hvelStickDeadband;
				if (rightCommand.abs() < hvelStickDeadband)
					rightCommand = 0;
				else if (rightCommand > 0)
					rightCommand -= hvelStickDeadband;
				else
					rightCommand += hvelStickDeadband;
				if (rightCommand || fwdCommand) {
					eVelSetpoint = cosHeading * rightCommand + sinHeading * fwdCommand;
					nVelSetpoint = -sinHeading * rightCommand + cosHeading * fwdCommand;
					eVelSetpoint = eVelSetpoint >> 9; // +-512 => +-1 (slightly less due to deadband)
					nVelSetpoint = nVelSetpoint >> 9; // +-512 => +-1 (slightly less due to deadband)
					eVelSetpoint *= maxTargetHvel; // +-1 => +-12m/s
					nVelSetpoint *= maxTargetHvel; // +-1 => +-12m/s
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
					pushEastVel = constrain(pushEastVel, -maxTargetHvel, maxTargetHvel);
					pushNorthVel = fix32(pushNorth) / 4;
					pushNorthVel = constrain(pushNorthVel, -maxTargetHvel, maxTargetHvel);
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
				if (targetRoll.abs() > maxAngle || targetPitch.abs() > maxAngle) {
					// limit the tilt to maxAngle
					if (burstCooldown > angleBurstCooldownTime) {
						// restart burst timer
						burstTimer = 0;
					}
					if (burstTimer < angleBurstTime) {
						// allowed to tilt more for a short time
						targetRoll = constrain(targetRoll, -maxAngleBurst, maxAngleBurst);
						targetPitch = constrain(targetPitch, -maxAngleBurst, maxAngleBurst);
						burstCooldown = 0;
					} else {
						// limit the tilt to maxAngle
						targetRoll = constrain(targetRoll, -maxAngle, maxAngle);
						targetPitch = constrain(targetPitch, -maxAngle, maxAngle);
					}
				}
				dRoll = targetRoll + (FIX_RAD_TO_DEG * roll);
				dPitch = targetPitch - (FIX_RAD_TO_DEG * pitch);
				newRollSetpoint = dRoll * velocityModeP;
				newRollSetpoint = constrain(newRollSetpoint, -1000, 1000);
				newPitchSetpoint = dPitch * velocityModeP;
				newPitchSetpoint = constrain(newPitchSetpoint, -1000, 1000);
				lastNVelSetpoint = nVelSetpoint;
				lastEVelSetpoint = eVelSetpoint;
				nVelLast = nVel;
				eVelLast = eVel;
			}
			startRateInterp();
			newYawSetpoint = getRateInterp(stickPos[2], AXIS_YAW);

			// convert (new...) global roll, pitch and yaw to local roll, pitch and yaw
			rollSetpoint = newRollSetpoint * cosPitch - newYawSetpoint * sinPitch;
			pitchSetpoint = newPitchSetpoint * cosRoll + newYawSetpoint * cosPitch * sinRoll;
			yawSetpoint = -newPitchSetpoint * sinRoll + newYawSetpoint * cosPitch * cosRoll - newRollSetpoint * sinPitch;

			if (flightMode == FlightMode::ALT_HOLD || flightMode == FlightMode::GPS_VEL) {
				fix32 t = throttle - 512;
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
			// acro is the simplest: we just need to calculate the setpoints based on the sticks, no fancy algos for althold or poshold
			startRateInterp();
			rollSetpoint = getRateInterp(stickPos[0], AXIS_ROLL);
			pitchSetpoint = getRateInterp(stickPos[1], AXIS_PITCH);
			yawSetpoint = getRateInterp(stickPos[2], AXIS_YAW);
		}
		rollError = rollSetpoint - gyroFiltered[AXIS_ROLL];
		pitchError = pitchSetpoint - gyroFiltered[AXIS_PITCH];
		yawError = yawSetpoint - gyroFiltered[AXIS_YAW];
		if (ELRS->channels[2] > 1020)
			takeoffCounter++;
		else if (takeoffCounter < 1000) // 1000 = ca. 0.3s
			takeoffCounter = 0; // if the quad hasn't "taken off" yet, reset the counter
		if (takeoffCounter < 1000) // enable i term falloff (windup prevention) only before takeoff
		{
			rollErrorSum = rollErrorSum - iFalloff / 3200 * rollErrorSum.sign() / pidGains[0][I];
			pitchErrorSum = pitchErrorSum - iFalloff / 3200 * pitchErrorSum.sign() / pidGains[1][I];
			yawErrorSum = yawErrorSum - iFalloff / 3200 * yawErrorSum.sign() / pidGains[2][I];
		}

		fix32 pFactor = 1, iFactor = 1, dFactor = 1;
		if (pidBoostAxis) {
			pidBoostFilter.update(throttle - lastThrottle);
			fix32 boostStrength = (fix32(pidBoostFilter).abs() * 3200 - pidBoostStart) / (pidBoostFull - pidBoostStart);
			bbDebug1 = boostStrength.raw;
			bbDebug2 = fix32(pidBoostFilter).raw;
			if (boostStrength > 1)
				boostStrength = 1;
			else if (boostStrength < 0)
				boostStrength = 0;
			pFactor += pidBoostP * boostStrength;
			iFactor += pidBoostI * boostStrength;
			dFactor += pidBoostD * boostStrength;
		}

		setpointDiff[AXIS_ROLL].update(((rollSetpoint - lastSetpoints[AXIS_ROLL]) >> 4) * 3200); // e.g. 1250 for 1000 deg/s in 50ms
		setpointDiff[AXIS_PITCH].update(((pitchSetpoint - lastSetpoints[AXIS_PITCH]) >> 4) * 3200); // e.g. 1250 for 1000 deg/s in 50ms
		setpointDiff[AXIS_YAW].update(((yawSetpoint - lastSetpoints[AXIS_YAW]) >> 4) * 3200); // e.g. 1250 for 1000 deg/s in 50ms

		fix32 totalDiff = fix32(setpointDiff[AXIS_ROLL]).abs() + fix32(setpointDiff[AXIS_PITCH]).abs() + fix32(setpointDiff[AXIS_YAW]).abs();
		fix32 iRelaxMultiplier = 1;
		if (totalDiff > 450) {
			iRelaxMultiplier = fix32(0.125f);
		} else if (totalDiff > 150) {
			iRelaxMultiplier = fix32(1) - (totalDiff - 150) / 300 * 7 / 8;
		}

		rollErrorSum = rollErrorSum + rollError * iRelaxMultiplier * iFactor;
		pitchErrorSum = pitchErrorSum + pitchError * iRelaxMultiplier * iFactor;
		yawErrorSum = yawErrorSum + yawError * iRelaxMultiplier * iFactor;

		rollP = pidGains[0][P] * rollError * pFactor;
		pitchP = pidGains[1][P] * pitchError * pFactor;
		yawP = pidGains[2][P] * yawError * (pidBoostAxis == 2 ? pFactor : 1);
		rollI = pidGains[0][I] * rollErrorSum;
		pitchI = pidGains[1][I] * pitchErrorSum;
		yawI = pidGains[2][I] * yawErrorSum;
		rollD = pidGains[0][D] * dFilterRoll.update(rollLast - gyroFiltered[AXIS_ROLL]) * dFactor;
		pitchD = pidGains[1][D] * dFilterPitch.update(pitchLast - gyroFiltered[AXIS_PITCH]) * dFactor;
		yawD = pidGains[2][D] * dFilterYaw.update(yawLast - gyroFiltered[AXIS_YAW]) * (pidBoostAxis == 2 ? dFactor : 1);
		bbDebug3 = (pidGains[0][D] * fix32(dFilterRoll) * dFactor).geti32();
		bbDebug4 = (pidGains[0][D] * fix32(dFilterRoll)).geti32();
		rollFF = pidGains[0][FF] * setpointDiff[AXIS_ROLL];
		pitchFF = pidGains[1][FF] * setpointDiff[AXIS_PITCH];
		yawFF = pidGains[2][FF] * setpointDiff[AXIS_YAW];
		rollS = pidGains[0][S] * rollSetpoint;
		pitchS = pidGains[1][S] * pitchSetpoint;
		yawS = pidGains[2][S] * yawSetpoint;

		lastSetpoints[AXIS_ROLL] = rollSetpoint;
		lastSetpoints[AXIS_PITCH] = pitchSetpoint;
		lastSetpoints[AXIS_YAW] = yawSetpoint;
		lastThrottle = throttle;

		fix32 rollTerm = rollP + rollI + rollD + rollFF + rollS;
		fix32 pitchTerm = pitchP + pitchI + pitchD + pitchFF + pitchS;
		fix32 yawTerm = yawP + yawI + yawD + yawFF + yawS;
		// scale throttle from 0...1024 to idlePermille*2...2000 (DShot output is 0...2000)
		throttle *= throttleScale; // 0...1024 => 0...2000-idlePermille*2
		throttle += idlePermille * 2; // 0...2000-idlePermille*2 => idlePermille*2...2000
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
		if (throttles[(u8)MOTOR::RR] < idlePermille * 2) {
			i16 diff = idlePermille * 2 - throttles[(u8)MOTOR::RR];
			throttles[(u8)MOTOR::RR] = idlePermille * 2;
			throttles[(u8)MOTOR::FL] += diff;
			throttles[(u8)MOTOR::FR] += diff;
			throttles[(u8)MOTOR::RL] += diff;
		}
		if (throttles[(u8)MOTOR::RL] < idlePermille * 2) {
			i16 diff = idlePermille * 2 - throttles[(u8)MOTOR::RL];
			throttles[(u8)MOTOR::RL] = idlePermille * 2;
			throttles[(u8)MOTOR::FL] += diff;
			throttles[(u8)MOTOR::FR] += diff;
			throttles[(u8)MOTOR::RR] += diff;
		}
		if (throttles[(u8)MOTOR::FR] < idlePermille * 2) {
			i16 diff = idlePermille * 2 - throttles[(u8)MOTOR::FR];
			throttles[(u8)MOTOR::FR] = idlePermille * 2;
			throttles[(u8)MOTOR::FL] += diff;
			throttles[(u8)MOTOR::RL] += diff;
			throttles[(u8)MOTOR::RR] += diff;
		}
		if (throttles[(u8)MOTOR::FL] < idlePermille * 2) {
			i16 diff = idlePermille * 2 - throttles[(u8)MOTOR::FL];
			throttles[(u8)MOTOR::FL] = idlePermille * 2;
			throttles[(u8)MOTOR::RL] += diff;
			throttles[(u8)MOTOR::RR] += diff;
			throttles[(u8)MOTOR::FR] += diff;
		}
		for (int i = 0; i < 4; i++)
			throttles[i] = throttles[i] > 2000 ? 2000 : throttles[i];
		sendThrottles(throttles);
		rollLast = gyroFiltered[AXIS_ROLL];
		pitchLast = gyroFiltered[AXIS_PITCH];
		yawLast = gyroFiltered[AXIS_YAW];
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
		if (!rxModes[RxModeIndex::BEEPER].isActive()) {
			sendThrottles(throttles);
		} else {
			static elapsedMillis motorBeepTimer = 0;
			if (motorBeepTimer > 500)
				motorBeepTimer = 0;
			if (motorBeepTimer < 50) {
				u16 cmd = DSHOT_BEEP_CMD(dshotBeepTone);
				u16 motors[4] = {cmd, cmd, cmd, cmd};
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
	tasks[TASK_PID].totalDuration += duration;
	if (duration < tasks[TASK_PID].minDuration) {
		tasks[TASK_PID].minDuration = duration;
	}
	if (duration > tasks[TASK_PID].maxDuration) {
		tasks[TASK_PID].maxDuration = duration;
	}
	taskTimerPid = 0;
}
