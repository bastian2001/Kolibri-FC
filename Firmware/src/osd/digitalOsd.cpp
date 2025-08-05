#include "global.h"

DigitalOsd digitalOsd;

void DigitalOsd::init() {
	u8 i = 0;
	for (u32 serialFn : serialFunctions) {
		if (serialFn == SERIAL_MSP_DISPLAYPORT) {
			this->mspSerialId = i;
			break;
		}
		i++;
	}
}

void DigitalOsd::write(u8 row, u8 col, const char *str, bool blinking) {
	// // TODO write to Digital OSD
	// char *data = new char[30];
	// u8 row = 0;
	// u8 column = 0;
	// u32 ready = (u32)serials[MspDpSerial]->availableForWrite();
	// if (ready > 33) ready = 33;
	// if (ready <= 3) return;
	// ready -= 3; //
	// // frameBuffer->nextBytes(ready, data, &row, &column);
	// dpWriteString(row, column, 0x00, data); // 0x00 = no blink (done by FC)

	dpWriteString(row, col, 1 << 6, str);
}

void DigitalOsd::draw() {
	sendMspDp(MspDpFn::HEARTBEAT);
	sendMspDp(MspDpFn::DRAW_SCREEN);
}
