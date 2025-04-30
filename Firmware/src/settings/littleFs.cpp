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

void initSettings() {
	openSettingsFile();

	addArraySetting(SETTING_ACC_CAL, accelCalibrationOffset);
	addSetting(SETTING_ACC_FILTER_CUTOFF, &accelFilterCutoff, 100);

	addSetting(SETTING_BB_FLAGS, &bbFlags, 0b1111111111111111100000000000000011111111111ULL);
	addSetting(SETTING_BB_DIV, &bbFreqDivider, 2);

	addSetting(SETTING_GPS_UPDATE_RATE, &gpsUpdateRate, 20);
	addSetting(SETTING_GPS_VEL_FILTER_CUTOFF, &gpsVelocityFilterCutoff, 0.2f);

	addArraySetting(SETTING_MAG_CAL_HARD, magOffset);
	addSetting(SETTING_MAG_FILTER_CUTOFF, &magFilterCutoff, 0.02f);

	addSetting(SETTING_BEEP_TONE, &dshotBeepTone, 2);

	addSetting(SETTING_EMPTY_VOLTAGE, &emptyVoltage, 1400);

	addArraySetting(SETTING_PID_GAINS, pidGains, &initPidGains);
	addArraySetting(SETTING_RATE_FACTORS, rateFactors, &initRateFactors);
	addSetting(SETTING_IDLE_PERMILLE, &idlePermille, 25);
	addSetting(SETTING_MAX_ANGLE, &maxAngle, 40);
	addSetting(SETTING_DFILTER_CUTOFF, &dFilterCutoff, 70);
	addSetting(SETTING_MAX_TARGET_HVEL, &maxTargetHvel, 12);
	addArraySetting(SETTING_PID_VVEL, pidGainsVVel, &initPidVVel);
	addSetting(SETTING_VVEL_FF_FILTER_CUTOFF, &vvelFFFilterCutoff, 2);
	addSetting(SETTING_VVEL_D_FILTER_CUTOFF, &vvelDFilterCutoff, 15);
	addArraySetting(SETTING_PID_HVEL, pidGainsHVel, &initPidHVel);
	addSetting(SETTING_HVEL_FF_FILTER_CUTOFF, &hvelFfFilterCutoff, 2);
	addSetting(SETTING_HVEL_I_RELAX_FILTER_CUTOFF, &hvelIRelaxFilterCutoff, 0.5f);
	addSetting(SETTING_HVEL_PUSH_FILTER_CUTOFF, &hvelPushFilterCutoff, 4);
	addSetting(SETTING_MAX_ANGLE_BURST, &maxAngleBurst, 60);
	addSetting(SETTING_ANGLE_BURST_TIME, &angleBurstTime, 3000);
	addSetting(SETTING_ANGLE_BURST_COOLDOWN, &angleBurstCooldownTime, 5000);
	addSetting(SETTING_HVEL_STICK_DEADBAND, &hvelStickDeadband, 30);
	addSetting(SETTING_IFALLOFF, &iFalloff, 400);

	addSetting(SETTING_TIMEZONE_OFFSET, &rtcTimezoneOffset, 0);
	addSetting(SETTING_UAV_NAME, &uavName, "Kolibri UAV");
	addSetting(SETTING_START_SOUND, &startSoundFile, "start.wav");
	addSetting(SETTING_START_FALLBACK_RTTTL, &fallbackRtttl, "Drone:o=6,b=800:1c#6,1d#6,1g#6.,1d#6$1,1g#6.$1,1d#6$2,1g#6$2");

	closeSettingsFile();
}
