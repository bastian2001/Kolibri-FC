/**
 * @file osdOutput.cpp
 * @brief Implementation of the OsdOutput base class, which defines the interface for OSD outputs and some common functionality
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

void OsdOutput::sendFrame(char *frameBuffer, u8 width, u8 height) {
	newFrame = true;
	if (width == this->width && height == this->height) {
		memcpy(this->frameBuffer, frameBuffer, width * height);
	} else if (width == this->width) {
		u8 minHeight = MIN(height, this->height);
		memcpy(this->frameBuffer, frameBuffer, width * minHeight);
		if (height < this->height) memset(&frameBuffer[height * width], 0, (this->height - height) * width);
	} else {
		u8 minWidth = MIN(width, this->width);
		u8 minHeight = MIN(height, this->height);
		memset(this->frameBuffer, 0, this->height * this->width);
		for (int row = 0; row < minHeight; row++) {
			memcpy(&this->frameBuffer[row * this->width], &frameBuffer[row * width], minWidth);
		}
	}
}

void OsdOutput::setSize(u8 width, u8 height) {
	void *fb = malloc(width * height);
	if (fb == nullptr) return;
	free(frameBuffer);
	frameBuffer = (char *)fb;
	this->width = width;
	this->height = height;
	newFrame = true;
}
