/**
 * @file koliSerial.cpp
 * @brief Partial implementation of KoliSerial
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

char KoliSerial::serialTypeNames[4][8] = {
	"USB    ",
	"UART   ",
	"PIO    ",
	"PIO HDx",
};
elapsedMicros KoliSerial::sinceReset = 0;

void KoliSerial::begin(unsigned long baudrate) {
	return begin(baudrate, SERIAL_8N1);
}
void KoliSerial::begin(unsigned long baudrate, uint16_t config) {
	this->baudrate = baudrate;
	switch (serialType) {
	case SerialType::USB:
		static_cast<UsbSerialClass *>(stream)->begin(baudrate, config);
		break;
	case SerialType::UART:
		static_cast<SerialUART *>(stream)->begin(baudrate, config);
		break;
	case SerialType::PIO:
		static_cast<SerialPio *>(stream)->begin(baudrate, config);
		break;
	case SerialType::PIO_HDX:
		static_cast<SerialPioHdx *>(stream)->begin(baudrate, config); // config unused
		break;
	}
}
void KoliSerial::end() {
	switch (serialType) {
	case SerialType::USB:
		static_cast<UsbSerialClass *>(stream)->end();
		break;
	case SerialType::UART:
		static_cast<SerialUART *>(stream)->end();
		break;
	case SerialType::PIO:
		static_cast<SerialPio *>(stream)->end();
		break;
	case SerialType::PIO_HDX:
		static_cast<SerialPioHdx *>(stream)->end();
		break;
	}
}

KoliSerial::operator bool() {
	switch (serialType) {
	case SerialType::USB:
		return (bool)(*static_cast<UsbSerialClass *>(stream));
	case SerialType::UART:
		return (bool)(*static_cast<SerialUART *>(stream));
	case SerialType::PIO:
		return (bool)(*static_cast<SerialPio *>(stream));
	case SerialType::PIO_HDX:
		return (bool)(*static_cast<SerialPioHdx *>(stream));
	}
	return false;
}

KoliSerial::~KoliSerial() {
	end();

	switch (serialType) {
	case SerialType::USB:
	case SerialType::UART:
		// The USB and UART streams are not owned by this instance (pointer transferred to constructor)
		break;
	case SerialType::PIO:
		delete static_cast<SerialPio *>(stream);
		break;
	case SerialType::PIO_HDX:
		delete static_cast<SerialPioHdx *>(stream);
		break;
	}
}

bool KoliSerial::setRxFifoSize(size_t size) {
	switch (serialType) {
	case SerialType::USB:
		// FIFO size is set at compile time in TinyUSB config
		return false;
	case SerialType::UART:
		return static_cast<SerialUART *>(stream)->setFIFOSize(size);
	case SerialType::PIO:
		return static_cast<SerialPio *>(stream)->setFIFOSize(size);
	case SerialType::PIO_HDX:
		return static_cast<SerialPioHdx *>(stream)->setFIFOSize(size);
	}
	return false;
}

bool KoliSerial::setPinout(pin_size_t tx, pin_size_t rx) {
	txPin = tx;
	rxPin = rx;
	switch (serialType) {
	case SerialType::USB:
		// cannot set pinout on USB interface
		return false;
	case SerialType::UART:
		return static_cast<SerialUART *>(stream)->setPinout(tx, rx);
	case SerialType::PIO:
		return static_cast<SerialPio *>(stream)->setPinout(tx, rx);
	case SerialType::PIO_HDX:
		// halfduplex => use "tx" pin for all IO
		rxPin = tx;
		return static_cast<SerialPioHdx *>(stream)->setPinout(tx, tx);
	}
	return false;
}
