#include "global.h"

static fix32 angleModeP = 10;

fix64 rthStartLat, rthStartLon;
u8 rthState = 0, lastRthState = 255; // 0: climb, 1: navigate home, 2: descend, 3: land

fix32 rateCoeffs[3][3];
static fix32 rateInterp[3][258]; // +-128 (=257) for sticks, index 257 only exists so that high + 1 is valid even if stick = 1

static u8 maxAngle; // degrees, applied in angle mode and GPS mode
static u8 maxAngleBurst; // degrees, this angle is allowed for a short time, e.g. when accelerating in GPS mode (NOT used in angle mode)
static elapsedMicros burstTimer;
static elapsedMicros burstCooldown;
static u32 angleBurstTime; // microseconds, time for which the maxAngleBurst is allowed
static u16 angleBurstCooldownTime; // microseconds, time for which the maxAngleBurst is not allowed after the burst time

static i8 forceZeroVvelSetpoint = 0;
static elapsedMicros flightModeChangeTimer;

static interp_config rateInterpConfig0, rateInterpConfig1, rateInterpConfig2;

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
	// the peek value is shifted and clamped to -0.5...0.5 (-32768...32768), after >> 8 shifting it is -128...128, then move that range to 0...256 for the LUT
	i32 high = ((i32)interp1->peek[0] >> 8) + 128; // high byte (from 0-255 for -1 to <1 stick, 256 only for stick = 1)

	interp0->accum[1] = interp1->peek[0]; // interpolation alpha value (only 8 LSBs (the ones we removed in the high byte) are used), this is the low byte
	interp0->base[0] = rateInterp[axis][high].raw; // lower bound from LUT
	interp0->base[1] = rateInterp[axis][high + 1].raw; // upper bound from LUT
	return fix32().setRaw(interp0->peek[1]); // alpha applied between lower and upper
}

static fix32 stickPos[3]; // -1...+1
static fix32 stickThr; // 0...1024

static void getStickPos() {
	fix32 smoothChannels[4]; // smoothed RC channel values (1000ish to 2000ish)
	ELRS->getSmoothChannels(smoothChannels);

	stickPos[0] = (smoothChannels[0] - 1500) >> 9;
	stickPos[1] = (smoothChannels[1] - 1500) >> 9;
	stickPos[2] = (smoothChannels[3] - 1500) >> 9;
	stickThr = (smoothChannels[2] - 988);
}

static void runAcroMode() {
	getStickPos();

	// acro is the simplest: we just need to calculate the setpoints based on the sticks, no fancy algos for althold or poshold
	startRateInterp();
	rollSetpoint = getRateInterp(stickPos[0], AXIS_ROLL);
	pitchSetpoint = getRateInterp(stickPos[1], AXIS_PITCH);
	yawSetpoint = getRateInterp(stickPos[2], AXIS_YAW);
}

static Quaternion headQuat; // has only target heading (based on yaw, not combinedHeading)
static Quaternion targetRPQuat; // has only target roll + pitch
static Quaternion targetQuat; // target roll + pitch applied onto target heading: headQuat * targetRPQuat
//= from current orientation to targetQuat
//= currentQuat^-1 * targetQuat
// extract roll, pitch and yaw from diffQuat, then generate target angular rates
static Quaternion diffQuat;

static fix32 targetAngleHeading;
static fix32 targetRoll, targetPitch;

static void runAngleMode1() {
	getStickPos();

	// angle or alt hold: sticks => target tilt => target angular rate
	targetRoll = stickPos[0] * maxAngle;
	targetPitch = stickPos[1] * maxAngle;
	// in case of angle, throttle is unchanged
}

static void setAutoThrottle() {
	// throttle stick => vertical velocity
	vVelSetpoint = stickToTargetVvel(throttle);
	if (forceZeroVvelSetpoint) {
		// force zero vVel until the stick crosses 1500
		vVelSetpoint = 0;
		if ((forceZeroVvelSetpoint == 1 && stickThr > 512) ||
			(forceZeroVvelSetpoint == -1 && stickThr < 512) ||
			flightModeChangeTimer > 2000000) {
			forceZeroVvelSetpoint = 0;
		}
	}
	// vertical velocity => auto throttle
	throttle = calcThrottle(vVelSetpoint);
}

