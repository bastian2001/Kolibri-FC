#include "global.h"

u8 osdCanvasSizeSrc = 0;
u8 osdRefreshRate = 12;

void OsdCanvas::begin() {
	addSetting(SETTING_OSD_CANVAS_SIZE_SRC, &osdCanvasSizeSrc, 0);
	addSetting(SETTING_OSD_REFRESH_HZ, &osdRefreshRate, 12);

	canvasSettingsFile.emplace(File());

	setSize(30, 16, 255);
	setUpdateRate(osdRefreshRate);

	bool newFile = openCanvasSettingsFile();
	resetElements();

	if (*canvasSettingsFile) {
		if (newFile) {
			setDefaultElements();
			saveElements();
		} else {
			loadElementsFromSettings();
		}
	}
	closeCanvasSettingsFile();

	updateTimer = 0;
}

void OsdCanvas::loop() {
	if (frameBuffer == nullptr) return;
	TASK_START(TASK_OSD);
	if (outputs[loopIndex] != nullptr) {
		outputs[loopIndex]->loop();
	}
	if (++loopIndex >= MAX_OUTPUTS) loopIndex = 0;
	switch (state) {
	case CanvasState::CLEAR:
		memset(frameBuffer, 0, width * height);
		state = CanvasState::DRAW;
		pushIndex = 0;
		currentlyDrawing = 0;
		break;
	case CanvasState::DRAW:
		drawElement(currentlyDrawing++);
		if (dirty) {
			for (; currentlyDrawing < MAX_ELEMENTS + 1; currentlyDrawing++) {
				if (elements[currentlyDrawing].type != OsdElementType::DISABLED) break;
			}
			if (currentlyDrawing == MAX_ELEMENTS + 1) state = CanvasState::DRAW_DND;
		} else {
			if (elements[currentlyDrawing].type == OsdElementType::DISABLED || currentlyDrawing == MAX_ELEMENTS) {
				state = CanvasState::PUSH;
			}
		}
		break;
	case CanvasState::DRAW_DND:
		copyFrameBuffer(frameBuffer, dragNDropData, width, height, dragNDropCol, dragNDropRow, dragNDropWidth, dragNDropHeight);
		state = CanvasState::PUSH;
		break;
	case CanvasState::PUSH:
		if (outputs[pushIndex] != nullptr) outputs[pushIndex]->sendFrame(frameBuffer, width, height);
		if (++pushIndex >= MAX_OUTPUTS) state = CanvasState::WAIT;
		break;
	case CanvasState::WAIT:
		if (updateTimer >= updateMicros) {
			updateTimer = 0;
			state = CanvasState::CLEAR;
		}
		break;
	}
	TASK_END(TASK_OSD);
}

void OsdCanvas::setSize(u8 width, u8 height, u8 source) {
	if (source != 255 && source != osdCanvasSizeSrc) return;
	if (width > 192 || height > 192) return;
	void *fb = malloc(width * height);
	if (fb == nullptr) return;
	if (frameBuffer != nullptr) free(frameBuffer);
	frameBuffer = (char *)fb;
	this->width = width;
	this->height = height;
	state = CanvasState::CLEAR;
}

void OsdCanvas::setElement(u32 index, const OsdElement &el) {
	if (index < MAX_ELEMENTS) elements[index] = el;
	dirty = true;
}

void OsdCanvas::drawCursor(i8 col, i8 row) {
	elements[MAX_ELEMENTS] = {
		.type = OsdElementType::CURSOR,
		.col = col,
		.row = row,
		.option = 0,
	};
	dirty = true;
}

void OsdCanvas::setDragNDrop(const char *data, int col, int row, int width, int height) {
	dirty = true;
}

void OsdCanvas::resetElements() {
	OsdElement reset = {
		.type = OsdElementType::DISABLED,
		.col = 0,
		.row = 0,
		.option = 0,
	};
	for (auto &el : elements) {
		el = reset;
	}
}

