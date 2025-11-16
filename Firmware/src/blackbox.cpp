#include "global.h"

#if BLACKBOX_STORAGE == SD_BB
SdFs sdCard;
#endif

// rather conservative estimates of available buffers. Doesn't need to be perfect.
#define BLACKBOX_CHUNK_SIZE 1024
#define BLACKBOX_CRSF_CHUNK_SIZE 480
#define BLACKBOX_MSPV1_CHUNK_SIZE 230

#define BLACKBOX_WRITE_BUFFER_SIZE 8192

u64 bbFlags = 0;
u64 currentBBFlags = 0;

volatile bool bbLogging = false;
volatile bool fsReady = false;

u8 bbFreqDivider = 2;

u32 bbDebug1, bbDebug2;
u16 bbDebug3, bbDebug4;

RingBuffer<u8 *> bbFramePtrBuffer(64);

typedef struct bbPrintConfig {
	u8 serialNum;
	bool open;
	bool printing;
	MspVersion mspVer;
	FsFile logFile;
	i32 currentChunk;
	u32 chunkSize;
	u16 logNum;
} BbPrintConfig;
BbPrintConfig bbPrintLog = {
	.serialNum = 255,
	.open = false,
	.printing = false,
	.mspVer = MspVersion::V2,
	.currentChunk = 0,
	.chunkSize = 0,
	.logNum = 0,
};

FsFile blackboxFile;
elapsedMillis bbDuration;

i32 bbFrameNum = 0;
elapsedMicros frametime;
u8 bbWriteBuffer[BLACKBOX_WRITE_BUFFER_SIZE];
u32 bbWriteBufferPos = 0;
#define BB_WR_BUF_HAS_FREE(bytes) ((bytes) < BLACKBOX_WRITE_BUFFER_SIZE - bbWriteBufferPos)
bool lastHighlightState = false;
static FlightMode lastSavedFlightMode = FlightMode::LENGTH;
static u32 writtenFrameNum = 0;
static u32 lastSyncPos = 0;
static u8 fmHighlightFlag = 0; // used for SYNC. bit 0 set to indicate HL, bit 1 set to indicate FM change in the last 100 frames

/**
 * @brief replaces every SYN with SYN! and sends it off to the blackbox buffer
 *
 * @param buf buffer that needs to be escaped
 * @param len (net) length of the buffer buf
 */
static void writeToBlackboxWithEscape(const u8 *buf, size_t len) {
	bool foundSyn = false;
	const size_t maxScan = len - 2;
	for (int i = 0; i < maxScan; i++) {
		// broad filtering, faster than memcpy
		if (buf[i] != 'S')
			continue;

		if (buf[i + 1] == 'Y' && buf[i + 2] == 'N') {
			foundSyn = true;
			break;
		}
	}
	if (foundSyn) {
		int i = 0;
		for (; i < maxScan;) {
			if (buf[i] != 'S' || buf[i + 1] != 'Y' || buf[i + 2] != 'N') {
				bbWriteBuffer[bbWriteBufferPos++] = buf[i++];
			} else {
				// SYN detected, write SYN!
				bbWriteBuffer[bbWriteBufferPos++] = 'S';
				bbWriteBuffer[bbWriteBufferPos++] = 'Y';
				bbWriteBuffer[bbWriteBufferPos++] = 'N';
				bbWriteBuffer[bbWriteBufferPos++] = '!';
				i += 3;
			}
		}
		for (; i < len; i++) {
			// SYN can not happen here
			bbWriteBuffer[bbWriteBufferPos++] = buf[i];
		}
	} else {
		// did not find SYN anywhere, just write raw
		memcpy(bbWriteBuffer + bbWriteBufferPos, buf, len);
		bbWriteBufferPos += len;
	}
}

/**
 * @brief unescapes a byte array into a new array
 *
 * @param in input array
 * @param out output array
 * @param inputLen total length of input buffer
 * @param outputLen target (maximum) length of output
 * @param usedInputLen the amount of bytes that were used will be written in here, set to nullptr to ignore
 * @return actual output length
 */
static size_t unescapeBytes(u8 *in, u8 *out, size_t inputLen, size_t outputLen, size_t *usedInputLen = nullptr) {
	u8 synState = 0;
	size_t o = 0;
	size_t i = 0;
	for (; i < inputLen && o < outputLen; i++) {
		u8 &c = in[i];
		if (!synState) {
			out[o++] = c;
			if (c == 'S') synState = 1;
			continue;
		}

		switch (synState) {
		case 1:
			out[o++] = c;
			if (c == 'Y')
				synState = 2;
			else if (c != 'S')
				synState = 0;
			break;
		case 2:
			out[o++] = c;
			if (c == 'N')
				synState = 3;
			else if (c == 'S')
				synState = 1;
			else
				synState = 0;
			break;
		case 3:
			if (c == '!') {
				synState = 0;
			} else if (c == 'C') {
				synState = 0;
				out[o++] = c;
			} else if (c != 'S') {
				synState = 1;
			} else {
				// should not occur
				synState = 0;
				out[o++] = c;
			}
		}
	}
	if (usedInputLen != nullptr) *usedInputLen = i;
	return o++;
}

static void writeFlightModeToBlackbox() {
	FlightMode fm = flightMode;
	bbWriteBuffer[bbWriteBufferPos++] = BB_FRAME_FLIGHTMODE;
	bbWriteBuffer[bbWriteBufferPos++] = (u8)fm;
	lastSavedFlightMode = fm;
	fmHighlightFlag |= 0b10;
}

static void writeElrsToBlackbox() {
	bbWriteBuffer[bbWriteBufferPos++] = BB_FRAME_RC;
	u64 channels = 0;
	channels |= ELRS->channels[0];
	channels |= ELRS->channels[1] << 12;
	channels |= (u64)ELRS->channels[2] << 24;
	channels |= (u64)ELRS->channels[3] << 36;
	writeToBlackboxWithEscape((u8 *)&channels, 6);
	ELRS->newPacketFlag &= ~(1 << 1);
}

static void writeGpsToBlackbox() {
	bbWriteBuffer[bbWriteBufferPos++] = BB_FRAME_GPS;
	writeToBlackboxWithEscape(currentPvtMsg, 92);
	newPvtMessageFlag &= ~(1 << 0);
}

static void writeVbatToBlackbox() {
	adcFlag &= ~(1 << 0);
	bbWriteBuffer[bbWriteBufferPos++] = BB_FRAME_VBAT;
	bbWriteBuffer[bbWriteBufferPos++] = adcVoltage;
	bbWriteBuffer[bbWriteBufferPos++] = adcVoltage >> 8;
}

static void writeElrsLinkToBlackbox() {
	bbWriteBuffer[bbWriteBufferPos++] = BB_FRAME_LINK_STATS;
	u8 buf[11];
	buf[0] = -ELRS->uplinkRssi[0];
	buf[1] = -ELRS->uplinkRssi[1];
	buf[2] = ELRS->uplinkLinkQuality;
	buf[3] = ELRS->uplinkSNR;
	buf[4] = ELRS->antennaSelection;
	buf[5] = ELRS->targetPacketRate;
	buf[6] = ELRS->targetPacketRate >> 8;
	buf[7] = ELRS->actualPacketRate;
	buf[8] = ELRS->actualPacketRate >> 8;
	buf[9] = ELRS->txPower;
	buf[10] = ELRS->txPower >> 8;
	writeToBlackboxWithEscape(buf, 11);
	ELRS->newLinkStatsFlag &= ~(1 << 0);
}

