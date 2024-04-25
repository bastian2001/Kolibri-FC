#pragma once

#include <Arduino.h>

#define SERIAL_DISABLED 1 << 0
#define SERIAL_CRSF 1 << 1
#define SERIAL_MSP 1 << 2
#define SERIAL_GPS 1 << 3
#define SERIAL_4WAY 1 << 4
#define SERIAL_IRC_TRAMP 1 << 5
#define SERIAL_SMARTAUDIO 1 << 6
#define SERIAL_ESC_TELEM 1 << 7

// 0 = Serial (USB CDC), 1 = Serial1 = UART0, 2 = Serial2 = UART1
extern u32 serialFunctions[3];

extern u32 crcLutD5[256]; // u32 is used because it is faster than u8
#define CRC_LUT_D5_APPLY(crc, data) crc = crcLutD5[(crc ^ data) & 0xFF]

/// @brief fills crcLutD5
void initSerial();

/// @brief reads the serial port and sends it to the appropriate handler
void serialLoop();