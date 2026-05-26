/**
 * @file littleFs.cpp
 * @brief LittleFS management functions for settings storage on flash chip
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

bool littleFsReady = false;
File settingsFile;

void initLittleFs() {
	if (littleFsReady) return;
	littleFsReady = LittleFS.begin();
	if (!littleFsReady) {
		DEBUG_PRINTLN("LittleFS mount failed, trying to format...");
		if (LittleFS.format()) {
			littleFsReady = LittleFS.begin();
			if (littleFsReady) {
				DEBUG_PRINTLN("LittleFS formatted and mounted successfully.");
			} else {
				DEBUG_PRINTLN("LittleFS format failed.");
			}
		} else {
			DEBUG_PRINTLN("LittleFS format failed.");
		}
	}
	SettingBase::setSettingsFile(&settingsFile);
}

void openSettingsFile() {
	rp2040.wdt_reset();
	if (!littleFsReady) return;
	if (settingsFile) {
		settingsFile.close();
	}
	settingsFile = LittleFS.open("/settings.txt", "r+");
	if (!settingsFile) {
		DEBUG_PRINTLN("Failed to open settings file, creating new one...");
		settingsFile = LittleFS.open("/settings.txt", "w+");
		if (!settingsFile) {
			DEBUG_PRINTLN("Failed to create settings file.");
		}
	}
}

void closeSettingsFile() {
	if (settingsFile) {
		rp2040.wdt_reset();
		settingsFile.close();
	}
}
