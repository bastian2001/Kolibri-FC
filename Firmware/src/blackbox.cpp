#include "global.h"

#define BLACKBOX_CHUNK_SIZE 1024

u64 bbFlags = 0;
u64 currentBBFlags = 0;

volatile bool bbLogging = false;
volatile bool fsReady = false;

FSInfo fsInfo;
int currentLogNum = 0;
u8 bbFreqDivider = 2;

u32 bbDebug1, bbDebug2;
u16 bbDebug3, bbDebug4;

RingBuffer<u8 *> bbFramePtrBuffer(64);

File blackboxFile;

u32 bbFrameNum = 0, newestPvtStartedAt = 0;
elapsedMicros frametime;
void blackboxLoop() {
	if (rp2040.fifo.available() && bbLogging && fsReady) {
		elapsedMicros taskTimer = 0;
		tasks[TASK_BLACKBOX].runCounter++;
		u8 *frame = (u8 *)rp2040.fifo.pop();
		u8 len = frame[0];
		if (len > 0 && bbLogging)
			blackboxFile.write(frame + 1, len);
		free(frame);
		u32 duration = taskTimer;
		tasks[TASK_BLACKBOX].totalDuration += duration;
		if (duration < tasks[TASK_BLACKBOX].minDuration) {
			tasks[TASK_BLACKBOX].minDuration = duration;
		}
		if (duration > tasks[TASK_BLACKBOX].maxDuration) {
			tasks[TASK_BLACKBOX].maxDuration = duration;
		}
	}
}

void initBlackbox() {
	addSetting(SETTING_BB_FLAGS, &bbFlags, 0b1111111111111111100000000000000011111111111ULL);
	addSetting(SETTING_BB_DIV, &bbFreqDivider, 2);

#if BLACKBOX_STORAGE == SD_BB
	SDFSConfig cfg(PIN_SD_SCLK, PIN_SD_CMD, PIN_SD_DAT);
	SDFS.setConfig(cfg);
	SDFS.setTimeCallback(rtcGetUnixTimestamp);
	fsReady = SDFS.begin();
	if (!SDFS.exists("/kolibri")) {
		SDFS.mkdir("/kolibri");
	}
	Serial.println(fsReady ? "SD card ready" : "SD card not ready");
#endif
}

bool clearBlackbox() {
	if (!fsReady || bbLogging)
		return false;
#if BLACKBOX_STORAGE == SD_BB
	for (int i = 0; i < 100; i++) {
		char path[32];
		snprintf(path, 32, "/kolibri/%01d.kbb", i);
		SDFS.remove(path);
		rp2040.wdt_reset();
	}
	if (!SDFS.rmdir("/kolibri")) return false;
	if (!SDFS.mkdir("/kolibri")) return false;
	return true;
#endif
}

void printFileInit(u8 serialNum, MspVersion mspVer, u16 logNum) {
	char path[32];
#if BLACKBOX_STORAGE == SD_BB
	snprintf(path, 32, "/kolibri/%01d.kbb", logNum);
	File logFile = SDFS.open(path, "r");
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
	b[6] = BLACKBOX_CHUNK_SIZE & 0xFF;
	b[7] = (BLACKBOX_CHUNK_SIZE >> 8) & 0xFF;
	b[8] = (BLACKBOX_CHUNK_SIZE >> 16) & 0xFF;
	b[9] = (BLACKBOX_CHUNK_SIZE >> 24) & 0xFF;
	sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::BB_FILE_INIT, mspVer, (char *)b, 10);
	logFile.close();
}

void printLogBin(u8 serialNum, MspVersion mspVer, u16 logNum, i32 singleChunk) {
	char path[32];
#if BLACKBOX_STORAGE == SD_BB
	snprintf(path, 32, "/kolibri/%01d.kbb", logNum);
	File logFile = SDFS.open(path, "r");
#endif
	if (!logFile) {
		sendMsp(serialNum, MspMsgType::ERROR, MspFn::BB_FILE_DOWNLOAD, mspVer, "File not found", strlen("File not found"));
		return;
	}

	u8 buffer[BLACKBOX_CHUNK_SIZE + 6];
	buffer[0] = logNum & 0xFF;
	buffer[1] = logNum >> 8;
	u32 chunkNum = 0;
	if (singleChunk >= 0) {
		chunkNum = singleChunk;
		logFile.seek(chunkNum * BLACKBOX_CHUNK_SIZE, SeekSet);
	}
	size_t bytesRead = 1;
	while (true) {
		rp2040.wdt_reset();
		if (chunkNum % 1000 == 0)
			p.neoPixelSetValue(0, chunkNum & 0xFF, chunkNum & 0xFF, chunkNum & 0xFF, true);
		bytesRead = logFile.read(buffer + 6, BLACKBOX_CHUNK_SIZE);
		if (bytesRead <= 0)
			break;
		buffer[2] = chunkNum & 0xFF;
		buffer[3] = chunkNum >> 8;
		buffer[4] = chunkNum >> 16;
		buffer[5] = chunkNum >> 24;
		sendMsp(serialNum, MspMsgType::RESPONSE, MspFn::BB_FILE_DOWNLOAD, mspVer, (char *)buffer, bytesRead + 6);
		Serial.flush();
		while (Serial.available()) // discard any data, prevents eventual panic
			Serial.read();
		chunkNum++;
		if (singleChunk >= 0)
			break;
	}
	logFile.close();
}

