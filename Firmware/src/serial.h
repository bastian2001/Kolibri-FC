/**
 * @file serial.h
 * @brief Serial functions, variables and constants
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

#include "utils/koliSerial.h"
#include <Arduino.h>
#include <optional>

//! when updating this list, also update SERIAL_FUNCTION_COUNT and serialFunctionNames
#define SERIAL_CRSF (1 << 0)
#define SERIAL_MSP (1 << 1)
#define SERIAL_GPS (1 << 2)
#define SERIAL_4WAY_HOST (1 << 3)
#define SERIAL_IRC_TRAMP (1 << 4)
#define SERIAL_SMARTAUDIO (1 << 5)
#define SERIAL_ESC_TELEM (1 << 6)
#define SERIAL_MSP_DISPLAYPORT (1 << 7)

#define SERIAL_COUNT 5
#define SERIAL_FUNCTION_COUNT 8

typedef struct serialConfig {
	SerialType type = SerialType::DISABLED;
	u8 hwParam = 255; // pio number, serial number
	pin_size_t txPin = 255;
	pin_size_t rxPin = 255;
	u32 baud = 0;
	u32 functions = 0;
	u8 mspDpSettings = 0;
} SerialConfig;

void revertSerials();
void stopSerials();
bool startSerials(SerialConfig newCfgs[SERIAL_COUNT - 1]);
const SerialConfig &getSerialConfig(int i);
extern const char serialFunctionNames[SERIAL_FUNCTION_COUNT][20];
u32 getFreeInstructions(int pioNum);
u8 getFreeSms(int pioNum);

// 0 = Serial (USB CDC), 1 = Serial1 = UART0, 2 = Serial2 = UART1, 3 = Software Serial
extern std::optional<KoliSerial> serials[SERIAL_COUNT];

extern u32 crcLutD5[256]; // u32 is used because it is faster than u8
#define CRC_LUT_D5_APPLY(crc, data) crc = crcLutD5[(crc) ^ (u8)(data)]

/// @brief fills crcLutD5
void initSerial();

/// @brief reads the serial port and sends it to the appropriate handler
void serialLoop();
