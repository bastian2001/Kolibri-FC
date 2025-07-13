#include "global.h"
bool armed = false;
i32 startPointLat, startPointLon;
File modesSettingsFile;
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

RxMode rxModes[RxModeIndex::LENGTH];
u32 consecutiveArmedCycles = 0; // number of cycles the switch is in the armed position, reset to 0 when disarmed

void disarm() {
	armed = false;
	p.neoPixelSetValue(1, 0, 0, 0, true);
	endLogging();
}

void modesLoop() {
	if (ELRS->newPacketFlag & (1 << 0)) {
		elapsedMicros taskTimer = 0;
		tasks[TASK_MODES].runCounter++;

		ELRS->newPacketFlag &= ~(1 << 0); // clear the flag

		if (rxModes[RxModeIndex::ARMED].isActive()) {
			consecutiveArmedCycles++;
		} else {
			consecutiveArmedCycles = 0;
		}

		if (!armed) {
			if (consecutiveArmedCycles == 10)
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
				// arm the drone
				startLogging();
				armed = true;
				p.neoPixelSetValue(1, 255, 255, 255, true);
				startPointLat = gpsMotion.lat;
				startPointLon = gpsMotion.lon;
			} else if (consecutiveArmedCycles == 10 && ELRS->isLinkUp) {
				// the user wanted to arm, but there are some errors, so play a sound
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
		} else if (!rxModes[RxModeIndex::ARMED].isActive()) {
			// drone is armed, but the switch says disarm
			disarm();
		}

		bool angleModeActive = rxModes[RxModeIndex::ANGLE].isActive();
		bool altHoldModeActive = rxModes[RxModeIndex::ALT_HOLD].isActive();
		bool gpsModeActive = rxModes[RxModeIndex::GPS].isActive();
		bool waypointModeActive = rxModes[RxModeIndex::WAYPOINT].isActive();

		FlightMode newFlightMode = FlightMode::ACRO;
		if (waypointModeActive)
			newFlightMode = FlightMode::GPS_POS;
		else if (gpsModeActive)
			newFlightMode = FlightMode::GPS_VEL;
		else if (altHoldModeActive)
			newFlightMode = FlightMode::ALT_HOLD;
		else if (angleModeActive)
			newFlightMode = FlightMode::ANGLE;

		if (newFlightMode != flightMode) {
			switch (newFlightMode) {
			case FlightMode::ACRO:
				updateElem(OSDElem::FLIGHT_MODE, "ACRO ");
				break;
			case FlightMode::ANGLE:
				updateElem(OSDElem::FLIGHT_MODE, "ANGLE");
				break;
			case FlightMode::ALT_HOLD:
				updateElem(OSDElem::FLIGHT_MODE, "ALT  ");
				break;
			case FlightMode::GPS_VEL:
				updateElem(OSDElem::FLIGHT_MODE, "GPSV ");
				break;
			case FlightMode::GPS_POS:
				updateElem(OSDElem::FLIGHT_MODE, "GPSP ");
				break;
			default:
				break;
			}
			if (flightMode <= FlightMode::ANGLE && newFlightMode > FlightMode::ANGLE) {
				// just switched to an altitude hold mode, make sure the quad doesn't just fall at the beginning
				vVelErrorSum = throttle.getfix64() / pidGainsVVel[I];
				altSetpoint = combinedAltitude;
			}
			if (flightMode <= FlightMode::ALT_HOLD && newFlightMode > FlightMode::ALT_HOLD) {
				// just switched to a GPS mode, prevent suddenly flying away to the old position lock
				targetLat = gpsLatitudeFiltered;
				targetLon = gpsLongitudeFiltered;
			}
			flightMode = newFlightMode;
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
		disarm();
	}
}

void modesInit() {
	placeElem(OSDElem::FLIGHT_MODE, 1, 2);
	enableElem(OSDElem::FLIGHT_MODE);

	rp2040.wdt_reset();
	if (!littleFsReady) return;
	if (modesSettingsFile) {
		modesSettingsFile.close();
	}
	modesSettingsFile = LittleFS.open("/modes.txt", "r+");
	if (!modesSettingsFile) {
		Serial.println("Failed to open modes file, creating new one...");
		modesSettingsFile = LittleFS.open("/modes.txt", "w+");
		if (!modesSettingsFile) {
			Serial.println("Failed to create modes file.");
		}
	}
}

void mspGetRxModes(u8 serialNum, MspVersion version) {
	char buf[64];
	u8 bufIndex = 0;
	for (int i = 0; i < RxModeIndex::LENGTH; i++) {
		RxMode &mode = rxModes[i];
		i8 channel;
		u16 minRange, maxRange;
		mode.getValues(channel, minRange, maxRange);
		i8 min = ((int)minRange - 1500) / 5;
		i8 max = ((int)maxRange - 1500) / 5;
		buf[bufIndex++] = channel;
		buf[bufIndex++] = min;
		buf[bufIndex++] = max;
		buf[bufIndex++] = 0; // reserved byte
	}
}

void mspSetRxModes(u8 serialNum, MspVersion version, const char *reqPayload, u16 reqLen) {
	// Parse the incoming payload and update the Rx modes
	if (reqLen < 4 || reqLen % 4 != 0) {
		tasks[TASK_MODES].lastError = 3;
		tasks[TASK_MODES].errorCount++;
		return;
	}
	for (u16 i = 0; i < reqLen; i += 4) {
		i8 channel = reqPayload[i];
		i8 minRange = reqPayload[i + 1];
		i8 maxRange = reqPayload[i + 2];
		if (channel < -1 || channel > 15) {
			tasks[TASK_MODES].lastError = 2;
			tasks[TASK_MODES].errorCount++;
			sendMsp(serialNum, MspMsgType::ERROR, MspFn::SET_RX_MODES, version, "Invalid channel", 15);
			return; // Invalid channel
		}
		if (minRange * 5 + 1500 < 900 || minRange * 5 + 1500 > 2100 ||
			maxRange * 5 + 1500 < 900 || maxRange * 5 + 1500 > 2100) {
			tasks[TASK_MODES].lastError = 1;
			tasks[TASK_MODES].errorCount++;
			sendMsp(serialNum, MspMsgType::ERROR, MspFn::SET_RX_MODES, version, "Invalid range", 13);
			return; // Invalid range
		}
	}
	for (int i = 0; i < reqLen; i += 4) {
		RxMode &mode = rxModes[i / 4];
		i8 channel = reqPayload[i];
		i8 minRange = reqPayload[i + 1];
		i8 maxRange = reqPayload[i + 2];
		if (channel == -1) {
			mode.disable(); // Disable the mode
		} else {
			mode.setChannel(channel);
			mode.setRange(minRange * 5 + 1500, maxRange * 5 + 1500); // Convert to actual range
		}
	}
}

RxMode::RxMode() {
	// Initialize the RxMode with default values
	channel = -1; // No channel set
	minRange = 255; // Default minimum range
	maxRange = 255; // Default maximum range
}

bool RxMode::isActive() const {
	if (channel < 0 || channel > 15) return false; // Not configured
	u16 value = ELRS->channels[channel];
	return value >= minRange && value <= maxRange;
}

bool RxMode::isConfigured() const {
	return channel >= 0 && channel <= 15;
}

void RxMode::setChannel(i8 channel) {
	if (channel < 0 || channel > 15) {
		tasks[TASK_MODES].lastError = 2;
		tasks[TASK_MODES].errorCount++;
		return; // Invalid channel
	}
	this->channel = channel;
}

void RxMode::setRange(u16 minRange, u16 maxRange) {
	if (minRange < 900 || minRange > 2100 || maxRange < 900 || maxRange > 2100) {
		tasks[TASK_MODES].lastError = 1;
		tasks[TASK_MODES].errorCount++;
		return; // Invalid range
	}
	if (minRange > maxRange) std::swap(minRange, maxRange); // Swap if in wrong order
	minRange = (minRange / 5) * 5;
	maxRange = (maxRange / 5) * 5;
	this->minRange = minRange;
	this->maxRange = maxRange;
}

void RxMode::getValues(i8 &channel, u16 &minRange, u16 &maxRange) const {
	channel = this->channel;
	minRange = this->minRange;
	maxRange = this->maxRange;
}

void RxMode::disable() {
	channel = -1; // Disable the mode
	minRange = 2000; // Reset to default minimum range
	maxRange = 2000; // Reset to default maximum range
}
