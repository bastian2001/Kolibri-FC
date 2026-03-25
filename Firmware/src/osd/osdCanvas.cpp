#include "global.h"

void OsdCanvas::begin() {
	canvasSettingsFile.emplace(File());

	setSize(30, 16);
	setUpdateRate(12);

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
	switch (state) {
	case CanvasState::CLEAR:
		memset(frameBuffer, 0, width * height);
		state = CanvasState::DRAW;
		pushIndex = 0;
		currentlyDrawing = 0;
		break;
	case CanvasState::DRAW:
		drawElement(currentlyDrawing);
		currentlyDrawing++;
		for (; currentlyDrawing < MAX_ELEMENTS; currentlyDrawing++) {
			drawElement(currentlyDrawing);
		}
		if (currentlyDrawing == MAX_ELEMENTS) state = CanvasState::PUSH;
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

void OsdCanvas::setSize(u8 width, u8 height) {
	void *fb = malloc(width * height);
	if (fb == nullptr) return;
	if (frameBuffer != nullptr) free(frameBuffer);
	frameBuffer = (char *)fb;
	this->width = width;
	this->height = height;
	state = CanvasState::CLEAR;
}

void OsdCanvas::setElement(u32 index, const OsdElement &el) {
	elements[index] = el;
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
		.type = OsdElementType::ALARM_WARNING,
		.col = 4,
		.row = 6,
	};
	elements[2] = {
		.type = OsdElementType::ALARM_CRITICAL,
		.col = 4,
		.row = 6,
	};
	elements[3] = {
		.type = OsdElementType::LINK_QUALITY,
		.col = 11,
		.row = 1,
	};
}

void OsdCanvas::saveElements() {
	openCanvasSettingsFile();
	saveElementsToSettings();
	closeCanvasSettingsFile();
}
void OsdCanvas::revertElements() {
	openCanvasSettingsFile();
	loadElementsFromSettings();
	closeCanvasSettingsFile();
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
	} break;
	case OsdElementType::LINK_QUALITY: {
	} break;
	case OsdElementType::ELRS_RX_STATUS: {
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
	case OsdElementType::RC_YAW: {
	} break;
	case OsdElementType::RC_THROTTLE: {
	} break;
	case OsdElementType::BAT_TIME: {
	} break;
	case OsdElementType::ARM_TIME: {
	} break;
	case OsdElementType::USER_TIME: {
	} break;
	case OsdElementType::ALARM_CRITICAL: {
	} break;
	case OsdElementType::ALARM_WARNING: {
	} break;
	case OsdElementType::ALARM_INFO: {
	} break;

	case OsdElementType::DISABLED:
	default:
		break;
	}
}

bool OsdCanvas::openCanvasSettingsFile() {
	if (!littleFsReady) return false;
	if (*canvasSettingsFile) {
		canvasSettingsFile->close();
	}

	*canvasSettingsFile = LittleFS.open("/osd.bin", "r+");
	if (!*canvasSettingsFile) {
		Serial.println("Failed to open OSD file, creating new one...");
		*canvasSettingsFile = LittleFS.open("/osd.bin", "w+");
		if (!*canvasSettingsFile) {
			Serial.println("Failed to create OSD file.");
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
	u8 buf[MAX_ELEMENTS * 8];
	canvasSettingsFile->seek(0);
	u32 read = canvasSettingsFile->readBytes((char *)buf, MAX_ELEMENTS * 8);
	for (int i = 0; i < read / 8 && i < MAX_ELEMENTS; i++) {
		u32 pos = i * 8;
		elements[i] = {
			.type = (OsdElementType)(buf[pos] | (buf[pos + 1] << 8)),
			.col = buf[pos + 2],
			.row = buf[pos + 3],
			.option = DECODE_U4(&buf[pos + 4]),
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
	canvasSettingsFile->write(buf, MAX_ELEMENTS * 4);
	rp2040.wdt_reset();
	canvasSettingsFile->flush();
	rp2040.wdt_reset();
}
