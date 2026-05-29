/**
 * @file osdCanvas.cpp
 * @brief Implementation of the OsdCanvas class, which draws the OSD elements to a virtual canvas and sends it to the outputs
 *
 * Copyright (c) 2026 Kolibri-FC contributors
 *
 * This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 *
 * Kolibri-FC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kolibri-FC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
 */

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
		blinkingOn = (millis() % 600) > 300;
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
		if (updateTimer >= updateMicros || fastRedraw) {
			updateTimer = 0;
			state = CanvasState::CLEAR;
			wasFastRedraw = fastRedraw;
			fastRedraw = false;
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
	dragNDropHeight = 0;
	dragNDropWidth = 0;
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
	fastRedraw = true;
}

void OsdCanvas::setDragNDrop(const char *data, int col, int row, int width, int height) {
	i32 size = width * height;
	if (size < 0) return;
	if (size > sizeof(dragNDropData)) size = sizeof(dragNDropData);
	if (size) memcpy(dragNDropData, data, size);
	dragNDropCol = col;
	dragNDropRow = row;
	dragNDropWidth = width;
	dragNDropHeight = height;
	dirty = true;
	fastRedraw = true;
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
	dragNDropHeight = 0;
	dragNDropWidth = 0;
	dirty = false;
}

void OsdCanvas::setDefaultElements() {
	elements[0] = {
		.type = OsdElementType::BATTERY_VOLTAGE,
		.col = 3,
		.row = 1,
	};
	elements[1] = {
		.type = OsdElementType::WARNINGS,
		.col = 8,
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
		elements[MAX_ELEMENTS].type = OsdElementType::DISABLED;
		dirty = false;
	}
}