void blackboxLoop() {
	if (!fsReady) return;
	if (!bbLogging) {
		if (bbPrintLog.printing) {
			TASK_START(TASK_CONFIGURATOR);

			u8 buffer[bbPrintLog.chunkSize + 6];
			buffer[0] = bbPrintLog.logNum & 0xFF;
			buffer[1] = bbPrintLog.logNum >> 8;
			buffer[2] = bbPrintLog.currentChunk & 0xFF;
			buffer[3] = bbPrintLog.currentChunk >> 8;
			buffer[4] = bbPrintLog.currentChunk >> 16;
			buffer[5] = bbPrintLog.currentChunk >> 24;
			bbPrintLog.logFile.seek(bbPrintLog.currentChunk * bbPrintLog.chunkSize);
			u32 bytesRead = bbPrintLog.logFile.read(buffer + 6, bbPrintLog.chunkSize);
			if (bytesRead <= 0) {
				bbPrintLog.printing = false;
				TASK_END(TASK_CONFIGURATOR);
				return;
			}
			sendMsp(bbPrintLog.serialNum, MspMsgType::RESPONSE, MspFn::BB_FILE_DOWNLOAD, bbPrintLog.mspVer, (char *)buffer, bytesRead + 6);
			bbPrintLog.currentChunk++;
			serials[bbPrintLog.serialNum].stream->flush();

			TASK_END(TASK_CONFIGURATOR);
		}
		return;
	}
	TASK_START(TASK_BLACKBOX_WRITE);

	// write a normal blackbox frame
	if (rp2040.fifo.available()) {
		u8 *frame = (u8 *)rp2040.fifo.pop();
		u8 &len = frame[0];
		// i32 frameIndex;
		// memcpy(&frameIndex, &frame[1], 4);
		bool needsSync = (writtenFrameNum % 100) == 0;
		size_t spaceNeeded = (len + (needsSync ? 10 : 1)) * 4 / 3;
		if (BB_WR_BUF_HAS_FREE(spaceNeeded)) {
			if (needsSync) {
				u32 thisSyncPos = blackboxFile.position() + bbWriteBufferPos;
				bbWriteBuffer[bbWriteBufferPos++] = 'S';
				bbWriteBuffer[bbWriteBufferPos++] = 'Y';
				bbWriteBuffer[bbWriteBufferPos++] = 'N';
				bbWriteBuffer[bbWriteBufferPos++] = 'C';
				u8 buf[9];
				buf[0] = fmHighlightFlag;
				fmHighlightFlag = 0;
				memcpy(&buf[1], &writtenFrameNum, 4);
				memcpy(&buf[5], &lastSyncPos, 4);
				lastSyncPos = thisSyncPos;
				writeToBlackboxWithEscape(buf, 9);
			}
			bbWriteBuffer[bbWriteBufferPos++] = BB_FRAME_NORMAL;
			writeToBlackboxWithEscape(frame + 5, len);
			writtenFrameNum++;
		}
		free(frame);
	}

	// write a flight mode change
	if (flightMode != lastSavedFlightMode && BB_WR_BUF_HAS_FREE(1 + 1)) {
		writeFlightModeToBlackbox();
	}

	// save a highlight frame
	bool highlightSwitch = rxModes[(int)RxModeIndex::BB_HIGHLIGHT].isActive();
	if (highlightSwitch != lastHighlightState && BB_WR_BUF_HAS_FREE(1)) {
		if (highlightSwitch) {
			bbWriteBuffer[bbWriteBufferPos++] = BB_FRAME_HIGHLIGHT;
			lastHighlightState = true;
		} else {
			lastHighlightState = false;
		}
		fmHighlightFlag |= 0b01;
	}

	// save GPS PVT message
	if (newPvtMessageFlag & (1 << 0) && currentBBFlags & LOG_GPS && BB_WR_BUF_HAS_FREE(92 * 4 / 3 + 1)) {
		writeGpsToBlackbox();
	}

	// save ELRS joysticks
	if (ELRS->newPacketFlag & (1 << 1) && currentBBFlags & LOG_ELRS_RAW && BB_WR_BUF_HAS_FREE(6 * 4 / 3 + 1)) {
		writeElrsToBlackbox();
	}

	// save VBat to Blackbox
	if (adcFlag & (1 << 0) && currentBBFlags & LOG_VBAT && BB_WR_BUF_HAS_FREE(2 + 1)) {
		writeVbatToBlackbox();
	}

	// save ELRS link statistics
	if (ELRS->newLinkStatsFlag & (1 << 0) && currentBBFlags & LOG_LINK_STATS && BB_WR_BUF_HAS_FREE(11 * 4 / 3 + 1)) {
		writeElrsLinkToBlackbox();
	}

	// write buffer
	if (bbWriteBufferPos) {
		u16 writeBytes = bbWriteBufferPos;
		if (writeBytes > 512) writeBytes = 512;
		if (!blackboxFile.write(bbWriteBuffer, writeBytes)) {
			fsReady = false;
			bbLogging = false;
			TASK_END(TASK_BLACKBOX_WRITE);
			return;
		}
		bbWriteBufferPos -= writeBytes;
		if (bbWriteBufferPos) {
			memmove(bbWriteBuffer, bbWriteBuffer + writeBytes, bbWriteBufferPos);
		}
	}

	TASK_END(TASK_BLACKBOX_WRITE);
}

void initBlackbox() {
	addSetting(SETTING_BB_FLAGS, &bbFlags, 0b1111111111111111100000000000000011111111111ULL);
	addSetting(SETTING_BB_DIV, &bbFreqDivider, 2);

#if BLACKBOX_STORAGE == SD_BB
	SdioConfig sdConfig(PIN_SD_SCLK, PIN_SD_CMD, PIN_SD_DAT);
	FsDateTime::setCallback(getFsTime);
	fsReady = sdCard.begin(sdConfig);

	if (!sdCard.exists("/blackbox")) {
		sdCard.mkdir("/blackbox");
	}
	Serial.println(fsReady ? "SD card ready" : "SD card not ready");
#endif
}

bool clearBlackbox() {
	if (!fsReady || bbLogging)
		return false;
#if BLACKBOX_STORAGE == SD_BB
	FsFile dir = sdCard.open("/blackbox");
	FsFile file;
	if (!dir) return true;
	if (!dir.isDir()) return false;

	while (file.openNext(&dir)) {
		char path[32];
		file.getName(path, 32);
		rp2040.wdt_reset();
		if (!file.isFile()) return false;
		if (!dir.remove(path)) {
			file.close();
			return false;
		}
	}
	if (!sdCard.rmdir("/blackbox")) return false;
	if (!sdCard.mkdir("/blackbox")) return false;
	return true;
#endif
}

u32 getBlackboxChunkSize(MspVersion v) {
	switch (v) {
	case MspVersion::V1:
	case MspVersion::V2_OVER_V1:
	case MspVersion::V1_OVER_CRSF:
		return BLACKBOX_MSPV1_CHUNK_SIZE;

	case MspVersion::V2_OVER_CRSF:
	case MspVersion::V1_JUMBO_OVER_CRSF:
		return BLACKBOX_CRSF_CHUNK_SIZE;

	case MspVersion::V2:
	case MspVersion::V1_JUMBO:
	default:
		return BLACKBOX_CHUNK_SIZE;
	}
}

/**
 * @brief opens a log file into bbPrintLog.logFile if provided logNum differs
 * @return true if a file is open afterwards
 */
static bool openLogFileIfDiffNum(u16 logNum) {
	if (bbPrintLog.open && bbPrintLog.logNum != logNum) {
		bbPrintLog.printing = false;
		bbPrintLog.open = false;
		bbPrintLog.logFile.close();
	}

	if (!bbPrintLog.open) {
		char path[32];
#if BLACKBOX_STORAGE == SD_BB
		snprintf(path, 32, "/blackbox/KOLI%04d.kbb", logNum);
		bbPrintLog.logFile = sdCard.open(path);
#endif
		if (!bbPrintLog.logFile) {
			return false;
		}

		bbPrintLog.open = true;
		bbPrintLog.logNum = logNum;
	}
	return true;
}

