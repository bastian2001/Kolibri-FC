#include "global.h"

// largely copied from https://github.com/raphaelcoeffic/betaflight/blob/9681929f98cfac2b3433b605bcf788ef8bacf0a2/src/main/io/vtx_tramp.c

RingBuffer<u8> trampRxBuffer(64);

static u8 trampReqBuffer[16];

static u32 trampMinFreq;
static u32 trampMaxFreq;
static u32 trampMaxPwr;

static u32 trampCurFreq = 0;
static u8 trampCurBand = 0;
static u8 trampCurChan = 0;
static u16 trampCurPower = 0; // Actual transmitting power
static u16 trampCurConfigPower = 0; // Configured transmitting power
static i16 trampCurTemp = 0;
static bool trampCurPitmode = 0;

static u32 trampConfFreq = 5917;
static u16 trampConfPower = 25;

static u8 trampSerialNum = 255;
static elapsedMicros trampLastSend = 0;

static const u16 vtx58FreqTable[5][8] = {
	{5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725}, // Boscam A
	{5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866}, // Boscam B
	{5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945}, // Boscam E
	{5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880}, // FatShark
	{5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917}, // RaceBand
};

enum class TrampStatus {
	OFFLINE = 0,
	INIT_GET_SETTINGS,
	ONLINE_CHECK,
	ONLINE_SETTINGS,
	ONLINE_TEMP,
	SET_FREQ,
	CHECK_FREQ,
	SET_PWR,
	CHECK_PWR,
	SET_PITMODE,
	CHECK_PITMODE,
};

enum class TrampReceiveState {
	SYNC = 0, // waiting for sync byte (0x0F)
	CMD, // waiting for command byte
	DATA, // receiving data (bytes 2-13 (0-indexed))
	CHECKSUM, // waiting for checksum (byte 14 (0-indexed))
	END, // waiting for end byte (byte 15 (0-indexed))
};

enum class TrampCommand : u8 {
	INIT = 'r',
	SET_FREQ = 'F',
	SET_PWR = 'P',
	SET_PITMODE = 'I',
	GET_SETTINGS = 'v',
	GET_TEMP = 's',
};

static TrampStatus trampStatus = TrampStatus::OFFLINE;

static void trampWriteBuf(u8 *buf) {
	serials[trampSerialNum].stream->write(buf, 16);
}

static u8 trampCalcChecksum(u8 *trampBuf) {
	uint8_t cksum = 0;
	for (int i = 0; i < 13; i++)
		cksum += trampBuf[i];
	return cksum;
}

static void trampSendCmd(TrampCommand cmd, u16 param = 0) {
	memset(trampReqBuffer, 0, ARRAYLEN(trampReqBuffer));
	trampReqBuffer[0] = 15;
	trampReqBuffer[1] = (u8)cmd;
	trampReqBuffer[2] = param & 0xff;
	trampReqBuffer[3] = (param >> 8) & 0xff;
	trampReqBuffer[14] = trampCalcChecksum(trampReqBuffer + 1);
	trampWriteBuf(trampReqBuffer);
	trampLastSend = 0;
}

static bool trampHandleMessage(TrampCommand cmd, u8 data[12]) {
	Serial.printf("%d %02X %c\n", (u8)cmd, (u8)cmd, (u8)cmd);
	switch (cmd) {
	case TrampCommand::INIT: {
		trampMinFreq = DECODE_U2(data);
		trampMaxFreq = DECODE_U2(data + 2);
		trampMaxPwr = DECODE_U2(data + 4);
		if (trampStatus == TrampStatus::OFFLINE) {
			trampStatus = TrampStatus::INIT_GET_SETTINGS;
		} else if (trampStatus == TrampStatus::ONLINE_CHECK) {
			trampStatus = TrampStatus::ONLINE_SETTINGS;
		}
		Serial.printf("Got Init Message: Min: %d, Max: %d, MaxPwr: %d\n", trampMinFreq, trampMaxFreq, trampMaxPwr);
		return true;
	} break;
	case TrampCommand::GET_SETTINGS: {
		trampCurFreq = DECODE_U2(data);
		trampCurConfigPower = DECODE_U2(data + 2);
		trampCurPitmode = data[6] > 0;
		trampCurPower = DECODE_U2(data + 6);
		trampCurBand = 255;
		trampCurChan = 255;
		for (int band = 0; band < 5; band++) {
			for (int chan = 0; chan < 8; chan++) {
				if (vtx58FreqTable[band][chan] == trampCurFreq) {
					trampCurBand = band;
					trampCurChan = chan;
				}
			}
		}
		if (trampStatus == TrampStatus::INIT_GET_SETTINGS) {
			trampStatus = TrampStatus::ONLINE_CHECK;
		} else if (trampStatus == TrampStatus::ONLINE_SETTINGS) {
			if (trampCurFreq != trampConfFreq)
				trampStatus = TrampStatus::SET_FREQ;
			else if (trampCurConfigPower != trampConfPower)
				trampStatus = TrampStatus::SET_PWR;
			else
				trampStatus = TrampStatus::ONLINE_TEMP;
		}
		DEBUG_PRINTF("Got Settings Message: Freq: %d, Pit: %d, ConfPwr: %d, Pwr: %d\n", trampCurFreq, trampCurPitmode, trampCurConfigPower, trampCurPower);
		return true;
	} break;
	case TrampCommand::GET_TEMP: {
		trampCurTemp = DECODE_U2(data);
		if (trampStatus == TrampStatus::ONLINE_TEMP) {
			trampStatus = TrampStatus::ONLINE_CHECK;
		}
		DEBUG_PRINTF("Tramp Temp: %d\n", trampCurTemp);
	} break;
	}
	return false;
}

