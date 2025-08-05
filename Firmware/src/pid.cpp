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

i16 throttles[4] __attribute__((aligned(4)));

fix32 gyroScaled[3];

u16 pidGainsNice[3][5] = {0};
fix32 pidGains[3][5];
fix32 iFalloff;
fix32 pidGainsVVel[4], pidGainsHVel[4];
fix32 angleModeP = 10;

fix64 rthStartLat, rthStartLon;
u8 rthState = 0, lastRthState = 255; // 0: climb, 1: navigate home, 2: descend, 3: land

fix32 rollSetpoint, pitchSetpoint, yawSetpoint, rollError, pitchError, yawError, rollLast, pitchLast, yawLast, vVelSetpoint, vVelError, vVelLast, eVelSetpoint, eVelError, eVelLast, nVelSetpoint, nVelError, nVelLast, vVelLastSetpoint;
fix64 rollErrorSum, pitchErrorSum, yawErrorSum, vVelErrorSum, eVelErrorSum, nVelErrorSum;
fix32 rollP, pitchP, yawP, rollI, pitchI, yawI, rollD, pitchD, yawD, rollFF, pitchFF, yawFF, rollS, pitchS, yawS, vVelP, vVelI, vVelD, vVelFF, eVelP, eVelI, eVelD, eVelFF, nVelP, nVelI, nVelD, nVelFF;
fix32 altSetpoint;
fix32 throttle;
fix64 targetLat, targetLon;
fix32 targetAngleHeading;
u16 dFilterCutoff;
PT2 dFilterRoll, dFilterPitch, dFilterYaw;
u16 gyroFilterCutoff;
PT1 gyroFiltered[3];

bool useDynamicIdle = true;
u16 dynamicIdleRpm = 3000;
fix32 dynamicIdlePids[4][3] = {0}; // [motor][P, I, D]
fix32 dynamicIdlePidGains[3] = {.2, 0.0015, .07};

fix32 setpointDiffCutoff = 12;
PT1 setpointDiff[3];
fix32 lastSetpoints[3];
i8 forceZeroVvelSetpoint = 0;
static elapsedMicros flightModeChangeTimer;

fix32 pidBoostCutoff = 5; // cutoff frequency for pid boost throttle filter
PT1 pidBoostFilter;
fix32 lastThrottle;
u8 pidBoostAxis = 0; // 0: off, 1: RP only, 2: RPY
fix32 pidBoostP = 5; // addition boost factor, e.g. when set to 2 in full effect, P is 3x
fix32 pidBoostI = 5; // addition boost factor, e.g. when set to 2 in full effect, I is 3x
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
u8 idlePermille;
fix32 throttleScale;
fix32 maxTargetHvel; // maximum target horizontal velocity (m/s)
fix32 smoothChannels[4];
static elapsedMicros burstTimer;
static elapsedMicros burstCooldown;
u32 angleBurstTime; // microseconds, time for which the maxAngleBurst is allowed
u16 angleBurstCooldownTime; // microseconds, time for which the maxAngleBurst is not allowed after the burst time

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

static fix32 calcThrottle(fix32 targetVvel);
static void sticksToGpsSetpoint(const fix32 *channels, fix32 *eVelSetpoint, fix32 *nVelSetpoint);
static void autopilotNavigate(fix64 toLat, fix64 toLon, fix32 toAlt, fix32 *eVelSetpoint, fix32 *nVelSetpoint, fix32 *targetVvel);
static fix32 stickToTargetVvel(fix32 stickPos);
interp_config rateInterpConfig0, rateInterpConfig1, rateInterpConfig2;
interp_config dynIdleInterpConfig;

void initPidGains() {
	for (int i = 0; i < 3; i++) {
		pidGainsNice[i][0] = 80;
		pidGainsNice[i][1] = 40;
		pidGainsNice[i][2] = 500;
		pidGainsNice[i][3] = 40;
		pidGainsNice[i][4] = 0;
	}
}

