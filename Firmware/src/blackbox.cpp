#include "global.h"

uint64_t bbFlags		= 0;
uint64_t currentBBFlags = 0;

bool bbLogging = false;
bool fsReady   = false;

FSInfo64 fsInfo;
int currentLogNum	  = 0;
uint8_t bbFreqDivider = 2;

File blackboxFile;

int32_t maxFileSize = 0;
elapsedMicros frametime;
void blackboxLoop() {
	// Serial.println(rp2040.fifo.available());
	if (rp2040.fifo.available()) {
		uint8_t *frame = (uint8_t *)rp2040.fifo.pop();
		uint8_t len	   = frame[0];
		if (len > 0 && bbLogging)
			blackboxFile.write(frame + 1, len);
		free(frame);
	}
}

void initBlackbox() {
#if BLACKBOX_STORAGE == LITTLEFS
	fsReady = LittleFS.begin();
	fsReady = fsReady && LittleFS.info64(fsInfo);
#elif BLACKBOX_STORAGE == SD_BB
	SPI1.setRX(PIN_SD_MISO);
	SPI1.setTX(PIN_SD_MOSI);
	SPI1.setSCK(PIN_SD_SCK);
	SDFSConfig cfg;
	cfg.setCSPin(PIN_SD_CS);
	cfg.setSPI(SPI1);
	SDFS.setConfig(cfg);
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
#if BLACKBOX_STORAGE == LITTLEFS
	LittleFS.format();
	return true;
#elif BLACKBOX_STORAGE == SD_BB
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

void setFlags(uint64_t flags) {
	bbFlags = flags;
	EEPROM.put((uint16_t)EEPROM_POS::BB_FLAGS, bbFlags);
}
void setDivider(uint8_t divider) {
	if (divider > 0)
		bbFreqDivider = divider;
	EEPROM.put((uint16_t)EEPROM_POS::BB_FREQ_DIVIDER, bbFreqDivider);
}

void printLogBinRaw(uint8_t logNum) {
	char path[32];
	logNum %= 100;
	rp2040.wdt_reset();
#if BLACKBOX_STORAGE == LITTLEFS
	snprintf(path, 32, "/logs%01d/%01d.kbb", logNum / 10, logNum % 10);
	File logFile = LittleFS.open(path, "r");
#elif BLACKBOX_STORAGE == SD_BB
	snprintf(path, 32, "/kolibri/%01d.kbb", logNum);
	File logFile = SDFS.open(path, "r");
#endif
	if (!logFile)
		return;
	uint32_t fileSize = logFile.size();
	for (uint32_t i = 0; i < fileSize; i++) {
		Serial.write(logFile.read());
		if ((i % 1024) == 0) {
			rp2040.wdt_reset();
			Serial.flush();
		}
	}
	logFile.close();
}

void printLogBin(uint8_t logNum, int16_t singleChunk) {
	char path[32];
#if BLACKBOX_STORAGE == LITTLEFS
	snprintf(path, 32, "/logs%01d/%01d.kbb", logNum / 10, logNum % 10);
	File logFile = LittleFS.open(path, "r");
#elif BLACKBOX_STORAGE == SD_BB
	snprintf(path, 32, "/kolibri/%01d.kbb", logNum);
	File logFile = SDFS.open(path, "r");
#endif
	if (!logFile) {
		sendCommand(((uint16_t)ConfigCmd::BB_FILE_DOWNLOAD) | 0x8000, "File not found", strlen("File not found"));
		return;
	}
	uint8_t buffer[1027];
	buffer[0]		  = logNum;
	uint16_t chunkNum = 0;
	if (singleChunk >= 0) {
		chunkNum = singleChunk;
		logFile.seek(chunkNum * 1024, SeekSet);
	}
	size_t bytesRead = 1;
	while (bytesRead > 0) {
		rp2040.wdt_reset();
		gpio_put(PIN_LED_ACTIVITY, !gpio_get(PIN_LED_ACTIVITY));
		bytesRead = logFile.read(buffer + 3, 1024);
		buffer[1] = chunkNum & 0xFF;
		buffer[2] = chunkNum >> 8;
		sendCommand(((uint16_t)ConfigCmd::BB_FILE_DOWNLOAD) | 0x4000, (char *)buffer, bytesRead + 3);
		Serial.flush();
		chunkNum++;
		if (singleChunk >= 0)
			break;
	}
	logFile.close();
	// finish frame includes 0xFFFF as chunk number, and then the actual max chunk number
	if (singleChunk >= 0)
		return;
	buffer[0] = logNum;
	buffer[1] = 0xFF;
	buffer[2] = 0xFF;
	buffer[3] = chunkNum & 0xFF;
	buffer[4] = chunkNum >> 8;
	sendCommand(((uint16_t)ConfigCmd::BB_FILE_DOWNLOAD) | 0x4000, (char *)buffer, 5);
}

void startLogging() {
	if (!bbFlags || !fsReady || bbLogging)
		return;
	currentBBFlags = bbFlags;
#if BLACKBOX_STORAGE == LITTLEFS
	if (!(LittleFS.info64(fsInfo)))
		return;
	maxFileSize = fsInfo.totalBytes - fsInfo.usedBytes - 50000;
	if (maxFileSize < 20000) {
		return;
	}
#elif BLACKBOX_STORAGE == SD_BB
	// if (!(SDFS.info64(fsInfo)))
	// 	return;
#endif
	char path[32];
	for (int i = 0; i < 100; i++) {
		rp2040.wdt_reset();
#if BLACKBOX_STORAGE == LITTLEFS
		snprintf(path, 32, "/logs%01d/%01d.kbb", ((i + currentLogNum) % 100) / 10, (i + currentLogNum) % 10);
		if (!LittleFS.exists(path)) {
			currentLogNum += i + 1;
			break;
		}
#elif BLACKBOX_STORAGE == SD_BB
		snprintf(path, 32, "/kolibri/%01d.kbb", (i + currentLogNum) % 100);
		if (!SDFS.exists(path)) {
			currentLogNum += i + 1;
			break;
		}
#endif
		if (i == 99) {
			clearBlackbox();
			return;
		}
	}
#if BLACKBOX_STORAGE == LITTLEFS
	blackboxFile = LittleFS.open(path, "a");
#elif BLACKBOX_STORAGE == SD_BB
	blackboxFile = SDFS.open(path, "a");
#endif
	if (!blackboxFile)
		return;
	bbLogging			 = true;
	const uint8_t data[] = {
		0x20, 0x27, 0xA1, 0x99, 0, 0, 0 // magic bytes, version
	};
	blackboxFile.write(data, 7);
	uint32_t recordTime = timestamp;
	if (recordTime == 0) {
		uint32_t m = millis() / 1000;
		recordTime = (m % 60) & 0x3F;
		recordTime |= ((m / 60) % 60) << 6;
		recordTime |= ((m / 3600) % 24) << 12;
		recordTime |= 1 << 17; // days start in 1
		recordTime |= 1 << 22; // months start in 1
	}
	blackboxFile.write((uint8_t *)&recordTime, 4);
	blackboxFile.write((uint8_t)0); // 3200Hz gyro
	blackboxFile.write((uint8_t)bbFreqDivider);
	blackboxFile.write((uint8_t)3); // 2000deg/sec and 16g
	int32_t rf[5][3];
	for (int i = 0; i < 5; i++)
		for (int j = 0; j < 3; j++)
			rf[i][j] = rateFactors[i][j].getRaw();
	blackboxFile.write((uint8_t *)rf, 60);
	int32_t pg[3][7];
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 7; j++)
			pg[i][j] = pidGains[i][j].getRaw();
	blackboxFile.write((uint8_t *)pg, 84);
	blackboxFile.write((uint8_t *)&bbFlags, 8);
	// 166 bytes header
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

uint8_t bbBuffer[128];
void writeSingleFrame() {
	size_t bufferPos = 0;
	if (!fsReady || !bbLogging) {
		return;
	}
#if BLACKBOX_STORAGE == LITTLEFS
	if (blackboxFile.size() > maxFileSize) {
		endLogging();
		return;
	}
#endif
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
		int16_t setpoint	  = (int16_t)(rollSetpoint.getRaw() >> 12);
		bbBuffer[bufferPos++] = setpoint;
		bbBuffer[bufferPos++] = setpoint >> 8;
	}
	if (currentBBFlags & LOG_PITCH_SETPOINT) {
		int16_t setpoint	  = (int16_t)(pitchSetpoint.getRaw() >> 12);
		bbBuffer[bufferPos++] = setpoint;
		bbBuffer[bufferPos++] = setpoint >> 8;
	}
	if (currentBBFlags & LOG_THROTTLE_SETPOINT) {
		bbBuffer[bufferPos++] = smoothChannels[2];
		bbBuffer[bufferPos++] = smoothChannels[2] >> 8;
	}
	if (currentBBFlags & LOG_YAW_SETPOINT) {
		int16_t setpoint	  = (int16_t)(yawSetpoint.getRaw() >> 12);
		bbBuffer[bufferPos++] = setpoint;
		bbBuffer[bufferPos++] = setpoint >> 8;
	}
	if (currentBBFlags & LOG_ROLL_GYRO_RAW) {
		int16_t gyroData	  = (imuData[AXIS_ROLL].getRaw() >> 12);
		bbBuffer[bufferPos++] = gyroData;
		bbBuffer[bufferPos++] = gyroData >> 8;
	}
	if (currentBBFlags & LOG_PITCH_GYRO_RAW) {
		int16_t gyroData	  = (imuData[AXIS_PITCH].getRaw() >> 12);
		bbBuffer[bufferPos++] = gyroData;
		bbBuffer[bufferPos++] = gyroData >> 8;
	}
	if (currentBBFlags & LOG_YAW_GYRO_RAW) {
		int16_t gyroData	  = (imuData[AXIS_YAW].getRaw() >> 12);
		bbBuffer[bufferPos++] = gyroData;
		bbBuffer[bufferPos++] = gyroData >> 8;
	}
	if (currentBBFlags & LOG_ROLL_PID_P) {
		bbBuffer[bufferPos++] = rollP.getInt();
		bbBuffer[bufferPos++] = rollP.getInt() >> 8;
	}
	if (currentBBFlags & LOG_ROLL_PID_I) {
		bbBuffer[bufferPos++] = rollI.getInt();
		bbBuffer[bufferPos++] = rollI.getInt() >> 8;
	}
	if (currentBBFlags & LOG_ROLL_PID_D) {
		bbBuffer[bufferPos++] = rollD.getInt();
		bbBuffer[bufferPos++] = rollD.getInt() >> 8;
	}
	if (currentBBFlags & LOG_ROLL_PID_FF) {
		bbBuffer[bufferPos++] = rollFF.getInt();
		bbBuffer[bufferPos++] = rollFF.getInt() >> 8;
	}
	if (currentBBFlags & LOG_ROLL_PID_S) {
		bbBuffer[bufferPos++] = rollS.getInt();
		bbBuffer[bufferPos++] = rollS.getInt() >> 8;
	}
	if (currentBBFlags & LOG_PITCH_PID_P) {
		bbBuffer[bufferPos++] = pitchP.getInt();
		bbBuffer[bufferPos++] = pitchP.getInt() >> 8;
	}
	if (currentBBFlags & LOG_PITCH_PID_I) {
		bbBuffer[bufferPos++] = pitchI.getInt();
		bbBuffer[bufferPos++] = pitchI.getInt() >> 8;
	}
	if (currentBBFlags & LOG_PITCH_PID_D) {
		bbBuffer[bufferPos++] = pitchD.getInt();
		bbBuffer[bufferPos++] = pitchD.getInt() >> 8;
	}
	if (currentBBFlags & LOG_PITCH_PID_FF) {
		bbBuffer[bufferPos++] = pitchFF.getInt();
		bbBuffer[bufferPos++] = pitchFF.getInt() >> 8;
	}
	if (currentBBFlags & LOG_PITCH_PID_S) {
		bbBuffer[bufferPos++] = pitchS.getInt();
		bbBuffer[bufferPos++] = pitchS.getInt() >> 8;
	}
	if (currentBBFlags & LOG_YAW_PID_P) {
		bbBuffer[bufferPos++] = yawP.getInt();
		bbBuffer[bufferPos++] = yawP.getInt() >> 8;
	}
	if (currentBBFlags & LOG_YAW_PID_I) {
		bbBuffer[bufferPos++] = yawI.getInt();
		bbBuffer[bufferPos++] = yawI.getInt() >> 8;
	}
	if (currentBBFlags & LOG_YAW_PID_D) {
		bbBuffer[bufferPos++] = yawD.getInt();
		bbBuffer[bufferPos++] = yawD.getInt() >> 8;
	}
	if (currentBBFlags & LOG_YAW_PID_FF) {
		bbBuffer[bufferPos++] = yawFF.getInt();
		bbBuffer[bufferPos++] = yawFF.getInt() >> 8;
	}
	if (currentBBFlags & LOG_YAW_PID_S) {
		bbBuffer[bufferPos++] = yawS.getInt();
		bbBuffer[bufferPos++] = yawS.getInt() >> 8;
	}
	if (currentBBFlags & LOG_MOTOR_OUTPUTS) {
		int64_t throttles64	  = ((uint64_t)(throttles[(uint8_t)MOTOR::RR])) << 36 | ((uint64_t)throttles[(uint8_t)MOTOR::FR]) << 24 | throttles[(uint8_t)MOTOR::RL] << 12 | throttles[(uint8_t)MOTOR::FL];
		bbBuffer[bufferPos++] = throttles64 >> 40;
		bbBuffer[bufferPos++] = throttles64 >> 32;
		bbBuffer[bufferPos++] = throttles64 >> 24;
		bbBuffer[bufferPos++] = throttles64 >> 16;
		bbBuffer[bufferPos++] = throttles64 >> 8;
		bbBuffer[bufferPos++] = throttles64;
	}
	if (currentBBFlags & LOG_ALTITUDE) {
		bbBuffer[bufferPos++] = baroATO;
		bbBuffer[bufferPos++] = baroATO >> 8;
	}
	if (currentBBFlags & LOG_FRAMETIME) {
		uint16_t ft			  = frametime;
		frametime			  = 0;
		bbBuffer[bufferPos++] = ft;
		bbBuffer[bufferPos++] = ft >> 8;
	}
#if BLACKBOX_STORAGE == LITTLEFS
	blackboxFile.write(bbBuffer, bufferPos);
#elif BLACKBOX_STORAGE == SD_BB
	void *buf = malloc(bufferPos + 1);
	memcpy((void *)((uint8_t *)buf + 1), bbBuffer, bufferPos);
	((uint8_t *)buf)[0] = bufferPos;
	if (!(rp2040.fifo.push_nb((uint32_t)buf))) {
		free(buf);
		// Serial.println("FIFO full");
		// if the fifo is full, we just drop the frame :(
	}
#endif
}