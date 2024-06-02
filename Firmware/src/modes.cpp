#include "global.h"
bool armed = false;
FLIGHT_MODE lastFlightMode = FLIGHT_MODE::LENGTH;
i32 startPointLat, startPointLon;
/**
 * 0: switch in armed position for >= 10 cycles
 * 1: throttle down
 * 2: GPS fix and >= 6 satellites
 * 3: configurator not attached (by configurator ping) -> used by Kolibri configurator
 * 4: at least one good ELRS packet before arming
 * 5: allowed modes during arm: acro, angle
 * 6: Gyro calibrated
 * 7: takeoff prevention from configurator (MspFn::SET_ARMING_DISABLED) -> not used by Kolibri configurator
 */
u32 armingDisableFlags = 0;

void modesLoop() {
	if (ELRS->newPacketFlag & 0x00000001) {
		elapsedMicros taskTimer = 0;
		tasks[TASK_MODES].runCounter++;
		ELRS->newPacketFlag &= 0xFFFFFFFE;
		if (!armed) {
			if (ELRS->consecutiveArmedCycles == 10)
				armingDisableFlags &= 0xFFFFFFFE;
			else
				armingDisableFlags |= 0x00000001;
			if (ELRS->channels[2] < 1020)
				armingDisableFlags &= 0xFFFFFFFD;
			else
				armingDisableFlags |= 0x00000002;
			// if (lastPvtMessage > 1000)
			// 	armingDisableFlags |= 0x00000004;
			if (configuratorConnected && mspOverrideMotors > 1000)
				armingDisableFlags |= 0x00000008;
			else
				armingDisableFlags &= 0xFFFFFFF7;
			if (ELRS->isLinkUp)
				armingDisableFlags &= 0xFFFFFFEF;
			else
				armingDisableFlags |= 0x00000010;
			if (flightMode <= FLIGHT_MODE::ANGLE)
				armingDisableFlags &= 0xFFFFFFDF;
			else
				armingDisableFlags |= 0x00000020;
			if (!armingDisableFlags) {
				startLogging();
				armed = true;
				gpio_put(PIN_LED_DEBUG, 1);
				startPointLat = gpsMotion.lat;
				startPointLon = gpsMotion.lon;
			} else if (ELRS->consecutiveArmedCycles == 10 && ELRS->isLinkUp) {
				u8 wavSuccess = 0;
				if (armingDisableFlags & 0x00000040) {
					wavSuccess = playWav("armingErrors/gyro.wav");
				} else if (armingDisableFlags & 0x00000004) {
					wavSuccess = playWav("armingErrors/nogpsmodule.wav");
				} else if (armingDisableFlags & 0x00000008) {
					wavSuccess = playWav("armingErrors/configurator.wav");
				} else if (armingDisableFlags & 0x00000010) {
					wavSuccess = playWav("armingErrors/elrsLink.wav");
				} else if (armingDisableFlags & 0x00000020) {
					wavSuccess = playWav("armingErrors/wrongMode.wav");
				} else if (armingDisableFlags & 0x00000002) {
					wavSuccess = playWav("armingErrors/throttle.wav");
				}
				if (!wavSuccess)
					makeSound(2500, 599, 70, 50);
			}
		} else if (ELRS->channels[4] < 1500) {
			armed = false;
			gpio_put(PIN_LED_DEBUG, 0);
			// just disarmed, stop logging
			if (ELRS->lastChannels[4] > 1500)
				endLogging();
		}
		flightMode = (FLIGHT_MODE)((ELRS->channels[6] - 900) / 200);
		if (flightMode >= FLIGHT_MODE::LENGTH)
			flightMode = FLIGHT_MODE::ACRO;
		if (flightMode != lastFlightMode) {
			switch (flightMode) {
			case FLIGHT_MODE::ACRO:
				updateElem(OSDElem::FLIGHT_MODE, "ACRO ");
				break;
			case FLIGHT_MODE::ANGLE:
				updateElem(OSDElem::FLIGHT_MODE, "ANGLE");
				break;
			case FLIGHT_MODE::ALT_HOLD:
				updateElem(OSDElem::FLIGHT_MODE, "ALT  ");
				break;
			case FLIGHT_MODE::GPS_VEL:
				updateElem(OSDElem::FLIGHT_MODE, "GPSV ");
				break;
			case FLIGHT_MODE::GPS_POS:
				updateElem(OSDElem::FLIGHT_MODE, "GPSP ");
				break;
			default:
				break;
			}
			if (lastFlightMode <= FLIGHT_MODE::ANGLE && flightMode > FLIGHT_MODE::ANGLE) {
				// just switched to GPS mode, make sure the quad doesn't just fall at the beginning
				volatile i64 helper = throttle.getInt() / pidGainsVVel[I].getf32();
				vVelErrorSum.setRaw(helper << 32);
				altSetpoint = combinedAltitude;
			}
			lastFlightMode = flightMode;
		}
		u32 duration = taskTimer;
		tasks[TASK_MODES].totalDuration += duration;
		if (duration < tasks[TASK_MODES].minDuration) {
			tasks[TASK_MODES].minDuration = duration;
		}
		if (duration > tasks[TASK_MODES].maxDuration) {
			tasks[TASK_MODES].maxDuration = duration;
		}
	} else if (ELRS->sinceLastRCMessage >= 500000) {
		armed = false;
		endLogging();
	}
}

void modesInit() {
	placeElem(OSDElem::FLIGHT_MODE, 1, 2);
	enableElem(OSDElem::FLIGHT_MODE);
}