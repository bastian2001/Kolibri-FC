#include "global.h"

u8 readChar = 0;
u32 crcLutD5[256] = {0};
Stream *serials[3] = {
	&Serial,
	&Serial1,
	&Serial2};

u32 serialFunctions[3] = {
	SERIAL_MSP,
	SERIAL_CRSF,
	SERIAL_GPS};

void initSerial() {
	for (u32 i = 0; i < 256; i++) {
		u32 crc = i;
		for (u32 j = 0; j < 8; j++) {
			if (crc & 0x80)
				crc = (crc << 1) ^ 0xD5;
			else
				crc <<= 1;
		}
		crcLutD5[i] = crc & 0xFF;
	}
}

void serialLoop() {
	TASK_START(TASK_SERIAL);
	for (int i = 0; i < 3; i++) {
		if (serialFunctions[i] & SERIAL_DISABLED)
			continue;
		Stream *serial = serials[i];
		int available = serial->available();
		for (int j = 0; j < available; j++) {
			readChar = serial->read();
			if (serialFunctions[i] & SERIAL_CRSF) {
				if (!elrsBuffer.isFull())
					elrsBuffer.push(readChar);
			}
			if (serialFunctions[i] & SERIAL_MSP) {
				rp2040.wdt_reset();
				elapsedMicros timer = 0;
				mspHandleByte(readChar, i);
				taskTimerTASK_SERIAL -= timer;
			}
			if (serialFunctions[i] & SERIAL_GPS) {
				if (!gpsBuffer.isFull())
					gpsBuffer.push(readChar);
			}
			if (serialFunctions[i] & SERIAL_4WAY) {
				process4Way(readChar);
			}
			if (serialFunctions[i] & SERIAL_IRC_TRAMP) {
			}
			if (serialFunctions[i] & SERIAL_SMARTAUDIO) {
			}
			if (serialFunctions[i] & SERIAL_ESC_TELEM) {
			}
		}
	}
	TASK_END(TASK_SERIAL);
}