void convertPidsFromNice() {
	for (int i = 0; i < 3; i++) {
		pidGains[i][P].setRaw(pidGainsNice[i][0] << P_SHIFT);
		pidGains[i][I].setRaw(pidGainsNice[i][1] << I_SHIFT);
		pidGains[i][D].setRaw(pidGainsNice[i][2] << D_SHIFT);
		pidGains[i][FF].setRaw(pidGainsNice[i][3] << FF_SHIFT);
		pidGains[i][S].setRaw(pidGainsNice[i][4] << S_SHIFT);
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

static void startDynIdleInterp() {
	interp_set_config(interp1, 0, &dynIdleInterpConfig);
	interp1->base[0] = 0 << 16;
	interp1->base[1] = 400 << 16;
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

static void initRateInterp() {
	// prepare configs
	rateInterpConfig0 = interp_default_config();
	interp_config_set_blend(&rateInterpConfig0, true);
	rateInterpConfig1 = interp_default_config();
	interp_config_set_signed(&rateInterpConfig1, true);
	rateInterpConfig2 = interp_default_config();
	interp_config_set_signed(&rateInterpConfig2, true);
	interp_config_set_clamp(&rateInterpConfig2, true);
	interp_config_set_shift(&rateInterpConfig2, 1);
	interp_config_set_mask(&rateInterpConfig2, 0, 30);

	// fill interpolator values
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 257; j++) {
			fix32 stick = fix32(j - 128) / 128; // from -1 to 1
			rateInterp[i][j] = calculateActual(stick, i);
		}
	}
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

void initDynIdlePids() {
	dynamicIdlePidGains[P] = .2;
	dynamicIdlePidGains[I] = .0015;
	dynamicIdlePidGains[D] = .07;
}

void initPid() {
	addArraySetting(SETTING_PID_GAINS, pidGainsNice, &initPidGains);
	addArraySetting(SETTING_RATE_COEFFS, rateCoeffs, &initRateCoeffs);
	addArraySetting(SETTING_PID_VVEL, pidGainsVVel, &initPidVVel);
	addArraySetting(SETTING_PID_HVEL, pidGainsHVel, &initPidHVel);
	addSetting(SETTING_IDLE_PERMILLE, &idlePermille, 35);
	addSetting(SETTING_MAX_ANGLE, &maxAngle, 40);
	addSetting(SETTING_ANGLE_MODE_P, &angleModeP, 5);
	addSetting(SETTING_DFILTER_CUTOFF, &dFilterCutoff, 70);
	addSetting(SETTING_GYRO_FILTER_CUTOFF, &gyroFilterCutoff, 100);
	addSetting(SETTING_SETPOINT_DIFF_CUTOFF, &setpointDiffCutoff, 12);
	addSetting(SETTING_PID_BOOST_CUTOFF, &pidBoostCutoff, 5);
	addSetting(SETTING_PID_BOOST_START, &pidBoostStart, 2000000);
	addSetting(SETTING_PID_BOOST_FULL, &pidBoostFull, 6000000);
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
	addSetting(SETTING_DYNAMIC_IDLE_EN, &useDynamicIdle, true);
	addSetting(SETTING_DYNAMIC_IDLE_RPM, &dynamicIdleRpm, 3000);
	addArraySetting(SETTING_DYNAMIC_IDLE_PIDS, dynamicIdlePidGains, &initDynIdlePids);

	convertPidsFromNice();
	for (int i = 0; i < 3; i++) {
		const char ax[3][6] = {"ROLL", "PITCH", "YAW"};
		const char type[5][3] = {" P", " I", " D", "FF", " S"};
		for (int j = 0; j < 5; j++) {
			char buf[10];
			snprintf(buf, 10, "%s %s", ax[i], type[j]);
			inFlightTuningParams.push_back(TunableParameter(&pidGainsNice[i][j], (u16)5, (u16)0, (u16)5000, (const char *)buf, &convertPidsFromNice));
		}
	}

	initRateInterp();
	dynIdleInterpConfig = interp_default_config();
	interp_config_set_clamp(&dynIdleInterpConfig, true);
	interp_config_set_signed(&dynIdleInterpConfig, true);

	vVelMaxErrorSum = 1024 / pidGainsVVel[I].getf32();
	vVelMinErrorSum = idlePermille * 2 / pidGainsVVel[I].getf32();

	stickToAngle = fix32(maxAngle) / 512;
	throttleScale = fix32(2000 - idlePermille * 2) / 1024;

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
void __not_in_flash_func(pidLoop)() {
	u32 duration = taskTimerPid;
	if (tasks[TASK_PID].maxGap < duration)
		tasks[TASK_PID].maxGap = duration;
	TASK_START(TASK_PID);

	gyroFiltered[AXIS_ROLL].update(gyroScaled[AXIS_ROLL]);
	gyroFiltered[AXIS_PITCH].update(gyroScaled[AXIS_PITCH]);
	gyroFiltered[AXIS_YAW].update(gyroScaled[AXIS_YAW]);

	if (armed) {
		fix32 stickPos[3]; // RPY stick positions, -1...+1
		ELRS->getSmoothChannels(smoothChannels);
		// extract smoothed channels
		stickPos[0] = (smoothChannels[0] - 1500) >> 9; //-1...+1
		stickPos[1] = (smoothChannels[1] - 1500) >> 9;
		stickPos[2] = (smoothChannels[3] - 1500) >> 9;
		throttle = (smoothChannels[2] - 988); // 0...1024
		rollSetpoint = 0;
		pitchSetpoint = 0;
		yawSetpoint = 0;

		if (flightMode >= FlightMode::ANGLE) {
			startFixMath();
			// these will be populated below
			fix32 targetRoll, targetPitch;

			if (flightMode >= FlightMode::GPS) {

				static fix32 lastNVelSetpoint = 0;
				static fix32 lastEVelSetpoint = 0;

				if (flightMode == FlightMode::GPS) {
					// GPS: sticks => target velocity => target tilt => target angular rate
					sticksToGpsSetpoint(smoothChannels, &eVelSetpoint, &nVelSetpoint);
					// throttle stick => vertical velocity
					vVelSetpoint = stickToTargetVvel(throttle);
					if (forceZeroVvelSetpoint) {
						// force zero vVel until the stick crosses 1500
						vVelSetpoint = 0;
						if ((forceZeroVvelSetpoint == 1 && smoothChannels[2] > 1500) ||
							(forceZeroVvelSetpoint == -1 && smoothChannels[2] < 1500) ||
							flightModeChangeTimer > 2000000) {
							forceZeroVvelSetpoint = 0;
						}
					}
				} else {
					// GPS_WP: sticks do nothing, for now just fly home
					bool newState = (rthState != lastRthState);
					lastRthState = rthState;
					switch (rthState) {
					case 0: {
						// climb to 30m above home altitude
						const fix32 targetAlt = homepointAlt + 30;
						autopilotNavigate(rthStartLat, rthStartLon, targetAlt, &eVelSetpoint, &nVelSetpoint, &vVelSetpoint);
						if (combinedAltitude > targetAlt - 1)
							rthState = 1;
					} break;
					case 1: {
						// navigate home
						const fix32 targetAlt = homepointAlt + 30;
						autopilotNavigate(homepointLat, homepointLon, targetAlt, &eVelSetpoint, &nVelSetpoint, &vVelSetpoint);
						if (eVelSetpoint.abs() < 0.5f && nVelSetpoint.abs() < 0.5f)
							rthState = 2;
					} break;
					case 2: {
						// descend to 3m above home altitude, then hover
						static elapsedMillis hoverTimer;
						if (newState) hoverTimer = 0;
						const fix32 targetAlt = homepointAlt + 3;
						autopilotNavigate(homepointLat, homepointLon, targetAlt, &eVelSetpoint, &nVelSetpoint, &vVelSetpoint);
						if (eVelSetpoint.abs() > 0.5f || nVelSetpoint.abs() > 0.5f || vVelSetpoint.abs() > 0.5f) {
							hoverTimer = 0; // reset hover timer if we are still moving
						} else if (hoverTimer > 3000) {
							// after 3 seconds of hovering, land
							rthState = 3;
						}
					} break;
					case 3: {
						static elapsedMillis landTimer;
						static fix32 altitudes[8];
						static u8 altIndex = 0;
						if (newState) {
							landTimer = 0;
							for (int i = 0; i < 8; i++) {
								altitudes[i] = FIX32_MAX;
							}
						}
						autopilotNavigate(homepointLat, homepointLon, homepointAlt, &eVelSetpoint, &nVelSetpoint, &vVelSetpoint);
						vVelSetpoint = -0.3f;
						if (landTimer > 1000) {
							landTimer = 0;
							altitudes[altIndex++] = combinedAltitude;
							if (altIndex >= 8) {
								altIndex = 0;
							}
							fix32 minAlt = FIX32_MAX;
							fix32 maxAlt = FIX32_MIN;
							for (int i = 0; i < 8; i++) {
								if (altitudes[i] < minAlt)
									minAlt = altitudes[i];
								else if (altitudes[i] > maxAlt)
									maxAlt = altitudes[i];
							}
							if (maxAlt - minAlt < 0.5) {
								// if the altitude is stable for 8 seconds, we can stop
								rthState = 4;
								vVelSetpoint = 0;
								eVelSetpoint = 0;
								nVelSetpoint = 0;
							}
						}
					} break;
					case 4: {
						armed = false; // disarm after landing
						rthState = 0;
					}
					}
				}

				throttle = calcThrottle(vVelSetpoint);

				eVelError = eVelSetpoint - eVel;
				nVelError = nVelSetpoint - nVel;

				// shift to get more resolution in the filter
				// after shifting, we get a value of 0.08 for 1m/s² commanded acceleration
				// (1m/s / 1s / 3200f/s * 256 = 0.08)
				// assuming full 12m/s in 50ms, this gives us a value of 19.2 (0.08 * 12 * 20)
				ffFilterNVel.update((nVelSetpoint - lastNVelSetpoint) << 8);
				ffFilterEVel.update((eVelSetpoint - lastEVelSetpoint) << 8);
				// same here, just more shifting
				iRelaxFilterNVel.update((nVelSetpoint - lastNVelSetpoint) << 12);
				iRelaxFilterEVel.update((eVelSetpoint - lastEVelSetpoint) << 12);
				fix32 iRelaxTotal = sqrtFix(fix32(iRelaxFilterNVel) * fix32(iRelaxFilterNVel) + fix32(iRelaxFilterEVel) * fix32(iRelaxFilterEVel));
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

				// calculate PID terms
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
				targetRoll = eVelPID * cosHeading - nVelPID * sinHeading;
				targetPitch = eVelPID * sinHeading + nVelPID * cosHeading;
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

				lastNVelSetpoint = nVelSetpoint;
				lastEVelSetpoint = eVelSetpoint;
				nVelLast = nVel;
				eVelLast = eVel;
			} else {
				// angle or alt hold: sticks => target tilt => target angular rate
				targetRoll = (smoothChannels[0] - 1500) * stickToAngle; // -1000...+1000
				targetPitch = (smoothChannels[1] - 1500) * stickToAngle; // -1000...+1000

				if (flightMode == FlightMode::ALT_HOLD) {
					// throttle stick => vertical velocity
					vVelSetpoint = stickToTargetVvel(throttle);
					if (forceZeroVvelSetpoint) {
						// force zero vVel until the stick crosses 1500
						vVelSetpoint = 0;
						if ((forceZeroVvelSetpoint == 1 && smoothChannels[2] > 1500) ||
							(forceZeroVvelSetpoint == -1 && smoothChannels[2] < 1500) ||
							flightModeChangeTimer > 2000000) {
							forceZeroVvelSetpoint = 0;
						}
					}
					throttle = calcThrottle(vVelSetpoint);
				}
				// in case of angle, throttle is unchanged
			}

			// calculate yaw setpoint
			startRateInterp();
			fix32 newYawSetpoint = getRateInterp(stickPos[2], AXIS_YAW);

			/*
			headQuat = Quaternion that has target heading
			targetRPQuat = Quaternion that has target roll + target pitch
			targetQuat = headQuat * targetRPQuat
			diffQuat = from current orientation to targetQuat
					 = currentQuat^-1 * targetQuat
			extract roll, pitch and yaw from diffQuat, then generate target angular rates
			 */
			Quaternion headQuat, targetRPQuat, targetQuat, diffQuat;
			startFixMath();

			// set headQuat
			targetAngleHeading += newYawSetpoint / 3200;
			if (targetAngleHeading > 180)
				targetAngleHeading -= 360;
			else if (targetAngleHeading <= -180)
				targetAngleHeading += 360;
			fix32 halfHeading = -targetAngleHeading * FIX_DEG_TO_RAD / 2;
			fix32 co, si;
			sinCosFix(halfHeading, si, co);
			headQuat.w = co.getf32();
			headQuat.v[0] = 0;
			headQuat.v[1] = 0;
			headQuat.v[2] = si.getf32();

			// create targetRPQuat
			fix32 totalAngle = sqrtFix(targetRoll * targetRoll + targetPitch * targetPitch);
			fix32 ratios[3] = {
				-targetRoll / totalAngle,
				targetPitch / totalAngle,
				0 // yaw is not set
			};
			if (totalAngle > maxAngle)
				totalAngle = maxAngle;
			// Quaternion_fromAxisAngle(ratios, (totalAngle * FIX_DEG_TO_RAD).getf32(), &targetRPQuat);
			// using fix32:
			fix32 halfTotalAngle = totalAngle * FIX_DEG_TO_RAD / 2;
			sinCosFix(halfTotalAngle, si, co);
			targetRPQuat.w = co.getf32();
			targetRPQuat.v[0] = (ratios[0] * si).getf32();
			targetRPQuat.v[1] = (ratios[1] * si).getf32();
			targetRPQuat.v[2] = (ratios[2] * si).getf32();

			// create targetQuat
			Quaternion_multiply(&targetRPQuat, &headQuat, &targetQuat);

			// create diffQuat
			Quaternion currentQuatInv;
			Quaternion_conjugate(&q, &currentQuatInv);
			Quaternion_multiply(&targetQuat, &currentQuatInv, &diffQuat);
			Quaternion_normalize(&diffQuat, &diffQuat);
			// Ensure shortest rotation path by checking w component
			if (diffQuat.w < 0) {
				diffQuat.w = -diffQuat.w;
				diffQuat.v[0] = -diffQuat.v[0];
				diffQuat.v[1] = -diffQuat.v[1];
				diffQuat.v[2] = -diffQuat.v[2];
			}

			// extract roll, pitch and yaw from diffQuat
			fix32 axis[3];
			// fix32 angle = Quaternion_toAxisAngle(&diffQuat, axis);
			fix32 wFix = diffQuat.w;
			fix32 angle = acosFix(wFix) * 2;
			fix32 divider = sqrtFix(fix32(1) - wFix * wFix);
			if (divider > 0.0001f) {
				// Calculate the axis
				fix32 divNew = fix32(1) / divider;
				axis[0] = divNew * diffQuat.v[0];
				axis[1] = divNew * diffQuat.v[1];
				axis[2] = divNew * diffQuat.v[2];
			} else {
				// Arbitrary normalized axis
				axis[0] = 1;
				axis[1] = 0;
				axis[2] = 0;
			}

			// apply P gain and limit to total 1000 deg/s
			angle *= angleModeP;
			if (angle > FIX_DEG_TO_RAD * 1000) angle = FIX_DEG_TO_RAD * 1000;
			rollSetpoint = -angle * axis[0] * FIX_RAD_TO_DEG;
			pitchSetpoint = angle * axis[1] * FIX_RAD_TO_DEG;
			yawSetpoint = -angle * axis[2] * FIX_RAD_TO_DEG;
		} else {
			// acro is the simplest: we just need to calculate the setpoints based on the sticks, no fancy algos for althold or poshold
			startRateInterp();
			rollSetpoint = getRateInterp(stickPos[0], AXIS_ROLL);
			pitchSetpoint = getRateInterp(stickPos[1], AXIS_PITCH);
			yawSetpoint = getRateInterp(stickPos[2], AXIS_YAW);
		}

		// the setpoints have been calculated above using the respective flight mode
		// below is the PID controller (setpoint -> ESC output)

		// get errors
		rollError = rollSetpoint - gyroFiltered[AXIS_ROLL];
		pitchError = pitchSetpoint - gyroFiltered[AXIS_PITCH];
		yawError = yawSetpoint - gyroFiltered[AXIS_YAW];

		// I term windup prevention
		if (ELRS->channels[2] > 1020) {
			takeoffCounter++;
		} else if (takeoffCounter < 1000) { // 1000 = ca. 0.3s
			takeoffCounter = 0;
		} // if the quad hasn't "taken off" yet, reset the counter
		if (takeoffCounter < 1000) { // enable i term falloff (windup prevention) only before takeoff
			rollErrorSum = rollErrorSum - iFalloff / 3200 * rollErrorSum.sign() / pidGains[0][I];
			pitchErrorSum = pitchErrorSum - iFalloff / 3200 * pitchErrorSum.sign() / pidGains[1][I];
			yawErrorSum = yawErrorSum - iFalloff / 3200 * yawErrorSum.sign() / pidGains[2][I];
		}

		// PID boost / anti gravity
		fix32 pFactor = 1, iFactor = 1, dFactor = 1;
		if (pidBoostAxis) {
			pidBoostFilter.update(throttle - lastThrottle);
			fix32 boostStrength = (fix32(pidBoostFilter).abs() * 3200 - pidBoostStart) / (pidBoostFull - pidBoostStart);
			if (boostStrength > 1)
				boostStrength = 1;
			else if (boostStrength < 0)
				boostStrength = 0;
			pFactor += pidBoostP * boostStrength;
			iFactor += pidBoostI * boostStrength;
			dFactor += pidBoostD * boostStrength;
		}

		// setpoint differentiator for iRelax and FF
		setpointDiff[AXIS_ROLL].update(((rollSetpoint - lastSetpoints[AXIS_ROLL]) >> 4) * 3200); // e.g. 1250 for 1000 deg/s in 50ms
		setpointDiff[AXIS_PITCH].update(((pitchSetpoint - lastSetpoints[AXIS_PITCH]) >> 4) * 3200); // e.g. 1250 for 1000 deg/s in 50ms
		setpointDiff[AXIS_YAW].update(((yawSetpoint - lastSetpoints[AXIS_YAW]) >> 4) * 3200); // e.g. 1250 for 1000 deg/s in 50ms

		// I term relax multiplier
		fix32 totalDiff = fix32(setpointDiff[AXIS_ROLL]).abs() + fix32(setpointDiff[AXIS_PITCH]).abs() + fix32(setpointDiff[AXIS_YAW]).abs();
		fix32 iRelaxMultiplier = 1;
		if (totalDiff > 300) {
			iRelaxMultiplier = fix32(0.0625f);
		} else if (totalDiff > 70) {
			iRelaxMultiplier = fix32(1) - (totalDiff - 70) / 230 * 15 / 16;
		}

		// I sum
		rollErrorSum = rollErrorSum + rollError * iRelaxMultiplier * iFactor;
		pitchErrorSum = pitchErrorSum + pitchError * iRelaxMultiplier * iFactor;
		yawErrorSum = yawErrorSum + yawError * iRelaxMultiplier * iFactor;

		// PID terms
		rollP = pidGains[0][P] * rollError * pFactor;
		pitchP = pidGains[1][P] * pitchError * pFactor;
		yawP = pidGains[2][P] * yawError * (pidBoostAxis == 2 ? pFactor : 1);
		rollI = pidGains[0][I] * rollErrorSum;
		pitchI = pidGains[1][I] * pitchErrorSum;
		yawI = pidGains[2][I] * yawErrorSum;
		rollD = pidGains[0][D] * dFilterRoll.update(rollLast - gyroFiltered[AXIS_ROLL]) * dFactor;
		pitchD = pidGains[1][D] * dFilterPitch.update(pitchLast - gyroFiltered[AXIS_PITCH]) * dFactor;
		yawD = pidGains[2][D] * dFilterYaw.update(yawLast - gyroFiltered[AXIS_YAW]) * (pidBoostAxis == 2 ? dFactor : 1);
		rollFF = pidGains[0][FF] * setpointDiff[AXIS_ROLL];
		pitchFF = pidGains[1][FF] * setpointDiff[AXIS_PITCH];
		yawFF = pidGains[2][FF] * setpointDiff[AXIS_YAW];
		rollS = pidGains[0][S] * rollSetpoint;
		pitchS = pidGains[1][S] * pitchSetpoint;
		yawS = pidGains[2][S] * yawSetpoint;

		lastThrottle = throttle;

		// RPY terms
		fix32 rollTerm = rollP + rollI + rollD + rollFF + rollS;
		fix32 pitchTerm = pitchP + pitchI + pitchD + pitchFF + pitchS;
		fix32 yawTerm = yawP + yawI + yawD + yawFF + yawS;

		bool runDynIdle = useDynamicIdle && escErpmFailCounter < 10; // make sure rpm data is valid, tolerate up to 10 cycles without a valid RPM before switching to static idle
		if (!runDynIdle) {
			// scale throttle from 0...1024 to idlePermille*2...2000 (DShot output is 0...2000)
			throttle *= throttleScale; // 0...1024 => 0...2000-idlePermille*2
			throttle += idlePermille * 2; // 0...2000-idlePermille*2 => idlePermille*2...2000
		}

		// apply mixer
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

		// apply idling / throttle clamping
		if (runDynIdle) {
			static i32 lastRpm[4] = {0};
			i32 minIncrease = INT32_MIN; // typically negative, but >0 when one channels wants to lift
			i32 tryDecrease = 0;
			startDynIdleInterp();

			for (int i = 0; i < 4; i++) {
				auto &t = throttles[i];
				fix32 minT = 0;
				if (escRpm[i] < dynamicIdleRpm * 2) {
					// if lower than 2x idle RPM, run PID
					i32 rpmError = dynamicIdleRpm - escRpm[i];
					dynamicIdlePids[i][P] = dynamicIdlePidGains[P] * rpmError;
					dynamicIdlePids[i][I] += dynamicIdlePidGains[I] * rpmError;
					interp1->accum[0] = dynamicIdlePids[i][I].raw;
					dynamicIdlePids[i][I].raw = interp1->peek[0]; // clamp to 0-400
					dynamicIdlePids[i][D] = dynamicIdlePidGains[D] * (lastRpm[i] - (i32)escRpm[i]);
					minT = dynamicIdlePids[i][P] + dynamicIdlePids[i][I] + dynamicIdlePids[i][D];
					interp1->accum[0] = minT.raw;
					minT.raw = interp1->peek[0]; // clamp to 0-400
				} else {
					dynamicIdlePids[i][I] = 0;
				}
				lastRpm[i] = escRpm[i];

				i32 temp = (minT - t).geti32();
				if (temp > minIncrease) minIncrease = temp;
				temp = 2000 - t;
				if (temp < tryDecrease) tryDecrease = temp;
			}
			i32 diff = tryDecrease;
			if (diff < minIncrease) diff = minIncrease;

#if __ARM_FEATURE_SIMD32
			int16x2_t *const thr = (int16x2_t *)&throttles; // make the following code easier to read
			i32 x = (diff & 0xFFFF) | (diff << 16);
			thr[0] = __sadd16(thr[0], x); // add diff to 0 and 1
			thr[1] = __sadd16(thr[1], x); // add diff to 2 and 3
			x = 2000 << 16 | 2000; // set max value
			thr[0] = min16x2(thr[0], x); // choose the min of 2000 and 0/1
			thr[1] = min16x2(thr[1], x); // choose the min of 2000 and 2/3
#else
			for (int i = 0; i < 4; i++) {
				auto &t = throttles[i];
				t += diff;
				if (t > 2000) t = 2000;
			}
#endif
		} else {
#if __ARM_FEATURE_SIMD32
			int16x2_t *const thr = (int16x2_t *)&throttles; // make the following code easier to read
			i32 low = min16x2(thr[0], thr[1]); // low has the min of {0; 2} and {1; 3}, e.g. 0 and 3
			i32 high = max16x2(thr[0], thr[1]); // high has the max of {0; 2} and {1; 3}, e.g. 1 and 2

			int16x2_t x = low >> 16;
			low = (i16)low;
			low = min16x2(low, x); // get lower of the two low values
			x = high >> 16;
			high = (i16)high;
			high = max16x2(high, x); // get the higher of the two high values

			// find largest diff to add to all such that all are >= 0 (and preferrably <= 2000)
			i32 diff = 0;
			if (high > 2000) diff = 2000 - high; // calc the diff just like in non simd
			if (low + diff < idlePermille * 2) diff = idlePermille * 2 - low;

			x = (diff & 0xFFFF) | (diff << 16);
			thr[0] = __sadd16(thr[0], x); // add diff to 0 and 1
			thr[1] = __sadd16(thr[1], x); // add diff to 2 and 3
			x = 2000 << 16 | 2000; // set max value
			thr[0] = min16x2(thr[0], x); // choose the min of 2000 and 0/1
			thr[1] = min16x2(thr[1], x); // choose the min of 2000 and 2/3
#else
			i16 low = 0x7FFF, high = 0, diff = 0;
			for (int i = 0; i < 4; i++) {
				auto &t = throttles[i];
				if (t < low) low = t;
				if (t > high) high = t;
			}
			// find largest diff to add to all such that all are >= 0 (and preferrably <= 2000)
			if (high > 2000) diff = 2000 - high;
			if (low + diff < idlePermille * 2) diff = idlePermille * 2 - low;
			for (int i = 0; i < 4; i++) {
				auto &t = throttles[i];
				t += diff;
				if (t > 2000) t = 2000;
			}
#endif
		}

		// send to ESCs
		sendThrottles(throttles);

		// save states
		rollLast = gyroFiltered[AXIS_ROLL];
		pitchLast = gyroFiltered[AXIS_PITCH];
		yawLast = gyroFiltered[AXIS_YAW];
		lastSetpoints[AXIS_ROLL] = rollSetpoint;
		lastSetpoints[AXIS_PITCH] = pitchSetpoint;
		lastSetpoints[AXIS_YAW] = yawSetpoint;
		vVelLast = vVel;

		// write blackbox if needed
		if ((pidLoopCounter % bbFreqDivider) == 0 && bbFreqDivider) {
			taskTimerTASK_PID -= writeSingleFrame();
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
			static elapsedMillis motorBeepTimer;
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
		targetAngleHeading = yaw * FIX_RAD_TO_DEG;
	}
	TASK_END(TASK_PID);
	taskTimerPid = 0;
}

static fix32 stickToTargetVvel(fix32 stickPos) {
	fix32 t = stickPos - 512;
	static elapsedMicros setAltSetpointTimer;
	static u32 stickWasCentered = 0;

	// deadband in center of stick
	if (t.abs() < 50)
		t = 0;
	else if (t > 0)
		t -= 50;
	else
		t += 50;

	// estimate throttle
	fix32 target = t / 90; // +/- 5 m/s
	if (target == 0) {
		if (!stickWasCentered) {
			setAltSetpointTimer = 0;
			stickWasCentered = 1;
		} else if (setAltSetpointTimer > 1000000) {
			if (stickWasCentered == 1) {
				// set altitude setpoint 1s after throttle is centered
				altSetpoint = combinedAltitude;
				stickWasCentered = 2;
			}
			target += (altSetpoint - combinedAltitude) / 5; // prevent vVel drift slowly
		}
	} else {
		stickWasCentered = 0;
	}
	return target;
}

static fix32 calcThrottle(fix32 targetVvel) {
	vVelError = targetVvel - vVel;
	vVelErrorSum = vVelErrorSum + ((vVelFFFilter.update(targetVvel - vVelLastSetpoint).abs() < 0.001f) ? vVelError : vVelError / 2); // reduce windup during fast changes
	vVelErrorSum = constrain(vVelErrorSum, vVelMinErrorSum, vVelMaxErrorSum);
	vVelP = pidGainsVVel[P] * vVelError;
	vVelI = pidGainsVVel[I] * vVelErrorSum;
	vVelD = pidGainsVVel[D] * vVelDFilter.update(vVelLast - vVel);
	vVelFF = pidGainsVVel[FF] * vVelFFFilter;
	vVelLastSetpoint = targetVvel;
	fix32 t = vVelP + vVelI + vVelD + vVelFF;
	/* The cos of the thrust angle gives us the thrust "efficiency" (=cos(acos(...))),
	aka how much of the thrust is actually used to lift the quad.
	Dividing by this "efficiency" will give us the actual thrust needed to lift the quad.
	This acts much quicker than the PID would ever increase the throttle when tilting the quad. */
	fix32 throttleFactor = cosRoll * cosPitch;
	if (throttleFactor < 0) // quad is upside down
		throttleFactor = 1;
	throttleFactor = constrain(throttleFactor, 0.33f, 1); // we limit the throttle increase to 3x (ca. 72° tilt), and also prevent division by zero
	throttleFactor = fix32(1) / throttleFactor; // 1/cos(acos(...)) = 1/cos(thrust angle)
	t = t * throttleFactor;
	t = constrain(t, 0, 1024);
	return t;
}

void distFromCoordinates(fix64 lat1, fix64 lon1, fix64 lat2, fix64 lon2, fix32 *distNorth, fix32 *distEast) {
	// calculate distance in meters between two coordinates
	fix64 latDiff = lat2 - lat1;
	fix64 lonDiff = lon2 - lon1;
	if (lonDiff > 180)
		lonDiff = lonDiff - 360;
	else if (lonDiff < -180)
		lonDiff = lonDiff + 360;
	*distNorth = latDiff * (40075000 / 360); // in m
	*distEast = lonDiff * (40075000 / 360); // in m
	*distEast *= cosFix(fix32(lat1) * DEG_TO_RAD); // adjust for latitude
}

static void sticksToGpsSetpoint(const fix32 *channels, fix32 *eVelSetpoint, fix32 *nVelSetpoint) {
	static elapsedMicros locationSetpointTimer;
	fix32 rightCommand = channels[0] - 1500;
	fix32 fwdCommand = channels[1] - 1500;

	// fwd/right stick deadband
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

	// calculate nVel/eVel setpoints
	if (rightCommand || fwdCommand) {
		// fly into a direction
		*eVelSetpoint = cosHeading * rightCommand + sinHeading * fwdCommand;
		*nVelSetpoint = -sinHeading * rightCommand + cosHeading * fwdCommand;
		*eVelSetpoint = *eVelSetpoint >> 9; // +-512 => +-1 (slightly less due to deadband)
		*nVelSetpoint = *nVelSetpoint >> 9; // +-512 => +-1 (slightly less due to deadband)
		*eVelSetpoint *= maxTargetHvel; // +-1 => +-12m/s
		*nVelSetpoint *= maxTargetHvel; // +-1 => +-12m/s
		pushNorth.set(0);
		pushEast.set(0);
		locationSetpointTimer = 0;
	} else if (locationSetpointTimer < 2000000) {
		// stop craft within the first 2s after releasing sticks
		targetLat = gpsLatitudeFiltered;
		targetLon = gpsLongitudeFiltered;
		*eVelSetpoint = 0;
		*nVelSetpoint = 0;
	} else {
		// lock position 2s after releasing sticks (push..., target...)
		fix32 distNorth, distEast;
		distFromCoordinates(gpsLatitudeFiltered, gpsLongitudeFiltered, targetLat, targetLon, &distNorth, &distEast);
		pushNorth.update(distNorth);
		pushEast.update(distEast);
		*eVelSetpoint = fix32(pushEast) / 4;
		*eVelSetpoint = constrain(*eVelSetpoint, -maxTargetHvel, maxTargetHvel);
		*nVelSetpoint = fix32(pushNorth) / 4;
		*nVelSetpoint = constrain(*nVelSetpoint, -maxTargetHvel, maxTargetHvel);
	}
}

static void autopilotNavigate(fix64 toLat, fix64 toLon, fix32 toAlt, fix32 *eVelSetpoint, fix32 *nVelSetpoint, fix32 *targetVvel) {
	// autopilot navigation: fly to a location
	fix32 distNorth, distEast;
	distFromCoordinates(gpsLatitudeFiltered, gpsLongitudeFiltered, toLat, toLon, &distNorth, &distEast);
	fix32 dist = sqrtFix(distNorth * distNorth + distEast * distEast);
	fix32 angle = atan2Fix(distEast, distNorth); // in radians

	// calculate velocity setpoints
	fix32 speed = dist / 2; // 2s to reach the target
	fix32 est = 2; // estimated time to reach the target
	if (speed > maxTargetHvel) {
		speed = maxTargetHvel;
		est = dist / speed;
	}
	fix32 si, co;
	sinCosFix(angle, si, co);
	*eVelSetpoint = speed * si;
	*nVelSetpoint = speed * co;

	fix32 altDiff = toAlt - combinedAltitude;
	fix32 temp = altDiff / est;
	if (temp > 5)
		temp = 5; // max 5m/s vertical speed
	else if (temp < -5)
		temp = -5; // min -5m/s vertical speed
	*targetVvel = temp;
}

void setFlightMode(FlightMode mode) {
	if (mode >= FlightMode::LENGTH) return;

	// update OSD element
	switch (mode) {
	case FlightMode::ACRO:
		updateElem(OSDElem::FLIGHT_MODE, "ACRO ");
		break;
	case FlightMode::ANGLE:
		updateElem(OSDElem::FLIGHT_MODE, "ANGLE");
		break;
	case FlightMode::ALT_HOLD:
		updateElem(OSDElem::FLIGHT_MODE, "ALT  ");
		break;
	case FlightMode::GPS:
		updateElem(OSDElem::FLIGHT_MODE, "GPS  ");
		break;
	case FlightMode::GPS_WP:
		updateElem(OSDElem::FLIGHT_MODE, "WAYPT");
		break;
	default:
		break;
	}

	if (flightMode < FlightMode::ALT_HOLD && mode >= FlightMode::ALT_HOLD) {
		// just switched to an altitude hold mode, make sure the quad doesn't just fall at the beginning
		vVelErrorSum = throttle.getfix64() / pidGainsVVel[I];
		if (mode == FlightMode::ALT_HOLD || mode == FlightMode::GPS)
			forceZeroVvelSetpoint = ELRS->channels[2] > 1500 ? 1 : -1; // flag to force zero vVel until the stick crossed 1500
		altSetpoint = combinedAltitude;
	}
	if (flightMode < FlightMode::ANGLE && mode >= FlightMode::ANGLE) {
		targetAngleHeading = yaw * FIX_RAD_TO_DEG;
	}
	if (mode == FlightMode::GPS) {
		// just switched to GPS mode, prevent suddenly flying away to the old position lock
		targetLat = gpsLatitudeFiltered;
		targetLon = gpsLongitudeFiltered;
		altSetpoint = combinedAltitude;
	}
	if (mode == FlightMode::GPS_WP) {
		rthStartLat = gpsLatitudeFiltered;
		rthStartLon = gpsLongitudeFiltered;
		rthState = 0;
		lastRthState = 255;
	}

	flightModeChangeTimer = 0;
	flightMode = mode;
}
