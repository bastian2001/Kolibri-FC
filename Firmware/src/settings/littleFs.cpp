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