void OsdCanvas::setDefaultElements() {
	elements[0] = {
		.type = OsdElementType::BATTERY_VOLTAGE,
		.col = 3,
		.row = 1,
	};
	elements[1] = {
		.type = OsdElementType::WARNINGS,
		.col = 10,
		.row = 1,
	};
	elements[2] = {
		.type = OsdElementType::RSSI_VAL,
		.col = 23,
		.row = 1,
	};
}

void OsdCanvas::saveElements() {
	optimize();
	openCanvasSettingsFile();
	saveElementsToSettings();
	closeCanvasSettingsFile();
}
void OsdCanvas::revertElements() {
	if (dirty) {
		openCanvasSettingsFile();
		loadElementsFromSettings();
		closeCanvasSettingsFile();
	}
}

void OsdCanvas::drawElement(u32 index) {
	OsdElement &el = elements[index];
	switch (el.type) {
	case OsdElementType::BATTERY_VOLTAGE: {
		char buf[16];
		int len = snprintf(buf, 16, "%.1f\x06", (float)adcVoltage / 100);
		int maxLen = width - el.col;
		if (len > 16) len = 16;
		if (len > maxLen) len = maxLen;
		char *ptr = getBufferPtr(el.col, el.row);
		if (ptr != nullptr) memcpy(ptr, buf, len);
	} break;
	case OsdElementType::BATTERY_CELL_VOLTAGE: {
	} break;
	case OsdElementType::BATTERY_CELL_COUNT: {
	} break;
	case OsdElementType::BATTERY_CURRENT: {
	} break;
	case OsdElementType::BATTERY_MAH_DRAWN: {
	} break;
	case OsdElementType::BATTERY_VOLTAGE_MIN: {
	} break;
	case OsdElementType::GPS_LONGITUDE: {
	} break;
	case OsdElementType::GPS_LATITUDE: {
	} break;
	case OsdElementType::GPS_PLUSCODE: {
	} break;
	case OsdElementType::SPEED: {
	} break;
	case OsdElementType::ALTITUDE: {
	} break;
	case OsdElementType::HOME_DISTANCE: {
	} break;
	case OsdElementType::HOME_DIRECTION: {
	} break;
	case OsdElementType::FLIGHT_MODE: {
	} break;
	case OsdElementType::RESCUE_STATUS: {
	} break;
	case OsdElementType::RSSI_VAL: {
		if (!elrs) break;
		char buf[16];
		int len = snprintf(buf, 16, "\x01%d", elrs->uplinkRssi[0]);
		int maxLen = width - el.col;
		if (len > 16) len = 16;
		if (len > maxLen) len = maxLen;
		char *ptr = getBufferPtr(el.col, el.row);
		if (ptr != nullptr) memcpy(ptr, buf, len);
	} break;
	case OsdElementType::LINK_QUALITY: {
		if (!elrs) break;
		char buf[16];
		int len = snprintf(buf, 16, "%d%%", elrs->uplinkLinkQuality);
		int maxLen = width - el.col;
		if (len > 16) len = 16;
		if (len > maxLen) len = maxLen;
		char *ptr = getBufferPtr(el.col, el.row);
		if (ptr != nullptr) memcpy(ptr, buf, len);
	} break;
	case OsdElementType::BARO_ALTITUDE: {
	} break;
	case OsdElementType::ESC_TEMP_0: {
	} break;
	case OsdElementType::ESC_TEMP_1: {
	} break;
	case OsdElementType::ESC_TEMP_2: {
	} break;
	case OsdElementType::ESC_TEMP_3: {
	} break;
	case OsdElementType::ESC_TEMP_AVG: {
	} break;
	case OsdElementType::IMU_ACCELERATION: {
	} break;
	case OsdElementType::IMU_PITCH: {
	} break;
	case OsdElementType::IMU_ROLL: {
	} break;
	case OsdElementType::IMU_YAW: {
	} break;
	case OsdElementType::RC_ROLL: {
	} break;
	case OsdElementType::RC_PITCH: {
	} break;
	case OsdElementType::RC_THROTTLE: {
	} break;
	case OsdElementType::RC_YAW: {
	} break;
	case OsdElementType::BAT_TIME: {
	} break;
	case OsdElementType::ARM_TIME: {
	} break;
	case OsdElementType::WARNINGS: {
	} break;
	case OsdElementType::DEBUG_1: {
		// https://testufo.com/frameskipping
		static u8 row = 0;
		static u8 col = 0;
		char *ptr = getBufferPtr(col + el.col, row + el.row);
		if (++col == 6) {
			col = 0;
			if (++row == 5) {
				row = 0;
			}
		}
		if (ptr != nullptr) *ptr = 0x9B;
	} break;
	case OsdElementType::DEBUG_2: {
	} break;
	case OsdElementType::DEBUG_3: {
	} break;
	case OsdElementType::DEBUG_4: {
	} break;
	case OsdElementType::DISABLED:
	default:
		break;
	}
}

