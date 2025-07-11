#include "global.h"

SerialPIO Serial3(PIN_TX2, PIN_RX2, 1024);

u32 crcLutD5[256] = {0};
Stream *serials[SERIAL_COUNT] = {
	&Serial,
	&Serial1,
	&Serial2,
	&Serial3};

u32 serialFunctions[SERIAL_COUNT] = {
	SERIAL_MSP,
	SERIAL_CRSF,
	SERIAL_GPS,
	SERIAL_MSP | SERIAL_MSP_DISPLAYPORT};

MspParser *mspParsers[SERIAL_COUNT];

void initSerial() {
	for (int i = 0; i < SERIAL_COUNT; i++) {
		if (serialFunctions[i] & SERIAL_MSP) {
			mspParsers[i] = new MspParser;
		} else {
			mspParsers[i] = nullptr;
		}
	}
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
	for (int i = 0; i < SERIAL_COUNT; i++) {
		if (serialFunctions[i] & SERIAL_DISABLED)
			continue;
		Stream *serial = serials[i];
		while (serial->available()) {
			u8 readChar = serial->read();
			if (serialFunctions[i] & SERIAL_CRSF) {
				if (!elrsBuffer.isFull())
					elrsBuffer.push(readChar);
			}
			if (serialFunctions[i] & SERIAL_MSP) {
				rp2040.wdt_reset();
				elapsedMicros timer = 0;
				if (mspParsers[i] != nullptr) mspParsers[i]->mspHandleByte(readChar, i);
				taskTimer -= timer;
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
			if (serialFunctions[i] & SERIAL_MSP_DISPLAYPORT) {
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
