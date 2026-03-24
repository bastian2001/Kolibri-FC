#pragma once
#include "typedefs.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <list>
#include <optional>

enum class OsdElementType : u16 {
	DISABLED = 0xFFFF,

	// Battery info
	BATTERY_VOLTAGE = 0x0000,
	BATTERY_CELL_VOLTAGE,
	BATTERY_CELL_COUNT,
	BATTERY_CURRENT,
	BATTERY_MAH_DRAWN,
	BATTERY_VOLTAGE_MIN,

	// GPS info
	GPS_LONGITUDE = 0x0020,
	GPS_LATITUDE,
	GPS_PLUSCODE,
	SPEED,
	ALTITUDE,
	HOME_DISTANCE,
	HOME_DIRECTION,

	// Flight info
	FLIGHT_MODE = 0x0040,
	RESCUE_STATUS,

	// RC link info
	RSSI_VAL = 0x0060,
	LINK_QUALITY,
	ELRS_RX_STATUS,

	// Sensors
	BARO_ALTITUDE = 0x0070,
	ESC_TEMP_0,
	ESC_TEMP_1,
	ESC_TEMP_2,
	ESC_TEMP_3,
	ESC_TEMP_AVG,
	IMU_ACCELERATION,
	IMU_PITCH,
	IMU_ROLL,
	IMU_YAW,

	// RC sticks
	RC_ROLL = 0x00A0,
	RC_PITCH,
	RC_YAW,
	RC_THROTTLE,

	// Timers
	BAT_TIME = 0x00B0,
	ARM_TIME,
	USER_TIME,

	// Warn
	ALARM_CRITICAL = 0x00C0,
	ALARM_WARNING,
	ALARM_INFO,

	// // HUD
	// HUD_COMPASS_HEADING,
	// HUD_ARTIFICIAL_HORIZON,
	// HUD_ASCEND_RATE,
	// HUD_ASCEND_INDICATOR,

};

class OsdOutput;
typedef struct osdElement {
	OsdElementType type = OsdElementType::DISABLED;
	u8 col = 0;
	u8 row = 0;
	u32 option = 0;
} OsdElement;

class OsdCanvas {
public:
	[[gnu::const]] static OsdCanvas &get() {
		static OsdCanvas c;
		return c;
	}

	/**
	 * @brief starts up the OSD
	 *
	 * Loads the config incl. all the elements
	 */
	void begin();

	/**
	 * @brief Main function of the OSD Canvas
	 *
	 * Has a couple of different states:
	 *
	 * 1. Clears the internal frame buffer
	 *
	 * 2. Draws one element at a time
	 *
	 * 3. Sends the updated frame buffer to each OSD output, and requests them to send it.
	 *
	 * 4. waiting until the next frame shall be drawn
	 */
	void loop();

	void setSize(u8 width, u8 height);
	inline void getSize(u8 *const width, u8 *const height) const {
		*width = this->width;
		*height = this->height;
	};

	inline constexpr const OsdElement &getElement(u32 index) const { return elements[index]; };
	void setElement(u32 index, const OsdElement &el);

	void resetElements();
	void setDefaultElements();
	void saveElements();
	void revertElements();

	/**
	 * @brief Adds an OSD output
	 *
	 * @param output pointer to the OSD output (e.g. digital OSD)
	 * @return true if it was added
	 * @return false if not (list is limited to 5 outputs)
	 */
	inline bool addOutput(OsdOutput *output) {
		if (outputs.size() >= 5) return false;
		outputs.push_back(output);
		state = CanvasState::CLEAR;
		return true;
	};
	/**
	 * @brief Remove an OSD output
	 *
	 * Does not delete the output! Just removes it from the scheduler.
	 *
	 * @param output pointer to the output to remove
	 */
	inline void removeOutput(OsdOutput *output) {
		outputs.remove(output);
		state = CanvasState::CLEAR;
	}

	inline void setUpdateRate(u8 updateHz) { updateMicros = 1000000 / updateHz; };

private:
	static constexpr int MAX_ELEMENTS = 256;

	constexpr OsdCanvas() {};

	void drawElement(u32 index);

	bool openCanvasSettingsFile();
	void closeCanvasSettingsFile();
	std::optional<File> canvasSettingsFile;

	void loadElementsFromSettings();
	void saveElementsToSettings();

	char *frameBuffer = nullptr;
	/**
	 * @brief Gets a pointer to write an element to
	 *
	 * @warning Client needs to check for maximum width/height individually. Only the first character is guaranteed to be ok.
	 *
	 * @param col the column to use
	 * @param row the row to use
	 * @return char* pointer to write to
	 */
	inline char *getBufferPtr(u8 col, u8 row) {
		if (col >= width || row >= height) return nullptr;
		char *ret = &frameBuffer[row * width + col];
	}
	u8 width = 0;
	u8 height = 0;
	std::list<OsdOutput *> outputs;
	OsdElement elements[MAX_ELEMENTS] = {};

	enum class CanvasState {
		CLEAR,
		DRAW,
		PUSH,
		WAIT,
	};
	CanvasState state = CanvasState::CLEAR;
	u32 currentlyDrawing = 0;
	std::list<OsdOutput *>::const_iterator pushIterator = outputs.cbegin();

	u32 updateMicros = 83000;
	elapsedMicros updateTimer{(struct dummyStruct){}};
};
