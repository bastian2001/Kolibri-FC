/**
 * @file mspOsdOutput.cpp
 * @brief Implementation of MspOsdOutput, a digital OSD output via MSP Displayport
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

void MspOsdOutput::loop() {
	const bool isRegularMsp = serial.lastMspVersion < MspVersion::V2_OVER_CRSF;
	if (heartbeatTimer > (isRegularMsp ? 50000 : 150000)) {
		heartbeatTimer = 0;
		MspMsgSetup s = {
			.serial = serial,
			.fn = MspFn::MSP_DISPLAYPORT,
			.type = MspMsgType::RESPONSE,
			.version = serial.lastMspVersion,
		};
		char data = MSP_DP_HEARTBEAT;
		sendMsp(s, &data, 1);
	}
	switch (state) {
	case MspDpState::CLEAR: {
		MspMsgSetup s = {
			.serial = serial,
			.fn = MspFn::MSP_DISPLAYPORT,
			.type = MspMsgType::RESPONSE,
			.version = serial.lastMspVersion,
		};
		char data = MSP_DP_CLEAR_SCREEN;
		sendMsp(s, &data, 1);
		drawingLine = 0;
		drawingChar = 0;
		state = MspDpState::WRITE;
	} break;
	case MspDpState::WRITE: {
		char data[34];
		data[0] = MSP_DP_WRITE_STRING;
		// fast forward drawingChar to the first valid position
		// valid means non-zero
		while (drawingChar < width) {
			if (frameBuffer[drawingLine * width + drawingChar] == '\0')
				drawingChar++;
			else
				break;
		}
		if (drawingChar < width) { // if we found some valid character in this line
			u8 max = drawingChar + 30;
			if (max > width) max = width;
			data[1] = drawingLine;
			data[2] = drawingChar;
			data[3] = 0b00; // select font with these two bits
			u8 pos = drawingChar + 1; // currently checking position
			u8 last = drawingChar; // last valid position
			for (u8 zeros = 0; zeros < 8 && pos < max; zeros++, pos++) {
				if (frameBuffer[drawingLine * width + pos] != '\0') {
					zeros = 0;
					last = pos;
				}
			}
			u8 len = last + 1 - drawingChar;
			memcpy(&data[4], &frameBuffer[drawingLine * width + drawingChar], len);
			MspMsgSetup s = {
				.serial = serial,
				.fn = MspFn::MSP_DISPLAYPORT,
				.type = MspMsgType::RESPONSE,
				.version = serial.lastMspVersion,
			};
			sendMsp(s, data, 4 + len);
			drawingChar = pos;
		}
		if (drawingChar >= width) {
			drawingChar = 0;
			if (++drawingLine >= height) {
				state = MspDpState::DRAW;
			}
		}
	} break;
	case MspDpState::DRAW: {
		MspMsgSetup s = {
			.serial = serial,
			.fn = MspFn::MSP_DISPLAYPORT,
			.type = MspMsgType::RESPONSE,
			.version = serial.lastMspVersion,
		};
		char data = MSP_DP_DRAW_SCREEN;
		sendMsp(s, &data, 1);
		state = MspDpState::IDLE;
	} break;
	case MspDpState::IDLE: {
		if (newFrame) {
			newFrame = false;
			// for CRSF we apply light throttling because it's probably WiFi
			if (isRegularMsp || sinceLastFrame > 120000) {
				state = MspDpState::CLEAR;
				sinceLastFrame = 0;
			}
		}
	} break;
	default:
		break;
	}
}

void MspOsdOutput::setSize(u8 width, u8 height) {
	if (width > 192 || height > 192) return;
	state = MspDpState::CLEAR;
	OsdCanvas::get().setSize(width, height, 3);
	OsdOutput::setSize(width, height);
	sizeSet = true;
}

void MspOsdOutput::propagateSize() {
	if (sizeSet) OsdCanvas::get().setSize(width, height, 3);
}
