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
	bool printing;
	MspVersion mspVer;
	FsFile logFile;
	i32 currentChunk;
	u32 chunkSize;
	u16 logNum;
} BbPrintConfig;
BbPrintConfig bbPrintLog = {
	.serialNum = 255,
	.printing = false,
	.mspVer = MspVersion::V2,
	.currentChunk = 0,
	.chunkSize = 0,
	.logNum = 0,
};

FsFile blackboxFile;
elapsedMillis bbDuration;

u32 bbFrameNum = 0;
elapsedMicros frametime;
u8 bbWriteBuffer[BLACKBOX_WRITE_BUFFER_SIZE];
u32 bbWriteBufferPos = 0;
#define BB_WR_BUF_HAS_FREE(bytes) ((bytes) < BLACKBOX_WRITE_BUFFER_SIZE - bbWriteBufferPos)
bool lastHighlightState = false;
static FlightMode lastSavedFlightMode = FlightMode::LENGTH;

static void writeFlightModeToBlackbox() {
	FlightMode fm = flightMode;
	bbWriteBuffer[bbWriteBufferPos++] = BB_FRAME_FLIGHTMODE;
	bbWriteBuffer[bbWriteBufferPos++] = (u8)fm;
	lastSavedFlightMode = fm;
}

static void writeElrsToBlackbox() {
	bbWriteBuffer[bbWriteBufferPos++] = BB_FRAME_RC;
	u64 channels = 0;
	channels |= ELRS->channels[0];
	channels |= ELRS->channels[1] << 12;
	channels |= (u64)ELRS->channels[2] << 24;
	channels |= (u64)ELRS->channels[3] << 36;
	memcpy(bbWriteBuffer + bbWriteBufferPos, &channels, 6);
	bbWriteBufferPos += 6;
	ELRS->newPacketFlag &= ~(1 << 1);
}

static void writeGpsToBlackbox() {
	bbWriteBuffer[bbWriteBufferPos++] = BB_FRAME_GPS;
	memcpy(bbWriteBuffer + bbWriteBufferPos, currentPvtMsg, 92);
	bbWriteBufferPos += 92;
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
	bbWriteBuffer[bbWriteBufferPos++] = -ELRS->uplinkRssi[0];
	bbWriteBuffer[bbWriteBufferPos++] = -ELRS->uplinkRssi[1];
	bbWriteBuffer[bbWriteBufferPos++] = ELRS->uplinkLinkQuality;
	bbWriteBuffer[bbWriteBufferPos++] = ELRS->uplinkSNR;
	bbWriteBuffer[bbWriteBufferPos++] = ELRS->antennaSelection;
	bbWriteBuffer[bbWriteBufferPos++] = ELRS->targetPacketRate;
	bbWriteBuffer[bbWriteBufferPos++] = ELRS->targetPacketRate >> 8;
	bbWriteBuffer[bbWriteBufferPos++] = ELRS->actualPacketRate;
	bbWriteBuffer[bbWriteBufferPos++] = ELRS->actualPacketRate >> 8;
	bbWriteBuffer[bbWriteBufferPos++] = ELRS->txPower;
	bbWriteBuffer[bbWriteBufferPos++] = ELRS->txPower >> 8;
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
				bbPrintLog.logFile.close();
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
		if (BB_WR_BUF_HAS_FREE(len + 1)) {
			bbWriteBuffer[bbWriteBufferPos++] = BB_FRAME_NORMAL;
			memcpy(bbWriteBuffer + bbWriteBufferPos, frame + 1, len);
			bbWriteBufferPos += len;
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
	}

	// save GPS PVT message
	if (newPvtMessageFlag & (1 << 0) && currentBBFlags & LOG_GPS && BB_WR_BUF_HAS_FREE(92 + 1)) {
		writeGpsToBlackbox();
	}

	// save ELRS joysticks
	if (ELRS->newPacketFlag & (1 << 1) && currentBBFlags & LOG_ELRS_RAW && BB_WR_BUF_HAS_FREE(6 + 1)) {
		writeElrsToBlackbox();
	}

	// save VBat to Blackbox
	if (adcFlag & (1 << 0) && currentBBFlags & LOG_VBAT && BB_WR_BUF_HAS_FREE(2 + 1)) {
		writeVbatToBlackbox();
	}

	if (ELRS->newLinkStatsFlag && currentBBFlags & LOG_LINK_STATS && BB_WR_BUF_HAS_FREE(11 + 1)) {
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

void printFileInit(u8 serialNum, MspVersion mspVer, u16 logNum) {
	if (bbPrintLog.printing) {
		bbPrintLog.printing = false;
		bbPrintLog.logFile.close();
	}
	char path[32];
#if BLACKBOX_STORAGE == SD_BB
	snprintf(path, 32, "/blackbox/KOLI%04d.kbb", logNum);
	FsFile logFile = sdCard.open(path);
#endif
	if (!logFile) {
		sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FILE_INIT, mspVer, "File not found", strlen("File not found"));
		return;
	}

	// send init frame
	u8 b[10];
	b[0] = logNum & 0xFF;
	b[1] = logNum >> 8;
	u32 size = logFile.size();
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
	logFile.close();
}

void printLogBin(u8 serialNum, MspVersion mspVer, u16 logNum, i32 singleChunk) {
	if (bbPrintLog.printing) {
		bbPrintLog.printing = false;
		bbPrintLog.logFile.close();
	}
	char path[32];
#if BLACKBOX_STORAGE == SD_BB
	snprintf(path, 32, "/blackbox/KOLI%04d.kbb", logNum);
	bbPrintLog.logFile = sdCard.open(path);
#endif
	if (!bbPrintLog.logFile) {
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
	bbPrintLog.logNum = logNum;
}

void startLogging() {
	if (!bbFlags || !fsReady || bbLogging || !bbFreqDivider)
		return;
	if (bbPrintLog.printing) {
		bbPrintLog.printing = false;
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
	while (blackboxFile.position() < LOG_DATA_START) {
		blackboxFile.write((u8)0);
	}
	bbDuration = 0;
	bbFrameNum = 0;
	bbWriteBufferPos = 0;
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
	size_t bufferPos = 1;
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
	bbBuffer[0] = bufferPos - 1;
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
			// Both FIFOs are full, we can't keep up with the logging, dropping oldest frame
		}
	}
	TASK_END(TASK_BLACKBOX);
	return durationTASK_BLACKBOX;
}
