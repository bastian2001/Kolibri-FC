#include <osd/mspDisplayport.h>

void sendMspDp(u8 subcmd, const char *payload, u8 payloadLength) {
	if (payload == nullptr && payloadLength > 0) return;
	for (int i = 0; i < SERIAL_COUNT; i++) {
		if (serials[i].functions & SERIAL_MSP_DISPLAYPORT) {
			char data[256];
			data[0] = subcmd;
			if (payload != nullptr) memcpy(&data[1], payload, payloadLength);
			sendMsp(i, MspMsgType::RESPONSE, MspFn::MSP_DISPLAYPORT, MspVersion::V1, data, payloadLength + 1);
		}
	}
}

void onSetCanvas(u8 cols, u8 rows) {
	sendMspDp(MspDpFn::CLEAR_SCREEN);
	OsdHandler::get().setDigitalSize(rows, cols);
}

void dpWriteString(u8 row, u8 column, u8 attribute, const char *content) {
	char data[34] = {0};
	data[0] = row & 0x3F;
	data[1] = column & 0x3F;
	data[2] = attribute; // attribute
	strncpy((char *)&data[3], content, 30);
	u8 len = strlen(content) + 4;
	sendMspDp(MspDpFn::WRITE_STRING, data, len);
}

void mspDisplayportLoop() {
	static elapsedMillis updateTimer = 0;
	if (updateTimer > 50) {
		sendMspDp(MspDpFn::HEARTBEAT);
		sendMspDp(MspDpFn::DRAW_SCREEN);
	}
}
