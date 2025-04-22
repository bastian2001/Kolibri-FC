#include "global.h"

template <typename T>
Setting<T>::Setting(const char *id, T *data, T defaultValue)
	: id(id),
	  data(data),
	  defaultValue(defaultValue) {
	string s;
	bool success = getValueString(s, id);
	if (success) {
		if (!setDataFromString(s)) {
			// parser error
			*this->data = defaultValue;
		}
	} else {
		// setting not found, set to default value
		*this->data = defaultValue;
	}

	// set min/max
	if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
		this->minValue = std::numeric_limits<T>::min();
		this->maxValue = std::numeric_limits<T>::max();
	} else if constexpr (std::is_same_v<T, fix32>) {
		this->minValue = FIX32_MIN;
		this->maxValue = FIX32_MAX;
	} else if constexpr (std::is_same_v<T, fix64>) {
		this->minValue = FIX64_MIN;
		this->maxValue = FIX64_MAX;
	} else {
		static_assert(false, "Unsupported type for Setting constructor");
	}
}

template <typename T>
bool Setting<T>::getValueString(string &s, const char *id) {
	if (!Settings::settingsFile) return false;
	if (!(*Settings::settingsFile)) return false;
	Settings::settingsFile->seek(0);
	while (settingsFile.available()) {
		string line = settingsFile.readStringUntil('\n').c_str();
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

template <typename T>
bool Setting<T>::setDataFromString(string s) {
	try {
		if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
			*this->data = std::stoll(s);
			return true;
		} else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
			*this->data = std::stoull(s);
			return true;
		} else if constexpr (std::is_floating_point_v<T>) {
			*this->data = std::stold(s);
			return true;
		} else if constexpr (std::is_same_v<T, fix32>) {
			*this->data = fix32().setRaw(std::stol(s));
			return true;
		} else if constexpr (std::is_same_v<T, fix64>) {
			*this->data = fix64().setRaw(std::stoll(s));
			return true;
		} else if constexpr (std::is_same_v<T, bool>) {
			if (s == "true" || s == "1") {
				*this->data = true;
			} else if (s == "false" || s == "0") {
				*this->data = false;
			} else {
				return false; // invalid value
			}
			return true;
		} else if constexpr (std::is_same_v<T, string>) {
			replaceEscapeSequences(s); // replace escape sequences in the string
			*this->data = s;
			return true;
		} else {
			static_assert(false, "Unsupported type for setDataFromString()");
			return false; // unsupported type
		}
	} catch (const std::exception &e) {
		return false;
	}
}

template <typename T>
void Setting<T>::resetToDefault() {
	*this->data = defaultValue;
}

template <typename T>
bool Setting<T>::checkValidity() {
	if constexpr (std::is_integral_v<T> ||
				  std::is_floating_point_v<T> ||
				  std::is_same_v<T, fix32> ||
				  std::is_same_v<T, fix64>) {
		return (*data >= minValue && *data <= maxValue); // Check if the value is within the range
	}
	return true;
}

template <typename T>
bool Setting<T>::updateSettingInFile() {
	if (!Settings::settingsFile) return false;
	if (!(*Settings::settingsFile)) return false;

	string newLine = id + string("=") + this->toString() + "\n";
	u32 newLineLen = newLine.length() - 1; // Exclude the \n

	Settings::settingsFile->seek(0);
	// look for the setting id in the file
	string line;
	while (Settings::settingsFile->available()) {
		line = Settings::settingsFile->readStringUntil('\n').c_str();
		if (line.find(id) == 0) {
			// found the setting, update it
			u32 oldLineLen = line.length();
			Settings::settingsFile->seek(Settings::settingsFile->position() - oldLineLen);
			if (newLineLen == oldLineLen) {
				// just overwrite the old line
				Settings::settingsFile->print(newLine.c_str());
				return true;
			} else {
				// place a semicolon at the beginning to mark the old line as commented/deleted
				Settings::settingsFile->print(";");
			}
		}
	}
	// if we reach here, the setting was not found or the new line has another length than the old one, so we need to (re)create the setting
	// start by searching for commented lines with identical length. If there are none, append to the end of the file, otherwise use these commented lines
	Settings::settingsFile->seek(0);
	while (Settings::settingsFile->available()) {
		line = Settings::settingsFile->readStringUntil('\n').c_str();
		u32 oldLineLen = line.length();
		if (line.find(";") == 0 && oldLineLen == newLineLen) {
			// found a commented line with the same length, replace it
			Settings::settingsFile->seek(Settings::settingsFile->position() - oldLineLen);
			Settings::settingsFile->print(newLine.c_str());
			return true;
		}
	}
	// if we reach here, we need to append the new line to the end of the file
	Settings::settingsFile->seek(0, SeekEnd); // Move to the end of the file
	Settings::settingsFile->print(newLine.c_str());
	return true;
}

template <typename T>
string Setting<T>::toString() {
	string s;
	if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
		s = std::to_string(*data);
	} else if constexpr (std::is_same_v<T, fix32> || std::is_same_v<T, fix64>) {
		s = std::to_string(data->getRaw());
	} else if constexpr (std::is_same_v<T, bool>) {
		s = (*data) ? "1" : "0";
	} else if constexpr (std::is_same_v<T, string>) {
		s = *data;
		// Escape special characters in the string
		for (size_t i = 0; i < s.length(); ++i) {
			if (s[i] == '\n') {
				s.replace(i, 1, "\\n");
				i += 1; // Skip the next character since we just added an escape sequence
			} else if (s[i] == '\\') {
				s.replace(i, 1, "\\\\");
				i += 1; // Skip the next character since we just added an escape sequence
			}
		}
	} else {
		static_assert(false, "Unsupported type for toString()");
	}
	return s;
}

template <typename T>
void Setting<T>::setMinMax(T minValue, T maxValue) {
	this->minValue = minValue;
	this->maxValue = maxValue;
}

template <typename T>
void Setting<T>::setSettingsFile(File *file) {
	Settings::settingsFile = file;
}

template <typename T>
File *Setting<T>::settingsFile = nullptr;