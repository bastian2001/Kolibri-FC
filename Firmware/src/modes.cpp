#include "global.h"
bool armed = false;
fix64 homepointLat, homepointLon;
fix32 homepointAlt;

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
	if (ELRS->newPacketFlag & (1 << 0)) {
		elapsedMicros taskTimer = 0;
		tasks[TASK_MODES].runCounter++;
		ELRS->newPacketFlag &= ~(1 << 0); // clear the flag
		if (!armed) {
			if (ELRS->consecutiveArmedCycles == 10)
				armingDisableFlags &= ~0x01;
			else
				armingDisableFlags |= 0x01;
			if (ELRS->channels[2] < 1020)
				armingDisableFlags &= ~0x02;
			else
				armingDisableFlags |= 0x02;
			// if (lastPvtMessage > 1000)
			// 	armingDisableFlags |= 0x04;
			if (configuratorConnected && mspOverrideMotors > 1000)
				armingDisableFlags |= 0x08;
			else
				armingDisableFlags &= ~0x08;
			if (ELRS->isLinkUp)
				armingDisableFlags &= ~0x10;
			else
				armingDisableFlags |= 0x10;
			if (flightMode <= FlightMode::ANGLE)
				armingDisableFlags &= ~0x20;
			else
				armingDisableFlags |= 0x20;
			if (!armingDisableFlags) {
				startLogging();
				armed = true;
				p.neoPixelSetValue(1, 255, 255, 255, true);
				homepointLat = gpsLatitudeFiltered;
				homepointLon = gpsLongitudeFiltered;
				homepointAlt = combinedAltitude;
			} else if (ELRS->consecutiveArmedCycles == 10 && ELRS->isLinkUp) {
				u8 wavSuccess = 0;
				if (armingDisableFlags & 0x40) {
					wavSuccess = playWav("armingErrors/gyro.wav");
				} else if (armingDisableFlags & 0x04) {
					wavSuccess = playWav("armingErrors/nogpsmodule.wav");
				} else if (armingDisableFlags & (0x08 | 0x80)) { // 08 by kolibri configurator, 80 by other configurators
					wavSuccess = playWav("armingErrors/configurator.wav");
				} else if (armingDisableFlags & 0x10) {
					wavSuccess = playWav("armingErrors/elrsLink.wav");
				} else if (armingDisableFlags & 0x20) {
					wavSuccess = playWav("armingErrors/wrongMode.wav");
				} else if (armingDisableFlags & 0x02) {
					wavSuccess = playWav("armingErrors/throttle.wav");
				}
				if (!wavSuccess)
					makeSound(2500, 599, 70, 50);
			}
		} else if (ELRS->channels[4] < 1500) {
			armed = false;
			p.neoPixelSetValue(1, 0, 0, 0, true);
			// just disarmed, stop logging
			if (ELRS->lastChannels[4] > 1500)
				endLogging();
		}
		FlightMode newFlightMode = (FlightMode)((ELRS->channels[6] - 900) / 200);
		if (newFlightMode >= FlightMode::LENGTH)
			newFlightMode = FlightMode::ACRO;
		if (newFlightMode != flightMode) {
			setFlightMode(newFlightMode);
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
