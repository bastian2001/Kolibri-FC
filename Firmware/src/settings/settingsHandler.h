#pragma once
#include "global.h"

class SettingsHandler {
public:
	static SettingsHandler &Get() {
		static SettingsHandler instance;
		if (!instance.init()) {
			Serial.println("Failed to initialize SettingsHandler");
			// Dont know what to do rn.
		}
		return instance;
	}

private:
	bool init();
	File settingsFile;
	std::vector<SettingBase> settingsList;
	SettingsHandler() = default;
	SettingsHandler(const SettingsHandler &) = delete;
	SettingsHandler &operator=(const SettingsHandler &) = delete;
};
