#pragma once

#include <Arduino.h>

#define SERIAL_DISABLED 1 << 0
#define SERIAL_ELRS 1 << 1
#define SERIAL_GPS 1 << 2
#define SERIAL_IRC_TRAMP 1 << 3
#define SERIAL_SMARTAUDIO 1 << 4
#define SERIAL_ESC_TELEM 1 << 5
#define SERIAL_MSP 1 << 6

/// @brief reads the serial port and sends it to the appropriate handler
void serialLoop();