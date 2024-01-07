#include "global.h"

uint8_t configSerialBuffer[256] = {0};
uint8_t configSerialBufferIndex = 0;
uint8_t configMsgLength			= 0;
uint16_t configMsgCommand		= 0;

elapsedMillis configTimer = 0;

elapsedMillis configOverrideMotors = 1001;

elapsedMillis lastConfigPing = 0;
bool configuratorConnected	 = false;

void configuratorLoop() {
	if (lastConfigPing > 1000)
		configuratorConnected = false;
}

void sendCommand(uint16_t command, const char *data, uint16_t len) {
	Serial.write('_');
	Serial.write('K');
	Serial.write(len & 0xFF);
	Serial.write(len >> 8);
	Serial.write(command & 0xFF);
	Serial.write(command >> 8);

	if (data == nullptr)
		len = 0;
	uint8_t crc = (command & 0xFF) ^ (command >> 8) ^ (len & 0xFF) ^ (len >> 8);
	for (int i = 0; i < len; i++) {
		Serial.write(data[i]);
		crc ^= data[i];
	}
	Serial.write(crc);
}

void handleConfigCmd() {
	char buf[256] = {0};
	uint8_t len	  = 0;
	switch ((ConfigCmd)configMsgCommand) {
	case ConfigCmd::STATUS: {
		uint16_t voltage = adcVoltage;
		buf[len++]		 = voltage & 0xFF;
		buf[len++]		 = voltage >> 8;
		buf[len++]		 = armed;
		buf[len++]		 = (uint8_t)flightMode;
		buf[len++]		 = (uint8_t)(armingDisableFlags & 0xFF);
		buf[len++]		 = (uint8_t)(armingDisableFlags >> 8);
		buf[len++]		 = (uint8_t)(armingDisableFlags >> 16);
		buf[len++]		 = (uint8_t)(armingDisableFlags >> 24);
		buf[len++]		 = (uint8_t)(configuratorConnected & 0xFF);
		sendCommand(configMsgCommand | 0x4000, buf, len);
	} break;
	case ConfigCmd::TASK_STATUS:
		len = snprintf(buf, 256, "PID loop counter: %d", pidLoopCounter);
		sendCommand(configMsgCommand | 0x4000, buf, len);
		break;
	case ConfigCmd::REBOOT:
		sendCommand(configMsgCommand | 0x4000);
		delay(100);
		rp2040.reboot();
		break;
	case ConfigCmd::SAVE_SETTINGS:
		rp2040.wdt_reset();
		EEPROM.commit();
		sendCommand(configMsgCommand | 0x4000);
		break;
	case ConfigCmd::PLAY_SOUND: {
		const uint16_t startFreq	 = random(1000, 5000);
		const uint16_t endFreq		 = random(1000, 5000);
		const uint16_t sweepDuration = random(400, 1000);
		uint16_t pauseDuration		 = random(100, 1000);
		const uint16_t pauseEn		 = random(0, 2);
		pauseDuration *= pauseEn;
		const uint16_t repeat = random(1, 11);
		makeSweepSound(startFreq, endFreq, ((sweepDuration + pauseDuration) * repeat) - 1, sweepDuration, pauseDuration);
		uint8_t len = 0;
		buf[len++]	= startFreq & 0xFF;
		buf[len++]	= startFreq >> 8;
		buf[len++]	= endFreq & 0xFF;
		buf[len++]	= endFreq >> 8;
		buf[len++]	= sweepDuration & 0xFF;
		buf[len++]	= sweepDuration >> 8;
		buf[len++]	= pauseDuration & 0xFF;
		buf[len++]	= pauseDuration >> 8;
		buf[len++]	= pauseEn;
		buf[len++]	= repeat;
		buf[len++]	= (((sweepDuration + pauseDuration) * repeat) - 1) & 0xFF;
		buf[len++]	= (((sweepDuration + pauseDuration) * repeat) - 1) >> 8;
		sendCommand(configMsgCommand | 0x4000, buf, len);
	} break;
	case ConfigCmd::BB_FILE_LIST: {
		int index		  = 0;
		char shortbuf[16] = {0};
		for (int i = 0; i < 100; i++) {
			rp2040.wdt_reset();
#if BLACKBOX_STORAGE == LITTLEFS_BB
			snprintf(shortbuf, 16, "/logs%01d/%01d.kbb", i / 10, i % 10);
			if (LittleFS.exists(shortbuf)) {
				buf[index++] = i;
			}
#elif BLACKBOX_STORAGE == SD_BB
			snprintf(shortbuf, 16, "/kolibri/%01d.kbb", i);
			if (SDFS.exists(shortbuf)) {
				buf[index++] = i;
			}
#endif
		}
		sendCommand(configMsgCommand | 0x4000, buf, index);
	} break;
	case ConfigCmd::BB_FILE_DOWNLOAD: {
		uint8_t fileNum	 = configSerialBuffer[CONFIG_BUFFER_DATA];
		int16_t chunkNum = -1;
		if (configMsgLength > 1) {
			chunkNum = configSerialBuffer[CONFIG_BUFFER_DATA + 1] + (configSerialBuffer[CONFIG_BUFFER_DATA + 2] << 8);
		}
		printLogBin(fileNum, chunkNum);
	} break;
	case ConfigCmd::BB_FILE_DELETE: {
		// data just includes one byte of file number
		uint8_t fileNum = configSerialBuffer[CONFIG_BUFFER_DATA];
		char path[32];
#if BLACKBOX_STORAGE == LITTLEFS_BB
		snprintf(path, 32, "/logs%01d/%01d.kbb", fileNum / 10, fileNum % 10);
		if (LittleFS.remove(path))
#elif BLACKBOX_STORAGE == SD_BB
		snprintf(path, 32, "/kolibri/%01d.kbb", fileNum);
		if (SDFS.remove(path))
#endif
			sendCommand(configMsgCommand | 0x4000, (char *)&fileNum, 1);
		else
			sendCommand(configMsgCommand | 0x8000, (char *)&fileNum, 1);
	} break;
	case ConfigCmd::BB_FILE_INFO: {
		/* data of command
		 * 0...len-1: file numbers
		 *
		 * data of response
		 * 0. file number
		 * 1-4. file size in bytes
		 * 5-7. version of bb file format
		 * 8-11: time of recording start
		 * 12. byte that indicates PID frequency
		 * 13. byte that indicates frequency divider
		 * 14-21: recording flags
		 */
		uint8_t len		  = configSerialBuffer[CONFIG_BUFFER_LENGTH];
		len				  = len > 12 ? 12 : len;
		uint8_t *fileNums = &configSerialBuffer[CONFIG_BUFFER_DATA];
		uint8_t buffer[22 * len];
		uint8_t index = 0;
		for (int i = 0; i < len; i++) {
			rp2040.wdt_reset();
			char path[32];
			uint8_t fileNum = fileNums[i];
#if BLACKBOX_STORAGE == LITTLEFS_BB
			snprintf(path, 32, "/logs%01d/%01d.kbb", fileNum / 10, fileNum % 10);
			File logFile = LittleFS.open(path, "r");
#elif BLACKBOX_STORAGE == SD_BB
			snprintf(path, 32, "/kolibri/%01d.kbb", fileNum);
			File logFile = SDFS.open(path, FILE_READ);
#endif
			if (!logFile)
				continue;
			buffer[index++] = fileNum;
			buffer[index++] = logFile.size() & 0xFF;
			buffer[index++] = (logFile.size() >> 8) & 0xFF;
			buffer[index++] = (logFile.size() >> 16) & 0xFF;
			buffer[index++] = (logFile.size() >> 24) & 0xFF;
			logFile.seek(LOG_HEAD_BB_VERSION, SeekSet);
			// version, timestamp, pid and divider can directly be read from the file
			for (int i = 0; i < 9; i++)
				buffer[index++] = logFile.read();
			logFile.seek(LOG_HEAD_LOGGED_FIELDS, SeekSet);
			// flags
			for (int i = 0; i < 8; i++)
				buffer[index++] = logFile.read();
			logFile.close();
		}
		sendCommand(configMsgCommand | 0x4000, (char *)buffer, index);
	} break;
	case ConfigCmd::BB_FORMAT:
		if (clearBlackbox())
			sendCommand(configMsgCommand | 0x4000);
		else
			sendCommand(configMsgCommand | 0x8000);
		break;
	case ConfigCmd::WRITE_OSD_FONT_CHARACTER:
		if (configMsgLength < 55) {
			sendCommand(configMsgCommand | 0x8000);
			break;
		}
		updateCharacter(configSerialBuffer[CONFIG_BUFFER_DATA], &configSerialBuffer[CONFIG_BUFFER_DATA + 1]);
		sendCommand(configMsgCommand | 0x4000, (const char *)&configSerialBuffer[CONFIG_BUFFER_DATA], 1);
		break;
	case ConfigCmd::SET_MOTORS:
		throttles[(uint8_t)MOTOR::RR] = (uint16_t)configSerialBuffer[CONFIG_BUFFER_DATA + 0] + ((uint16_t)configSerialBuffer[CONFIG_BUFFER_DATA + 1] << 8);
		throttles[(uint8_t)MOTOR::FR] = (uint16_t)configSerialBuffer[CONFIG_BUFFER_DATA + 2] + ((uint16_t)configSerialBuffer[CONFIG_BUFFER_DATA + 3] << 8);
		throttles[(uint8_t)MOTOR::RL] = (uint16_t)configSerialBuffer[CONFIG_BUFFER_DATA + 4] + ((uint16_t)configSerialBuffer[CONFIG_BUFFER_DATA + 5] << 8);
		throttles[(uint8_t)MOTOR::FL] = (uint16_t)configSerialBuffer[CONFIG_BUFFER_DATA + 6] + ((uint16_t)configSerialBuffer[CONFIG_BUFFER_DATA + 7] << 8);
		configOverrideMotors		  = 0;
		sendCommand(configMsgCommand | 0x4000);
		break;
	case ConfigCmd::GET_MOTORS: {
		uint16_t motors[4];
		motors[0] = throttles[(uint8_t)MOTOR::RR];
		motors[1] = throttles[(uint8_t)MOTOR::FR];
		motors[2] = throttles[(uint8_t)MOTOR::RL];
		motors[3] = throttles[(uint8_t)MOTOR::FL];
		sendCommand(configMsgCommand | 0x4000, (char *)motors, sizeof(motors));
	} break;
	case ConfigCmd::BB_FILE_DOWNLOAD_RAW:
		printLogBinRaw(configSerialBuffer[CONFIG_BUFFER_DATA]);
		break;
	case ConfigCmd::SET_DEBUG_LED:
		gpio_put(PIN_LED_DEBUG, configSerialBuffer[CONFIG_BUFFER_DATA]);
		sendCommand(configMsgCommand | 0x4000);
		Serial.println("Set debug LED");
		break;
	case ConfigCmd::CONFIGURATOR_PING:
		sendCommand(configMsgCommand | 0x4000);
		configuratorConnected = true;
		lastConfigPing		  = 0;
		break;
	case ConfigCmd::REBOOT_TO_BOOTLOADER:
		sendCommand(configMsgCommand | 0x4000);
		delay(100);
		rp2040.rebootToBootloader();
		break;
	case ConfigCmd::GET_NAME: {
		char name[20] = {0};
		for (int i = 0; i < 20; i++)
			name[i] = EEPROM.read((uint16_t)EEPROM_POS::UAV_NAME + i);
		name[19] = '\0';
		sendCommand(configMsgCommand | 0x4000, name, strlen(name));
	} break;
	case ConfigCmd::SET_NAME: {
		uint8_t len = configSerialBuffer[CONFIG_BUFFER_LENGTH];
		if (len > 20)
			sendCommand(configMsgCommand | 0x8000);
		break;
		for (int i = 0; i < len; i++)
			EEPROM.write((uint16_t)EEPROM_POS::UAV_NAME + i, configSerialBuffer[CONFIG_BUFFER_DATA + i]);
		sendCommand(configMsgCommand | 0x4000);
		break;
	}
	case ConfigCmd::GET_PIDS: {
		uint16_t pids[3][7];
		for (int i = 0; i < 3; i++) {
			pids[i][0] = pidGains[i][0].getRaw() >> P_SHIFT;
			pids[i][1] = pidGains[i][1].getRaw() >> I_SHIFT;
			pids[i][2] = pidGains[i][2].getRaw() >> D_SHIFT;
			pids[i][3] = pidGains[i][3].getRaw() >> FF_SHIFT;
			pids[i][4] = pidGains[i][4].getRaw() >> S_SHIFT;
			pids[i][5] = pidGains[i][5].getRaw() & 0xFFFF;
			pids[i][6] = 0;
		}
		sendCommand(configMsgCommand | 0x4000, (char *)pids, sizeof(pids));
	} break;
	case ConfigCmd::SET_PIDS: {
		uint16_t pids[3][7];
		memcpy(pids, &configSerialBuffer[CONFIG_BUFFER_DATA], sizeof(pids));
		for (int i = 0; i < 3; i++) {
			pidGains[i][0].setRaw(pids[i][0] << P_SHIFT);
			pidGains[i][1].setRaw(pids[i][1] << I_SHIFT);
			pidGains[i][2].setRaw(pids[i][2] << D_SHIFT);
			pidGains[i][3].setRaw(pids[i][3] << FF_SHIFT);
			pidGains[i][4].setRaw(pids[i][4] << S_SHIFT);
			pidGains[i][5].setRaw(pids[i][5]);
		}
		EEPROM.put((uint16_t)EEPROM_POS::PID_GAINS, pidGains);
		sendCommand(configMsgCommand | 0x4000);
	} break;
	case ConfigCmd::GET_RATES: {
		uint16_t rates[3][5];
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 5; j++)
				rates[i][j] = rateFactors[j][i].getInt();
		sendCommand(configMsgCommand | 0x4000, (char *)rates, sizeof(rates));
	} break;
	case ConfigCmd::SET_RATES: {
		uint16_t rates[3][5];
		memcpy(rates, &configSerialBuffer[CONFIG_BUFFER_DATA], sizeof(rates));
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 5; j++)
				rateFactors[j][i] = rates[i][j];
		sendCommand(configMsgCommand | 0x4000);
		EEPROM.put((uint16_t)EEPROM_POS::RATE_FACTORS, rateFactors);
	} break;
	case ConfigCmd::GET_BB_SETTINGS: {
		uint8_t bbSettings[9];
		bbSettings[0] = bbFreqDivider;
		memcpy(&bbSettings[1], &bbFlags, 8);
		sendCommand(configMsgCommand | 0x4000, (char *)bbSettings, sizeof(bbSettings));
	} break;
	case ConfigCmd::SET_BB_SETTINGS: {
		uint8_t bbSettings[9];
		memcpy(bbSettings, &configSerialBuffer[CONFIG_BUFFER_DATA], sizeof(bbSettings));
		bbFreqDivider = bbSettings[0];
		memcpy(&bbFlags, &bbSettings[1], 8);
		sendCommand(configMsgCommand | 0x4000);
		EEPROM.put((uint16_t)EEPROM_POS::BB_FLAGS, bbFlags);
		EEPROM.put((uint16_t)EEPROM_POS::BB_FREQ_DIVIDER, bbFreqDivider);
	} break;
	case ConfigCmd::GET_ROTATION: {
		// https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
		int rotationPitch = pitch * 8192;
		int rotationRoll  = roll * 8192;
		int rotationYaw	  = yaw * 8192;
		buf[0]			  = rotationPitch & 0xFF;
		buf[1]			  = rotationPitch >> 8;
		buf[2]			  = rotationRoll & 0xFF;
		buf[3]			  = rotationRoll >> 8;
		buf[4]			  = rotationYaw & 0xFF;
		buf[5]			  = rotationYaw >> 8;
		sendCommand(configMsgCommand | 0x4000, buf, 6);
	} break;
	// case ConfigCmd::GET_ROTATION: {
	// 	float rMat[3][3];
	// 	float wx		  = q.w * q.v[0];
	// 	float wy		  = q.w * q.v[1];
	// 	float wz		  = q.w * q.v[2];
	// 	float xx		  = q.v[0] * q.v[0];
	// 	float xy		  = q.v[0] * q.v[1];
	// 	float xz		  = q.v[0] * q.v[2];
	// 	float yy		  = q.v[1] * q.v[1];
	// 	float yz		  = q.v[1] * q.v[2];
	// 	float zz		  = q.v[2] * q.v[2];
	// 	rMat[0][0]		  = 1 - 2 * (yy + zz);
	// 	rMat[0][1]		  = 2 * (xy - wz);
	// 	rMat[0][2]		  = 2 * (xz + wy);
	// 	rMat[1][0]		  = 2 * (xy + wz);
	// 	rMat[1][1]		  = 1 - 2 * (xx + zz);
	// 	rMat[1][2]		  = 2 * (yz - wx);
	// 	rMat[2][0]		  = 2 * (xz - wy);
	// 	rMat[2][1]		  = 2 * (yz + wx);
	// 	rMat[2][2]		  = 1 - 2 * (xx + yy);
	// 	float rollX		  = atan2f(rMat[2][1], rMat[2][2]);
	// 	float pitchX	  = (0.5f * (float)PI) - acosf(-rMat[2][0]);
	// 	float yawX		  = -atan2f(rMat[1][0], rMat[0][0]);
	// 	int rotationPitch = pitchX * 8192;
	// 	int rotationRoll  = rollX * 8192;
	// 	int rotationYaw	  = yawX * 8192;
	// 	buf[0]			  = rotationPitch & 0xFF;
	// 	buf[1]			  = rotationPitch >> 8;
	// 	buf[2]			  = rotationRoll & 0xFF;
	// 	buf[3]			  = rotationRoll >> 8;
	// 	buf[4]			  = rotationYaw & 0xFF;
	// 	buf[5]			  = rotationYaw >> 8;
	// 	sendCommand(configMsgCommand | 0x4000, buf, 6);
	// } break;
	case ConfigCmd::SERIAL_PASSTHROUGH: {
		uint8_t serialNum = configSerialBuffer[CONFIG_BUFFER_DATA];
		uint32_t baud	  = DECODE_U4(&configSerialBuffer[CONFIG_BUFFER_DATA + 1]);
		sendCommand(configMsgCommand | 0x4000, (char *)&configSerialBuffer[CONFIG_BUFFER_DATA], 5);
		uint8_t plusCount			  = 0;
		elapsedMillis breakoutCounter = 0;
		switch (serialNum) {
		case 1:
			// Serial1.end();
			// Serial1.begin(baud);
			while (true) {
				if (breakoutCounter > 1000 && plusCount >= 3) break;
				if (Serial.available()) {
					breakoutCounter = 0;

					char c = Serial.read();
					if (c == '+')
						plusCount++;
					else
						plusCount = 0;
					Serial1.write(c);
				}
				if (Serial1.available()) {
					Serial.write(Serial1.read());
					Serial.flush();
				}
				rp2040.wdt_reset();
			}
			break;
		case 2:
			Serial2.end();
			Serial2.begin(baud);
			while (true) {
				if (breakoutCounter > 1000 && plusCount == 3) break;
				if (Serial.available()) {
					breakoutCounter = 0;

					char c = Serial.read();
					if (c == '+')
						plusCount++;
					else
						plusCount = 0;
					Serial2.write(c);
				}
				if (Serial2.available()) {
					Serial.write(Serial2.read());
					Serial.flush();
				}
				rp2040.wdt_reset();
			}
			break;
		}
	} break;
	case ConfigCmd::GET_GPS_ACCURACY: {
		// through padding and compiler optimization, it is not possible to just memcpy the struct
		memcpy(buf, &gpsAcc.tAcc, 4);
		memcpy(&buf[4], &gpsAcc.hAcc, 4);
		memcpy(&buf[8], &gpsAcc.vAcc, 4);
		memcpy(&buf[12], &gpsAcc.sAcc, 4);
		memcpy(&buf[16], &gpsAcc.headAcc, 4);
		memcpy(&buf[20], &gpsAcc.pDop, 4);
		sendCommand(configMsgCommand | 0x4000, buf, 24);
	} break;
	case ConfigCmd::GET_GPS_STATUS: {
		buf[0] = gpsStatus.gpsInited;
		buf[1] = gpsStatus.initStep;
		buf[2] = gpsStatus.fixType;
		buf[3] = gpsStatus.timeValidityFlags;
		buf[4] = gpsStatus.flags;
		buf[5] = gpsStatus.flags2;
		buf[6] = gpsStatus.flags3 & 0xFF;
		buf[7] = gpsStatus.flags3 >> 8;
		buf[8] = gpsStatus.satCount;
		sendCommand(configMsgCommand | 0x4000, buf, 9);
	} break;
	case ConfigCmd::GET_GPS_TIME: {
		buf[0] = gpsTime.year & 0xFF;
		buf[1] = gpsTime.year >> 8;
		buf[2] = gpsTime.month;
		buf[3] = gpsTime.day;
		buf[4] = gpsTime.hour;
		buf[5] = gpsTime.minute;
		buf[6] = gpsTime.second;
		sendCommand(configMsgCommand | 0x4000, buf, 7);
	} break;
	case ConfigCmd::GET_GPS_MOTION: {
		int32_t vVel2 = (combinedAltitude * 1000).getInt();
		memcpy(buf, &gpsMotion.lat, 4);
		memcpy(&buf[4], &gpsMotion.lon, 4);
		memcpy(&buf[8], &gpsMotion.alt, 4);
		memcpy(&buf[12], &gpsMotion.velN, 4);
		memcpy(&buf[16], &gpsMotion.velE, 4);
		memcpy(&buf[20], &vVel2, 4);
		memcpy(&buf[24], &gpsMotion.gSpeed, 4);
		memcpy(&buf[28], &gpsMotion.headMot, 4);
		sendCommand(configMsgCommand | 0x4000, buf, 32);
	} break;
	case ConfigCmd::REBOOT_BY_WATCHDOG:
		delay(1000);
		break;
	case ConfigCmd::GET_CRASH_DUMP: {
		for (int i = 0; i < 256; i++) {
			rp2040.wdt_reset();
			buf[i] = EEPROM.read(4096 - 256 + i);
		}
		sendCommand(configMsgCommand | 0x4000, buf, 256);
	} break;
	case ConfigCmd::CLEAR_CRASH_DUMP: {
		for (int i = 0; i < 256; i++) {
			rp2040.wdt_reset();
			EEPROM.write(4096 - 256 + i, 0);
		}
		sendCommand(configMsgCommand | 0x4000);
	} break;
	default: {
		sendCommand(configMsgCommand | 0x8000, "Unknown command", strlen("Unknown command"));
	} break;
	}
}

