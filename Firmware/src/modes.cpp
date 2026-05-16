/**
 * @file modes.cpp
 * @brief Implementation of flight mode management and RX switches (RxMode class)
 *
 * Copyright (c) 2026 Kolibri-FC contributors
 *
 * This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 *
 * Kolibri-FC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kolibri-FC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
 */

#include "global.h"
volatile bool armed = false;
fix64 homepointLat, homepointLon;
fix32 homepointAlt;
elapsedMillis armedTimer;

static File modesSettingsFile;

/**
 * 0: switch in armed position for == 10 cycles
 * 1: throttle down
 * 2: GPS fix and >= 6 satellites
 * 3: unused
 * 4: at least one good ELRS packet before arming
 * 5: allowed modes during arm: acro, angle
 * 6: Gyro calibrated
 * 7: takeoff prevention from configurator (MspFn::SET_ARMING_DISABLED) -> not used by Kolibri configurator
 */
u32 armingDisableFlags = 0;

RxMode rxModes[RxModeIndex::LENGTH];
static u32 consecutiveArmedCycles = 0; // number of cycles the switch is in the armed position, reset to 0 when disarmed

void disarm(DisarmReason reason) {
	if (!armed) return;
	armed = false;
	p.neoPixelSetValue(1, 0, 0, 0, true);
	DEBUG_PRINTF("Disarming for reason %d\n", (u8)reason);
#ifdef BLACKBOX_STORAGE
	endLogging(reason);
#endif
}

void modesLoop() {
	if (!elrs) return disarm(DisarmReason::UNSET);
	if (elrs->newPacketFlag & (1 << 0)) {
		TASK_START(TASK_MODES);

		elrs->newPacketFlag &= ~(1 << 0); // clear the flag

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
			if (elrs->channels[2] < 1020)
				armingDisableFlags &= ~0x02;
			else
				armingDisableFlags |= 0x02;
			// if (lastPvtMessage > 1000)
			// 	armingDisableFlags |= 0x04;
			if (elrs->isLinkUp)
				armingDisableFlags &= ~0x10;
			else
				armingDisableFlags |= 0x10;
			if (flightMode <= FlightMode::ANGLE)
				armingDisableFlags &= ~0x20;
			else
				armingDisableFlags |= 0x20;
			if (!armingDisableFlags) {
				// arm the drone
#ifdef BLACKBOX_STORAGE
				startLogging();
#endif
				armed = true;
				armedTimer = 0;
				p.neoPixelSetValue(1, 255, 255, 255, true);
				homepointLat = gpsLatitudeFiltered;
				homepointLon = gpsLongitudeFiltered;
				homepointAlt = combinedAltitude;
			} else if (consecutiveArmedCycles == 10 && elrs->isLinkUp) {
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
	} else if (elrs->sinceLastRCMessage >= 500000 && armed) {
		disarm(DisarmReason::RXLOSS);
	}

#ifdef PIN_DCDC_EN
	if (rxModes[RxModeIndex::DCDC_ENABLE].isActive() || !elrs || !rxModes[RxModeIndex::DCDC_ENABLE].isConfigured()) {
		gpio_put(PIN_DCDC_EN, true);
	} else {
		gpio_put(PIN_DCDC_EN, false);
	}
#endif
}

bool openModesSettingsFile() {
	if (!littleFsReady) return false;
	if (modesSettingsFile) {
		modesSettingsFile.close();
	}
	modesSettingsFile = LittleFS.open("/modes.txt", "r+");
	if (!modesSettingsFile) {
		DEBUG_PRINTLN("Failed to open modes file, creating new one...");
		modesSettingsFile = LittleFS.open("/modes.txt", "w+");
		if (!modesSettingsFile) {
			DEBUG_PRINTLN("Failed to create modes file.");
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
	mspSetRxModes(nullptr, MspVersion::V2, buf, bufIndex);
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

#ifdef PIN_DCDC_EN
	gpio_init(PIN_DCDC_EN);
	gpio_set_dir(PIN_DCDC_EN, GPIO_OUT);
	gpio_put(PIN_DCDC_EN, 0);
#endif
}

void mspGetRxModes(KoliSerial *serial, MspVersion version) {
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
	MspMsgSetup s = {
		.serial = *serial,
		.fn = MspFn::GET_RX_MODES,
		.type = MspMsgType::RESPONSE,
		.version = version,
	};
	sendMsp(s, buf, bufIndex);
}

void mspSetRxModes(KoliSerial *serial, MspVersion version, const char *reqPayload, u16 reqLen) {
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
			if (serial != nullptr) {
				MspMsgSetup s = {
					.serial = *serial,
					.fn = MspFn::SET_RX_MODES,
					.type = MspMsgType::ERROR,
					.version = version,
				};
				sendMsp(s, "Invalid channel", 15);
			}
			return; // Invalid channel
		}
		if (minRange * 5 + 1500 < 900 || minRange * 5 + 1500 > 2100 ||
			maxRange * 5 + 1500 < 900 || maxRange * 5 + 1500 > 2100) {
			tasks[TASK_MODES].lastError = 1;
			tasks[TASK_MODES].errorCount++;
			if (serial != nullptr) {
				MspMsgSetup s = {
					.serial = *serial,
					.fn = MspFn::SET_RX_MODES,
					.type = MspMsgType::ERROR,
					.version = version,
				};
				sendMsp(s, "Invalid range", 13);
			}
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
		if (serial != nullptr) {
			MspMsgSetup s = {
				.serial = *serial,
				.fn = MspFn::SET_RX_MODES,
				.type = MspMsgType::RESPONSE,
				.version = version,
			};
			sendMsp(s);
		}
	} else {
		tasks[TASK_MODES].lastError = 4;
		tasks[TASK_MODES].errorCount++;
		if (serial != nullptr) {
			MspMsgSetup s = {
				.serial = *serial,
				.fn = MspFn::SET_RX_MODES,
				.type = MspMsgType::ERROR,
				.version = version,
			};
			sendMsp(s, "Failed to open modes file", 26);
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
	if (channel < 0 || channel > 15 || !elrs) return false; // Not configured
	u16 value = elrs->channels[channel];
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
