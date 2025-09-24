#pragma once

#include "utils/bufferedWriter.h"
#include <Arduino.h>

//! when updating this list, also update SERIAL_FUNCTION_COUNT and serialFunctionNames
#define SERIAL_CRSF (1 << 0)
#define SERIAL_MSP (1 << 1)
#define SERIAL_GPS (1 << 2)
#define SERIAL_4WAY (1 << 3)
#define SERIAL_IRC_TRAMP (1 << 4)
#define SERIAL_SMARTAUDIO (1 << 5)
#define SERIAL_ESC_TELEM (1 << 6)
#define SERIAL_MSP_DISPLAYPORT (1 << 7)

#define SERIAL_COUNT 5
#define SERIAL_FUNCTION_COUNT 8

extern char serialFunctionNames[SERIAL_FUNCTION_COUNT][20];

typedef struct koliSerial {
	BufferedWriter *stream;
	u32 functions = 0; // OR of SERIAL_ defines, e.g. SERIAL_MSP
} KoliSerial;

// 0 = Serial (USB CDC), 1 = Serial1 = UART0, 2 = Serial2 = UART1, 3 = Software Serial
extern KoliSerial serials[SERIAL_COUNT];

extern u32 crcLutD5[256]; // u32 is used because it is faster than u8
#define CRC_LUT_D5_APPLY(crc, data) crc = crcLutD5[(crc) ^ (u8)(data)]

/// @brief fills crcLutD5
void initSerial();

/// @brief reads the serial port and sends it to the appropriate handler
void serialLoop();