void printFastFileInit(u8 serialNum, MspVersion mspVer, u16 logNum, u8 subCmd, const char *reqPayload, u16 reqLen) {
	if (!fsReady || bbLogging) {
		sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FAST_FILE_INIT, mspVer, "Cannot read blackbox during logging", strlen("Cannot read blackbox during logging"));
		return;
	}
	if (!openLogFileIfDiffNum(logNum)) {
		sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FAST_FILE_INIT, mspVer, "File not found", strlen("File not found"));
		return;
	}
	u32 chunkSize = getBlackboxChunkSize(mspVer);
	bbPrintLog.printing = false;
	bbPrintLog.currentChunk = 0;
	bbPrintLog.mspVer = mspVer;
	bbPrintLog.serialNum = serialNum;
	bbPrintLog.chunkSize = chunkSize;

	printfIndMessage("fast file init for file %d with subcmd %d", logNum, subCmd);

	switch (subCmd) {
	case 0: {
#if BLACKBOX_STORAGE == SD_BB
		FsFile &file = bbPrintLog.logFile;
#endif

		u8 b[288];
		b[0] = logNum & 0xFF;
		b[1] = logNum >> 8;
		b[2] = subCmd;
		u32 size = file.size();
		memcpy(&b[3], &size, 4);
		memcpy(&b[7], &chunkSize, 4);
		file.seek(0);
		file.read(&b[32], 256);
		u8 &frameSize = b[32 + 153];

		u8 fileBuf[1024 + 8]; // 1024 bytes to read, +8 to check for sync crossing 1024-boundaries (sync size 9 bytes)
		for (int i = 0; i < 8; i++) {
			fileBuf[i] = 0; // zero out the array to ensure no invalid sync read at the very end of the file
		}
		u32 syncPos = 0xFFFFFFFFU;
		u32 syncFrame = 0;
		for (i32 searchPos = size - 1024; searchPos > 256 - 1024; searchPos -= 1024) {
			u32 readSize = 1024;
			if (searchPos < 256) { // do not read into header (unescaped and useless anyway)
				i32 temp = searchPos;
				searchPos = 256;
				readSize -= searchPos - temp;
			}

			// shift the first 8 bytes of the last search position to the end so we can always search for a 9 byte sync phrase (sync is 13 byte but we only need the first 9 of it)
			for (int i = 0; i < 8; i++) {
				fileBuf[readSize + i] = fileBuf[i];
			}

			// get 1024 bytes so we have 1032 to read through to find a sync
			file.seek(searchPos);
			file.read(fileBuf, readSize);

			// read through the bytes and find sync
			for (i32 pos = readSize - 1; pos >= 0; pos--) {
				if (fileBuf[pos] == 'S' && fileBuf[pos + 1] == 'Y' && fileBuf[pos + 2] == 'N' && fileBuf[pos + 3] == 'C') {
					syncPos = searchPos + pos;
					// TODO SYNC is also escaped. needs to be unescaped
					syncFrame = DECODE_U4(&fileBuf[pos + 5]);
					break;
				}
			}
			if (syncPos != 0xFFFFFFFFU) {
				break;
			}
		}

		// search for last frame
		u32 readPos = 0;
		i32 bytesReadable = 0;
		u32 frameCount = syncFrame;
		u8 searchSize = 9;
		u8 frameProgress = 9;
		u8 synProgress = 0;
		u32 lastSp = 0;
		for (int searchPos = syncPos + BB_FRAMESIZE_SYNC; searchPos < size; searchPos++) {
			if (!bytesReadable) {
				rp2040.wdt_reset();
				file.seek(searchPos);
				lastSp = searchPos;
				bytesReadable = file.read(fileBuf, 1024);
				if (bytesReadable <= 0) {
					break;
				}
				readPos = 0;
			}

			if (frameProgress == searchSize) {
				// beginning of new frame
				synProgress = 0;
				switch (fileBuf[readPos]) {
				case BB_FRAME_NORMAL:
					searchSize = frameSize + 1;
					frameCount++;
					break;
				case BB_FRAME_FLIGHTMODE:
					searchSize = BB_FRAMESIZE_FLIGHTMODE;
					break;
				case BB_FRAME_HIGHLIGHT:
					searchSize = BB_FRAMESIZE_HIGHLIGHT;
					break;
				case BB_FRAME_GPS:
					searchSize = BB_FRAMESIZE_GPS;
					break;
				case BB_FRAME_RC:
					searchSize = BB_FRAMESIZE_RC;
					break;
				case BB_FRAME_VBAT:
					searchSize = BB_FRAMESIZE_VBAT;
					break;
				case BB_FRAME_LINK_STATS:
					searchSize = BB_FRAMESIZE_LINK_STATS;
					break;
				case BB_FRAME_SYNC: // shouldn't happen
					searchSize = BB_FRAMESIZE_SYNC;
					break;
				default:
					searchSize = 1;
					break;
				}
				frameProgress = 1;
			} else {
				u8 u = fileBuf[readPos];
				bool fp = true;
				switch (synProgress) {
				case 0:
					if (u == 'S')
						synProgress = 1;
					break;
				case 1:
					if (u == 'Y')
						synProgress = 2;
					else
						synProgress = 0;
					break;
				case 2:
					if (u == 'N')
						synProgress = 3;
					else
						synProgress = 0;
					break;
				case 3:
					if (u == '!')
						fp = false;
					synProgress = 0;
					break;
				}
				if (fp)
					frameProgress++;
			}
			bytesReadable--;
			readPos++;
		}
		memcpy(&b[11], &frameCount, 4);
		sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::BB_FAST_FILE_INIT, mspVer, (char *)b, 288);
	} break;
	case 1: {
		if (reqLen != 6)
			return sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FAST_FILE_INIT, mspVer, "Incorrect usage of fast file init subCmd 1", strlen("Incorrect usage of fast file init subCmd 1"));
#if BLACKBOX_STORAGE == SD_BB
		FsFile &file = bbPrintLog.logFile;
#endif
		u32 startPos = DECODE_U4((u8 *)reqPayload);
		u8 frameSize = reqPayload[4];
		u8 syncFreq = reqPayload[5];
		const u32 jumpAfterSync = syncFreq * (frameSize + 1) + BB_FRAMESIZE_SYNC; // no guarantee about elrs etc. so not adding any of that here
		const u32 maxSyncs = (chunkSize - 8) / 9; // per sync: pos (4), frame (4), status(1)
		u8 buf[maxSyncs * 9 + 7];
		buf[0] = logNum & 0xFF;
		buf[1] = logNum >> 8;
		buf[2] = subCmd;
		memcpy(&buf[3], &startPos, 4);
		u8 inBuf[1024];
		u32 syncCount = 0;
		file.seek(startPos);
		i32 maxReadNext = file.read(inBuf + 512, 512);
		if (maxReadNext < 0) {
			return sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FAST_FILE_INIT, mspVer, "Error reading file", strlen("Error reading file"));
		}

		for (bool doneAllSyncs = false; !doneAllSyncs;) {
			memcpy(inBuf, inBuf + 512, 512);
			i32 maxRead = maxReadNext;
			if (maxRead < BB_FRAMESIZE_SYNC) {
				// file end reached
				break;
			}

			rp2040.wdt_reset();
			maxReadNext = file.read(inBuf + 512, 512);
			if (maxReadNext < 0) {
				return sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FAST_FILE_INIT, mspVer, "Error reading file", strlen("Error reading file"));
			}
			// overwrite leftover data with 0 to prevent accidental SYNC finding, once end of file is reached
			for (int i = 512 + maxReadNext; i < 1024; i++) {
				inBuf[i] = 0;
			}

			// actually find SYNC
			for (int i = 0; i < maxRead; i++) {
				if (inBuf[i] != 'S') continue;
				if (inBuf[i + 1] == 'Y' && inBuf[i + 2] == 'N' && inBuf[i + 3] == 'C') {
					u32 syncPos = file.position() - maxReadNext - maxRead + i;
					u8 syncBuf[5];
					unescapeBytes(&inBuf[i + 4], syncBuf, 500, 5);
					u8 syncFlags = syncBuf[0];
					u32 syncFrame = DECODE_U4(&syncBuf[1]);
					u32 bufPos = syncCount * 9 + 7;
					memcpy(&buf[bufPos], &syncPos, 4);
					memcpy(&buf[bufPos + 4], &syncFrame, 4);
					buf[bufPos + 8] = syncFlags;
					syncCount++;
					if (syncCount >= maxSyncs) {
						doneAllSyncs = true;
						break;
					}
				}
			}
		}

		sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::BB_FAST_FILE_INIT, mspVer, (char *)buf, syncCount * 9 + 7);
	} break;
	case 2: {
		if (reqLen < 1)
			return sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FAST_FILE_INIT, mspVer, "Improper use of fast file init subCmd 2", strlen("Improper use of fast file init subCmd 2"));
		u8 frameSize = reqPayload[0];
		u8 b[1024];
		b[0] = logNum & 0xFF;
		b[1] = logNum >> 8;
		b[2] = subCmd;

		// search from startPos up to the next SYNC and report ALL found HLs and FM changes. Format: u8: count of total occurences until next sync, then an array of 5 byte structs: u32 frameNum, u8 flag. Where flag is of the following format: bit0-bit3: FM change: 0xF = no change, anything lower is the new FM. bit 4: HL indicator, bit5-7 reserved. Repeat this u8 + struct[] thing until all are done. flags can contain data for both, a HL and FM change, but they do not need to be merged.
		// reqData contains a bunch of the previously reported sync positions. These HAVE TO be actual starts of SYNC sequences

