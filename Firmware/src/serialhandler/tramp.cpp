#include "global.h"

// largely copied from https://github.com/raphaelcoeffic/betaflight/blob/9681929f98cfac2b3433b605bcf788ef8bacf0a2/src/main/io/vtx_tramp.c

RingBuffer<u8> trampRxBuffer(64);

static u8 trampReqBuffer[16];
static u8 trampRespBuffer[16];

u32 trampRFFreqMin;
u32 trampRFFreqMax;
u32 trampRFPowerMax;

u32 trampCurFreq = 0;
u8 trampCurBand = 0;
u8 trampCurChan = 0;
u16 trampCurPower = 0; // Actual transmitting power
u16 trampCurConfigPower = 0; // Configured transmitting power
i16 trampCurTemp = 0;
u8 trampCurPitmode = 0;

u32 trampConfFreq = 0;
u16 trampConfPower = 0;

const u16 vtx58FreqTable[5][8] =
	{
		{5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725}, // Boscam A
		{5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866}, // Boscam B
		{5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945}, // Boscam E
		{5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880}, // FatShark
		{5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917}, // RaceBand
};

typedef enum {
	TRAMP_STATUS_BAD_DEVICE = -1,
	TRAMP_STATUS_OFFLINE = 0,
	TRAMP_STATUS_ONLINE,
	TRAMP_STATUS_SET_FREQ_PW,
	TRAMP_STATUS_CHECK_FREQ_PW
} trampStatus_e;

typedef enum {
	S_WAIT_LEN = 0, // Waiting for a packet len
	S_WAIT_CODE, // Waiting for a response code
	S_DATA, // Waiting for rest of the packet.
} trampReceiveState_e;

static trampReceiveState_e trampReceiveState = S_WAIT_LEN;
static i8 trampReceivePos = 0;

trampStatus_e trampStatus = TRAMP_STATUS_OFFLINE;

void trampWriteBuf(u8 *buf) {
	SoftSerial1.write(buf, 16);
}

u8 trampChecksum(u8 *trampBuf) {
	uint8_t cksum = 0;

	for (int i = 1; i < 14; i++)
		cksum += trampBuf[i];

	return cksum;
}

void trampCmdU16(u8 cmd, u16 param) {
	memset(trampReqBuffer, 0, ARRAYLEN(trampReqBuffer));
	trampReqBuffer[0] = 15;
	trampReqBuffer[1] = cmd;
	trampReqBuffer[2] = param & 0xff;
	trampReqBuffer[3] = (param >> 8) & 0xff;
	trampReqBuffer[14] = trampChecksum(trampReqBuffer);
	trampWriteBuf(trampReqBuffer);
}

void trampSetFreq(u16 freq) {
	trampConfFreq = freq;
}

void trampSendFreq(u16 freq) {
	trampCmdU16('F', freq);
}

void trampSetBandChan(u8 band, u8 chan) {
	trampSetFreq(vtx58FreqTable[band - 1][chan - 1]);
}

void trampSetRFPower(u16 level) {
	trampConfPower = level;
}

void trampSendRFPower(u16 level) {
	trampCmdU16('P', level);
}

// return false if error
bool trampCommitChanges() {
	if (trampStatus != TRAMP_STATUS_ONLINE)
		return false;

	trampStatus = TRAMP_STATUS_SET_FREQ_PW;
	return true;
}

void trampSetPitmode(uint8_t onoff) {
	trampCmdU16('I', onoff ? 0 : 1);
}

// returns completed response code
char trampHandleResponse() {
	uint8_t respCode = trampRespBuffer[1];

	switch (respCode) {
	case 'r': {
		uint16_t min_freq = trampRespBuffer[2] | (trampRespBuffer[3] << 8);
		if (min_freq != 0) {
			trampRFFreqMin = min_freq;
			trampRFFreqMax = trampRespBuffer[4] | (trampRespBuffer[5] << 8);
			trampRFPowerMax = trampRespBuffer[6] | (trampRespBuffer[7] << 8);
			return 'r';
		}

		// throw bytes echoed from tx to rx in bidirectional mode away
	} break;

	case 'v': {
		uint16_t freq = trampRespBuffer[2] | (trampRespBuffer[3] << 8);
		if (freq != 0) {
			trampCurFreq = freq;
			trampCurConfigPower = trampRespBuffer[4] | (trampRespBuffer[5] << 8);
			trampCurPitmode = trampRespBuffer[7];
			trampCurPower = trampRespBuffer[8] | (trampRespBuffer[9] << 8);
			// vtx58_Freq2Bandchan(trampCurFreq, &trampCurBand, &trampCurChan);
			return 'v';
		}

		// throw bytes echoed from tx to rx in bidirectional mode away
	} break;

	case 's': {
		uint16_t temp = (int16_t)(trampRespBuffer[6] | (trampRespBuffer[7] << 8));
		if (temp != 0) {
			trampCurTemp = temp;
			return 's';
		}
	} break;
	}

	return 0;
}

