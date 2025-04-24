#include "global.h"

bool SettingBase::getValueString(string &s, const char *id) {
	if (!SettingBase::settingsFile) return false;
	if (!(*SettingBase::settingsFile)) return false;
	SettingBase::settingsFile->seek(0);
	while (SettingBase::settingsFile->available()) {
		string line = SettingBase::settingsFile->readStringUntil('\n').c_str();
		if (line.find(id) == 0) {
			s = line.substr(line.find('=') + 1);
			return true;
		}
	}
	return false;
}

void replaceEscapeSequences(string &s) {
	for (size_t i = 0; i < s.length(); ++i) {
		if (s[i] == '\\' && i + 1 < s.length()) {
			if (s[i + 1] == 'n') {
				// Replace "\n" with actual newline character
				s.replace(i, 2, 1, '\n');
				// Don't increment i here since we replaced 2 chars with 1
			} else if (s[i + 1] == '\\') {
				// Replace "\\" with "\"
				s.replace(i, 2, 1, '\\');
				// Don't increment i here since we replaced 2 chars with 1
			} else {
				// Not a recognized escape sequence, move on
				++i;
			}
		}
	}
}

void SettingBase::setSettingsFile(File *file) {
	SettingBase::settingsFile = file;
}

File *SettingBase::settingsFile = nullptr;

vector<SettingBase *> settingsList;

string errorSettingString = "";
Setting<string> dummySetting("error_setting", &errorSettingString, string("")); // Dummy setting for error handling, since it is a string, it accepts most values so it won't throw an error

// finds setting, returns dummySetting if not found
SettingBase *getSetting(const char *id) {
	for (auto setting : settingsList) {
		if (!strncmp(setting->id, id, 64)) {
			return setting;
		}
	}
	return &dummySetting; // Return dummy setting if not found
}

bool stringToBool(const string &s) {
	if (s == "true" || s == "1") {
		return true;
	} else if (s == "false" || s == "0") {
		return false;
	}
	throw "Invalid boolean string: " + s;
}

bool SettingBase::updateSettingInFile() {
	if (!SettingBase::settingsFile) return false;
	if (!(*SettingBase::settingsFile)) return false;

	string newLine = id + string("=") + this->toString() + "\n";
	u32 newLineLen = newLine.length() - 1; // Exclude the \n

	SettingBase::settingsFile->seek(0);
	// look for the setting id in the file
	string line;
	while (SettingBase::settingsFile->available()) {
		rp2040.wdt_reset();
		line = SettingBase::settingsFile->readStringUntil('\n').c_str();
		if (line.find(id) == 0) {
			// found the setting, update it
			u32 oldLineLen = line.length();
			SettingBase::settingsFile->seek(SettingBase::settingsFile->position() - oldLineLen - 1);
			if (newLineLen == oldLineLen) {
				// just overwrite the old line
				SettingBase::settingsFile->print(newLine.c_str());
				return true;
			} else {
				// place a semicolon at the beginning to mark the old line as commented/deleted
				SettingBase::settingsFile->print(";");
			}
		}
	}
	// if we reach here, the setting was not found or the new line has another length than the old one, so we need to (re)create the setting
	// start by searching for commented lines with identical length. If there are none, append to the end of the file, otherwise use these commented lines
	SettingBase::settingsFile->seek(0);
	while (SettingBase::settingsFile->available()) {
		rp2040.wdt_reset();
		line = SettingBase::settingsFile->readStringUntil('\n').c_str();
		u32 oldLineLen = line.length();
		if (line.find(";") == 0 && oldLineLen == newLineLen) {
			// found a commented line with the same length, replace it
			SettingBase::settingsFile->seek(SettingBase::settingsFile->position() - oldLineLen - 1);
			SettingBase::settingsFile->print(newLine.c_str());
			return true;
		}
	}
	// if we reach here, we need to append the new line to the end of the file
	SettingBase::settingsFile->seek(0, SeekEnd); // Move to the end of the file
	SettingBase::settingsFile->print(newLine.c_str());
	return true;
}
