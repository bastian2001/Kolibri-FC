#include "global.h"

bool littleFsReady = false;
File settingsFile;

void initLittleFs() {
	if (littleFsReady) return;
	littleFsReady = LittleFS.begin();
	if (!littleFsReady) {
		Serial.println("LittleFS mount failed, trying to format...");
		if (LittleFS.format()) {
			littleFsReady = LittleFS.begin();
			if (littleFsReady) {
				Serial.println("LittleFS formatted and mounted successfully.");
			} else {
				Serial.println("LittleFS format failed.");
			}
		} else {
			Serial.println("LittleFS format failed.");
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
		Serial.println("Failed to open settings file, creating new one...");
		settingsFile = LittleFS.open("/settings.txt", "w+");
		if (!settingsFile) {
			Serial.println("Failed to create settings file.");
		}
	}
}

void closeSettingsFile() {
	if (settingsFile) {
		rp2040.wdt_reset();
		settingsFile.close();
	}
}
