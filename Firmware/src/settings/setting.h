#pragma once
#include global.h

class SettingBase {
protected:
	const char *const id;
	const u32 position;
	const size_t length;

public:
	virtual ~SettingBase() = default;
	virtual bool readSetting() = 0;
	virtual bool writeSetting() const = 0;
	virtual bool revertChanges() = 0;
	virtual bool commitSettingToFile() const = 0;

	const char *const getId() const {
		return id;
	}
	const u32 getPosition() const {
		return position;
	}
	const size_t getLength() const {
		return length;
	}
};

template <typename T>
class Setting : public SettingBase {
	T *const data;
	const T stepSize = static_cast<T>(1);
	const T defaultValue;

	// For later use (when proof of concept works)
	//  const bool isArray;
	//  const size_t array
	// If worried about file integrity
	// const u64 checksum
public:
	Setting() = delete;
	Setting(const Setting &) = delete;
	&Setting operator=(const Setting &) = delete;

	Setting(const char *id, T *data, T defaultValue);
	Setting(const char *id, T *data, T defaultValue, T stepSize);

	bool readSetting();
	bool writeSetting() const;
	bool revertChanges();
	bool commitSettingToFile() const;
	const &T getValue() const;

	T increment() {
		static_assert(std::is_integral_v<T> || std::is_floating_point_v<T> ||
						  std::is_same_v<T, fix32> || std::is_same_v<T, fix64>,
					  "Unsupported type for increment");
		*data = *data + stepSize;
	}
	T decrement() {
		static_assert(std::is_integral_v<T> || std::is_floating_point_v<T> ||
						  std::is_same_v<T, fix32> || std::is_same_v<T, fix64>,
					  "Unsupported type for decrement");
		*data = *data - stepSize;
	}

	T operator++() {
		static_assert(std::is_integral_v<T> || std::is_floating_point_v<T> ||
						  std::is_same_v<T, fix32> || std::is_same_v<T, fix64>,
					  "Unsupported type for operator++");
		*data = *data - static_cast<T>(1);
	}

	T operator--() {
		static_assert(std::is_integral_v<T> || std::is_floating_point_v<T> ||
						  std::is_same_v<T, fix32> || std::is_same_v<T, fix64>,
					  "Unsupported type for operator--");
		*data = *data - static_cast<T>(1);
	}
};