void startLogging() {
	if (!bbFlags || !fsReady || bbLogging || !bbFreqDivider)
		return;
	currentBBFlags = bbFlags;
	char path[32];
	for (int i = 0; i < 100; i++) {
		rp2040.wdt_reset();
#if BLACKBOX_STORAGE == SD_BB
		snprintf(path, 32, "/kolibri/%01d.kbb", (i + currentLogNum) % 100);
		if (!SDFS.exists(path)) {
			currentLogNum += i + 1;
			break;
		}
#endif
		if (i == 99) {
			// clearBlackbox();
			return;
		}
	}
#if BLACKBOX_STORAGE == SD_BB
	blackboxFile = SDFS.open(path, "a");
#endif
	if (!blackboxFile)
		return;
	const u8 data[] = {
		0x20, 0x27, 0xA1, 0x99, 0, 0, 1 // magic bytes, version
	};
	blackboxFile.write(data, 7);
	u32 recordTime = rtcGetUnixTimestamp();
	blackboxFile.write((u8 *)&recordTime, 4);
	blackboxFile.write((u8)0); // 3200Hz gyro
	blackboxFile.write((u8)bbFreqDivider);
	blackboxFile.write((u8)3); // 2000deg/sec and 16g
	i32 rf[5][3];
	for (int i = 0; i < 5; i++)
		for (int j = 0; j < 3; j++)
			rf[i][j] = rateFactors[i][j].raw;
	blackboxFile.write((u8 *)rf, 60);
	i32 pg[3][5];
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 5; j++)
			pg[i][j] = pidGains[i][j].raw;
	blackboxFile.write((u8 *)pg, 60);
	blackboxFile.write((u8 *)&bbFlags, 8);
	blackboxFile.write((u8)MOTOR_POLES);
	while (blackboxFile.position() < 256) {
		blackboxFile.write((u8)0);
	}
	bbFrameNum = 0;
	bbLogging = true;
	// 256 bytes header
	frametime = 0;
}

void endLogging() {
	if (!fsReady)
		return;
	rp2040.wdt_reset();
	if (bbLogging)
		blackboxFile.close();
	bbLogging = false;
}

void writeSingleFrame() {
	size_t bufferPos = 1;
	if (!fsReady || !bbLogging) {
		return;
	}
	u8 *bbBuffer = (u8 *)malloc(128);
	if (currentBBFlags & LOG_ROLL_ELRS_RAW) {
		bbBuffer[bufferPos++] = ELRS->channels[0];
		bbBuffer[bufferPos++] = ELRS->channels[0] >> 8;
	}
	if (currentBBFlags & LOG_PITCH_ELRS_RAW) {
		bbBuffer[bufferPos++] = ELRS->channels[1];
		bbBuffer[bufferPos++] = ELRS->channels[1] >> 8;
	}
	if (currentBBFlags & LOG_THROTTLE_ELRS_RAW) {
		bbBuffer[bufferPos++] = ELRS->channels[2];
		bbBuffer[bufferPos++] = ELRS->channels[2] >> 8;
	}
	if (currentBBFlags & LOG_YAW_ELRS_RAW) {
		bbBuffer[bufferPos++] = ELRS->channels[3];
		bbBuffer[bufferPos++] = ELRS->channels[3] >> 8;
	}
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
		frametime = 0;
		bbBuffer[bufferPos++] = ft;
		bbBuffer[bufferPos++] = ft >> 8;
	}
	if (currentBBFlags & LOG_FLIGHT_MODE) {
		bbBuffer[bufferPos++] = (u8)flightMode;
	}
	if (currentBBFlags & LOG_ALTITUDE) {
		const u32 height = combinedAltitude.raw >> 12; // 12.4 fixed point, approx. 6cm resolution, 4km altitude
		bbBuffer[bufferPos++] = height;
		bbBuffer[bufferPos++] = height >> 8;
	}
	if (currentBBFlags & LOG_VVEL) {
		const i32 vvel = vVel.raw >> 8; // 8.8 fixed point, approx. 4mm/s resolution, +-128m/s max
		bbBuffer[bufferPos++] = vvel;
		bbBuffer[bufferPos++] = vvel >> 8;
	}
	if (currentBBFlags & LOG_GPS) {
		if (bbFrameNum - newestPvtStartedAt < 46) {
			u32 pos = (bbFrameNum - newestPvtStartedAt) * 2;
			bbBuffer[bufferPos++] = currentPvtMsg[pos];
			bbBuffer[bufferPos++] = currentPvtMsg[pos + 1];
		} else if (newPvtMessageFlag & 1) {
			// 6 magic bytes to identify the start of a new PVT message
			bbBuffer[bufferPos++] = 'G';
			bbBuffer[bufferPos++] = 'P';
			newPvtMessageFlag &= ~1;
		} else if (newPvtMessageFlag & 1 << 1) {
			bbBuffer[bufferPos++] = 'S';
			bbBuffer[bufferPos++] = 'P';
			newPvtMessageFlag &= ~(1 << 1);
		} else if (newPvtMessageFlag & 1 << 2) {
			bbBuffer[bufferPos++] = 'V';
			bbBuffer[bufferPos++] = 'T';
			newPvtMessageFlag &= ~(1 << 2);
			newestPvtStartedAt = bbFrameNum + 1;
		} else {
			// placeholder 0
			bbBuffer[bufferPos++] = 0;
			bbBuffer[bufferPos++] = 0;
		}
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
		i16 i = ((fix32)accelDataFiltered[0]).geti32();
		bbBuffer[bufferPos++] = i;
		bbBuffer[bufferPos++] = i >> 8;
		i = ((fix32)accelDataFiltered[1]).geti32();
		bbBuffer[bufferPos++] = i;
		bbBuffer[bufferPos++] = i >> 8;
		i = ((fix32)accelDataFiltered[2]).geti32();
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
		i32 v = fix32(nVel).raw >> 8;
		bbBuffer[bufferPos++] = v;
		bbBuffer[bufferPos++] = v >> 8;
		v = fix32(eVel).raw >> 8;
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
}
