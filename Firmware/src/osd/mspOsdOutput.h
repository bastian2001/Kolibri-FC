/**
 * @file mspOsdOutput.h
 * @brief MspOsdOutput class declaration, a digital OSD output via MSP Displayport
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

#pragma once
#include "osdCanvas.h"
#include "osdOutput.h"

#define MSP_DP_DEFAULT_WIDTH 50
#define MSP_DP_DEFAULT_HEIGHT 23

class MspOsdOutput : public OsdOutput {
public:
	MspOsdOutput(KoliSerial &serial) : serial(serial) {
		setSize(MSP_DP_DEFAULT_WIDTH, MSP_DP_DEFAULT_HEIGHT);
		sizeSet = false;
		OsdCanvas::get().addOutput(this);
	}
	~MspOsdOutput() {
		OsdCanvas::get().removeOutput(this);
	}
	MspOsdOutput(const MspOsdOutput &) = delete;
	MspOsdOutput &operator=(const MspOsdOutput &) = delete;
	void loop();
	void setSize(u8 width, u8 height) override;
	void propagateSize();

private:
	enum class MspDpState {
		CLEAR,
		WRITE,
		DRAW,
		IDLE,
		DISABLED,
	};
	enum MspDpSubcmd {
		MSP_DP_HEARTBEAT = 0,
		MSP_DP_RELEASE = 1,
		MSP_DP_CLEAR_SCREEN = 2,
		MSP_DP_WRITE_STRING = 3,
		MSP_DP_DRAW_SCREEN = 4,
		MSP_DP_OPTIONS = 5,
		MSP_DP_SYS = 6,
	};
	MspDpState state = MspDpState::CLEAR;
	KoliSerial &serial;
	u8 drawingLine = 0;
	u8 drawingChar = 0;
	bool sizeSet = false;
	elapsedMicros heartbeatTimer = 0;
	elapsedMicros sinceLastFrame = 0;
	void disableOutput() { state = MspDpState::DISABLED; }
	void enableOutput() { state = MspDpState::CLEAR; }
};