#if BLACKBOX_STORAGE == SD_BB
		FsFile &file = bbPrintLog.logFile;
#endif

		const u32 syncCount = (reqLen - 1) / 4;
		u32 bufPos = 3;

		for (u32 i = 0; i < syncCount; i++) {
			u32 filePos = DECODE_U4((u8 *)&reqPayload[1 + i * 4]);
			rp2040.wdt_reset();
			file.seek(filePos);
			u8 finding = 0;

			u8 inBuf[1024];
			i32 readable = file.read(inBuf, 1024);
			u32 readPos = 0;
			u32 frameNum = 0;
			u32 lastFlag = 0xFFFFFFFFUL;
			u8 dummy[256];
			if (readable < 0) {
				return sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FAST_FILE_INIT, mspVer, "Error reading file", strlen("Error reading file"));
			}

			u32 bufPosBackup = bufPos;
			b[bufPos++] = 0;
			for (u8 foundNextSync = 0; foundNextSync < 2;) {
				if (bufPos > chunkSize - 5) break;

				// read one (any frametype) frame at a time, and read 512 bytes again if more bytes are needed by that frametype than are readable.

				// shift and refill if needed
				if (readable - readPos < 512) {
					memmove(inBuf, &inBuf[readPos], 1024 - readPos);
					memset(&inBuf[1024 - readPos], 0, readPos);
					i32 newBytes = file.read(&inBuf[1024 - readPos], readPos);
					readable -= readPos;
					readPos = 0;
					if (newBytes < 0) break; // error
					readable += newBytes;
					if (readable <= 0) break; // file over, treat identical to finding sync
				}

				switch (inBuf[readPos++]) {
				case BB_FRAME_NORMAL: {
					frameNum++;
					u32 used = 0;
					u32 act = unescapeBytes(&inBuf[readPos], dummy, readable - readPos, frameSize, &used);
					if (act != frameSize) {
						// TODO error
					}
					readPos += used;
				} break;
				case BB_FRAME_FLIGHTMODE: {
					if (lastFlag == frameNum) {
						// can overwrite, does not need to, we skip it for now
					}
					memcpy(&b[bufPos], &frameNum, 4);
					b[bufPos + 4] = inBuf[readPos] & 0xF;
					bufPos += 5;
					lastFlag = frameNum;
					finding++;
					readPos++;
				} break;
				case BB_FRAME_HIGHLIGHT: {
					if (lastFlag == frameNum) {
						// can overwrite, does not need to, we skip it for now
					}
					memcpy(&b[bufPos], &frameNum, 4);
					b[bufPos + 4] = 0x1F;
					bufPos += 5;
					lastFlag = frameNum;
					finding++;
				} break;
				case BB_FRAME_GPS: {
					u32 used = 0;
					u32 act = unescapeBytes(&inBuf[readPos], dummy, readable - readPos, BB_FRAMESIZE_GPS - 1, &used);
					if (act != BB_FRAMESIZE_GPS - 1) {
						// TODO error
					}
					readPos += used;
				} break;
				case BB_FRAME_RC: {
					u32 used = 0;
					u32 act = unescapeBytes(&inBuf[readPos], dummy, readable - readPos, BB_FRAMESIZE_RC - 1, &used);
					if (act != BB_FRAMESIZE_RC - 1) {
						// TODO error
					}
					readPos += used;
				} break;
				case BB_FRAME_VBAT: {
					readPos += BB_FRAMESIZE_VBAT - 1;
				} break;
				case BB_FRAME_LINK_STATS: {
					u32 used = 0;
					u32 act = unescapeBytes(&inBuf[readPos], dummy, readable - readPos, BB_FRAMESIZE_LINK_STATS - 1, &used);
					if (act != BB_FRAMESIZE_LINK_STATS - 1) {
						// TODO error
					}
					readPos += used;
				} break;
				case BB_FRAME_SYNC: {
					u32 used = 0;
					u32 act = unescapeBytes(&inBuf[readPos], dummy, readable - readPos, BB_FRAMESIZE_SYNC - 1, &used);
					if (act != BB_FRAMESIZE_SYNC - 1) {
						// TODO error
					}
					foundNextSync++;
					frameNum = DECODE_U4(&dummy[4]);
					readPos += used;
				} break;
				}
			}
			b[bufPosBackup] = finding;
			if (bufPos > chunkSize - 5) break;
		}
		sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::BB_FAST_FILE_INIT, mspVer, (char *)b, bufPos);
	} break;
	default:
		sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FAST_FILE_INIT, mspVer, "invalid subCmd", strlen("invalid subCmd"));
		break;
	}
}