static void trampResetReceiver() {
	trampReceiveState = S_WAIT_LEN;
	trampReceivePos = 0;
}

static bool trampIsValidResponseCode(uint8_t code) {
	if (code == 'r' || code == 'v' || code == 's')
		return true;
	else
		return false;
}

// returns completed response code or 0
static char trampReceive() {
	while (!trampRxBuffer.isEmpty()) {
		uint8_t c = trampRxBuffer.pop();
		trampRespBuffer[trampReceivePos++] = c;

		switch (trampReceiveState) {
		case S_WAIT_LEN:
			if (c == 0x0F) {
				trampReceiveState = S_WAIT_CODE;
			} else {
				trampReceivePos = 0;
			}
			break;

		case S_WAIT_CODE:
			if (trampIsValidResponseCode(c)) {
				trampReceiveState = S_DATA;
			} else {
				trampResetReceiver();
			}
			break;

		case S_DATA:
			if (trampReceivePos == 16) {
				uint8_t cksum = trampChecksum(trampRespBuffer);

				trampResetReceiver();

				if ((trampRespBuffer[14] == cksum) && (trampRespBuffer[15] == 0)) {
					return trampHandleResponse();
				}
			}
			break;

		default:
			trampResetReceiver();
		}
	}

	return 0;
}

void trampQuery(uint8_t cmd) {
	trampResetReceiver();
	trampCmdU16(cmd, 0);
}

void trampQueryR(void) {
	trampQuery('r');
}

void trampQueryV(void) {
	trampQuery('v');
}

void trampQueryS(void) {
	trampQuery('s');
}

void trampLoop() {
	uint32_t currentTimeUs = micros();
	static uint32_t lastQueryTimeUs = 0;

#ifdef TRAMP_DEBUG
	static uint16_t debugFreqReqCounter = 0;
	static uint16_t debugPowReqCounter = 0;
#endif

	if (trampStatus == TRAMP_STATUS_BAD_DEVICE)
		return;

	char replyCode = trampReceive();

#ifdef TRAMP_DEBUG
	debug[0] = trampStatus;
#endif

	switch (replyCode) {
	case 'r':
		if (trampStatus <= TRAMP_STATUS_OFFLINE)
			trampStatus = TRAMP_STATUS_ONLINE;
		break;

	case 'v':
		if (trampStatus == TRAMP_STATUS_CHECK_FREQ_PW)
			trampStatus = TRAMP_STATUS_SET_FREQ_PW;
		break;
	}

	switch (trampStatus) {

	case TRAMP_STATUS_OFFLINE:
	case TRAMP_STATUS_ONLINE:
		if (currentTimeUs - lastQueryTimeUs > 1000 * 1000) { // 1s

			if (trampStatus == TRAMP_STATUS_OFFLINE)
				trampQueryR();
			else {
				static unsigned int cnt = 0;
				if (((cnt++) & 1) == 0)
					trampQueryV();
				else
					trampQueryS();
			}

			lastQueryTimeUs = currentTimeUs;
		}
		break;

	case TRAMP_STATUS_SET_FREQ_PW: {
		bool done = true;
		if (trampConfFreq != trampCurFreq) {
			trampSendFreq(trampConfFreq);
#ifdef TRAMP_DEBUG
			debugFreqReqCounter++;
#endif
			done = false;
		} else if (trampConfPower != trampCurConfigPower) {
			trampSendRFPower(trampConfPower);
#ifdef TRAMP_DEBUG
			debugPowReqCounter++;
#endif
			done = false;
		}

		if (!done) {
			trampStatus = TRAMP_STATUS_CHECK_FREQ_PW;

			// delay next status query by 300ms
			lastQueryTimeUs = currentTimeUs + 300 * 1000;
		} else {
			// everything has been done, let's return to original state
			trampStatus = TRAMP_STATUS_ONLINE;
		}
	} break;

	case TRAMP_STATUS_CHECK_FREQ_PW:
		if (currentTimeUs - lastQueryTimeUs > 200 * 1000) {
			trampQueryV();
			lastQueryTimeUs = currentTimeUs;
		}
		break;

	default:
		break;
	}

#ifdef TRAMP_DEBUG
	debug[1] = debugFreqReqCounter;
	debug[2] = debugPowReqCounter;
	debug[3] = 0;
#endif
}
