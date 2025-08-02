#include "global.h"

u8 readChar = 0;
u32 crcLutD5[256] = {0};

KoliSerial serials[3] = {
	{&Serial, SERIAL_MSP},
	{&Serial1, SERIAL_CRSF},
	{&Serial2, SERIAL_GPS},
};

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
	elapsedMicros taskTimer = 0;
	tasks[TASK_SERIAL].runCounter++;
	for (int i = 0; i < 3; i++) {
		u32 functions = serials[i].functions;
		if (functions & SERIAL_DISABLED)
			continue;
		Stream *serial = serials[i].stream;
		int available = serial->available();
		for (int j = 0; j < available; j++) {
			readChar = serial->read();
			if (functions & SERIAL_CRSF) {
				if (!elrsBuffer.isFull())
					elrsBuffer.push(readChar);
			}
			if (functions & SERIAL_MSP) {
				rp2040.wdt_reset();
				elapsedMicros timer = 0;
				mspHandleByte(readChar, i);
				taskTimer -= timer;
			}
			if (functions & SERIAL_GPS) {
				if (!gpsBuffer.isFull())
					gpsBuffer.push(readChar);
			}
			if (functions & SERIAL_4WAY) {
				process4Way(readChar);
			}
			if (functions & SERIAL_IRC_TRAMP) {
			}
			if (functions & SERIAL_SMARTAUDIO) {
			}
			if (functions & SERIAL_ESC_TELEM) {
			}
		}
	}
	u32 duration = taskTimer;
	tasks[TASK_SERIAL].totalDuration += duration;
	if (duration < tasks[TASK_SERIAL].minDuration) {
		tasks[TASK_SERIAL].minDuration = duration;
	}
	if (duration > tasks[TASK_SERIAL].maxDuration) {
		tasks[TASK_SERIAL].maxDuration = duration;
	}
}