void printFastDataReq(u8 serialNum, MspVersion mspVer, u16 sequenceNum, u16 logNum, u8 frameSize, const char *reqPayload, u16 reqLen) {
	if (!fsReady || bbLogging) {
		sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FAST_DATA_REQ, mspVer, "Cannot read blackbox during logging", strlen("Cannot read blackbox during logging"));
		return;
	}
	if (!openLogFileIfDiffNum(logNum)) {
		sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FAST_DATA_REQ, mspVer, "File not found", strlen("File not found"));
		return;
	}
	if (reqLen % 9 != 0) {
		return sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FAST_DATA_REQ, mspVer, "Incorrect Fast Data Request parameters", strlen("Incorrect Fast Data Request parameters"));
	}

	u8 elrsBuffer[8 + BB_FRAMESIZE_RC - 1];
	u8 gpsBuffer[8 + BB_FRAMESIZE_GPS - 1];
	u8 vbatBuffer[8 + BB_FRAMESIZE_VBAT - 1];
	u8 linkStatsBuffer[8 + BB_FRAMESIZE_LINK_STATS - 1];
	u8 frameBuffer[frameSize];
	u8 buf[1024];
	u8 dummy[frameSize > 92 ? frameSize : 92];
	u8 inBuf[1024];
	memset(buf, 0, 1024);
	buf[0] = sequenceNum;
	buf[1] = sequenceNum >> 8;
	u32 bufPos = 2;

	for (int i = 0; i < reqLen; i += 9) {
		u32 reqFrame = DECODE_U4((u8 *)&reqPayload[i]);
		u8 whichFrameTypes = reqPayload[i + 4];
		bool frameReq = whichFrameTypes & 0b00001;
		bool elrsReq = whichFrameTypes & 0b00010;
		bool gpsReq = whichFrameTypes & 0b00100;
		bool vbatReq = whichFrameTypes & 0b01000;
		bool linkStatsReq = whichFrameTypes & 0b10000;
		bool elrsFound = false, gpsFound = false, vbatFound = false, linkStatsFound = false;
		bool elrsCompleted = false, gpsCompleted = false, vbatCompleted = false, linkStatsCompleted = false;
		u32 totalSize =
			(frameReq ? frameSize : 0) +
			(elrsReq ? sizeof(elrsBuffer) : 0) +
			(gpsReq ? sizeof(gpsBuffer) : 0) +
			(vbatReq ? sizeof(linkStatsBuffer) : 0) +
			(linkStatsReq ? sizeof(vbatBuffer) : 0);
		if (bufPos + totalSize > getBlackboxChunkSize(mspVer)) break;
		u32 elrsFrame = 0, gpsFrame = 0, vbatFrame = 0, linkStatsFrame = 0;
		u32 suggestedSyncPos = DECODE_U4((u8 *)&reqPayload[i + 5]);
		u32 nextSyncPos = suggestedSyncPos;
		u32 framePos = 0;
		bool searchingBackwards = true;
		u32 frameNum = 0;

		/*
		 * EVERYTHING ALWAYS UNESCAPED
		 *
		 * in the following order, if wanted:
		 * - elrs: 4 byte from frame (inclusive), 4 byte to frame (inclusive), ELRS data. Total 14 bytes
		 * - gps: 4 byte from frame (inclusive), 4 byte to frame (inclusive), GPS data. Total 100 bytes
		 * - link stats: 4 byte from frame (inclusive), 4 byte to frame (inclusive), link stats data. Total 19 bytes
		 * - normal frame: length frameSize, just the frame
		 */

#if BLACKBOX_STORAGE == SD_BB
		FsFile &file = bbPrintLog.logFile;
#endif

		while (elrsReq != elrsCompleted || gpsReq != gpsCompleted || vbatReq != vbatCompleted || linkStatsReq != linkStatsCompleted) {
			rp2040.wdt_reset();
			printfIndMessage("seeking %d", nextSyncPos);
			if (nextSyncPos == 0xFFFFFFFFUL)
				return sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FAST_FILE_INIT, mspVer, "Error 1 while reading file", strlen("Error 1 while reading file"));
			if (nextSyncPos == 0) {
				nextSyncPos = 256;
				frameNum = 0;
			}
			printfIndMessage("actually seeking %d", nextSyncPos);
			file.seek(nextSyncPos);
			nextSyncPos = 0xFFFFFFFFUL;

			i32 readable = file.read(inBuf, 1024);
			if (readable < 0) {
				return sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FAST_FILE_INIT, mspVer, "Error 2 while reading file", strlen("Error 2 while reading file"));
			}
			u32 readPos = 0;

			for (bool goBack = false; !goBack;) {
				// read one (any frametype) frame at a time, and read 512 bytes again if more bytes are needed by that frametype than are readable.

				// shift and refill if needed
				if (readable - readPos < 512) {
					memmove(inBuf, &inBuf[readPos], 1024 - readPos);
					memset(&inBuf[1024 - readPos], 0, readPos);
					i32 newBytes = file.read(&inBuf[1024 - readPos], readPos);
					readable -= readPos;
					readPos = 0;
					if (newBytes < 0)
						return sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FAST_FILE_INIT, mspVer, "Error 3 while reading file", strlen("Error 3 while reading file"));
					readable += newBytes;
					if (readable <= 0) {
						if (searchingBackwards) {
							return sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FAST_FILE_INIT, mspVer, "Error 4 while reading file", strlen("Error 4 while reading file"));
						}
						elrsCompleted = elrsReq;
						gpsCompleted = gpsReq;
						vbatCompleted = vbatReq;
						linkStatsCompleted = linkStatsReq;
						u32 u = frameNum - 1;
						memcpy(&elrsBuffer[4], &u, 4);
						memcpy(&gpsBuffer[4], &u, 4);
						memcpy(&vbatBuffer[4], &u, 4);
						memcpy(&linkStatsBuffer[4], &u, 4);
						break;
					}
				}

				switch (inBuf[readPos++]) {
				case BB_FRAME_NORMAL: {
					u32 used = 0;
					u32 act = 0;
					if (frameNum >= reqFrame && searchingBackwards) {
						unescapeBytes(&inBuf[readPos], frameBuffer, readable - readPos, frameSize, &used);
						framePos = file.position() - readable + readPos - 1;

						if (elrsReq == elrsFound && gpsReq == gpsFound && vbatReq == vbatFound && linkStatsReq == linkStatsFound) {
							// if the frame is the last thing we find, just continue searching
							searchingBackwards = false;
						} else {
							goBack = true;
						}
					} else {
						unescapeBytes(&inBuf[readPos], dummy, readable - readPos, frameSize, &used);
					}
					if (act != frameSize) {
						// TODO error
					}
					frameNum++;
					readPos += used;
				} break;
				case BB_FRAME_FLIGHTMODE: {
					readPos++;
				} break;
				case BB_FRAME_HIGHLIGHT: {
				} break;
				case BB_FRAME_GPS: {
					u32 used = 0;
					u32 act = 0;
					if (frameNum >= gpsFrame && frameNum <= reqFrame) {
						unescapeBytes(&inBuf[readPos], &gpsBuffer[8], readable - readPos, BB_FRAMESIZE_GPS - 1, &used);
						memcpy(gpsBuffer, &frameNum, 4);
						gpsFound = true;
						gpsFrame = frameNum;
					} else {
						unescapeBytes(&inBuf[readPos], dummy, readable - readPos, BB_FRAMESIZE_GPS - 1, &used);
						if (gpsFound && !gpsCompleted) {
							u32 u = frameNum - 1;
							memcpy(&gpsBuffer[4], &u, 4);
							gpsCompleted = true;
						}
					}
					if (act != BB_FRAMESIZE_GPS - 1) {
						// TODO error
					}
					readPos += used;
				} break;
				case BB_FRAME_RC: {
					u32 used = 0;
					u32 act = 0;
					if (frameNum >= elrsFrame && frameNum <= reqFrame) {
						unescapeBytes(&inBuf[readPos], &elrsBuffer[8], readable - readPos, BB_FRAMESIZE_RC - 1, &used);
						memcpy(elrsBuffer, &frameNum, 4);
						elrsFound = true;
						elrsFrame = frameNum;
					} else {
						unescapeBytes(&inBuf[readPos], dummy, readable - readPos, BB_FRAMESIZE_RC - 1, &used);
						if (elrsFound && !elrsCompleted) {
							u32 u = frameNum - 1;
							memcpy(&elrsBuffer[4], &u, 4);
							elrsCompleted = true;
						}
					}
					if (act != BB_FRAMESIZE_RC - 1) {
						// TODO error
					}
					readPos += used;
				} break;
				case BB_FRAME_VBAT: {
					if (frameNum >= vbatFrame && frameNum <= reqFrame) {
						memcpy(&vbatBuffer[8], &inBuf[readPos], 2);
						memcpy(vbatBuffer, &frameNum, 4);
						vbatFound = true;
						vbatFrame = frameNum;
					} else {
						if (vbatFound && !vbatCompleted) {
							u32 u = frameNum - 1;
							memcpy(&vbatBuffer[4], &u, 4);
							vbatCompleted = true;
						}
					}
					readPos += BB_FRAMESIZE_VBAT - 1;
				} break;
				case BB_FRAME_LINK_STATS: {
					u32 used = 0;
					u32 act = 0;
					if (frameNum >= linkStatsFrame && frameNum <= reqFrame) {
						unescapeBytes(&inBuf[readPos], &linkStatsBuffer[8], readable - readPos, BB_FRAMESIZE_LINK_STATS - 1, &used);
						memcpy(linkStatsBuffer, &linkStatsBuffer, 4);
						linkStatsFound = true;
						linkStatsFrame = frameNum;
					} else {
						unescapeBytes(&inBuf[readPos], dummy, readable - readPos, BB_FRAMESIZE_LINK_STATS - 1, &used);
						if (linkStatsFound && !linkStatsCompleted) {
							u32 u = frameNum - 1;
							memcpy(&linkStatsBuffer[4], &u, 4);
							linkStatsCompleted = true;
						}
					}
					if (act != BB_FRAMESIZE_LINK_STATS - 1) {
						// TODO error
					}
					readPos += used;
				} break;
				case BB_FRAME_SYNC: {
					u32 used = 0;
					u32 act = unescapeBytes(&inBuf[readPos], dummy, readable - readPos, BB_FRAMESIZE_SYNC - 1, &used);
					if (act != BB_FRAMESIZE_SYNC - 1) {
						// TODO error
					}
					if (nextSyncPos != 0xFFFFFFFFUL && framePos != 0 && (elrsReq != elrsFound || gpsReq != gpsFound || vbatReq != vbatFound || linkStatsReq != linkStatsFound)) {
						// if we found the frame, but not all ELRS/GPS/... stuff, go back, else store the next jump point
						goBack = true;
						printfIndMessage("goBack");
					} else if (nextSyncPos == 0xFFFFFFFFUL) {
						nextSyncPos = DECODE_U4(&dummy[3 /*YNC*/ + 1 /*flag*/ + 4 /*frame number*/]);
						printfIndMessage("found SYNC around %d with nextSyncPos %d", (u32)file.position() - readable + readPos - 1, nextSyncPos);
					}
					frameNum = DECODE_U4(&dummy[4]);
					printfIndMessage("it has frameNum %d", frameNum);
					readPos += used;
				} break;
				}

				if (elrsReq == elrsFound && gpsReq == gpsFound && vbatReq == vbatFound && linkStatsReq == linkStatsFound && framePos != 0 && searchingBackwards) {
					searchingBackwards = false;
					nextSyncPos = framePos;
					frameNum = reqFrame;
					goBack = true;
				}
				if (elrsReq == elrsCompleted && gpsReq == gpsCompleted && vbatReq == vbatCompleted && linkStatsReq == linkStatsCompleted) {
					goBack = true; // will exit
				}
			}
		}

		if (frameReq) {
			memcpy(&buf[bufPos], frameBuffer, sizeof(frameBuffer));
			bufPos += sizeof(frameBuffer);
		}
		if (elrsReq) {
			memcpy(&buf[bufPos], elrsBuffer, sizeof(elrsBuffer));
			bufPos += sizeof(elrsBuffer);
		}
		if (gpsReq) {
			memcpy(&buf[bufPos], gpsBuffer, sizeof(gpsBuffer));
			bufPos += sizeof(gpsBuffer);
		}
		if (vbatReq) {
			memcpy(&buf[bufPos], vbatBuffer, sizeof(vbatBuffer));
			bufPos += sizeof(vbatBuffer);
		}
		if (linkStatsReq) {
			memcpy(&buf[bufPos], linkStatsBuffer, sizeof(linkStatsBuffer));
			bufPos += sizeof(linkStatsBuffer);
		}
	}
	sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::BB_FAST_DATA_REQ, mspVer, (char *)buf, bufPos);
}

