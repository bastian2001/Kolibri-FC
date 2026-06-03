/**
 * @file analogOsdOutput.h
 * @brief AnalogOsdOutput class declaration, which handles OSD output to a MAX7456 chip
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
#include "osdOutput.h"

class AnalogOsdOutput : public OsdOutput {
public:
	AnalogOsdOutput(const AnalogOsdOutput &) = delete;
	AnalogOsdOutput &operator=(const AnalogOsdOutput &) = delete;
	[[gnu::const]] static AnalogOsdOutput &get() {
		static AnalogOsdOutput a;
		return a;
	}

	void begin();
	void loop();

	void updateCharacter(u8 cmAddr, u8 data[54]);

	void setSize(u8 width, u8 height) override;

	bool devDetected() { return chipDetected; }
	bool isPal() { return isPalOutput; }
	bool isNtsc() { return isNtscOutput; }

private:
	AnalogOsdOutput() {
		setSize(OSD_WIDTH_PAL_NTSC, OSD_HEIGHT_PAL);
	}

	void disableOutput();
	void enableOutput();

	bool chipDetected = false;
	bool hasVideoIn = false;
	bool isPalOutput = false;
	bool isNtscOutput = false;
	elapsedMicros checkTimer = 0;
	u16 drawingPos = 0;

	enum {
		REG_VM0,
		REG_VM1,
		REG_HOS,
		REG_VOS,
		REG_DMM,
		REG_DMAH,
		REG_DMAL,
		REG_DMDI,
		REG_CMM,
		REG_CMAH,
		REG_CMAL,
		REG_CMDI,
		REG_OSDM,
		REG_RB0 = 0x10,
		REG_OSDBL = 0x6C,
		REG_STAT = 0xA0,
		REG_DMDO = 0xB0,
		REG_CMDO = 0xC0,
	};
};
