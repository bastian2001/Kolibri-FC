#include "global.h"

u8 mspSerialBuffer[2048] = {0};
u16 mspSerialBufferIndex = 0;
u16 mspMspLength         = 0;
u8 accelCalDone          = 0;
MspFn mspMsgCommand;
MspMsgType mspMsgType    = MspMsgType::ERROR;
u8 mspMsgFlag            = 0;
MspVersion mspMsgVersion = MspVersion::V2;

MspState mspState = MspState::IDLE;

elapsedMillis mspOverrideMotors = 1001;

elapsedMillis lastConfigPingRx = 0;
bool configuratorConnected     = false;
u16 payloadStartIndex = 0, payloadStopIndex = 0;

void configuratorLoop() {
	if (lastConfigPingRx > 1000)
		configuratorConnected = false;
	if (accelCalDone) {
		accelCalDone = 0;
		sendMsp(MspMsgType::RESPONSE, MspFn::CALIBRATE_ACCELEROMETER);
		EEPROM.put((u16)EEPROM_POS::ACCEL_CALIBRATION, (i16)accelCalibrationOffset[0]);
		EEPROM.put((u16)EEPROM_POS::ACCEL_CALIBRATION + 2, (i16)accelCalibrationOffset[1]);
		EEPROM.put((u16)EEPROM_POS::ACCEL_CALIBRATION + 4, (i16)accelCalibrationOffset[2]);
		EEPROM.commit();
		sendMsp(MspMsgType::RESPONSE, MspFn::SAVE_SETTINGS);
	}
}

void sendMsp(MspMsgType type, MspFn fn, const char *data, u16 len) {
	u8 header[8] = {'$', 'X', (u8)type, 0, (u8)fn, (u8)((u32)fn >> 8), (u8)len, (u8)(len >> 8)};
	Serial.write(header, 8);

	if (data == nullptr)
		len = 0;
	u8 crc = ((u32)fn & 0xFF) ^ ((u32)fn >> 8) ^ (len & 0xFF) ^ (len >> 8);
	for (int i = 0; i < len; i++) {
		Serial.write(data[i]);
		crc ^= data[i];
	}
	Serial.write(crc);
}

