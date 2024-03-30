#include "global.h"

u8 configSerialBuffer[256] = {0};
u8 configSerialBufferIndex = 0;
u8 configMsgLength         = 0;
u16 configMsgCommand       = 0;

elapsedMillis configTimer = 0;

elapsedMillis configOverrideMotors = 1001;

elapsedMillis lastConfigPingRx = 0;
elapsedMillis lastConfigPingTx = 0;
bool configuratorConnected     = false;

void configuratorLoop() {
	if (lastConfigPingRx > 1000)
		configuratorConnected = false;
	if (lastConfigPingTx > 1000 && configuratorConnected) {
		sendCommand((u16)ConfigCmd::CONFIGURATOR_PING);
		lastConfigPingTx = 0;
	}
}

void sendCommand(u16 command, const char *data, u16 len) {
	Serial.write('_');
	Serial.write('K');
	Serial.write(len & 0xFF);
	Serial.write(len >> 8);
	Serial.write(command & 0xFF);
	Serial.write(command >> 8);

	if (data == nullptr)
		len = 0;
	u8 crc = (command & 0xFF) ^ (command >> 8) ^ (len & 0xFF) ^ (len >> 8);
	for (int i = 0; i < len; i++) {
		Serial.write(data[i]);
		crc ^= data[i];
	}
	Serial.write(crc);
}

