#pragma once
#include "settings/settingBase.h"
#include <stdexcept>

template <typename T>
class Setting : public SettingBase {
public:
	Setting() = delete;
	Setting(const Setting &) = delete;
	Setting(Setting &&) = delete;
	Setting &operator=(const Setting &) = delete;
	Setting &operator=(Setting &&) = delete;

	/**
	 * @brief Construct a setting object.
	 *
	 * Only supports integer, fix, float and string type values.
	 *
	 * @param id snake_case id of the setting
	 * @param data pointer to the data to be set
	 * @param defaultValue default value in case the setting is not found, or invalid
	 */
	Setting(const char *id, T *data, T defaultValue);

	void resetToDefault() override;
	string toString(bool readable = false) override;
	bool setDataFromString(string s, bool readable = false) override;
	bool checkValidity() override;

	/**
	 * @brief Set minimum and maximum allowed values for this setting
	 *
	 * @param minValue Minimum allowed value
	 * @param maxValue Maximum allowed value
	 */
	void setMinMax(T minValue, T maxValue);

private:
	T *data;
	const T defaultValue;
	T minValue;
	T maxValue;
};

template <typename T>
Setting<T>::Setting(const char *id, T *data, T defaultValue)
	: SettingBase(id),
	  data(data),
	  defaultValue(defaultValue) {
	string s;
	bool success = getValueString(s, id);
	if (success) {
		if (!setDataFromString(s)) {
			// parser error
			*this->data = defaultValue;
			this->updateSettingInFile();
		}
	} else {
		// setting not found, set to default value
		*this->data = defaultValue;
		this->updateSettingInFile();
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
	} else if constexpr (std::is_same_v<T, bool>) {
	} else if constexpr (std::is_same_v<T, string>) {
	} else {
		static_assert(false, "Unsupported type for Setting constructor");
	}
}

template <typename T>
bool Setting<T>::setDataFromString(string s, bool readable) {
	rp2040.wdt_reset();
	try {
		if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
			*this->data = std::stoll(s);
			return true;
		} else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
			*this->data = std::stoull(s);
			return true;
		} else if constexpr (std::is_floating_point_v<T>) {
			*this->data = std::stod(s);
			return true;
		} else if constexpr (std::is_same_v<T, fix32> || std::is_same_v<T, fix64>) {
			if (readable) {
				*this->data = std::stod(s);
			} else {
				*this->data = T().setRaw(std::stoll(s));
			}
			return true;
		} else if constexpr (std::is_same_v<T, bool>) {
			*this->data = stringToBool(s);
			return true;
		} else if constexpr (std::is_same_v<T, string>) {
			replaceEscapeSequences(s);
			*this->data = s;
			return true;
		} else {
			static_assert(false, "Unsupported type for setDataFromString()");
			return false;
		}
	} catch (std::invalid_argument const &ex) {
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
string Setting<T>::toString(bool readable) {
	string s;
	if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
		s = std::to_string(*data);
	} else if constexpr (std::is_same_v<T, fix32> || std::is_same_v<T, fix64>) {
		if (readable) {
			char buffer[32];
			snprintf(buffer, sizeof(buffer), "%.2f", 6, data->getf64());
			s = string(buffer);
		} else {
			s = std::to_string(data->raw);
		}
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

template <typename T, typename U>
Setting<T> *addSetting(const char *id, T *data, U defaultValue) {
	Setting<T> *setting = new Setting<T>(id, data, static_cast<T>(defaultValue));
	settingsList.push_back(setting);
	return setting;
}