void printFileInit(u8 serialNum, MspVersion mspVer, u16 logNum) {
	if (!fsReady || bbLogging) {
		sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FILE_INIT, mspVer, "Cannot read blackbox during logging", strlen("Cannot read blackbox during logging"));
		return;
	}
	if (!openLogFileIfDiffNum(logNum)) {
		sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FILE_INIT, mspVer, "File not found", strlen("File not found"));
		return;
	}

	// send init frame
	u8 b[10];
	b[0] = logNum & 0xFF;
	b[1] = logNum >> 8;
	u32 size = bbPrintLog.logFile.size();
	b[2] = size & 0xFF;
	b[3] = (size >> 8) & 0xFF;
	b[4] = (size >> 16) & 0xFF;
	b[5] = (size >> 24) & 0xFF;
	u32 chunkSize = getBlackboxChunkSize(mspVer);
	b[6] = chunkSize & 0xFF;
	b[7] = (chunkSize >> 8) & 0xFF;
	b[8] = (chunkSize >> 16) & 0xFF;
	b[9] = (chunkSize >> 24) & 0xFF;
	sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::BB_FILE_INIT, mspVer, (char *)b, 10);

	bbPrintLog.printing = false;
	bbPrintLog.currentChunk = 0;
	bbPrintLog.mspVer = mspVer;
	bbPrintLog.serialNum = serialNum;
	bbPrintLog.chunkSize = chunkSize;
}

void printLogBin(u8 serialNum, MspVersion mspVer, u16 logNum, i32 singleChunk) {
	if (!fsReady || bbLogging) {
		sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FILE_DOWNLOAD, mspVer, "Cannot read blackbox during logging", strlen("Cannot read blackbox during logging"));
		return;
	}
	if (!openLogFileIfDiffNum(logNum)) {
		sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FILE_DOWNLOAD, mspVer, "File not found", strlen("File not found"));
		return;
	}

	u32 chunkSize = getBlackboxChunkSize(mspVer);
	if (singleChunk >= 0) {
		u8 buffer[chunkSize + 6];
		buffer[0] = logNum & 0xFF;
		buffer[1] = logNum >> 8;
		buffer[2] = singleChunk & 0xFF;
		buffer[3] = singleChunk >> 8;
		buffer[4] = singleChunk >> 16;
		buffer[5] = singleChunk >> 24;
		bbPrintLog.logFile.seek(singleChunk * chunkSize);
		u32 bytesRead = bbPrintLog.logFile.read(buffer + 6, chunkSize);
		sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::BB_FILE_DOWNLOAD, mspVer, (char *)buffer, bytesRead + 6);
		return;
	}

	bbPrintLog.printing = true;
	bbPrintLog.currentChunk = 0;
	bbPrintLog.mspVer = mspVer;
	bbPrintLog.serialNum = serialNum;
	bbPrintLog.chunkSize = chunkSize;
}

void bbClosePrintFile(u8 serialNum, MspVersion mspVer) {
	if (!fsReady || bbLogging) {
		sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_CLOSE_FILE, mspVer, "Cannot read blackbox during logging", strlen("Cannot read blackbox during logging"));
		return;
	}
	if (bbPrintLog.open) {
		bbPrintLog.open = false;
		bbPrintLog.printing = false;
		bbPrintLog.logFile.close();
	}
	sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::BB_CLOSE_FILE, mspVer);
}

static u8 getFrameSizeFromFlags() {
	u8 frameSize = 0;
	for (int i = 0; i < 47; i++) {
		// continue if unset
		if (!(bbFlags & (1ULL << i))) continue;

		switch (i) {
		case 23:
		case 31:
		case 32:
		case 33:
		case 44:
			frameSize += 6;
			break;
		case 38:
		case 40:
		case 41:
			frameSize += 4;
			break;
		case 39:
			frameSize += 3;
			break;
		case 0: // ELRS_RAW
		case 27: // GPS
		case 45: // VBAT
		case 46: // LINK_STATS
			break;
		default:
			frameSize += 2;
			break;
		}
	}
	return frameSize;
}

void startLogging() {
	if (!bbFlags || !fsReady || bbLogging || !bbFreqDivider)
		return;
	if (bbPrintLog.open) {
		bbPrintLog.printing = false;
		bbPrintLog.open = false;
		bbPrintLog.logFile.close();
	}
	currentBBFlags = bbFlags;
#if BLACKBOX_STORAGE == SD_BB
	char path[32];
	FsFile dir = sdCard.open("/blackbox");
	FsFile file;
	int i = 0;
	while (file.openNext(&dir)) {
		rp2040.wdt_reset();
		//  we want to find the highest numbered file in /blackbox
		if (file.isFile()) {
			String name;
			char n[32];
			file.getName(n, 32);
			name = n;
			if (!name.startsWith("KOLI") || !name.endsWith(".kbb")) {
				continue;
			}
			if (name.length() != 12) {
				continue;
			}
			char num[5];
			name.substring(4, 8).toCharArray(num, 5);
			u32 index = 0;
			for (int j = 0; j < 4; j++) {
				if (num[j] < '0' || num[j] > '9') {
					index = 99999;
				}
				index = index * 10 + (num[j] - '0');
			}
			if (index > i && index < 10000) {
				i = index;
			}
		}
	}
	snprintf(path, 32, "/blackbox/KOLI%04d.kbb", i + 1);
	blackboxFile = sdCard.open(path, O_WRITE | O_CREAT);
#endif
	if (!blackboxFile)
		return;
	const u8 data[] = {
		0xDC, 0xDF, 0x4B, 0x4F, 0x4C, 0x49, 0x01, 0x00, 0x00, 0x00, 0x01 // magic bytes, version
	};
	blackboxFile.write(data, 11);
	u32 recordTime = rtcGetUnixTimestamp();
	blackboxFile.write((u8 *)&recordTime, 4);
	u32 zero = 0;
	blackboxFile.write((u8 *)&zero, 4); // duration, will be filled later
	blackboxFile.write((u8)0); // 3200Hz PID Loop
	blackboxFile.write((u8)bbFreqDivider);
	blackboxFile.write((u8)3); // 2000deg/sec and 16g
	i32 rf[3][3];
	for (int ax = 0; ax < 3; ax++)
		for (int i = 0; i < 3; i++)
			rf[ax][i] = rateCoeffs[ax][i].raw;
	blackboxFile.write((u8 *)rf, 36);
	u8 dummy[30] = {0};
	blackboxFile.write(dummy, 24); // reserved space for future use
	blackboxFile.write((u8 *)pidGainsNice, 30);
	blackboxFile.write(dummy, 30);
	blackboxFile.write((u8 *)&bbFlags, 8);
	blackboxFile.write((u8)MOTOR_POLES);
	blackboxFile.write((u8)0); // disarm reason, will be filled at the end
	blackboxFile.write((u8)100); // one sync sequence every x frames. Set to 0 to indicate that ABV is disabled
	blackboxFile.write(getFrameSizeFromFlags());
	while (blackboxFile.position() < LOG_DATA_START) {
		blackboxFile.write((u8)0);
	}
	bbDuration = 0;
	bbFrameNum = 0;
	writtenFrameNum = 0;
	bbWriteBufferPos = 0;
	lastSyncPos = 0;
	writeFlightModeToBlackbox();
	if (currentBBFlags & LOG_GPS) writeGpsToBlackbox();
	if (currentBBFlags & LOG_ELRS_RAW) writeElrsToBlackbox();
	if (currentBBFlags & LOG_VBAT) writeVbatToBlackbox();
	if (currentBBFlags & LOG_LINK_STATS) writeElrsLinkToBlackbox();
	bbLogging = true;
	frametime = 0;
}