void configuratorHandleByte(uint8_t c, uint8_t _serialNum) {
	configSerialBuffer[configSerialBufferIndex++] = c;
	// every message from the configurator starts with _K, followed by two bytes of data length (only for the data, thus between 0 and 65535), then 2 bytes command, then the data, then the checksum (1 byte XOR of length, command and data)
	// 2 bytes prefix, 2 bytes length, 2 bytes command, data, 1 byte checksum
	if (configSerialBufferIndex == 1 && c != '_') {
		configSerialBufferIndex = 0;
		return;
	} else if (configSerialBufferIndex == 2 && c != 'K') {
		configSerialBufferIndex = 0;
		return;
	} else if (configSerialBufferIndex == 4) {
		configMsgLength = configSerialBuffer[2] + (configSerialBuffer[3] << 8); // low byte first
	} else if (configSerialBufferIndex == 6) {
		configMsgCommand = configSerialBuffer[4] + (configSerialBuffer[5] << 8); // low byte first
	}
	if (configSerialBufferIndex == configMsgLength + 7) {
		uint8_t crc = 0;
		for (int i = 2; i < configMsgLength + 7; i++)
			crc ^= configSerialBuffer[i];
		if (crc != 0) {
			configSerialBufferIndex = 0;
			Serial.printf("CRC error");
			return;
		}
		handleConfigCmd();
		configSerialBufferIndex = 0;
	}
}