void trampInit() {
	for (int i = 0; i < SERIAL_COUNT; i++) {
		if (serials[i].functions & SERIAL_IRC_TRAMP) {
			trampSerialNum = i;
			break;
		}
	}
}

void trampLoop() {
	if (trampSerialNum == 255) return;

	TASK_START(TASK_TRAMP);

	static TrampReceiveState rxState = TrampReceiveState::SYNC;
	static u8 rxBuf[13];
	static u8 *const dataBuf = rxBuf + 1;
	static TrampCommand &cmd = ((TrampCommand *)rxBuf)[0];
	static u32 dataIndex = 0;
	bool newMsg = false;

	// RX state machine
	for (int i = 0; i < 10 && !trampRxBuffer.isEmpty(); i++) {
		u8 c = trampRxBuffer.pop();
		switch (rxState) {
		case TrampReceiveState::SYNC:
			if (c == 0x0F) rxState = TrampReceiveState::CMD;
			break;
		case TrampReceiveState::CMD:
			cmd = (TrampCommand)c;
			rxState = TrampReceiveState::DATA;
			dataIndex = 0;
			break;
		case TrampReceiveState::DATA:
			dataBuf[dataIndex++] = c;
			if (dataIndex == 12) rxState = TrampReceiveState::CHECKSUM;
			break;
		case TrampReceiveState::CHECKSUM:
			if (trampCalcChecksum(rxBuf) == c)
				rxState = TrampReceiveState::END;
			else
				rxState = TrampReceiveState::SYNC;
			break;
		case TrampReceiveState::END:
			if (c == 0) {
				newMsg = trampHandleMessage(cmd, dataBuf);
			}
			rxState = TrampReceiveState::SYNC;
		}
	}

	if (trampLastSend > 500000) {
		DEBUG_PRINTF("Tramp Status: %d\n", (u8)trampStatus);
		switch (trampStatus) {
		case TrampStatus::OFFLINE:
		case TrampStatus::ONLINE_CHECK:
			trampSendCmd(TrampCommand::INIT);
			break;
		case TrampStatus::ONLINE_SETTINGS:
		case TrampStatus::INIT_GET_SETTINGS:
		case TrampStatus::CHECK_FREQ:
		case TrampStatus::CHECK_PWR:
		case TrampStatus::CHECK_PITMODE:
			trampSendCmd(TrampCommand::GET_SETTINGS);
			break;
		case TrampStatus::ONLINE_TEMP:
			trampSendCmd(TrampCommand::GET_TEMP);
			break;
		case TrampStatus::SET_FREQ:
			trampSendCmd(TrampCommand::SET_FREQ, trampConfFreq);
			trampStatus = TrampStatus::CHECK_FREQ;
			break;
		case TrampStatus::SET_PWR:
			trampSendCmd(TrampCommand::SET_PWR, trampConfPower);
			trampStatus = TrampStatus::CHECK_PWR;
			break;
		case TrampStatus::SET_PITMODE:
			trampSendCmd(TrampCommand::SET_PITMODE, 1); // 1: no pit mode, 0: pit mode
			trampStatus = TrampStatus::CHECK_PITMODE;
			break;
		}
	}

	TASK_END(TASK_TRAMP);
}

// void trampSetFreq(u16 freq) {
// 	trampConfFreq = freq;
// }

// void trampSendFreq(u16 freq) {
// 	trampCmdU16('F', freq);
// }

// void trampSetBandChan(u8 band, u8 chan) {
// 	trampSetFreq(vtx58FreqTable[band - 1][chan - 1]);
// }

// void trampSetRFPower(u16 level) {
// 	trampConfPower = level;
// }

// void trampSendRFPower(u16 level) {
// 	trampCmdU16('P', level);
// }

// // return false if error
// bool trampCommitChanges() {
// 	if (trampStatus != TRAMP_STATUS_ONLINE)
// 		return false;

// 	trampStatus = TRAMP_STATUS_SET_FREQ_PW;
// 	return true;
// }

// void trampSetPitmode(uint8_t onoff) {
// 	trampCmdU16('I', onoff ? 0 : 1);
// }