void processMspCmd() {
	char buf[256] = {0};
	u8 len        = 0;
	if (mspMsgType == MspMsgType::REQUEST) {
		switch ((MspFn)mspMsgCommand) {
		case MspFn::STATUS: {
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
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, buf, len);
		} break;
		case MspFn::TASK_STATUS: {
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
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, (char *)buf, sizeof(buf));
			for (int i = 0; i < 32; i++) {
				tasks[i].minDuration = 0xFFFFFFFF;
				tasks[i].maxDuration = 0;
				tasks[i].maxGap      = 0;
			}
		} break;
		case MspFn::REBOOT:
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand);
			Serial.flush();
			rebootReason = BootReason::CMD_REBOOT;
			delay(100);
			rp2040.reboot();
			break;
		case MspFn::SAVE_SETTINGS:
			rp2040.wdt_reset();
			EEPROM.commit();
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand);
			break;
		case MspFn::PLAY_SOUND: {
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
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, buf, len);
		} break;
		case MspFn::BB_FILE_LIST: {
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
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, buf, index);
		} break;
		case MspFn::BB_FILE_DOWNLOAD: {
			u8 fileNum   = mspSerialBuffer[payloadStartIndex];
			i16 chunkNum = -1;
			if (mspMspLength > 1) {
				chunkNum = mspSerialBuffer[payloadStartIndex + 1] + (mspSerialBuffer[payloadStartIndex + 2] << 8);
			}
			printLogBin(fileNum, chunkNum);
		} break;
		case MspFn::BB_FILE_DELETE: {
			// data just includes one byte of file number
			u8 fileNum = mspSerialBuffer[payloadStartIndex];
			char path[32];
#if BLACKBOX_STORAGE == LITTLEFS_BB
			snprintf(path, 32, "/logs%01d/%01d.kbb", fileNum / 10, fileNum % 10);
			if (LittleFS.remove(path))
#elif BLACKBOX_STORAGE == SD_BB
			snprintf(path, 32, "/kolibri/%01d.kbb", fileNum);
			if (SDFS.remove(path))
#endif
				sendMsp(MspMsgType::RESPONSE, mspMsgCommand, (char *)&fileNum, 1);
			else
				sendMsp(MspMsgType::ERROR, mspMsgCommand, (char *)&fileNum, 1);
		} break;
		case MspFn::BB_FILE_INFO: {
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
			u8 len       = mspMspLength;
			len          = len > 12 ? 12 : len;
			u8 *fileNums = &mspSerialBuffer[payloadStartIndex];
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
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, (char *)buffer, index);
		} break;
		case MspFn::BB_FORMAT:
			if (clearBlackbox())
				sendMsp(MspMsgType::RESPONSE, mspMsgCommand);
			else
				sendMsp(MspMsgType::ERROR, mspMsgCommand);
			break;
		case MspFn::WRITE_OSD_FONT_CHARACTER:
			if (mspMspLength < 55) {
				sendMsp(MspMsgType::ERROR, mspMsgCommand);
				break;
			}
			updateCharacter(mspSerialBuffer[payloadStartIndex], &mspSerialBuffer[payloadStartIndex + 1]);
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, (const char *)&mspSerialBuffer[payloadStartIndex], 1);
			break;
		case MspFn::SET_MOTORS:
			throttles[(u8)MOTOR::RR] = (u16)mspSerialBuffer[payloadStartIndex + 0] + ((u16)mspSerialBuffer[payloadStartIndex + 1] << 8);
			throttles[(u8)MOTOR::FR] = (u16)mspSerialBuffer[payloadStartIndex + 2] + ((u16)mspSerialBuffer[payloadStartIndex + 3] << 8);
			throttles[(u8)MOTOR::RL] = (u16)mspSerialBuffer[payloadStartIndex + 4] + ((u16)mspSerialBuffer[payloadStartIndex + 5] << 8);
			throttles[(u8)MOTOR::FL] = (u16)mspSerialBuffer[payloadStartIndex + 6] + ((u16)mspSerialBuffer[payloadStartIndex + 7] << 8);
			mspOverrideMotors        = 0;
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand);
			break;
		case MspFn::GET_MOTORS: {
			u16 motors[4];
			motors[0] = throttles[(u8)MOTOR::RR];
			motors[1] = throttles[(u8)MOTOR::FR];
			motors[2] = throttles[(u8)MOTOR::RL];
			motors[3] = throttles[(u8)MOTOR::FL];
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, (char *)motors, sizeof(motors));
		} break;
		case MspFn::BB_FILE_DOWNLOAD_RAW:
			printLogBinRaw(mspSerialBuffer[payloadStartIndex]);
			break;
		case MspFn::SET_DEBUG_LED:
			gpio_put(PIN_LED_DEBUG, mspSerialBuffer[payloadStartIndex]);
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand);
			break;
		case MspFn::CONFIGURATOR_PING:
			configuratorConnected = true;
			lastConfigPingRx      = 0;
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand);
			break;
		case MspFn::REBOOT_TO_BOOTLOADER:
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand);
			Serial.flush();
			rebootReason = BootReason::CMD_BOOTLOADER;
			delay(100);
			rp2040.rebootToBootloader();
			break;
		case MspFn::GET_NAME: {
			char name[20] = {0};
			for (int i = 0; i < 20; i++)
				name[i] = EEPROM.read((u16)EEPROM_POS::UAV_NAME + i);
			name[19] = '\0';
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, name, strlen(name));
		} break;
		case MspFn::SET_NAME: {
			u8 len = mspSerialBuffer[payloadStartIndex];
			if (len > 20) {
				sendMsp(MspMsgType::ERROR, mspMsgCommand);
				break;
			}
			for (int i = 0; i < len; i++)
				EEPROM.write((u16)EEPROM_POS::UAV_NAME + i, mspSerialBuffer[payloadStartIndex + i]);
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand);
			break;
		}
		case MspFn::GET_PIDS: {
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
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, (char *)pids, sizeof(pids));
		} break;
		case MspFn::SET_PIDS: {
			u16 pids[3][7];
			memcpy(pids, &mspSerialBuffer[payloadStartIndex], sizeof(pids));
			for (int i = 0; i < 3; i++) {
				pidGains[i][0].setRaw(pids[i][0] << P_SHIFT);
				pidGains[i][1].setRaw(pids[i][1] << I_SHIFT);
				pidGains[i][2].setRaw(pids[i][2] << D_SHIFT);
				pidGains[i][3].setRaw(pids[i][3] << FF_SHIFT);
				pidGains[i][4].setRaw(pids[i][4] << S_SHIFT);
				pidGains[i][5].setRaw(pids[i][5]);
			}
			EEPROM.put((u16)EEPROM_POS::PID_GAINS, pidGains);
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand);
		} break;
		case MspFn::GET_RATES: {
			u16 rates[3][5];
			for (int i = 0; i < 3; i++)
				for (int j = 0; j < 5; j++)
					rates[i][j] = rateFactors[j][i].getInt();
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, (char *)rates, sizeof(rates));
		} break;
		case MspFn::SET_RATES: {
			u16 rates[3][5];
			memcpy(rates, &mspSerialBuffer[payloadStartIndex], sizeof(rates));
			for (int i = 0; i < 3; i++)
				for (int j = 0; j < 5; j++)
					rateFactors[j][i] = rates[i][j];
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand);
			EEPROM.put((u16)EEPROM_POS::RATE_FACTORS, rateFactors);
		} break;
		case MspFn::GET_BB_SETTINGS: {
			u8 bbSettings[9];
			bbSettings[0] = bbFreqDivider;
			memcpy(&bbSettings[1], &bbFlags, 8);
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, (char *)bbSettings, sizeof(bbSettings));
		} break;
		case MspFn::SET_BB_SETTINGS: {
			u8 bbSettings[9];
			memcpy(bbSettings, &mspSerialBuffer[payloadStartIndex], sizeof(bbSettings));
			bbFreqDivider = bbSettings[0];
			memcpy(&bbFlags, &bbSettings[1], 8);
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand);
			EEPROM.put((u16)EEPROM_POS::BB_FLAGS, bbFlags);
			EEPROM.put((u16)EEPROM_POS::BB_FREQ_DIVIDER, bbFreqDivider);
		} break;
		case MspFn::GET_ROTATION: {
			// https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
			int rotationPitch = pitch * 8192;
			int rotationRoll  = roll * 8192;
			int rotationYaw   = yaw * 8192;
			int heading       = combinedHeading.getRaw() >> 3;
			buf[0]            = rotationPitch & 0xFF;
			buf[1]            = rotationPitch >> 8;
			buf[2]            = rotationRoll & 0xFF;
			buf[3]            = rotationRoll >> 8;
			buf[4]            = rotationYaw & 0xFF;
			buf[5]            = rotationYaw >> 8;
			buf[6]            = heading & 0xFF;
			buf[7]            = heading >> 8;
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, buf, 8);
		} break;
		case MspFn::SERIAL_PASSTHROUGH: {
			u8 serialNum = mspSerialBuffer[payloadStartIndex];
			u32 baud     = DECODE_U4(&mspSerialBuffer[payloadStartIndex + 1]);
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, (char *)&mspSerialBuffer[payloadStartIndex], 5);
			Serial.flush();
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
		case MspFn::GET_GPS_ACCURACY: {
			// through padding and compiler optimization, it is not possible to just memcpy the struct
			memcpy(buf, &gpsAcc.tAcc, 4);
			memcpy(&buf[4], &gpsAcc.hAcc, 4);
			memcpy(&buf[8], &gpsAcc.vAcc, 4);
			memcpy(&buf[12], &gpsAcc.sAcc, 4);
			memcpy(&buf[16], &gpsAcc.headAcc, 4);
			memcpy(&buf[20], &gpsAcc.pDop, 4);
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, buf, 24);
		} break;
		case MspFn::GET_GPS_STATUS: {
			buf[0] = gpsStatus.gpsInited;
			buf[1] = gpsStatus.initStep;
			buf[2] = gpsStatus.fixType;
			buf[3] = gpsStatus.timeValidityFlags;
			buf[4] = gpsStatus.flags;
			buf[5] = gpsStatus.flags2;
			buf[6] = gpsStatus.flags3 & 0xFF;
			buf[7] = gpsStatus.flags3 >> 8;
			buf[8] = gpsStatus.satCount;
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, buf, 9);
		} break;
		case MspFn::GET_GPS_TIME: {
			buf[0] = gpsTime.year & 0xFF;
			buf[1] = gpsTime.year >> 8;
			buf[2] = gpsTime.month;
			buf[3] = gpsTime.day;
			buf[4] = gpsTime.hour;
			buf[5] = gpsTime.minute;
			buf[6] = gpsTime.second;
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, buf, 7);
		} break;
		case MspFn::GET_GPS_MOTION: {
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
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, buf, 40);
		} break;
		case MspFn::ESC_PASSTHROUGH:
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand);
			Serial.flush();
			rebootReason = BootReason::CMD_ESC_PASSTHROUGH;
			delay(100);
			rp2040.reboot();
			break;
		case MspFn::GET_CRASH_DUMP: {
			for (int i = 0; i < 256; i++) {
				rp2040.wdt_reset();
				buf[i] = EEPROM.read(4096 - 256 + i);
			}
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, buf, 256);
		} break;
		case MspFn::CLEAR_CRASH_DUMP: {
			for (int i = 0; i < 256; i++) {
				rp2040.wdt_reset();
				EEPROM.write(4096 - 256 + i, 0);
			}
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand);
		} break;
		case MspFn::CALIBRATE_ACCELEROMETER:
			accelCalibrationCycles = QUIET_SAMPLES + CALIBRATION_SAMPLES;
			armingDisableFlags |= 0x40;
			break;
		case MspFn::GET_MAG_DATA: {
			i16 raw[6] = {(i16)magData[0], (i16)magData[1], (i16)magData[2], (i16)magX.getInt(), (i16)magY.getInt(), (i16)(magHeading * 180 / (fix32)PI).getInt()};
			sendMsp(MspMsgType::RESPONSE, mspMsgCommand, (char *)raw, 12);
		} break;
		case MspFn::MAG_CALIBRATE: {
			magStateAfterRead = MAG_CALIBRATE;
			char calString[128];
			snprintf(calString, 128, "Offsets: %d %d %d", magOffset[0], magOffset[1], magOffset[2]);
			sendMsp(MspMsgType::REQUEST, MspFn::IND_MESSAGE, (char *)calString, strlen(calString));
		} break;
		default: {
			sendMsp(MspMsgType::ERROR, mspMsgCommand, "Unknown command", strlen("Unknown command"));
		} break;
		}
	}
}

