#pragma once
#include "settings/settingBase.h"

template <typename T>
class ArraySetting : public SettingBase {
public:
	ArraySetting() = delete;
	ArraySetting(const ArraySetting &) = delete;
	ArraySetting(ArraySetting &&) = delete;
	ArraySetting &operator=(const ArraySetting &) = delete;
	ArraySetting &operator=(ArraySetting &&) = delete;

	/**
	 * @brief Construct a setting object.
	 *
	 * Only supports integer, fix, float and string type values.
	 *
	 * @param id snake_case id of the setting
	 * @param data pointer to the data to be set
	 * @param defaultValue default value in case the setting is not found, or invalid
	 */
	ArraySetting(const char *id, T *dataArray, size_t itemCount, void (*setDefaultValueFn)() = nullptr,
				 void (*applyBoundsFn)() = nullptr, bool (*checkValidityFn)() = nullptr);

	void resetToDefault() override;
	string toString(bool readable = false) override;
	bool setDataFromString(string s, bool readable = false) override;
	bool checkValidity() override;

	/// @brief Calls the applyBoundsFn function to set the min and max values for the setting
	void setMinMax();

private:
	T *data;
	const u32 itemCount;
	void (*setDefaultValueFn)();
	void (*applyBoundsFn)();
	bool (*checkValidityFn)();
};

template <typename T>
ArraySetting<T>::ArraySetting(const char *id, T *dataArray, size_t itemCount, void (*setDefaultValueFn)(),
							  void (*applyBoundsFn)(), bool (*checkValidityFn)())
	: SettingBase(id),
	  data(dataArray),
	  itemCount(itemCount),
	  setDefaultValueFn(setDefaultValueFn),
	  applyBoundsFn(applyBoundsFn),
	  checkValidityFn(checkValidityFn) {
	string s;
	bool success = getValueString(s, id);
	if (success) {
		if (!setDataFromString(s)) {
			// parser error
			this->resetToDefault();
			this->updateSettingInFile();
		}
	} else {
		// setting not found, set to default value
		this->resetToDefault();
		this->updateSettingInFile();
	}
}

template <typename T>
bool ArraySetting<T>::setDataFromString(string s, bool readable) {
	try {
		if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
			for (u32 i = 0; i < itemCount; i++) {
				// parse the string and set the data
				size_t pos = s.find(',');
				if (pos == string::npos) {
					// no more commas, set the last value
					this->data[i] = std::stoll(s);
					break;
				} else {
					this->data[i] = std::stoll(s.substr(0, pos));
					s.erase(0, pos + 1); // remove the parsed value from the string
				}
			}
		} else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
			for (u32 i = 0; i < itemCount; i++) {
				size_t pos = s.find(',');
				if (pos == string::npos) {
					this->data[i] = std::stoull(s);
					break;
				} else {
					this->data[i] = std::stoull(s.substr(0, pos));
					s.erase(0, pos + 1);
				}
			}
		} else if constexpr (std::is_floating_point_v<T>) {
			for (u32 i = 0; i < itemCount; i++) {
				size_t pos = s.find(',');
				if (pos == string::npos) {
					this->data[i] = std::stod(s);
					break;
				} else {
					this->data[i] = std::stold(s.substr(0, pos));
					s.erase(0, pos + 1);
				}
			}
		} else if constexpr (std::is_same_v<T, fix32> || std::is_same_v<T, fix64>) {
			for (u32 i = 0; i < itemCount; i++) {
				size_t pos = s.find(',');
				if (pos == string::npos) {
					if (readable)
						this->data[i] = std::stod(s);
					else
						this->data[i] = T().setRaw(std::stoll(s));
					break;
				} else {
					if (readable)
						this->data[i] = std::stod(s.substr(0, pos));
					else
						this->data[i] = T().setRaw(std::stoll(s.substr(0, pos)));
					s.erase(0, pos + 1);
				}
			}
		} else if constexpr (std::is_same_v<T, bool>) {
			for (u32 i = 0; i < itemCount; i++) {
				size_t pos = s.find(',');
				if (pos == string::npos) {
					this->data[i] = stringToBool(s);
					break;
				} else {
					this->data[i] = stringToBool(s.substr(0, pos));
					s.erase(0, pos + 1);
				}
			}
		}
	} catch (const std::exception &e) {
		return false;
	}
	return true;
}

template <typename T>
void ArraySetting<T>::resetToDefault() {
	if (this->setDefaultValueFn) {
		this->setDefaultValueFn();
	} else {
		for (u32 i = 0; i < itemCount; i++) {
			this->data[i] = 0;
		}
	}
}

template <typename T>
bool ArraySetting<T>::checkValidity() {
	if (this->checkValidityFn) {
		return this->checkValidityFn();
	}
	return true;
}

template <typename T>
string ArraySetting<T>::toString(bool readable) {
	string s;
	if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
		for (u32 i = 0; i < itemCount; i++) {
			s += std::to_string(this->data[i]);
			if (i < itemCount - 1) {
				s += ",";
			}
		}
	} else if constexpr (std::is_same_v<T, fix32> || std::is_same_v<T, fix64>) {
		for (u32 i = 0; i < itemCount; i++) {
			if (readable) {
				char buffer[32];
				snprintf(buffer, sizeof(buffer), "%.2f", 6, data[i].getf64());
				s += string(buffer);
			} else {
				s += std::to_string(this->data[i].raw);
			}
			if (i < itemCount - 1) {
				s += ",";
			}
		}
	} else if constexpr (std::is_same_v<T, bool>) {
		for (u32 i = 0; i < itemCount; i++) {
			s += this->data[i] ? "1" : "0";
			if (i < itemCount - 1) {
				s += ",";
			}
		}
	} else {
		static_assert(false, "Unsupported type for toString()");
	}
	return s;
}

template <typename T>
void ArraySetting<T>::setMinMax() {
	if (this->applyBoundsFn) {
		this->applyBoundsFn();
	}
}

template <typename T, size_t X, size_t Y>
ArraySetting<T> *addArraySetting(const char *id, T (&data)[X][Y], void (*setDefaultValueFn)() = nullptr,
								 void (*applyBoundsFn)() = nullptr, bool (*checkValidityFn)() = nullptr) {
	if constexpr (std::is_array_v<T>) {
		static_assert(false, "Only 2D arrays are supported");
	}
	ArraySetting<T> *setting = new ArraySetting<T>(id, (T *)data, X * Y, setDefaultValueFn, applyBoundsFn, checkValidityFn);
	settingsList.push_back(setting);
	return setting;
}

template <typename T, size_t N>
ArraySetting<T> *addArraySetting(const char *id, T (&data)[N], void (*setDefaultValueFn)() = nullptr,
								 void (*applyBoundsFn)() = nullptr, bool (*checkValidityFn)() = nullptr) {
	if constexpr (std::is_array_v<T>) {
		static_assert(false, "Only 1D arrays are supported");
	}
	ArraySetting<T> *setting = new ArraySetting<T>(id, (T *)data, N, setDefaultValueFn, applyBoundsFn, checkValidityFn);
	settingsList.push_back(setting);
	return setting;
}

template <typename T>
ArraySetting<T> *addPointerSetting(const char *id, T *data, size_t itemCount, void (*setDefaultValueFn)() = nullptr,
								   void (*applyBoundsFn)() = nullptr, bool (*checkValidityFn)() = nullptr) {
	ArraySetting<T> *setting = new ArraySetting<T>(id, (T *)data, itemCount, setDefaultValueFn, applyBoundsFn, checkValidityFn);
	settingsList.push_back(setting);
	return setting;
}
