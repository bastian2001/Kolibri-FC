#include "global.h"

u8 readChar		   = 0;
Stream *serials[3] = {
	&Serial,
	&Serial1,
	&Serial2};

// 0 = serial, 1 = serial1, 2 = serial2
u32 serialFunctions[3] = {
	SERIAL_CONFIGURATOR,
	SERIAL_ELRS,
	SERIAL_GPS};

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
			if (serialFunctions[i] & SERIAL_CONFIGURATOR) {
				rp2040.wdt_reset();
				elapsedMicros timer = 0;
				configuratorHandleByte(readChar, i % 3);
				taskTimer -= timer;
			}
			if (serialFunctions[i] & SERIAL_ELRS) {
				if (elrsBuffer.itemCount() < 260)
					elrsBuffer.push(readChar);
			}
			if (serialFunctions[i] & SERIAL_ESC_TELEM) {
			}
			if (serialFunctions[i] & SERIAL_GPS) {
				if (gpsBuffer.itemCount() < GPS_BUF_LEN)
					gpsBuffer.push(readChar);
			}
			if (serialFunctions[i] & SERIAL_IRC_TRAMP) {
			}
			if (serialFunctions[i] & SERIAL_MSP) {
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