void handleConfigCmd() {
	char buf[256] = {0};
	u8 len        = 0;
	switch ((ConfigCmd)configMsgCommand) {
	case ConfigCmd::STATUS: {
		u16 voltage = adcVoltage;
		buf[len++]  = voltage & 0xFF;
		buf[len++]  = voltage >> 8;
		buf[len++]  = armed;
		buf[len++]  = (u8)flightMode;
		buf[len++]  = (u8)(armingDisableFlags & 0xFF);
		buf[len++]  = (u8)(armingDisableFlags >> 8);
		buf[len++]  = (u8)(armingDisableFlags >> 16);
		buf[len++]  = (u8)(armingDisableFlags >> 24);
		buf[len++]  = (u8)(configuratorConnected & 0xFF);
		sendCommand(configMsgCommand | 0x4000, buf, len);
	} break;
	case ConfigCmd::TASK_STATUS: {
		u32 buf[256];
		for (int i = 0; i < 32; i++) {
			buf[i * 8 + 0] = tasks[i].debugInfo;
			buf[i * 8 + 1] = tasks[i].minDuration;
			buf[i * 8 + 2] = tasks[i].maxDuration;
			buf[i * 8 + 3] = tasks[i].frequency;
			buf[i * 8 + 4] = tasks[i].avgDuration;
			buf[i * 8 + 5] = tasks[i].errorCount;
			buf[i * 8 + 6] = tasks[i].lastError;
			buf[i * 8 + 7] = tasks[i].maxGap;
		}
		sendCommand(configMsgCommand | 0x4000, (char *)buf, sizeof(buf));
		for (int i = 0; i < 32; i++) {
			tasks[i].minDuration = 0xFFFFFFFF;
			tasks[i].maxDuration = 0;
			tasks[i].maxGap      = 0;
		}
	} break;
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
		const u16 startFreq     = random(1000, 5000);
		const u16 endFreq       = random(1000, 5000);
		const u16 sweepDuration = random(400, 1000);
		u16 pauseDuration       = random(100, 1000);
		const u16 pauseEn       = random(0, 2);
		pauseDuration *= pauseEn;
		const u16 repeat = random(1, 11);
		makeSweepSound(startFreq, endFreq, ((sweepDuration + pauseDuration) * repeat) - 1, sweepDuration, pauseDuration);
		u8 len     = 0;
		buf[len++] = startFreq & 0xFF;
		buf[len++] = startFreq >> 8;
		buf[len++] = endFreq & 0xFF;
		buf[len++] = endFreq >> 8;
		buf[len++] = sweepDuration & 0xFF;
		buf[len++] = sweepDuration >> 8;
		buf[len++] = pauseDuration & 0xFF;
		buf[len++] = pauseDuration >> 8;
		buf[len++] = pauseEn;
		buf[len++] = repeat;
		buf[len++] = (((sweepDuration + pauseDuration) * repeat) - 1) & 0xFF;
		buf[len++] = (((sweepDuration + pauseDuration) * repeat) - 1) >> 8;
		sendCommand(configMsgCommand | 0x4000, buf, len);
	} break;
	case ConfigCmd::BB_FILE_LIST: {
		int index         = 0;
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
		u8 fileNum   = configSerialBuffer[CONFIG_BUFFER_DATA];
		i16 chunkNum = -1;
		if (configMsgLength > 1) {
			chunkNum = configSerialBuffer[CONFIG_BUFFER_DATA + 1] + (configSerialBuffer[CONFIG_BUFFER_DATA + 2] << 8);
		}
		printLogBin(fileNum, chunkNum);
	} break;
	case ConfigCmd::BB_FILE_DELETE: {
		// data just includes one byte of file number
		u8 fileNum = configSerialBuffer[CONFIG_BUFFER_DATA];
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
		u8 len       = configSerialBuffer[CONFIG_BUFFER_LENGTH];
		len          = len > 12 ? 12 : len;
		u8 *fileNums = &configSerialBuffer[CONFIG_BUFFER_DATA];
		u8 buffer[22 * len];
		u8 index = 0;
		for (int i = 0; i < len; i++) {
			rp2040.wdt_reset();
			char path[32];
			u8 fileNum = fileNums[i];
#if BLACKBOX_STORAGE == LITTLEFS_BB
			snprintf(path, 32, "/logs%01d/%01d.kbb", fileNum / 10, fileNum % 10);
			File logFile = LittleFS.open(path, "r");
#elif BLACKBOX_STORAGE == SD_BB
			snprintf(path, 32, "/kolibri/%01d.kbb", fileNum);
			File logFile = SDFS.open(path, "r");
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
		throttles[(u8)MOTOR::RR] = (u16)configSerialBuffer[CONFIG_BUFFER_DATA + 0] + ((u16)configSerialBuffer[CONFIG_BUFFER_DATA + 1] << 8);
		throttles[(u8)MOTOR::FR] = (u16)configSerialBuffer[CONFIG_BUFFER_DATA + 2] + ((u16)configSerialBuffer[CONFIG_BUFFER_DATA + 3] << 8);
		throttles[(u8)MOTOR::RL] = (u16)configSerialBuffer[CONFIG_BUFFER_DATA + 4] + ((u16)configSerialBuffer[CONFIG_BUFFER_DATA + 5] << 8);
		throttles[(u8)MOTOR::FL] = (u16)configSerialBuffer[CONFIG_BUFFER_DATA + 6] + ((u16)configSerialBuffer[CONFIG_BUFFER_DATA + 7] << 8);
		configOverrideMotors     = 0;
		sendCommand(configMsgCommand | 0x4000);
		break;
	case ConfigCmd::GET_MOTORS: {
		u16 motors[4];
		motors[0] = throttles[(u8)MOTOR::RR];
		motors[1] = throttles[(u8)MOTOR::FR];
		motors[2] = throttles[(u8)MOTOR::RL];
		motors[3] = throttles[(u8)MOTOR::FL];
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
		lastConfigPingRx      = 0;
		break;
	case (ConfigCmd)((u16)ConfigCmd::CONFIGURATOR_PING | 0x4000): {
		u32 duration = lastConfigPingTx;
		sendCommand((u16)ConfigCmd::CONFIGURATOR_PING | 0xC000, (char *)&duration, 4);
	} break;
	case ConfigCmd::REBOOT_TO_BOOTLOADER:
		sendCommand(configMsgCommand | 0x4000);
		delay(100);
		rp2040.rebootToBootloader();
		break;
	case ConfigCmd::GET_NAME: {
		char name[20] = {0};
		for (int i = 0; i < 20; i++)
			name[i] = EEPROM.read((u16)EEPROM_POS::UAV_NAME + i);
		name[19] = '\0';
		sendCommand(configMsgCommand | 0x4000, name, strlen(name));
	} break;
	case ConfigCmd::SET_NAME: {
		u8 len = configSerialBuffer[CONFIG_BUFFER_LENGTH];
		if (len > 20)
			sendCommand(configMsgCommand | 0x8000);
		break;
		for (int i = 0; i < len; i++)
			EEPROM.write((u16)EEPROM_POS::UAV_NAME + i, configSerialBuffer[CONFIG_BUFFER_DATA + i]);
		sendCommand(configMsgCommand | 0x4000);
		break;
	}
	case ConfigCmd::GET_PIDS: {
		u16 pids[3][7];
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
		u16 pids[3][7];
		memcpy(pids, &configSerialBuffer[CONFIG_BUFFER_DATA], sizeof(pids));
		for (int i = 0; i < 3; i++) {
			pidGains[i][0].setRaw(pids[i][0] << P_SHIFT);
			pidGains[i][1].setRaw(pids[i][1] << I_SHIFT);
			pidGains[i][2].setRaw(pids[i][2] << D_SHIFT);
			pidGains[i][3].setRaw(pids[i][3] << FF_SHIFT);
			pidGains[i][4].setRaw(pids[i][4] << S_SHIFT);
			pidGains[i][5].setRaw(pids[i][5]);
		}
		EEPROM.put((u16)EEPROM_POS::PID_GAINS, pidGains);
		sendCommand(configMsgCommand | 0x4000);
	} break;
	case ConfigCmd::GET_RATES: {
		u16 rates[3][5];
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 5; j++)
				rates[i][j] = rateFactors[j][i].getInt();
		sendCommand(configMsgCommand | 0x4000, (char *)rates, sizeof(rates));
	} break;
	case ConfigCmd::SET_RATES: {
		u16 rates[3][5];
		memcpy(rates, &configSerialBuffer[CONFIG_BUFFER_DATA], sizeof(rates));
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 5; j++)
				rateFactors[j][i] = rates[i][j];
		sendCommand(configMsgCommand | 0x4000);
		EEPROM.put((u16)EEPROM_POS::RATE_FACTORS, rateFactors);
	} break;
	case ConfigCmd::GET_BB_SETTINGS: {
		u8 bbSettings[9];
		bbSettings[0] = bbFreqDivider;
		memcpy(&bbSettings[1], &bbFlags, 8);
		sendCommand(configMsgCommand | 0x4000, (char *)bbSettings, sizeof(bbSettings));
	} break;
	case ConfigCmd::SET_BB_SETTINGS: {
		u8 bbSettings[9];
		memcpy(bbSettings, &configSerialBuffer[CONFIG_BUFFER_DATA], sizeof(bbSettings));
		bbFreqDivider = bbSettings[0];
		memcpy(&bbFlags, &bbSettings[1], 8);
		sendCommand(configMsgCommand | 0x4000);
		EEPROM.put((u16)EEPROM_POS::BB_FLAGS, bbFlags);
		EEPROM.put((u16)EEPROM_POS::BB_FREQ_DIVIDER, bbFreqDivider);
	} break;
	case ConfigCmd::GET_ROTATION: {
		// https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
		int rotationPitch = pitch * 8192;
		int rotationRoll  = roll * 8192;
		int rotationYaw   = yaw * 8192;
		buf[0]            = rotationPitch & 0xFF;
		buf[1]            = rotationPitch >> 8;
		buf[2]            = rotationRoll & 0xFF;
		buf[3]            = rotationRoll >> 8;
		buf[4]            = rotationYaw & 0xFF;
		buf[5]            = rotationYaw >> 8;
		sendCommand(configMsgCommand | 0x4000, buf, 6);
	} break;
	case ConfigCmd::SERIAL_PASSTHROUGH: {
		u8 serialNum = configSerialBuffer[CONFIG_BUFFER_DATA];
		u32 baud     = DECODE_U4(&configSerialBuffer[CONFIG_BUFFER_DATA + 1]);
		sendCommand(configMsgCommand | 0x4000, (char *)&configSerialBuffer[CONFIG_BUFFER_DATA], 5);
		u8 plusCount                  = 0;
		elapsedMillis breakoutCounter = 0;
		switch (serialNum) {
		case 1:
			Serial1.end();
			Serial1.begin(baud);
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
		memcpy(buf, &gpsMotion.lat, 4);
		memcpy(&buf[4], &gpsMotion.lon, 4);
		memcpy(&buf[8], &gpsMotion.alt, 4);
		memcpy(&buf[12], &gpsMotion.velN, 4);
		memcpy(&buf[16], &gpsMotion.velE, 4);
		memcpy(&buf[20], &gpsMotion.velD, 4);
		memcpy(&buf[24], &gpsMotion.gSpeed, 4);
		memcpy(&buf[28], &gpsMotion.headMot, 4);
		i32 cAlt    = combinedAltitude.getRaw();
		i32 vVelRaw = vVel.getRaw();
		memcpy(&buf[32], &cAlt, 4);
		memcpy(&buf[36], &vVelRaw, 4);
		memcpy(&buf[40], &magHeading, 4);
		sendCommand(configMsgCommand | 0x4000, buf, 44);
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
	case ConfigCmd::MAG_CALIBRATE: {
		magStateAfterRead = MAG_CALIBRATE;
		char calString[128];
		snprintf(calString, 128, "Offsets: %d %d %d", magOffset[0], magOffset[1], magOffset[2]);
		sendCommand((u16)ConfigCmd::IND_MESSAGE, (char *)calString, strlen(calString));
	} break;
	default: {
		sendCommand(configMsgCommand | 0x8000, "Unknown command", strlen("Unknown command"));
	} break;
	}
}

void configuratorHandleByte(u8 c, u8 _serialNum) {
	elapsedMicros taskTimer = 0;
	tasks[TASK_CONFIGURATOR].runCounter++;
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
		u8 crc = 0;
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
	u32 duration = taskTimer;
	tasks[TASK_CONFIGURATOR].totalDuration += duration;
	if (duration < tasks[TASK_CONFIGURATOR].minDuration) {
		tasks[TASK_CONFIGURATOR].minDuration = duration;
	}
	if (duration > tasks[TASK_CONFIGURATOR].maxDuration) {
		tasks[TASK_CONFIGURATOR].maxDuration = duration;
		tasks[TASK_CONFIGURATOR].debugInfo   = configMsgCommand;
	}
}