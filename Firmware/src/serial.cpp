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

	Serial3.begin(115200);
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
	static elapsedMillis hearbeatTimer = 0;
	static u8 c = 'Z' + 6;

	if (Serial.available()) {
		char c = Serial.read();
		char data[30];
		switch (c) {
		case '1':
			data[0] = MspDpFn::WRITE_STRING;
			data[1] = 5; // x
			data[2] = 5; // y
			data[3] = 2;
			data[4] = 'T';
			data[5] = 'H';
			data[6] = 'E';
			data[7] = 'R';
			data[8] = 'N';
			data[9] = 'U';
			data[10] = 0;
			sendMsp(3, MspMsgType::RESPONSE, MspFn::MSP_DISPLAYPORT, MspVersion::V1, data, 11);
			break;
		case '2':
			data[0] = MspDpFn::CLEAR_SCREEN;
			sendMsp(3, MspMsgType::RESPONSE, MspFn::MSP_DISPLAYPORT, MspVersion::V1, data, 1);
			break;
		case '3':
			data[0] = MspDpFn::OPTIONS;
			data[1] = 1;
			data[2] = 0;
			sendMsp(3, MspMsgType::RESPONSE, MspFn::MSP_DISPLAYPORT, MspVersion::V1, data, 2);
			break;
		case '4':
			data[0] = MspDpFn::OPTIONS;
			data[1] = 1;
			data[2] = 1;
			sendMsp(3, MspMsgType::RESPONSE, MspFn::MSP_DISPLAYPORT, MspVersion::V1, data, 2);
			break;
		case '5':
			data[0] = MspDpFn::OPTIONS;
			data[1] = 1;
			data[2] = 2;
			sendMsp(3, MspMsgType::RESPONSE, MspFn::MSP_DISPLAYPORT, MspVersion::V1, data, 2);
			break;
		case '6':
			data[0] = MspDpFn::OPTIONS;
			data[1] = 1;
			data[2] = 3;
			sendMsp(3, MspMsgType::RESPONSE, MspFn::MSP_DISPLAYPORT, MspVersion::V1, data, 2);
			break;
		case '7':
			data[0] = MspDpFn::RELEASE;
			sendMsp(3, MspMsgType::RESPONSE, MspFn::MSP_DISPLAYPORT, MspVersion::V1, data, 1);
			break;
		}
	}
	if (hearbeatTimer >= 50) {
		hearbeatTimer = 0;
		char dummy = MspDpFn::HEARTBEAT;
		sendMsp(3, MspMsgType::RESPONSE, MspFn::MSP_DISPLAYPORT, MspVersion::V1, &dummy, 1);
		char data[30];
		data[0] = MspDpFn::WRITE_STRING;
		data[1] = 4;
		data[2] = 4;
		data[3] = 1 | (1 << 6);
		data[4] = 'H';
		data[5] = 'E';
		data[6] = 'L';
		data[7] = 'L';
		data[8] = 'O';
		data[9] = ' ';
		data[10] = 'K';
		data[11] = 'O';
		data[12] = 'L';
		data[13] = 'I';
		data[14] = 'B';
		data[15] = 'R';
		data[16] = 'I';
		data[17] = '!';
		data[18] = c;
		data[19] = 0; // end of string
		sendMsp(3, MspMsgType::RESPONSE, MspFn::MSP_DISPLAYPORT, MspVersion::V1, data, 20);
		if (++c == 'Z' + 22) c = 'Z' + 6;
		data[0] = MspDpFn::DRAW_SCREEN;
		sendMsp(3, MspMsgType::RESPONSE, MspFn::MSP_DISPLAYPORT, MspVersion::V1, data, 1);
		data[0] = MspDpFn::SYS;
		data[1] = 2;
		data[2] = 3;
		data[3] = 1;
		sendMsp(3, MspMsgType::RESPONSE, MspFn::MSP_DISPLAYPORT, MspVersion::V1, data, 4);
		Serial.print(".");
	}
	elapsedMicros taskTimer = 0;
	tasks[TASK_SERIAL].runCounter++;
	for (int i = 1; i < SERIAL_COUNT; i++) { // TODO: set back to 0
		if (serialFunctions[i] & SERIAL_DISABLED)
			continue;
		Stream *serial = serials[i];
		while (serial->available()) {
			u8 readChar = serial->read();
			// if (i == 3)
			// 	Serial.printf("%02X ", readChar); //TODO remove printf
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
