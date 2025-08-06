#pragma once
#include "typedefs.h"
#include <LittleFS.h>
#include <string>

using std::string;
using std::vector;

void replaceEscapeSequences(string &s);

class SettingBase {
public:
	/**
	 * @brief Virtual destructor for proper inheritance
	 */
	virtual ~SettingBase() = default;

	SettingBase() = delete;
	SettingBase(const SettingBase &) = delete;
	SettingBase(SettingBase &&) = delete;
	SettingBase &operator=(const SettingBase &) = delete;
	SettingBase &operator=(SettingBase &&) = delete;

	SettingBase(const char *id) : id(id) {};

	/**
	 * @brief Set the settings file for all settings
	 *
	 * @param file Pointer to the file to use for all settings
	 */
	static void setSettingsFile(File *file);

	/**
	 * @brief Get the value of a setting as a string (from the settingsFile)
	 *
	 * Only supports single line strings, not arrays or multi-line strings.
	 *
	 * @param s buffer to store the string
	 * @param id setting id
	 * @return true if the value was successfully found
	 * @return false if the setting is not found
	 */
	static bool getValueString(string &s, const char *id);

	/**
	 * @brief Resets the setting to the default value.
	 */
	virtual void resetToDefault() = 0;

	/**
	 * @brief Convert the setting to a string for saving to a file.
	 *
	 * @return string string representation of the setting
	 */
	virtual string toString(bool readable = false) = 0;

	/**
	 * @brief Set the data from a string.
	 *
	 * @param s string to set the data from
	 * @param readable if true, fix values are read as floats, otherwise as raw values
	 *
	 * @return true if successful
	 * @return false if failed (e.g. parser error)
	 */
	virtual bool setDataFromString(string s, bool readable = false) = 0;

	/**
	 * @brief Check if the setting is valid.
	 *
	 * @return true if valid
	 * @return false if invalid
	 */
	virtual bool checkValidity() = 0;

	/**
	 * @brief Update the setting in the settings file.
	 *
	 * @return true if successful
	 * @return false if failed
	 */
	bool updateSettingInFile();

	const char *id;

protected:
	static File *settingsFile; // Shared settings file for all setting types
};

extern vector<SettingBase *> settingsList; // List of all settings

template <typename T>
class Setting;

extern Setting<string> dummySetting;

SettingBase *getSetting(const char *id);

/**
 * @brief Convert a string to a boolean value
 * @param s String to convert
 * @return result
 * @throws
 */
bool stringToBool(const string &s);