static void runAngleMode2() {
	// calculate yaw setpoint
	startRateInterp();
	fix32 newYawSetpoint = getRateInterp(stickPos[2], AXIS_YAW);

	startFixMath();

	// set headQuat
	targetAngleHeading += newYawSetpoint / 3200;
	if (targetAngleHeading > 180)
		targetAngleHeading -= 360;
	else if (targetAngleHeading <= -180)
		targetAngleHeading += 360;

	fix32 halfHeading = -targetAngleHeading * FIX_DEG_TO_RAD / 2;
	fix32 si, co;
	sinCosFix(halfHeading, si, co);
	headQuat.w = co.getf32();
	headQuat.v[0] = 0;
	headQuat.v[1] = 0;
	headQuat.v[2] = si.getf32();
}

static void runAngleMode3() {
	startFixMath();

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
	fix32 si, co;
	sinCosFix(halfTotalAngle, si, co);
	targetRPQuat.w = co.getf32();
	targetRPQuat.v[0] = (ratios[0] * si).getf32();
	targetRPQuat.v[1] = (ratios[1] * si).getf32();
	targetRPQuat.v[2] = (ratios[2] * si).getf32();
}

static void runAngleMode4() {
	// create targetQuat
	Quaternion_multiply(&targetRPQuat, &headQuat, &targetQuat);

	// create diffQuat
	Quaternion currentQuatInv;
	Quaternion_conjugate(&q, &currentQuatInv);
	Quaternion_multiply(&targetQuat, &currentQuatInv, &diffQuat);
}

static void runAngleMode5() {
	// continuation from 4:
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
}

void controlLoop() {
	TASK_START(TASK_CONTROL);
	if (flightMode == FlightMode::ACRO) {
		runAcroMode();
	} else {
		static u8 controlCycle = 0;
		switch (controlCycle) {
		case 0:
			TASK_START(TASK_CONTROL_1);
			TASK_END(TASK_CONTROL_1);
			break;
		case 1:
			TASK_START(TASK_CONTROL_2);
			TASK_END(TASK_CONTROL_2);
			break;
		case 2:
			TASK_START(TASK_CONTROL_3);
			TASK_END(TASK_CONTROL_3);
			break;
		case 3:
			TASK_START(TASK_CONTROL_4);
			TASK_END(TASK_CONTROL_4);
			break;
		case 4:
			TASK_START(TASK_CONTROL_5);
			runAngleMode2();
			TASK_END(TASK_CONTROL_5);
			break;
		case 5:
			TASK_START(TASK_CONTROL_6);
			runAngleMode3();
			TASK_END(TASK_CONTROL_6);
			break;
		case 6:
			TASK_START(TASK_CONTROL_7);
			runAngleMode4();
			TASK_END(TASK_CONTROL_7);
			break;
		case 7:
			TASK_START(TASK_CONTROL_8);
			runAngleMode5();
			TASK_END(TASK_CONTROL_8);
			break;
		}
		if (++controlCycle >= 8) controlCycle = 0;
	}

	TASK_END(TASK_CONTROL);
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

void initControl() {
	// Acro mode
	addArraySetting(SETTING_RATE_COEFFS, rateCoeffs, &initRateCoeffs);
	// Angle Mode
	addSetting(SETTING_MAX_ANGLE, &maxAngle, 40);
	addSetting(SETTING_ANGLE_MODE_P, &angleModeP, 5);
	// GPS mode
	addSetting(SETTING_MAX_ANGLE_BURST, &maxAngleBurst, 60);
	addSetting(SETTING_ANGLE_BURST_TIME, &angleBurstTime, 3000);
	addSetting(SETTING_ANGLE_BURST_COOLDOWN, &angleBurstCooldownTime, 5000);

	initRateInterp();

	stickToAngle = fix32(maxAngle) / 512;
}
