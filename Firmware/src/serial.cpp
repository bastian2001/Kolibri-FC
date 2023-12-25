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
	crashInfo[11] = 1;
	for (int i = 0; i < 3; i++) {
		crashInfo[11] = 2;
		crashInfo[12] = i;
		if (serialFunctions[i % 3] & SERIAL_DISABLED)
			continue;
		crashInfo[11]	  = 3;
		Stream *serial	  = serials[i % 3];
		crashInfo[11]	  = 33;
		int available	  = serial->available();
		crashInfo[19 + i] = available;
		crashInfo[11]	  = 4;
		crashInfo[13]	  = available;
		if (available <= 0)
			continue;
		crashInfo[11] = 5;
		for (int j = 0; j < available; j++) {
			crashInfo[11] = 6;
			crashInfo[14] = j;
			readChar	  = serial->read();
			crashInfo[11] = 7;
			if (serialFunctions[i % 3] & SERIAL_CONFIGURATOR) {
				crashInfo[11] = 8;
				rp2040.wdt_reset();
				configuratorHandleByte(readChar, i % 3);
				crashInfo[11] = 9;
			}
			if (serialFunctions[i % 3] & SERIAL_ELRS) {
				crashInfo[11] = 10;
				if (elrsBuffer.itemCount() < 260)
					elrsBuffer.push(readChar);
				crashInfo[11] = 11;
			}
			if (serialFunctions[i % 3] & SERIAL_ESC_TELEM) {
			}
			if (serialFunctions[i % 3] & SERIAL_GPS) {
				crashInfo[11] = 12;
				if (gpsBuffer.itemCount() < GPS_BUF_LEN)
					gpsBuffer.push(readChar);
				crashInfo[11] = 13;
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