/**
 * @file osdOutput.cpp
 * @brief Declaration of the OsdOutput base class, which defines the interface for OSD outputs and some common functionality
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

class OsdOutput {
public:
	OsdOutput() {};
	OsdOutput(const OsdOutput &) = delete;
	OsdOutput &operator=(const OsdOutput &) = delete;
	virtual ~OsdOutput() {
		if (frameBuffer != nullptr) free(frameBuffer);
	}

	virtual void sendFrame(char *frameBuffer, u8 width, u8 height);
	virtual void setSize(u8 width, u8 height);
	inline void getSize(u8 *width, u8 *height) const {
		*width = this->width;
		*height = this->height;
	}
	virtual void loop() = 0;

	virtual void disableOutput() = 0;
	virtual void enableOutput() = 0;

protected:
	u8 width = 30;
	u8 height = 16;
	bool newFrame = true;
	char *frameBuffer = (char *)malloc(30 * 16);
};
