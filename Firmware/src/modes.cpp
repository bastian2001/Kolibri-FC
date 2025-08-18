#include "global.h"
bool armed = false;
fix64 homepointLat, homepointLon;
fix32 homepointAlt;

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

void disarm(DisarmReason reason) {
	armed = false;
	p.neoPixelSetValue(1, 0, 0, 0, true);
	Serial.printf("Disarming for reason %d\n", (u8)reason);
	endLogging(reason);
}

void modesLoop() {
	if (ELRS->newPacketFlag & (1 << 0)) {
		TASK_START(TASK_MODES);

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
				homepointLat = gpsLatitudeFiltered;
				homepointLon = gpsLongitudeFiltered;
				homepointAlt = combinedAltitude;
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
			disarm(DisarmReason::SWITCH);
		}

		bool angleModeActive = rxModes[RxModeIndex::ANGLE].isActive();
		bool altHoldModeActive = rxModes[RxModeIndex::ALT_HOLD].isActive();
		bool gpsModeActive = rxModes[RxModeIndex::GPS].isActive();
		bool waypointModeActive = rxModes[RxModeIndex::WAYPOINT].isActive();

		FlightMode newFlightMode = FlightMode::ACRO;
		if (waypointModeActive)
			newFlightMode = FlightMode::GPS_WP;
		else if (gpsModeActive)
			newFlightMode = FlightMode::GPS;
		else if (altHoldModeActive)
			newFlightMode = FlightMode::ALT_HOLD;
		else if (angleModeActive)
			newFlightMode = FlightMode::ANGLE;

		if (newFlightMode != flightMode) {
			setFlightMode(newFlightMode);
		}
		TASK_END(TASK_MODES);
	} else if (ELRS->sinceLastRCMessage >= 500000 && armed) {
		disarm(DisarmReason::RXLOSS);
	}
}

bool openModesSettingsFile() {
	if (!littleFsReady) return false;
	if (modesSettingsFile) {
		modesSettingsFile.close();
	}
	modesSettingsFile = LittleFS.open("/modes.txt", "r+");
	if (!modesSettingsFile) {
		Serial.println("Failed to open modes file, creating new one...");
		modesSettingsFile = LittleFS.open("/modes.txt", "w+");
		if (!modesSettingsFile) {
			Serial.println("Failed to create modes file.");
			return false;
		}
		return true;
	}
	return false;
}

void closeModesSettingsFile() {
	if (modesSettingsFile) {
		rp2040.wdt_reset();
		modesSettingsFile.close();
	}
}

void getModesFromFile() {
	char buf[64];
	u8 bufIndex = 0;
	modesSettingsFile.seek(0);
	for (int i = 0; i < 64 && modesSettingsFile.available(); i++) {
		buf[bufIndex++] = modesSettingsFile.read();
	}
	mspSetRxModes(255, MspVersion::V2, buf, bufIndex);
}

void setModesInFile() {
	modesSettingsFile.seek(0);
	for (int i = 0; i < RxModeIndex::LENGTH; i++) {
		RxMode &mode = rxModes[i];
		i8 channel, min, max, res = 0;
		mode.getValues8(channel, min, max);
		modesSettingsFile.write((u8)channel);
		modesSettingsFile.write((u8)min);
		modesSettingsFile.write((u8)max);
		modesSettingsFile.write((u8)res); // reserved byte
	}
	modesSettingsFile.flush();
}

void modesInit() {
	placeElem(OSDElem::FLIGHT_MODE, 1, 2);
	enableElem(OSDElem::FLIGHT_MODE);

	rp2040.wdt_reset();

	bool newFile = openModesSettingsFile();

	for (int i = 0; i < RxModeIndex::LENGTH; i++) {
		RxMode &mode = rxModes[i];
		mode.disable(); // disable all modes initially
	}

	if (modesSettingsFile && !newFile) {
		getModesFromFile();
	} else if (modesSettingsFile && newFile) {
		setModesInFile();
	}
	closeModesSettingsFile();
}

void mspGetRxModes(u8 serialNum, MspVersion version) {
	char buf[64];
	u8 bufIndex = 0;
	for (int i = 0; i < RxModeIndex::LENGTH; i++) {
		RxMode &mode = rxModes[i];
		i8 channel, min, max;
		mode.getValues8(channel, min, max);
		buf[bufIndex++] = channel;
		buf[bufIndex++] = min;
		buf[bufIndex++] = max;
		buf[bufIndex++] = 0; // reserved byte
	}
	sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::GET_RX_MODES, version, buf, bufIndex);
}

void mspSetRxModes(u8 serialNum, MspVersion version, const char *reqPayload, u16 reqLen) {
	// Parse the incoming payload and update the Rx modes
	if (reqLen < 4 || reqLen % 4 != 0) {
		tasks[TASK_MODES].lastError = 3;
		tasks[TASK_MODES].errorCount++;
		return;
	}
	for (u16 i = 0; i < reqLen && i < RxModeIndex::LENGTH; i += 4) {
		const i8 &channel = reqPayload[i];
		const i8 &minRange = reqPayload[i + 1];
		const i8 &maxRange = reqPayload[i + 2];
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
		const i8 &channel = reqPayload[i];
		const i8 &minRange = reqPayload[i + 1];
		const i8 &maxRange = reqPayload[i + 2];
		if (channel == -1) {
			mode.disable(); // Disable the mode
		} else {
			mode.setValues8(channel, minRange, maxRange); // Set the mode with the new values
		}
	}
	openModesSettingsFile();
	if (modesSettingsFile) {
		setModesInFile(); // Save the updated modes to the file
		closeModesSettingsFile();
		sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::SET_RX_MODES, version);
	} else {
		tasks[TASK_MODES].lastError = 4;
		tasks[TASK_MODES].errorCount++;
		sendMsp(serialNum, MspMsgType::ERROR, MspFn::SET_RX_MODES, version, "Failed to open modes file", 26);
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

void RxMode::getValues8(i8 &channel, i8 &minRange, i8 &maxRange) const {
	channel = this->channel;
	minRange = (this->minRange - 1500) / 5; // Convert to i8 range
	maxRange = (this->maxRange - 1500) / 5; // Convert to i8 range
}

void RxMode::setValues8(i8 channel, i8 minRange, i8 maxRange) {
	this->channel = channel;
	this->minRange = minRange * 5 + 1500; // Convert to u16 range
	this->maxRange = maxRange * 5 + 1500; // Convert to u16 range
}

void RxMode::disable() {
	channel = -1; // Disable the mode
	minRange = 2000; // Reset to default minimum range
	maxRange = 2000; // Reset to default maximum range
}
