#pragma once
#include "typedefs.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <list>
#include <optional>

inline static void copyFrameBuffer(char *dest, const char *src, i32 destWidth, i32 destHeight, i32 destCol, i32 destRow, i32 width, i32 height) {
	// if we cannot draw anything, just return
	// vertical pure off-screen drawing is prevented by the for loop (0 iterations)
	if (destWidth <= 0 || destHeight <= 0 || width <= 0 || height <= 0 || destCol + width <= 0 || destCol >= destWidth) return;
	i32 maxRow = MIN(destHeight - destRow, height);
	i32 startCopySrcCol = MAX(0, -destCol);
	i32 startCopyDestCol = MAX(0, destCol);
	i32 lenCopySrcCol = MIN(destWidth, width + destCol) - startCopyDestCol;
	src += startCopySrcCol;
	dest += startCopyDestCol + destRow * destWidth;
	for (i32 row = MAX(0, -destRow); row < maxRow; row++) {
		memcpy(dest + row * destWidth, src + row * width, lenCopySrcCol);
	}
}
inline static void copyFrameBuffer(char *dest, const char *src, i32 destWidth, i32 destHeight, i32 destCol, i32 destRow, i32 width) {
	// if we cannot draw anything, just return
	if (destWidth <= 0 || destHeight <= 0 || width <= 0 || destCol + width <= 0 || destCol >= destWidth || destRow < 0 || destRow >= destHeight) return;
	i32 startCopySrcCol = MAX(0, -destCol);
	i32 startCopyDestCol = MAX(0, destCol);
	i32 lenCopySrcCol = MIN(destWidth, destCol + width) - startCopyDestCol;
	memcpy(dest + destRow * destWidth + startCopyDestCol, src + startCopySrcCol, lenCopySrcCol);
}

// 0 = analog, 1 = MSP
extern u8 osdCanvasSizeSrc;

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
	RC_THROTTLE,
	RC_YAW,

	// Timers
	BAT_TIME = 0x00B0,
	ARM_TIME,

	// Warn
	WARNINGS = 0x00C0,

	// Debug, Drag and drop
	DEBUG_1 = 0xFFF0,
	DEBUG_2,
	DEBUG_3,
	DEBUG_4,
	CURSOR = 0xFFFE,

	// // HUD
	// HUD_COMPASS_HEADING,
	// HUD_ARTIFICIAL_HORIZON,
	// HUD_ASCEND_RATE,
	// HUD_ASCEND_INDICATOR,

};

class OsdOutput;
typedef struct osdElement {
	OsdElementType type = OsdElementType::DISABLED;
	i8 col = 0;
	i8 row = 0;
	u32 option = 0;
} OsdElement;

class OsdCanvas {
public:
	OsdCanvas(const OsdCanvas &) = delete;
	OsdCanvas &operator=(const OsdCanvas &) = delete;

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

	/**
	 * @brief Set the size of the OSD canvas
	 *
	 * @param width width in chars
	 * @param height height in chars
	 * @param source source of the size: 0 = analog, 1 = MSP, 255 = force anyway
	 */
	void setSize(u8 width, u8 height, u8 source);
	inline void getSize(u8 *width, u8 *height) const {
		*width = this->width;
		*height = this->height;
	};

	inline constexpr const OsdElement &getElement(u32 index) const { return elements[index]; };
	void setElement(u32 index, const OsdElement &el);
	void drawCursor(i8 col, i8 row);
	void setDragNDrop(const char *data, int col, int row, int width, int height);

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
		for (int i = 0; i < MAX_OUTPUTS; i++) {
			if (outputs[i] == output) return true;
		}
		for (int i = 0; i < MAX_OUTPUTS; i++) {
			if (outputs[i] == nullptr) {
				outputs[i] = output;
				state = CanvasState::CLEAR;
				return true;
			}
		}
		return false;
	};
	/**
	 * @brief Remove an OSD output
	 *
	 * Does not delete the output! Just removes it from the scheduler.
	 *
	 * @param output pointer to the output to remove
	 */
	inline void removeOutput(OsdOutput *output) {
		for (int i = 0; i < MAX_OUTPUTS; i++) {
			if (outputs[i] == output) outputs[i] = nullptr;
		}
		state = CanvasState::CLEAR;
	}

	inline void setUpdateRate(u8 updateHz) { updateMicros = 1000000 / updateHz; };

	static constexpr u32 MAX_ELEMENTS = 255;

private:
	static constexpr u32 MAX_OUTPUTS = 5;

	constexpr OsdCanvas() {};

	void drawElement(u32 index);
	void optimize();

	bool openCanvasSettingsFile();
	void closeCanvasSettingsFile();
	std::optional<File> canvasSettingsFile;

	void loadElementsFromSettings();
	void saveElementsToSettings();

	template <typename... Types>
	void printOnBuffer(const OsdElement &element, const char *str, const Types... args);

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
		return &frameBuffer[row * width + col];
	}
	i32 width = 0;
	i32 height = 0;
	OsdOutput *outputs[5] = {};
	OsdElement elements[MAX_ELEMENTS + 1] = {};

	enum class CanvasState {
		CLEAR,
		DRAW,
		DRAW_DND,
		PUSH,
		WAIT,
	};
	CanvasState state = CanvasState::CLEAR;
	bool dirty = false;
	char dragNDropData[256] = {};
	i32 dragNDropWidth = 0;
	i32 dragNDropHeight = 0;
	i32 dragNDropCol = 0;
	i32 dragNDropRow = 0;
	u32 currentlyDrawing = 0;
	u8 pushIndex = 0;
	u8 loopIndex = 0;

	u32 updateMicros = 83000;
	elapsedMicros updateTimer{(struct dummyStruct){}};
};
