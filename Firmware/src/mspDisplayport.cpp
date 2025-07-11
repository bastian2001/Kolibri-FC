#include <mspDisplayport.h>

// TODO maybe replace string with char array (Performance lol)
string dpWriteString(u8 row, u8 column, u8 attribute, string content) {
	if (content.length() > 29) {
		content = content.substr(0, 28); // shortens string if too long
	}
	string msg = "";
	msg += row;
	msg += column;
	msg += attribute;
	msg += content;
	msg += "\0";
	return msg;
}

// String dpShow() {}

// String dpClear() {}

void processMspDpMsg(const char *payload, u16 payloadLength, u8 serialNum, MspVersion MspVersion) {
	Serial.printf("Got: ");
	for (int i = 0; i < payloadLength; i++) {
		Serial.printf("%02X ", payload[i]);
	}
	Serial.println();
	u8 subCmd = payload[0];
	payload++;
	payloadLength--;
	switch (subCmd) {
	case MspDpFn::HEARTBEAT:
		break;
	case MspDpFn::RELEASE:
		break;
	case MspDpFn::CLEAR_SCREEN:
		break;
	case MspDpFn::WRITE_STRING:
		break;
	case MspDpFn::DRAW_SCREEN:
		break;
	case MspDpFn::OPTIONS:
		break;
	case MspDpFn::SYS:
		break;
	default:
		break;
	}
}
