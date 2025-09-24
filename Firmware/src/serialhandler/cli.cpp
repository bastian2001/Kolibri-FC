#include "global.h"

using std::string;

bool vectorHasString(const std::vector<string> &vec, const string &str) {
	for (const auto &s : vec) {
		if (s == str) {
			return true;
		}
	}
	return false;
}

string processCliCommand(const char *reqPayload, u16 reqLen) {
	string payload = string(reqPayload, reqLen);
	size_t pos = payload.find(' ');
	string cmd;
	if (pos != string::npos) {
		cmd = payload.substr(0, pos);
		payload = payload.substr(pos + 1);
	} else {
		cmd = payload;
		payload = "";
	}

	string response = "";

	if (cmd == "help") {
		response = "set, get, save, reboot, status, serial, gyro_calibration, print, echo, reset";
	} else if (cmd == "set") {
		string key = "";
		string value = "";
		// delimiter: '='
		size_t pos = payload.find('=');
		if (pos != string::npos) {
			key = payload.substr(0, pos);
			value = payload.substr(pos + 1);
		} else {
			response = "Invalid command format. Use 'set <key>=<value>'\n";
			return response;
		}

		SettingBase *setting = getSetting(key.c_str());
		if (setting == &dummySetting) {
			response = "Setting '" + string(key) + "' not found\n";
		} else {
			if (setting->setDataFromString(value, true)) {
				response = "Setting updated: " + string(key) + "\n";
				setting->updateSettingInFile();
			} else {
				response = "Invalid value for setting: " + string(key) + "\n";
			}
		}
	} else if (cmd == "get") {
		std::vector<string> ids;
		string &name = payload;
		// start with exact match
		for (auto setting : settingsList) {
			if (setting->id == name) {
				ids.push_back(setting->id);
			}
		}
		// then any match that starts with the name
		for (auto setting : settingsList) {
			string id = setting->id;
			if (id.find(name) == 0 && !vectorHasString(ids, id)) {
				ids.push_back(id);
			}
		}
		// then any match that contains the name
		for (auto setting : settingsList) {
			string id = setting->id;
			if (id.find(name) != string::npos && !vectorHasString(ids, id)) {
				ids.push_back(id);
			}
		}

		if (ids.empty()) {
			response = "Setting '" + string(name) + "' not found\n";
		} else {
			for (const auto &id : ids) {
				SettingBase *setting = getSetting(id.c_str());
				response += id + ": " + setting->toString(true) + "\n";
			}
		}
	} else if (cmd == "save") {
		closeSettingsFile();
		openSettingsFile();
		response = "Settings saved";
	} else if (cmd == "reboot") {
		sendMsp(lastMspSerial, MspMsgType::RESPONSE, MspFn::CLI_COMMAND, lastMspVersion, "Rebooting...", 13);
		serials[lastMspSerial].stream->flush();
		sleep_ms(100);
		rp2040.reboot();
	} else if (cmd == "status") {
		response = "Firmware: " FIRMWARE_NAME " " FIRMWARE_VERSION_STRING "\n";
		response += "UAV Name: " + uavName + "\n";
		response += "Uptime: " + std::to_string(millis()) + " ms\n";
	} else if (cmd == "serial") {
		if (payload == "stats get") {
			f32 timeSinceReset = f32(BufferedWriter::sinceReset) / 1000.f;
			response = "Statistics since the last reset (" + std::to_string((u32)timeSinceReset) + "ms ago)\n";
			for (int i = 0; i < SERIAL_COUNT; i++) {
				char line[128];
				BufferedWriter *s = serials[i].stream;
				snprintf(line, 128, "Serial %d (%s):     TX: %7d bytes (%.2fKB/s),     RX: %7d bytes (%.2fKB/s),     ", i + 1, BufferedWriter::serialTypeNames[(u8)s->serialType], s->totalTx, s->totalTx / timeSinceReset, s->totalRx, s->totalRx / timeSinceReset);
				response += line;
				bool firstFunction = true;
				for (int j = 0; j < SERIAL_FUNCTION_COUNT; j++) {
					if (serials[i].functions & (1 << j)) {
						if (firstFunction)
							firstFunction = false;
						else
							response += ", ";
						response += serialFunctionNames[j];
					}
				}
				response += '\n';
			}
		} else if (payload == "stats clear") {
			for (auto &s : serials) {
				while (s.stream->totalRx || s.stream->totalTx) {
					// clearing may fail in case a byte is read/written atm
					s.stream->totalRx = 0;
					s.stream->totalTx = 0;
					BufferedWriter::sinceReset = 0;
				}
			}
		} else {
			response = "Requires the following format: serial <subcmd> [arguments]\nwhere subcmd is one of the following: stats\nstats: arguments: get, clean\n";
		}
	} else if (cmd == "gyro_calibration") {
		if (payload == "start") {
			response = "Calibrating gyro";
			startGyroCalibration();
		} else if (payload == "get") {
			char str[100];
			i16 cal[3];
			getGyroCalibration(cal);
			snprintf(str, sizeof(str), "Gyro calibration data: %d %d %d", cal[0], cal[1], cal[2]);
			response = str;
		} else {
			response = "Invalid usage. Use 'gyro_calibration start' or 'gyro_calibration get'\n";
		}
	} else if (cmd == "print") {
		settingsFile.seek(0);
		string line;
		while (settingsFile.available()) {
			line = "\n" + string(settingsFile.readStringUntil('\n').c_str());
			sendMsp(lastMspSerial, MspMsgType::RESPONSE, MspFn::CLI_COMMAND, lastMspVersion, line.c_str(), line.length());
		}
	} else if (cmd == "echo") {
		response = payload;
	} else if (cmd == "reset") {
		if (payload == "confirm") {
			sendMsp(lastMspSerial, MspMsgType::RESPONSE, MspFn::CLI_COMMAND, lastMspVersion, "Resetting settings...", 22);
			serials[lastMspSerial].stream->flush();
			sleep_ms(100);
			closeSettingsFile();
			LittleFS.remove("/settings.txt");
			rp2040.reboot();
		} else {
			response = "If you are sure you want to reset the whole FC, use 'reset confirm'\n";
		}
	} else {
		response = "Unknown command: '" + std::string(cmd) + "', try 'help' to see available commands\n";
	}
	return response;
}
