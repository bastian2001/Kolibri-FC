/**
 * @file global.h
 * @brief Global includes, defines, variables and helper functions
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

// general C/C++ includes
#include <deque>
#include <list>
#include <string>

// architecture
#include <arm_acle.h>

// Pico-SDK
#include <hardware/adc.h>
#include <hardware/dma.h>
#include <hardware/i2c.h>
#include <hardware/pio.h>
#include <hardware/pwm.h>
#include <hardware/resets.h>
#include <hardware/spi.h>
#include <hardware/watchdog.h>
#include <pico/mutex.h>
#include <pico/stdlib.h>

// Arduino
#ifdef USE_TINYUSB
#include <Adafruit_TinyUSB.h>
#endif
#include <Arduino.h>
#include <LittleFS.h>
#if BLACKBOX_STORAGE == SD_BB
#include <SdFat.h>
#endif

// Libraries
#include <NeoPixelConnect.h>
#include <PIO_DShot.h>
#include <elapsedMillis.h>
#include <fixedPointInt.h>

// Kolibri stuff
#include "adc.h"
#include "blackbox.h"
#include "control.h"
#include "customSimdMath.h"
#include "drivers/baro.h"
#include "drivers/esc.h"
#include "drivers/flashBb.h"
#include "drivers/gyro.h"
#include "drivers/halfduplexUart.h"
#include "drivers/i2c.h"
#include "drivers/mag.h"
#include "drivers/pioUart.h"
#include "drivers/speaker.h"
#include "drivers/spi.h"
#include "imu.h"
#include "inFlightTuning.h"
#include "modes.h"
#include "osd/analogOsdOutput.h"
#include "osd/mspOsdOutput.h"
#include "osd/osdCanvas.h"
#include "osd/osdOutput.h"
#include "pid.h"
#include "pins.h"
#include "pioasm/halfduplex_spi.pio.h"
#include "pioasm/halfduplex_uart.pio.h"
#include "pioasm/uart_rx.pio.h"
#include "pioasm/uart_tx.pio.h"
#include "ringbuffer.h"
#include "rtc.h"
#include "serial.h"
#include "serialhandler/4way.h"
#include "serialhandler/cli/cli.h"
#include "serialhandler/elrs.h"
#include "serialhandler/gps.h"
#include "serialhandler/msp.h"
#include "serialhandler/tramp.h"
#include "settings/arraySetting.h"
#include "settings/littleFs.h"
#include "settings/setting.h"
#include "settings/settingIds.h"
#include "targets.h"
#include "taskManager.h"
#include "typedefs.h"
#include "unittest.h"
#include "utils/filters.h"
#include "utils/koliSerial.h"
#include "utils/quaternion.h"

#ifdef BARO_SPL06
#define SPI_BARO spi0 // SPI for baro
#endif

#if HW_BARO == BARO_LPS22
#define I2C_BARO i2c0 // I2C for baro
#endif

#define PROPS_OUT

#define ARRAYLEN(arr) (sizeof(arr) / sizeof(arr[0])) // Get the length of an array

extern std::optional<ExpressLRS> elrs; // global ELRS instance
#define DECODE_U2(buf) ((*(buf) & 0xFF) + ((u16)(*((u8 *)(buf) + 1)) << 8)) // Decode 2 bytes from a buffer into a 16-bit unsigned integer
#define DECODE_I2(buf) ((*(buf) & 0xFF) + ((i16)(*((u8 *)(buf) + 1)) << 8)) // Decode 2 bytes from a buffer into a 16-bit signed integer
// Decode 4 bytes from a buffer into a 32-bit unsigned integer
static inline u32 DECODE_U4(const u8 *buf) {
	u32 result;
	memcpy(&result, buf, 4); // memcpy needed because of 4-byte-alignment
	return result;
}
// Decode 4 bytes from a buffer into a 32-bit signed integer
static inline i32 DECODE_I4(const u8 *buf) {
	i32 result;
	memcpy(&result, buf, 4);
	return result;
}
// Decode 4 bytes from a buffer into a 32-bit float
static inline f32 DECODE_R4(const u8 *buf) {
	f32 result;
	memcpy(&result, buf, 4);
	return result;
}
// Decode 8 bytes from a buffer into a 64-bit signed integer
static inline i64 DECODE_I8(const u8 *buf) {
	i64 result;
	memcpy(&result, buf, 8);
	return result;
}
// Decode 8 bytes from a buffer into a 64-bit float / double
static inline f64 DECODE_R8(const u8 *buf) {
	f64 result;
	memcpy(&result, buf, 8);
	return result;
}
bool parseInt(const char *str, i64 &value); // Parse an integer from a string, return true if successful
bool parseFloat(const char *str, f64 &value); // Parse a float from a string, return true if successful

enum MspRebootMode {
	MSP_REBOOT_FIRMWARE = 0,
	MSP_REBOOT_BOOTLOADER_ROM,
	MSP_REBOOT_MSC,
	MSP_REBOOT_MSC_UTC,
	MSP_REBOOT_BOOTLOADER_FLASH
};

extern NeoPixelConnect p;
extern std::string uavName;

#define FIRMWARE_NAME "Kolibri"
#define FIRMWARE_VERSION_MAJOR 0
#define FIRMWARE_VERSION_MINOR 1
#define FIRMWARE_VERSION_PATCH 0
#define RELEASE_SUFFIX "-dev"
#define xstr(a) str(a)
#define str(a) #a
#define FIRMWARE_VERSION_STRING xstr(FIRMWARE_VERSION_MAJOR) "." xstr(FIRMWARE_VERSION_MINOR) "." xstr(FIRMWARE_VERSION_PATCH) RELEASE_SUFFIX

#ifdef PRINT_DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x)                             \
	Serial.printf("%15s:%3d: ", __FILE__, __LINE__); \
	Serial.println(x);
#define DEBUG_PRINTF(x, ...)                         \
	Serial.printf("%15s:%3d: ", __FILE__, __LINE__); \
	Serial.printf(x, __VA_ARGS__)
#endif
#ifndef PRINT_DEBUG
#define DEBUG_PRINT(x)
#define DEBUG_PRINTF(x, ...)
#endif

enum class MOTOR : u8 {
	RR = 0,
	FR,
	RL,
	FL,
};
