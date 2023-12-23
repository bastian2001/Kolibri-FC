#include "global.h"

uint8_t readChar   = 0;
Stream *serials[3] = {
	&Serial,
	&Serial1,
	&Serial2};

// 0 = serial, 1 = serial1, 2 = serial2
uint32_t serialFunctions[3] = {
	SERIAL_CONFIGURATOR,
	SERIAL_GPS,
	SERIAL_ELRS};

void serialLoop() {
	for (int i = 0; i < 3; i++) {
		if (serialFunctions[i % 3] & SERIAL_DISABLED)
			continue;
		Stream *serial = serials[i % 3];
		int available  = serial->available();
		if (available <= 0)
			continue;
		for (int j = 0; j < available; j++) {
			readChar = serial->read();
			if (serialFunctions[i % 3] & SERIAL_CONFIGURATOR) {
				configuratorHandleByte(readChar, i % 3);
			}
			if (serialFunctions[i % 3] & SERIAL_ELRS) {
				if (elrsBuffer.size() < 260)
					elrsBuffer.push_back(readChar);
			}
			if (serialFunctions[i % 3] & SERIAL_ESC_TELEM) {
			}
			if (serialFunctions[i % 3] & SERIAL_GPS) {
				if (gpsBuffer.size() < GPS_BUF_LEN)
					gpsBuffer.push_back(readChar);
			}
			if (serialFunctions[i % 3] & SERIAL_IRC_TRAMP) {
			}
			if (serialFunctions[i % 3] & SERIAL_MSP) {
			}
			if (serialFunctions[i % 3] & SERIAL_SMARTAUDIO) {
			}
		}
	}
}