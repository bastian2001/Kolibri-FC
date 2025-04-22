#include "typedefs.h"
#include <string>

using std::string;

template <typename T>
class Setting {
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

	const char *id;

	/// @brief Resets the setting to the default value.
	void resetToDefault();

	/**
	 * @brief Get the value of a setting as a string (from the settingsFile).
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
	 * @brief Convert the setting to a string for saving to a file.
	 *
	 * @return string string representation of the setting
	 */
	string toString();

	/**
	 * @brief Set the data from a string.
	 *
	 * @return true if successful
	 * @return false if failed (e.g. parser error)
	 */
	bool setDataFromString(string);

	/**
	 * @brief Check if the setting is valid.
	 *
	 * @return true if valid
	 * @return false if invalid
	 */
	bool checkValidity();

	/**
	 * @brief Update the setting in the settings file.
	 *
	 * @return true if successful
	 * @return false if failed
	 */
	bool updateSettingInFile();

	void setMinMax(T minValue, T maxValue);

	static void setSettingsFile(File *file);

private:
	T *data;
	T defaultValue;
	static File *settingsFile;
	T minValue;
	T maxValue;
};