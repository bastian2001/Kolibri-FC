/**
 * @file halfduplexUart.h
 * @brief Interface and partial implementation of SerialPioHdx class, a pio-based half-duplex UART implentation
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
#include "Arduino.h"
#include "typedefs.h"

typedef struct serialPioHdxConfig {
	i8 pio;
} SerialPioHdxConfig;

class SerialPioHdx : public HardwareSerial {
public:
	SerialPioHdx() = delete;
	SerialPioHdx(PIO pio, i8 sm = -1);
	~SerialPioHdx();

	// from Stream
	virtual int available() override;
	int read();
	int peek();

	// from Print
	virtual size_t write(uint8_t c) override;
	virtual size_t write(const uint8_t *buffer, size_t size) override;
	void flush();
	virtual int availableForWrite() override;

	// Serial-alike
	virtual void begin(unsigned long baudrate) override;
	/**
	 * @brief Same as begin(baudrate), config is UNUSED!
	 *
	 * @param baudrate
	 * @param _config
	 */
	virtual void begin(unsigned long baudrate, uint16_t _config) override;
	virtual void end() override;

	bool setPinout(u8 pin, u8 _);
	bool setFIFOSize(size_t size);

	operator bool() override;

private:
	void begin();
	u8 pin = 255;
	u32 baudrate = 0;
	PIO pio = nullptr;
	i8 beginSm = -1;
	i8 sm = -1;
	bool running = false;
	u8 pioIndex = 255;
	SerialPioHdxConfig pioConfig;
	size_t rxBufSize = 0;
	u8 *rxBuf = nullptr;
	u8 rxDmaChan = 255;
	u8 *rxPtr = nullptr;
};
