#include "global.h"

u8 readChar        = 0;
u32 crcLutD5[256]  = {0};
Stream *serials[3] = {
	&Serial,
	&Serial1,
	&Serial2};

// 0 = serial, 1 = serial1, 2 = serial2
u32 serialFunctions[3] = {
	SERIAL_MSP,
	SERIAL_ELRS,
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
	elapsedMicros taskTimer = 0;
	tasks[TASK_SERIAL].runCounter++;
	for (int i = 0; i < 3; i++) {
		if (serialFunctions[i] & SERIAL_DISABLED)
			continue;
		Stream *serial = serials[i];
		int available  = serial->available();
		for (int j = 0; j < available; j++) {
			readChar = serial->read();
			if (serialFunctions[i] & SERIAL_MSP) {
				rp2040.wdt_reset();
				elapsedMicros timer = 0;
				mspHandleByte(readChar, i % 3);
				taskTimer -= timer;
			}
			if (serialFunctions[i] & SERIAL_ELRS) {
				if (!elrsBuffer.isFull())
					elrsBuffer.push(readChar);
			}
			if (serialFunctions[i] & SERIAL_ESC_TELEM) {
			}
			if (serialFunctions[i] & SERIAL_GPS) {
				if (!gpsBuffer.isFull())
					gpsBuffer.push(readChar);
			}
			if (serialFunctions[i] & SERIAL_IRC_TRAMP) {
			}
			if (serialFunctions[i] & SERIAL_SMARTAUDIO) {
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