void OsdCanvas::drawElement(u32 index) {
	OsdElement &element = elements[index];
	switch (element.type) {
		//---------------------------
		//|     Battery Voltage     |
		//---------------------------
	case OsdElementType::BATTERY_VOLTAGE: {
		if (!batBlinkingAndBeeping || blinkingOn)
			printOnBuffer(element, "%.1f\x06", (f32)adcVoltage / 100);
	} break;
		//---------------------------
		//|  Battery Cell Voltage   |
		//---------------------------
	case OsdElementType::BATTERY_CELL_VOLTAGE:
		printOnBuffer(element, "%.2f\x06", ((f32)adcVoltage) / (100 * batCells));
		break;
	case OsdElementType::BATTERY_CELL_COUNT: {
		//---------------------------
		//|  Battery Cell Voltage   |
		//---------------------------
		printOnBuffer(element, "%dS", batCells);
	} break;
	case OsdElementType::BATTERY_CURRENT: {
		//---------------------------
		//|    Battery Current      |
		//---------------------------
		printOnBuffer(element, "%.0f\x9A", adcCurrent);
	} break;
	case OsdElementType::BATTERY_MAH_DRAWN: {
	} break;
	case OsdElementType::BATTERY_VOLTAGE_MIN: {
	} break;
	case OsdElementType::GPS_LONGITUDE: {
		//---------------------------
		//|     GPS Longitude       |
		//---------------------------
		printOnBuffer(element, "\x98%.6lf", gpsMotion.lon * 1E-7f); //! untested @Bastian.
	} break;
	case OsdElementType::GPS_LATITUDE: {
		//---------------------------
		//|     GPS Latitude        |
		//---------------------------
		printOnBuffer(element, "\x89%.6f", gpsMotion.lat * 1E-7f); //! untested @Bastian.
	} break;
	case OsdElementType::GPS_PLUSCODE: {
		//---------------------------
		//|     GPS Plus Code       |
		//---------------------------
		printOnBuffer(element, "%s", "NOT IMPLEMENTED"); // todo find the plus code var
	} break;
	case OsdElementType::SPEED: {
		//---------------------------
		//|     GPS Speed           |
		//---------------------------
		f32 speed = gpsMotion.gSpeed * (3.6f / 1000);
		if (speed < 0) speed *= -1;
		printOnBuffer(element, "%.1f\x9E", speed); //! untested @Bastian.
	} break;
	case OsdElementType::ALTITUDE: {
		//---------------------------
		//|     Altitude            |
		//---------------------------
		printOnBuffer(element, "%.1f\x0C", combinedAltitude.getf32()); //! untested @Bastian.
	} break;
	case OsdElementType::HOME_DISTANCE: {
		//---------------------------
		//|     GPS Home Distance   |
		//---------------------------
		fix32 distN, distE;
		startFixMath();
		distFromCoordinates(gpsLatitudeFiltered, gpsLongitudeFiltered, homepointLat, homepointLon, &distN, &distE);
		f32 dist = sqrtFix(distN * distN + distE * distE).getf32();

		if (dist < 100) {
			printOnBuffer(element, "%.1f\x0C", dist); // 12.3m //! untested @Bastian.
		} else if (dist < 1000) {
			printOnBuffer(element, "%.0f\x0C", dist); // 123m //! untested @Bastian.
		} else if (dist < 10000) {
			printOnBuffer(element, "%.2f\x7D", dist / 1000); // 1.23km //! untested @Bastian.
		} else if (dist < 100000) {
			printOnBuffer(element, "%.1f\x7D", dist / 1000); // 12.3km //! untested @Bastian.
		} else {
			printOnBuffer(element, "%.0f\x7D", dist / 1000); // 123km //! untested @Bastian.
		}
		break;
	}
	case OsdElementType::HOME_DIRECTION: {
		//---------------------------
		//|     GPS Home Direction  |
		//---------------------------
		fix32 distN, distE;
		startFixMath();
		distFromCoordinates(gpsLatitudeFiltered, gpsLongitudeFiltered, homepointLat, homepointLon, &distN, &distE);
		fix32 angle = FIX_PI + FIX_TWOPI + combinedHeading - atan2Fix(distE, distN); // FIX_PI for alignment, TWOPI for positive result
		u8 arrow = '\x60';
		arrow += (u8)((angle / FIX_TWOPI) * 16).geti32() & 0xF; // 16 arrows for a full circle
		char *ptr = getBufferPtr(element.col, element.row);
		if (ptr) {
			*ptr = arrow;
		}
	} break;
	case OsdElementType::SAT_COUNT: {
		//---------------------------
		//|     GPS Sat Count       |
		//---------------------------
		printOnBuffer(element, "\x1E\x1F%d", gpsStatus.satCount);
	} break;
	case OsdElementType::COMPASS_HEADING: {
		// //---------------------------
		// //|     Compass Heading	    /
		// //---------------------------

		// 		fix32 distN, distE;
		// startFixMath();
		// distFromCoordinates(gpsLatitudeFiltered, gpsLongitudeFiltered, homepointLat, homepointLon, &distN, &distE);
		// fix32 angle = FIX_PI + FIX_TWOPI + combinedHeading - atan2Fix(distE, distN); // FIX_PI for alignment, TWOPI for positive result
		// u8 arrow = '\x60';
		// fix32 hDir += ((angle / FIX_TWOPI) * 360).geti32()
		// //?Assuming 0 = north 90 = east, 180 = south, 270 = west
		// //!hDir is home direction NOT NORTH, otherwise you'll have some fun

		// fix32 heading = 0.0; //TODO get value

		// switch(element.option &0xF){ //we only need the last 4 bits for compass selection, leave the rest for other settings maybe

		// 	case 0:{
		// 	//Tiny compass
		// 		char tcStr[3]
		// 		u8 octantHeading = static_cast<u16>(heading)/45;
		// 		switch (octantHeading)
		// 		{
		// 			case 0: {tcStr = "N "}break;
		// 			case 1: {tcStr = "NE"}break;
		// 			case 2: {tcStr = "E "}break;
		// 			case 3: {tcStr = "SE"}break;
		// 			case 4: {tcStr = "S "}break;
		// 			case 5: {tcStr = "SW"}break;
		// 			case 6: {tcStr = "W "}break;
		// 			case 7: {tcStr = "NW"}break;
		// 			case 8: {tcStr = "N "}break;
		// 			printOnBuffer(element, tcStr);
		// 		}
		// 	}break;
		// 	case 1:{
		// 	//Heading in degrees
		// 		//? this may need to be filtered to be human readable, but we'll see
		// 		printOnBuffer(element, "%3.0f,  heading");
		// 	}break;
		// 	case 2:{
		// 	//Compass with needle
		// 	/*

		// 	\ N /  W | N
		// 	W ↑ E  - ↑ -
		// 	/ S \  S | E

		// 	*/
		// 	//? rotate the needle or rotate the rose, why not both?

		// 	snprintf(buf, 16, "\\N/W%cE/S\\", arrow + static_cast<u8>(heading/22.5));
		// 	copyFrameBuffer(frameBuffer, buf, width, height, element.col, element.row, 3);
		// 	}break;

		// }

	} break;
	case OsdElementType::FLIGHT_MODE: {
		//---------------------------
		//|     Flight Mode         |
		//---------------------------
		char flightModeStr[5] = {};
		switch (flightMode) {
		case FlightMode::ACRO:
			memcpy(flightModeStr, "ACRO", 4);
			break;
		case FlightMode::ALT_HOLD:
			memcpy(flightModeStr, "ALTH", 4);
			break;
		case FlightMode::ANGLE:
			memcpy(flightModeStr, "ANGL", 4);
			break;
		case FlightMode::GPS:
			memcpy(flightModeStr, "GPS\0", 4);
			break;
		case FlightMode::GPS_WP:
			memcpy(flightModeStr, "WAYP", 4);
			break;
		default:
			memcpy(flightModeStr, "ERR\0", 4);
			break;
		}
		printOnBuffer(element, "%s", flightModeStr);
	} break;
	case OsdElementType::RESCUE_STATUS: {
		//---------------------------
		//|     Rescue Status       |
		//---------------------------

		// RSCOFF
		// RSCRDY
		// CLIMB
		// MVTOWP
		// DESCEND
		// LANDING

	} break;
	case OsdElementType::RSSI_VAL: {
		if (!elrs) break;
		//---------------------------
		//|     RC RSSI             |
		//---------------------------
		printOnBuffer(element, "\x01%d", elrs ? elrs->uplinkRssi[0] : 0);
	} break;
	case OsdElementType::LINK_QUALITY: {
		if (!elrs) break;
		//---------------------------
		//|     RC Link Quality     |
		//---------------------------
		printOnBuffer(element, "\x7B%d%%", elrs ? elrs->uplinkLinkQuality : 0);
	} break;
	case OsdElementType::RC_CHANNEL: {
		if (!elrs) break;
		//---------------------------
		//|     RC Channel Value    |
		//---------------------------
		u32 channel = element.option & 0xF;
		if (element.option & 0x100) {
			if (channel < 4) {
				const char names[4][4] = {"ROL", "PIT", "THR", "YAW"};
				printOnBuffer(element, "%s:%d", names[channel], elrs->channels[channel]);
			} else {
				printOnBuffer(element, "A%2d:%d", channel - 3, elrs->channels[channel]);
			}
		} else {
			printOnBuffer(element, "%d", elrs->channels[channel]);
		}

	} break;
	case OsdElementType::BARO_ALTITUDE: {
		//---------------------------
		//|     Barometric Altitude |
		//---------------------------
		printOnBuffer(element, "\x7F%d\x0C", baroASL.geti32());
	} break;
	case OsdElementType::ESC_TEMP: {
		//---------------------------
		//|     ESC Temp            |
		//---------------------------
		switch (element.option) {
		case 0:
		case 1: { // max temp
			int maxTemp = escTemp[0];
			int maxIndex = 0;
			for (int i = 1; i < 4; i++) {
				if (escTemp[i] > maxTemp) {
					maxTemp = escTemp[i];
					maxIndex = i;
				}
			}
			if (element.option == 0) {
				printOnBuffer(element, "E\x7A%d\x0E@%d", maxTemp, maxIndex + 1);
			} else {
				printOnBuffer(element, "E\x7A%d\x0E", maxTemp);
			}
		} break;
		case 2: { // avg temp
			int avgTemp = (escTemp[0] + escTemp[1] + escTemp[2] + escTemp[3] + 2) / 4;
			printOnBuffer(element, "E\x7A%d\x0E", avgTemp);
		} break;
		default: {
			int escIndex = (element.option - 3) & 0x3;
			printOnBuffer(element, "E\x7A%d\x0E", escTemp[escIndex]);
		} break;
		}
	} break;
	case OsdElementType::IMU_ACCELERATION: {
		startFixMath();
		fix32 accel = sqrtFix(accelScaled[0] * accelScaled[0] + accelScaled[1] * accelScaled[1] + accelScaled[2] * accelScaled[2]) / 9.81f;
		printOnBuffer(element, "%.1fG", accel.getf32());
	} break;
	case OsdElementType::IMU_PITCH: {
		printOnBuffer(element, "\u0015%.1f\x08", (pitch * FIX_RAD_TO_DEG).getf32());
	} break;
	case OsdElementType::IMU_ROLL: {
		printOnBuffer(element, "\u0014%.1f\x08", (roll * FIX_RAD_TO_DEG).getf32());
	} break;
	case OsdElementType::IMU_YAW: {
		printOnBuffer(element, "%.1f\x08", (yaw * FIX_RAD_TO_DEG).getf32());
	} break;
	case OsdElementType::BAT_TIME: {
	} break;
	case OsdElementType::ARM_TIME: {
	} break;
	case OsdElementType::WARNINGS: {
		//---------------------------
		//|        Warnings         |
		//---------------------------
		// Basic implementation, would be nice to have an (at least partially) event driven system
		char warningStr[16] = {};
		memcpy(warningStr, "               ", 15);
		// The blinker ^~^
		if (elapsedMillis() % 1000 >= 250) { // 1Hz 75% On 25%

			// Beep
			if (batBlinkingAndBeeping) {
				uint32_t ms = elapsedMillis() % 1000;
				if (ms <= 500 && ms > 250) memcpy(warningStr, "    (CHIRP)    ", 15);
				if (ms <= 750 && ms > 500) memcpy(warningStr, "   ( CHIRP )   ", 15);
				if (ms > 750) memcpy(warningStr, "  (  CHIRP  )  ", 15);
			}

			// Rescue mode not available
			if (armed) {
				if (armedTimer <= 5000) {
					if (gpsStatus.satCount < 6) { // This needs to be a flag set by the RTX/RTH code
						memcpy(warningStr, "  RESCUE N/A   ", 15);
					}
				}
			}

			// Signal low
			if (elrs && elrs->uplinkRssi[0] < 20) {
				memcpy(warningStr, "  WEAK SIGNAL  ", 15);
			}

			// Overheat
			if (escTemp[0] > 80 || escTemp[1] > 80 || escTemp[2] > 80 || escTemp[3] > 80) {
				memcpy(warningStr, "   ESC TEMP    ", 15);
			}

			// Low battery
			if (adcVoltage < 3.5f * 100 * batCells) {
				if (adcVoltage < 3.3f * 100 * batCells) {
					memcpy(warningStr, "   LAND NOW    ", 15);
				} else {
					memcpy(warningStr, "  LOW VOLTAGE  ", 15);
				}
			}
		}
		printOnBuffer(element, "%s", warningStr); //! untested @Bastian.

	} break;
	case OsdElementType::CUSTOM_TEXT: {
		//---------------------------
		//|     Custom Text         |
		//---------------------------
		copyFrameBuffer(frameBuffer, (char *)&element.option, width, height, element.col, element.row, 4);
	} break;
	case OsdElementType::DEBUG_1: {
		// https://testufo.com/frameskipping
		static u8 row = 0;
		static u8 col = 0;
		char *ptr = getBufferPtr(col + element.col, row + element.row);
		if (++col == 6) {
			col = 0;
			if (++row == 5) {
				row = 0;
			}
		}
		if (ptr != nullptr) *ptr = 0x9B;
	} break;
	case OsdElementType::DEBUG_2: {
		char buf[2] = {0, 0};
		if (dirty) buf[0] = 'D';
		if (wasFastRedraw) buf[1] = 'F';
		copyFrameBuffer(frameBuffer, buf, width, height, element.col, element.row, 2);
	} break;
	case OsdElementType::DEBUG_3: {
		printOnBuffer(element, "HW:%d SW:%d", (hardMagHeading * FIX_RAD_TO_DEG).geti32(), (softMagHeading * FIX_RAD_TO_DEG).geti32());
	} break;
	case OsdElementType::DEBUG_4: {
	} break;
	case OsdElementType::CURSOR: {
		char *ptr = getBufferPtr(element.col, element.row);
		if (ptr != nullptr) *ptr = '\x6a';
	} break;
	case OsdElementType::DISABLED:
	default:
		break;
	}
}

template <typename... Types>
inline void OsdCanvas::printOnBuffer(const OsdElement &element, const char *str, const Types... args) {
	char buf[16];
	int len = snprintf(buf, 16, str, args...);
	if (len > 16) len = 16;
	copyFrameBuffer(frameBuffer, buf, width, height, element.col, element.row, len);
}

void OsdCanvas::optimize() {
	// disabling cursor
	elements[MAX_ELEMENTS].type = OsdElementType::DISABLED;

	// disable drag an drop
	dragNDropHeight = 0;
	dragNDropWidth = 0;

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
	u8 buf[MAX_ELEMENTS * 8];
	canvasSettingsFile->seek(0);
	u32 read = canvasSettingsFile->readBytes((char *)buf, MAX_ELEMENTS * 8);
	for (int i = 0; i < read / 8 && i < MAX_ELEMENTS; i++) {
		u32 pos = i * 8;
		elements[i] = {
			.type = (OsdElementType)(((u16)buf[pos]) | (((u16)buf[pos + 1]) << 8)),
			.col = (i8)buf[pos + 2],
			.row = (i8)buf[pos + 3],
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