void OsdCanvas::optimize() {
	// disabling cursor
	elements[MAX_ELEMENTS].type = OsdElementType::DISABLED;

	// remove empty slots
	u32 putting = 0;
	for (u32 scanning = 0; scanning < MAX_ELEMENTS; scanning++) {
		if (elements[scanning].type != OsdElementType::DISABLED)
			elements[putting++] = elements[scanning];
	}
	for (; putting < MAX_ELEMENTS; putting++) {
		elements[putting].type = OsdElementType::DISABLED;
	}

	// clear dirty flag
	dirty = false;
}

bool OsdCanvas::openCanvasSettingsFile() {
	if (!littleFsReady) return false;
	if (*canvasSettingsFile) {
		canvasSettingsFile->close();
	}

	*canvasSettingsFile = LittleFS.open("/osd.bin", "r+");
	if (!*canvasSettingsFile) {
		DEBUG_PRINTLN("Failed to open OSD file, creating new one...");
		*canvasSettingsFile = LittleFS.open("/osd.bin", "w+");
		if (!*canvasSettingsFile) {
			DEBUG_PRINTLN("Failed to create OSD file.");
			return false;
		}
		return true;
	}
	return false;
}

void OsdCanvas::closeCanvasSettingsFile() {
	if (*canvasSettingsFile) {
		rp2040.wdt_reset();
		canvasSettingsFile->close();
	}
}

void OsdCanvas::loadElementsFromSettings() {
	i8 buf[MAX_ELEMENTS * 8];
	canvasSettingsFile->seek(0);
	u32 read = canvasSettingsFile->readBytes((char *)buf, MAX_ELEMENTS * 8);
	for (int i = 0; i < read / 8 && i < MAX_ELEMENTS; i++) {
		u32 pos = i * 8;
		elements[i] = {
			.type = (OsdElementType)(buf[pos] | (buf[pos + 1] << 8)),
			.col = buf[pos + 2],
			.row = buf[pos + 3],
			.option = DECODE_U4((u8 *)&buf[pos + 4]),
		};
	}
}

void OsdCanvas::saveElementsToSettings() {
	u8 buf[MAX_ELEMENTS * 8];
	canvasSettingsFile->seek(0);
	for (u32 i = 0; i < MAX_ELEMENTS; i++) {
		u32 pos = i * 8;
		OsdElement &el = elements[i];
		u16 type = (u16)el.type;
		buf[pos + 0] = type;
		buf[pos + 1] = (u8)(type >> 8);
		buf[pos + 2] = el.col;
		buf[pos + 3] = el.row;
		memcpy(&buf[pos + 4], &el.option, 4);
	}
	canvasSettingsFile->write(buf, MAX_ELEMENTS * 8);
	rp2040.wdt_reset();
	canvasSettingsFile->flush();
	rp2040.wdt_reset();
}
