/**
 * @file analogOsdOutput.cpp
 * @brief Implementation of the AnalogOsdOutput class, which handles OSD output to a MAX7456 chip
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

void AnalogOsdOutput::begin() {
	OsdCanvas::get().addOutput(this);
	spi_init(SPI_OSD, 8000000);

	spi_set_format(SPI_OSD, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
	gpio_set_function(PIN_OSD_MOSI, GPIO_FUNC_SPI);
	gpio_set_function(PIN_OSD_MISO, GPIO_FUNC_SPI);
	gpio_set_function(PIN_OSD_SCLK, GPIO_FUNC_SPI);
	gpio_init(PIN_OSD_CS);
	gpio_set_dir(PIN_OSD_CS, GPIO_OUT);
	gpio_put(PIN_OSD_CS, 1);
	sleep_ms(2);
}

void AnalogOsdOutput::loop() {
	if (isReady) {
		if (newFrame) {
			newFrame = false;
			drawingPos = 0;
			regWrite(SPI_OSD, PIN_OSD_CS, REG_DMAH, (u8[]){0});
			regWrite(SPI_OSD, PIN_OSD_CS, REG_DMAL, (u8[]){0});
			regWrite(SPI_OSD, PIN_OSD_CS, REG_DMM, (u8[]){0x01}); // autoincrement
		}
		if (drawingPos < pixelCount) {
			// I could rant about this forever, but yeah, we need one transfer per byte for auto-increment...
			// one could implement a bulk transfer, but that would require sending 16 bits per char (both in 8 bit and in 16 bit mode), wtf
			// Either way, we handle one byte at a time to keep each function call short
			spiSingleWrite(SPI_OSD, PIN_OSD_CS, frameBuffer[drawingPos]);
			drawingPos++;
		} else if (drawingPos == pixelCount) {
			spiSingleWrite(SPI_OSD, PIN_OSD_CS, 0xFF); // end auto-increment
			drawingPos++;
		}
	}
	if (checkTimer > 1000000 && isReady != 1) {
		u8 data = 0;
		checkTimer = 0;
		regRead(SPI_OSD, PIN_OSD_CS, REG_STAT, &data);
		if (data && !(data & 0b01100000)) {
			isReady = 2;
		}
		if (!isReady) return;
		if (data & 1) {
			u8 data2 = 0b01001100; // dont care, pal, autosync (2 bits), enable osd, sync at next vsync, don't reset, enable output
			setSize(30, 16);
			isPalOutput = true;
			regWrite(SPI_OSD, PIN_OSD_CS, REG_VM0, &data2);
		} else if (data & (1 << 1)) {
			u8 data2 = 0b00001100; // dont care, ntsc, autosync (2 bits), enable osd, sync at next vsync, don't reset, enable output
			setSize(30, 13);
			isNtscOutput = true;
			regWrite(SPI_OSD, PIN_OSD_CS, REG_VM0, &data2);
		}
		isReady = (data & 0b00000011) ? 1 : 2;
	}
}

void AnalogOsdOutput::disableOutput() {
	u8 data;
	regRead(SPI_OSD, PIN_OSD_CS, REG_VM0, &data, 1, 0);
	data &= ~0b00001000;
	regWrite(SPI_OSD, PIN_OSD_CS, REG_VM0, &data);
}

void AnalogOsdOutput::enableOutput() {
	u8 data;
	regRead(SPI_OSD, PIN_OSD_CS, REG_VM0, &data, 1, 0);
	data |= 0b00001000;
	regWrite(SPI_OSD, PIN_OSD_CS, REG_VM0, &data);
}

void AnalogOsdOutput::updateCharacter(u8 cmAddr, u8 data[54]) {
	disableOutput();
	sleep_us(100);
	regWrite(SPI_OSD, PIN_OSD_CS, REG_CMAH, &cmAddr);
	for (int i = 0; i < 54; i++) {
		regWrite(SPI_OSD, PIN_OSD_CS, REG_CMAL, (u8 *)&i);
		regWrite(SPI_OSD, PIN_OSD_CS, REG_CMDI, &data[i]);
	}
	u8 scratch = 0b10100000;
	regWrite(SPI_OSD, PIN_OSD_CS, REG_CMM, &scratch);
	sleep_ms(15);
	enableOutput();
}

void AnalogOsdOutput::setSize(u8 width, u8 height) {
	drawingPos = 0;
	OsdCanvas::get().setSize(width, height, 0);
	OsdOutput::setSize(width, height);
}