void endLogging(DisarmReason reason) {
	if (!fsReady)
		return;
	rp2040.wdt_reset();
	if (bbLogging) {
		bbLogging = false;
		u32 duration = bbDuration;
		blackboxFile.seek(LOG_HEAD_DURATION);
		blackboxFile.write((u8 *)&duration, 4);
		blackboxFile.seek(LOG_HEAD_DISARM_REASON);
		blackboxFile.write((u8)reason);
		blackboxFile.close();
	}
}

u32 writeSingleFrame() {
	size_t bufferPos = 5;
	if (!fsReady || !bbLogging) {
		return 0;
	}
	TASK_START(TASK_BLACKBOX);
	u8 *bbBuffer = (u8 *)malloc(128);
	if (currentBBFlags & LOG_ROLL_SETPOINT) {
		i16 setpoint = (i16)(rollSetpoint.raw >> 12);
		bbBuffer[bufferPos++] = setpoint;
		bbBuffer[bufferPos++] = setpoint >> 8;
	}
	if (currentBBFlags & LOG_PITCH_SETPOINT) {
		i16 setpoint = (i16)(pitchSetpoint.raw >> 12);
		bbBuffer[bufferPos++] = setpoint;
		bbBuffer[bufferPos++] = setpoint >> 8;
	}
	if (currentBBFlags & LOG_THROTTLE_SETPOINT) {
		i16 t = (i16)(throttle.raw >> 12);
		bbBuffer[bufferPos++] = t;
		bbBuffer[bufferPos++] = t >> 8;
	}
	if (currentBBFlags & LOG_YAW_SETPOINT) {
		i16 setpoint = (i16)(yawSetpoint.raw >> 12);
		bbBuffer[bufferPos++] = setpoint;
		bbBuffer[bufferPos++] = setpoint >> 8;
	}
	if (currentBBFlags & LOG_ROLL_GYRO_RAW) {
		i16 i = (gyroScaled[AXIS_ROLL].raw >> 12);
		bbBuffer[bufferPos++] = i;
		bbBuffer[bufferPos++] = i >> 8;
	}
	if (currentBBFlags & LOG_PITCH_GYRO_RAW) {
		i16 i = (gyroScaled[AXIS_PITCH].raw >> 12);
		bbBuffer[bufferPos++] = i;
		bbBuffer[bufferPos++] = i >> 8;
	}
	if (currentBBFlags & LOG_YAW_GYRO_RAW) {
		i16 i = (gyroScaled[AXIS_YAW].raw >> 12);
		bbBuffer[bufferPos++] = i;
		bbBuffer[bufferPos++] = i >> 8;
	}
	if (currentBBFlags & LOG_ROLL_PID_P) {
		bbBuffer[bufferPos++] = rollP.geti32();
		bbBuffer[bufferPos++] = rollP.geti32() >> 8;
	}
	if (currentBBFlags & LOG_ROLL_PID_I) {
		bbBuffer[bufferPos++] = rollI.geti32();
		bbBuffer[bufferPos++] = rollI.geti32() >> 8;
	}
	if (currentBBFlags & LOG_ROLL_PID_D) {
		bbBuffer[bufferPos++] = rollD.geti32();
		bbBuffer[bufferPos++] = rollD.geti32() >> 8;
	}
	if (currentBBFlags & LOG_ROLL_PID_FF) {
		bbBuffer[bufferPos++] = rollFF.geti32();
		bbBuffer[bufferPos++] = rollFF.geti32() >> 8;
	}
	if (currentBBFlags & LOG_ROLL_PID_S) {
		bbBuffer[bufferPos++] = rollS.geti32();
		bbBuffer[bufferPos++] = rollS.geti32() >> 8;
	}
	if (currentBBFlags & LOG_PITCH_PID_P) {
		bbBuffer[bufferPos++] = pitchP.geti32();
		bbBuffer[bufferPos++] = pitchP.geti32() >> 8;
	}
	if (currentBBFlags & LOG_PITCH_PID_I) {
		bbBuffer[bufferPos++] = pitchI.geti32();
		bbBuffer[bufferPos++] = pitchI.geti32() >> 8;
	}
	if (currentBBFlags & LOG_PITCH_PID_D) {
		bbBuffer[bufferPos++] = pitchD.geti32();
		bbBuffer[bufferPos++] = pitchD.geti32() >> 8;
	}
	if (currentBBFlags & LOG_PITCH_PID_FF) {
		bbBuffer[bufferPos++] = pitchFF.geti32();
		bbBuffer[bufferPos++] = pitchFF.geti32() >> 8;
	}
	if (currentBBFlags & LOG_PITCH_PID_S) {
		bbBuffer[bufferPos++] = pitchS.geti32();
		bbBuffer[bufferPos++] = pitchS.geti32() >> 8;
	}
	if (currentBBFlags & LOG_YAW_PID_P) {
		bbBuffer[bufferPos++] = yawP.geti32();
		bbBuffer[bufferPos++] = yawP.geti32() >> 8;
	}
	if (currentBBFlags & LOG_YAW_PID_I) {
		bbBuffer[bufferPos++] = yawI.geti32();
		bbBuffer[bufferPos++] = yawI.geti32() >> 8;
	}
	if (currentBBFlags & LOG_YAW_PID_D) {
		bbBuffer[bufferPos++] = yawD.geti32();
		bbBuffer[bufferPos++] = yawD.geti32() >> 8;
	}
	if (currentBBFlags & LOG_YAW_PID_FF) {
		bbBuffer[bufferPos++] = yawFF.geti32();
		bbBuffer[bufferPos++] = yawFF.geti32() >> 8;
	}
	if (currentBBFlags & LOG_YAW_PID_S) {
		bbBuffer[bufferPos++] = yawS.geti32();
		bbBuffer[bufferPos++] = yawS.geti32() >> 8;
	}
	if (currentBBFlags & LOG_MOTOR_OUTPUTS) {
		u64 throttles64 = throttles[(u8)MOTOR::RR] | (u64)throttles[(u8)MOTOR::FR] << 12 | (u64)throttles[(u8)MOTOR::RL] << 24 | (u64)throttles[(u8)MOTOR::FL] << 36;
		bbBuffer[bufferPos++] = throttles64;
		bbBuffer[bufferPos++] = throttles64 >> 8;
		bbBuffer[bufferPos++] = throttles64 >> 16;
		bbBuffer[bufferPos++] = throttles64 >> 24;
		bbBuffer[bufferPos++] = throttles64 >> 32;
		bbBuffer[bufferPos++] = throttles64 >> 40;
	}
	if (currentBBFlags & LOG_FRAMETIME) {
		u16 ft = frametime;
		frametime -= ft;
		bbBuffer[bufferPos++] = ft;
		bbBuffer[bufferPos++] = ft >> 8;
	}
	if (currentBBFlags & LOG_ALTITUDE) {
		const u32 height = combinedAltitude.raw >> 10; // 10.6 fixed point, approx. 6cm resolution, 4km altitude
		bbBuffer[bufferPos++] = height;
		bbBuffer[bufferPos++] = height >> 8;
	}
	if (currentBBFlags & LOG_VVEL) {
		const i32 vvel = vVel.raw >> 8; // 8.8 fixed point, approx. 4mm/s resolution, +-128m/s max
		bbBuffer[bufferPos++] = vvel;
		bbBuffer[bufferPos++] = vvel >> 8;
	}
	if (currentBBFlags & LOG_ATT_ROLL) {
		i16 r = (roll * 10000).geti32();
		bbBuffer[bufferPos++] = r;
		bbBuffer[bufferPos++] = r >> 8;
	}
	if (currentBBFlags & LOG_ATT_PITCH) {
		i16 p = (pitch * 10000).geti32();
		bbBuffer[bufferPos++] = p;
		bbBuffer[bufferPos++] = p >> 8;
	}
	if (currentBBFlags & LOG_ATT_YAW) {
		i16 y = (yaw * 10000).geti32();
		bbBuffer[bufferPos++] = y;
		bbBuffer[bufferPos++] = y >> 8;
	}
	if (currentBBFlags & LOG_MOTOR_RPM) {
		u64 rpmPacket = escRawTelemetry[(u8)MOTOR::RR] | escRawTelemetry[(u8)MOTOR::FR] << 12 | (u64)escRawTelemetry[(u8)MOTOR::RL] << 24 | (u64)escRawTelemetry[(u8)MOTOR::FL] << 36;
		bbBuffer[bufferPos++] = rpmPacket;
		bbBuffer[bufferPos++] = rpmPacket >> 8;
		bbBuffer[bufferPos++] = rpmPacket >> 16;
		bbBuffer[bufferPos++] = rpmPacket >> 24;
		bbBuffer[bufferPos++] = rpmPacket >> 32;
		bbBuffer[bufferPos++] = rpmPacket >> 40;
	}
	if (currentBBFlags & LOG_ACCEL_RAW) {
		memcpy(&bbBuffer[bufferPos], (void *)accelDataRaw, 6);
		bufferPos += 6;
	}
	if (currentBBFlags & LOG_ACCEL_FILTERED) {
		i16 i = accelDataFiltered[0]->geti32();
		bbBuffer[bufferPos++] = i;
		bbBuffer[bufferPos++] = i >> 8;
		i = accelDataFiltered[1]->geti32();
		bbBuffer[bufferPos++] = i;
		bbBuffer[bufferPos++] = i >> 8;
		i = accelDataFiltered[2]->geti32();
		bbBuffer[bufferPos++] = i;
		bbBuffer[bufferPos++] = i >> 8;
	}
	if (currentBBFlags & LOG_VERTICAL_ACCEL) {
		i16 a = (i16)(vAccel.raw >> 9);
		bbBuffer[bufferPos++] = a;
		bbBuffer[bufferPos++] = a >> 8;
	}
	if (currentBBFlags & LOG_VVEL_SETPOINT) {
		i16 v = (i16)(vVelSetpoint.raw >> 4) * ((u32)flightMode >= 2);
		bbBuffer[bufferPos++] = v;
		bbBuffer[bufferPos++] = v >> 8;
	}
	if (currentBBFlags & LOG_MAG_HEADING) {
		i16 h = (i16)(magHeading.raw >> 3);
		bbBuffer[bufferPos++] = h;
		bbBuffer[bufferPos++] = h >> 8;
	}
	if (currentBBFlags & LOG_COMBINED_HEADING) {
		int h = combinedHeading.raw >> 3;
		bbBuffer[bufferPos++] = h;
		bbBuffer[bufferPos++] = h >> 8;
	}
	if (currentBBFlags & LOG_HVEL) {
		// same as vvel: 8.8 fixed point in m/s, +-128m/s max, 4mm/s resolution
		// first nVel (north positive), then eVel (east positive)
		i32 v = nVel.raw >> 8;
		bbBuffer[bufferPos++] = v;
		bbBuffer[bufferPos++] = v >> 8;
		v = eVel.raw >> 8;
		bbBuffer[bufferPos++] = v;
		bbBuffer[bufferPos++] = v >> 8;
	}
	if (currentBBFlags & LOG_BARO) {
		i32 val = blackboxPres;
		bbBuffer[bufferPos++] = val;
		bbBuffer[bufferPos++] = val >> 8;
		bbBuffer[bufferPos++] = val >> 16;
	}
	if (currentBBFlags & LOG_DEBUG_1) {
		bbBuffer[bufferPos++] = bbDebug1;
		bbBuffer[bufferPos++] = bbDebug1 >> 8;
		bbBuffer[bufferPos++] = bbDebug1 >> 16;
		bbBuffer[bufferPos++] = bbDebug1 >> 24;
	}
	if (currentBBFlags & LOG_DEBUG_2) {
		bbBuffer[bufferPos++] = bbDebug2;
		bbBuffer[bufferPos++] = bbDebug2 >> 8;
		bbBuffer[bufferPos++] = bbDebug2 >> 16;
		bbBuffer[bufferPos++] = bbDebug2 >> 24;
	}
	if (currentBBFlags & LOG_DEBUG_3) {
		bbBuffer[bufferPos++] = bbDebug3;
		bbBuffer[bufferPos++] = bbDebug3 >> 8;
	}
	if (currentBBFlags & LOG_DEBUG_4) {
		bbBuffer[bufferPos++] = bbDebug4;
		bbBuffer[bufferPos++] = bbDebug4 >> 8;
	}
	if (currentBBFlags & LOG_PID_SUM) {
		i16 i = rollSum.geti32();
		bbBuffer[bufferPos++] = i;
		bbBuffer[bufferPos++] = i >> 8;
		i = pitchSum.geti32();
		bbBuffer[bufferPos++] = i;
		bbBuffer[bufferPos++] = i >> 8;
		i = yawSum.geti32();
		bbBuffer[bufferPos++] = i;
		bbBuffer[bufferPos++] = i >> 8;
	}
	bbBuffer[0] = bufferPos - 5;
	memcpy(&bbBuffer[1], &bbFrameNum, 4);
	bbFrameNum++;
	if (bbFramePtrBuffer.isEmpty()) {
		if (!rp2040.fifo.push_nb((u32)bbBuffer)) {
			bbFramePtrBuffer.push(bbBuffer);
		}
	} else if (!bbFramePtrBuffer.isFull()) {
		bbFramePtrBuffer.push(bbBuffer);
		for (u32 i = bbFramePtrBuffer.itemCount(); i; i--) {
			u8 *frame = bbFramePtrBuffer[0];
			if (rp2040.fifo.push_nb((u32)frame)) {
				bbFramePtrBuffer.pop();
			} else {
				break;
			}
		}
	} else {
		u8 *frame = bbFramePtrBuffer[0];
		if (rp2040.fifo.push_nb((u32)frame)) {
			bbFramePtrBuffer.pop();
			bbFramePtrBuffer.push(bbBuffer);
			for (u32 i = bbFramePtrBuffer.itemCount(); i; i--) {
				u8 *frame = bbFramePtrBuffer[0];
				if (rp2040.fifo.push_nb((u32)frame)) {
					bbFramePtrBuffer.pop();
				} else {
					break;
				}
			}
		} else {
			free(bbBuffer);
			bbFrameNum--;
			// Both FIFOs are full, we can't keep up with the logging, dropping newest frame
		}
	}
	TASK_END(TASK_BLACKBOX);
	return durationTASK_BLACKBOX;
}