void mspHandleByte(u8 c, u8 _serialNum) {
	elapsedMicros taskTimer = 0;
	tasks[TASK_CONFIGURATOR].runCounter++;
	mspSerialBuffer[mspSerialBufferIndex++] = c;

	switch (mspState) {
	case MspState::IDLE:
		if (c == '$')
			mspState = MspState::PACKET_START;
		else
			mspSerialBufferIndex = 0;
		break;
	case MspState::PACKET_START:
		switch (c) {
		case 'M':
			mspState = MspState::TYPE_V1;
			break;
		case 'X':
			mspState = MspState::TYPE_V2;
			break;
		default:
			mspSerialBufferIndex = 0;
			mspState             = MspState::IDLE;
			break;
		}
		break;
	case MspState::TYPE_V1:
		mspState = MspState::LEN_V1;
		switch (c) {
		case '<':
			mspMsgType = MspMsgType::REQUEST;
			break;
		case '>':
			mspMsgType = MspMsgType::RESPONSE;
			break;
		case '!':
			mspMsgType = MspMsgType::ERROR;
			break;
		default:
			mspSerialBufferIndex = 0;
			mspState             = MspState::IDLE;
			break;
		}
		break;
	case MspState::LEN_V1:
		if (c == 255) {
			mspMsgVersion = MspVersion::V1_JUMBO;
			mspState      = MspState::JUMBO_LEN_LO_V1;
		} else {
			mspMspLength  = c;
			mspMsgVersion = MspVersion::V1;
			mspState      = MspState::CMD_V1;
		}
		break;
	case MspState::JUMBO_LEN_LO_V1:
		mspMspLength = c;
		mspState     = MspState::JUMBO_LEN_HI_V1;
		break;
	case MspState::JUMBO_LEN_HI_V1:
		mspMspLength |= ((u16)c << 8);
		mspState = MspState::CMD_V1;
		break;
	case MspState::CMD_V1:
		if (c == 255) {
			mspMsgVersion = mspMsgVersion == MspVersion::V1 ? MspVersion::V2_OVER_V1 : MspVersion::V2_OVER_V1_JUMBO;
			mspState      = MspState::FLAG_V2_OVER_V1;
		} else {
			mspMsgCommand = (MspFn)c;
			mspState      = mspMspLength ? MspState::PAYLOAD_V1 : MspState::CHECKSUM_V1;
		}
		break;
	case MspState::PAYLOAD_V1:
		if (mspSerialBufferIndex == mspMspLength)
			mspState = MspState::CHECKSUM_V1;
		break;
	case MspState::FLAG_V2_OVER_V1:
		mspMsgFlag = c;
		mspState   = MspState::CMD_LO_V2_OVER_V1;
		break;
	case MspState::CMD_LO_V2_OVER_V1:
		mspMsgCommand = (MspFn)c;
		mspState      = MspState::CMD_HI_V2_OVER_V1;
		break;
	case MspState::CMD_HI_V2_OVER_V1:
		mspMsgCommand = (MspFn)((u32)mspMsgCommand | (u32)c << 8);
		mspState      = MspState::LEN_LO_V2_OVER_V1;
		break;
	case MspState::LEN_LO_V2_OVER_V1:
		mspMspLength = c;
		mspState     = MspState::LEN_HI_V2_OVER_V1;
		break;
	case MspState::LEN_HI_V2_OVER_V1:
		mspMspLength |= ((u16)c << 8);
		mspState = mspMspLength ? MspState::PAYLOAD_V2_OVER_V1 : MspState::CHECKSUM_V2_OVER_V1;
		break;
	case MspState::PAYLOAD_V2_OVER_V1:
		if (mspSerialBufferIndex == mspMspLength)
			mspState = MspState::CHECKSUM_V2_OVER_V1;
		break;
	case MspState::CHECKSUM_V2_OVER_V1:
		mspState = MspState::CHECKSUM_V1;
		break;
	case MspState::CHECKSUM_V1:
		processMspCmd();
		mspSerialBufferIndex = 0;
		mspState             = MspState::IDLE;
		break;
	case MspState::TYPE_V2:
		mspState      = MspState::FLAG_V2;
		mspMsgVersion = MspVersion::V2;
		switch (c) {
		case '<':
			mspMsgType = MspMsgType::REQUEST;
			break;
		case '>':
			mspMsgType = MspMsgType::RESPONSE;
			break;
		case '!':
			mspMsgType = MspMsgType::ERROR;
			break;
		default:
			mspSerialBufferIndex = 0;
			mspState             = MspState::IDLE;
			break;
		}
		break;
	case MspState::FLAG_V2:
		mspMsgFlag = c;
		mspState   = MspState::CMD_LO_V2;
		break;
	case MspState::CMD_LO_V2:
		mspMsgCommand = (MspFn)c;
		mspState      = MspState::CMD_HI_V2;
		break;
	case MspState::CMD_HI_V2:
		mspMsgCommand = (MspFn)((u32)mspMsgCommand | (u32)c << 8);
		mspState      = MspState::LEN_LO_V2;
		break;
	case MspState::LEN_LO_V2:
		mspMspLength = c;
		mspState     = MspState::LEN_HI_V2;
		break;
	case MspState::LEN_HI_V2:
		mspMspLength |= ((u16)c << 8);
		payloadStartIndex = mspSerialBufferIndex;
		payloadStopIndex  = payloadStartIndex + mspMspLength;
		mspState          = mspMspLength ? MspState::PAYLOAD_V2 : MspState::CHECKSUM_V2;
		break;
	case MspState::PAYLOAD_V2:
		if (mspSerialBufferIndex == payloadStopIndex) {
			mspState = MspState::CHECKSUM_V2;
		}
		break;
	case MspState::CHECKSUM_V2:
		processMspCmd();
		mspSerialBufferIndex = 0;
		mspState             = MspState::IDLE;
		break;
	}
	u32 duration = taskTimer;
	tasks[TASK_CONFIGURATOR].totalDuration += duration;
	if (duration < tasks[TASK_CONFIGURATOR].minDuration) {
		tasks[TASK_CONFIGURATOR].minDuration = duration;
	}
	if (duration > tasks[TASK_CONFIGURATOR].maxDuration) {
		tasks[TASK_CONFIGURATOR].maxDuration = duration;
		tasks[TASK_CONFIGURATOR].debugInfo   = (u32)mspMsgCommand;
	}
}