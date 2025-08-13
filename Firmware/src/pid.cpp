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

// ESC outputs
i16 throttles[4] __attribute__((aligned(4)));

// Gyro data
fix32 gyroScaled[3];
PT1 gyroFiltered[3];

// PID controller config
u16 pidGainsNice[3][5] = {0};
fix32 pidGains[3][5];
fix32 iFalloff;
u16 dFilterCutoff;
u16 gyroFilterCutoff;

// setpoints from control.cpp
fix32 rollSetpoint, pitchSetpoint, yawSetpoint;
fix32 throttleSetpoint;

// PID controller runtime variables
static fix32 rollLast, pitchLast, yawLast; // rate of last PID cycle (deg/s)
static fix64 rollErrorSum, pitchErrorSum, yawErrorSum; // I term sum for the PID controller
fix32 rollP, pitchP, yawP, rollI, pitchI, yawI, rollD, pitchD, yawD, rollFF, pitchFF, yawFF, rollS, pitchS, yawS;
fix32 throttle;
PT2 dFilterRoll, dFilterPitch, dFilterYaw;

// Idling
u8 idlePermille;
bool useDynamicIdle = true;
u16 dynamicIdleRpm = 3000;
fix32 dynamicIdlePids[4][3] = {0}; // [motor][P, I, D]
fix32 dynamicIdlePidGains[3] = {.2, 0.0015, .07};
fix32 throttleScale;
interp_config dynIdleInterpConfig;

// FF / PID boost filter
fix32 setpointDiffCutoff = 12;
PT1 setpointDiff[3];
fix32 lastSetpoints[3];

// PID boost
fix32 pidBoostCutoff = 5; // cutoff frequency for pid boost throttle filter
PT1 pidBoostFilter;
fix32 lastThrottle;
u8 pidBoostAxis = 0; // 0: off, 1: RP only, 2: RPY
fix32 pidBoostP = 5; // addition boost factor, e.g. when set to 2 in full effect, P is 3x
fix32 pidBoostI = 5; // addition boost factor, e.g. when set to 2 in full effect, I is 3x
fix32 pidBoostD = 0; // addition boost factor, e.g. when set to 2 in full effect, D is 3x
fix32 pidBoostStart; // dThrottle/dt in 1/1024 / s when pidBoost starts
fix32 pidBoostFull; // dThrottle/dt in 1/1024 / s when pidBoost is in full effect

// Misc
u32 pidLoopCounter = 0; // counter of PID controller loops

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

static void startDynIdleInterp() {
	interp_set_config(interp1, 0, &dynIdleInterpConfig);
	interp1->base[0] = 0 << 16;
	interp1->base[1] = 400 << 16;
}

void initDynIdlePids() {
	dynamicIdlePidGains[P] = .2;
	dynamicIdlePidGains[I] = .0015;
	dynamicIdlePidGains[D] = .07;
}

void initPid() {
	addArraySetting(SETTING_PID_GAINS, pidGainsNice, &initPidGains);
	addSetting(SETTING_IDLE_PERMILLE, &idlePermille, 35);
	addSetting(SETTING_DFILTER_CUTOFF, &dFilterCutoff, 70);
	addSetting(SETTING_GYRO_FILTER_CUTOFF, &gyroFilterCutoff, 100);
	addSetting(SETTING_SETPOINT_DIFF_CUTOFF, &setpointDiffCutoff, 12);
	addSetting(SETTING_PID_BOOST_CUTOFF, &pidBoostCutoff, 5);
	addSetting(SETTING_PID_BOOST_START, &pidBoostStart, 2000000);
	addSetting(SETTING_PID_BOOST_FULL, &pidBoostFull, 6000000);
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
			inFlightTuningParams.push_back(TunableParameter(&pidGainsNice[i][j], 2, 0, 2000, (const char *)buf, &convertPidsFromNice));
		}
	}

	dynIdleInterpConfig = interp_default_config();
	interp_config_set_clamp(&dynIdleInterpConfig, true);
	interp_config_set_signed(&dynIdleInterpConfig, true);

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
}

static u32 takeoffCounter = 0;
static elapsedMicros taskTimerPid;
void pidLoop() {
	u32 duration = taskTimerPid;
	if (tasks[TASK_PID].maxGap < duration)
		tasks[TASK_PID].maxGap = duration;
	TASK_START(TASK_PID);

	// the setpoints have been calculated in control.cpp using the respective flight mode
	// below is the PID controller (setpoint -> ESC output)

	// get errors (deg/s)
	fix32 rollError = rollSetpoint - gyroFiltered[AXIS_ROLL];
	fix32 pitchError = pitchSetpoint - gyroFiltered[AXIS_PITCH];
	fix32 yawError = yawSetpoint - gyroFiltered[AXIS_YAW];

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
	if (runDynIdle) {
		throttle *= fix32(2000) >> 10;
	} else {
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

	// write blackbox if needed
	if ((pidLoopCounter % bbFreqDivider) == 0 && bbFreqDivider) {
		taskTimerTASK_PID -= writeSingleFrame();
	}
	pidLoopCounter++;

	TASK_END(TASK_PID);
	taskTimerPid = 0;
}

void pidDisarmedLoop() {
	u32 duration = taskTimerPid;
	if (tasks[TASK_PID].maxGap < duration)
		tasks[TASK_PID].maxGap = duration;
	TASK_START(TASK_PID);

	// Quad disarmed
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

	TASK_END(TASK_PID);
	taskTimerPid = 0;
}
