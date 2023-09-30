#include "global.h"

uint8_t configSerialBuffer[256] = {0};
uint8_t configSerialBufferIndex = 0;
uint8_t configMsgLength = 0;
uint16_t configMsgCommand = 0;

elapsedMillis configTimer = 0;

elapsedMillis configOverrideMotors = 1001;

elapsedMillis lastConfigPing = 0;
bool configuratorConnected = false;

void configuratorLoop()
{
	if (lastConfigPing > 1000)
		configuratorConnected = false;
}

void sendCommand(uint16_t command, const char *data, uint16_t len)
{
	Serial.write('_');
	Serial.write('K');
	Serial.write(len & 0xFF);
	Serial.write(len >> 8);
	Serial.write(command & 0xFF);
	Serial.write(command >> 8);

	if (data == nullptr)
		len = 0;
	uint8_t crc = (command & 0xFF) ^ (command >> 8) ^ (len & 0xFF) ^ (len >> 8);
	for (int i = 0; i < len; i++)
	{
		Serial.write(data[i]);
		crc ^= data[i];
	}
	Serial.write(crc);
}

void handleConfigCmd()
{
	char buf[256] = {0};
	uint8_t len = 0;
	switch ((ConfigCmd)configMsgCommand)
	{
	case ConfigCmd::STATUS:
	{
		uint16_t voltage = adcVoltage;
		buf[len++] = voltage & 0xFF;
		buf[len++] = voltage >> 8;
		sendCommand(configMsgCommand | 0x4000, buf, len);
	}
	break;
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
	case ConfigCmd::PLAY_SOUND:
	{
		const uint16_t startFreq = random(1000, 5000);
		const uint16_t endFreq = random(1000, 5000);
		const uint16_t sweepDuration = random(400, 1000);
		uint16_t pauseDuration = random(100, 1000);
		const uint16_t pauseEn = random(0, 2);
		pauseDuration *= pauseEn;
		const uint16_t repeat = random(1, 11);
		makeSweepSound(startFreq, endFreq, ((sweepDuration + pauseDuration) * repeat) - 1, sweepDuration, pauseDuration);
		uint8_t len = 0;
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
	}
	break;
	case ConfigCmd::BB_FILE_LIST:
	{
		int index = 0;
		char shortbuf[16] = {0};
		for (int i = 0; i < 100; i++)
		{
			rp2040.wdt_reset();
			snprintf(shortbuf, 16, "/logs%01d/%01d.kbb", i / 10, i % 10);
			if (LittleFS.exists(shortbuf))
			{
				buf[index++] = i;
			}
		}
		sendCommand(configMsgCommand | 0x4000, buf, index);
	}
	break;
	case ConfigCmd::BB_FILE_DOWNLOAD:
	{
		uint8_t fileNum = configSerialBuffer[CONFIG_BUFFER_DATA];
		int16_t chunkNum = -1;
		if (configMsgLength > 1)
		{
			chunkNum = configSerialBuffer[CONFIG_BUFFER_DATA + 1] + (configSerialBuffer[CONFIG_BUFFER_DATA + 2] << 8);
		}
		printLogBin(fileNum, chunkNum);
	}
	break;
	case ConfigCmd::BB_FILE_DELETE:
	{
		// data just includes one byte of file number
		uint8_t fileNum = configSerialBuffer[CONFIG_BUFFER_DATA];
		char path[32];
		snprintf(path, 32, "/logs%01d/%01d.kbb", fileNum / 10, fileNum % 10);
		if (LittleFS.remove(path))
			sendCommand(configMsgCommand | 0x4000, (char *)&fileNum, 1);
		else
			sendCommand(configMsgCommand | 0x8000, (char *)&fileNum, 1);
	}
	break;
	case ConfigCmd::BB_FILE_INFO:
	{
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
		uint8_t len = configSerialBuffer[CONFIG_BUFFER_LENGTH];
		len = len > 12 ? 12 : len;
		uint8_t *fileNums = &configSerialBuffer[CONFIG_BUFFER_DATA];
		uint8_t buffer[22 * len];
		uint8_t index = 0;
		for (int i = 0; i < len; i++)
		{
			rp2040.wdt_reset();
			char path[32];
			uint8_t fileNum = fileNums[i];
			snprintf(path, 32, "/logs%01d/%01d.kbb", fileNum / 10, fileNum % 10);
			File logFile = LittleFS.open(path, "r");
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
	}
	break;
	case ConfigCmd::BB_FORMAT:
		if (clearBlackbox())
			sendCommand(configMsgCommand | 0x4000);
		else
			sendCommand(configMsgCommand | 0x8000);
		break;
	case ConfigCmd::WRITE_OSD_FONT_CHARACTER:
		if (configMsgLength < 55)
		{
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
		configOverrideMotors = 0;
		break;
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
		break;
	case ConfigCmd::REBOOT_TO_BOOTLOADER:
		sendCommand(configMsgCommand | 0x4000);
		delay(100);
		rp2040.rebootToBootloader();
	case ConfigCmd::GET_NAME:
	{
		char name[20] = {0};
		for (int i = 0; i < 20; i++)
			name[i] = EEPROM.read((uint16_t)EEPROM_POS::UAV_NAME + i);
		name[19] = '\0';
		sendCommand(configMsgCommand | 0x4000, name, strlen(name));
	}
	case ConfigCmd::SET_NAME:
	{
		uint8_t len = configSerialBuffer[CONFIG_BUFFER_LENGTH];
		if (len > 20)
			sendCommand(configMsgCommand | 0x8000);
		break;
		for (int i = 0; i < len; i++)
			EEPROM.write((uint16_t)EEPROM_POS::UAV_NAME + i, configSerialBuffer[CONFIG_BUFFER_DATA + i]);
		sendCommand(configMsgCommand | 0x4000);
		break;
	}
	case ConfigCmd::GET_PIDS:
	{
		uint16_t pids[3][7];
		for (int i = 0; i < 3; i++)
		{
			pids[i][0] = pidGains[i][0] >> P_SHIFT;
			pids[i][1] = pidGains[i][1] >> I_SHIFT;
			pids[i][2] = pidGains[i][2] >> D_SHIFT;
			pids[i][3] = pidGains[i][3] >> FF_SHIFT;
			pids[i][4] = pidGains[i][4] >> S_SHIFT;
			pids[i][5] = pidGains[i][5] & 0xFFFF;
			pids[i][6] = 0;
		}
		sendCommand(configMsgCommand | 0x4000, (char *)pids, sizeof(pids));
	}
	break;
	case ConfigCmd::SET_PIDS:
	{
		uint16_t pids[3][7];
		memcpy(pids, &configSerialBuffer[CONFIG_BUFFER_DATA], sizeof(pids));
		for (int i = 0; i < 3; i++)
		{
			pidGains[i][0] = pids[i][0] << P_SHIFT;
			pidGains[i][1] = pids[i][1] << I_SHIFT;
			pidGains[i][2] = pids[i][2] << D_SHIFT;
			pidGains[i][3] = pids[i][3] << FF_SHIFT;
			pidGains[i][4] = pids[i][4] << S_SHIFT;
			pidGains[i][5] = pids[i][5];
		}
		EEPROM.put((uint16_t)EEPROM_POS::PID_GAINS, pidGains);
		sendCommand(configMsgCommand | 0x4000);
	}
	break;
	case ConfigCmd::GET_RATES:
	{
		uint16_t rates[3][5];
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 5; j++)
				rates[i][j] = rateFactors[j][i] >> 16;
		sendCommand(configMsgCommand | 0x4000, (char *)rates, sizeof(rates));
	}
	break;
	case ConfigCmd::SET_RATES:
	{
		uint16_t rates[3][5];
		memcpy(rates, &configSerialBuffer[CONFIG_BUFFER_DATA], sizeof(rates));
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 5; j++)
				rateFactors[j][i] = rates[i][j] << 16;
		sendCommand(configMsgCommand | 0x4000);
		EEPROM.put((uint16_t)EEPROM_POS::RATE_FACTORS, rateFactors);
	}
	default:
	{
		sendCommand(configMsgCommand | 0x8000, "Unknown command", strlen("Unknown command"));
	}
	break;
	}
}

void configuratorHandleByte(uint8_t c, uint8_t _serialNum)
{
	configSerialBuffer[configSerialBufferIndex++] = c;
	// every message from the configurator starts with _K, followed by two bytes of data length (only for the data, thus between 0 and 65535), then 2 bytes command, then the data, then the checksum (1 byte XOR of length, command and data)
	// 2 bytes prefix, 2 bytes length, 2 bytes command, data, 1 byte checksum
	if (configSerialBufferIndex == 1 && c != '_')
	{
		configSerialBufferIndex = 0;
		return;
	}
	else if (configSerialBufferIndex == 2 && c != 'K')
	{
		configSerialBufferIndex = 0;
		return;
	}
	else if (configSerialBufferIndex == 4)
	{
		configMsgLength = configSerialBuffer[2] + (configSerialBuffer[3] << 8); // low byte first
	}
	else if (configSerialBufferIndex == 6)
	{
		configMsgCommand = configSerialBuffer[4] + (configSerialBuffer[5] << 8); // low byte first
	}
	if (configSerialBufferIndex == configMsgLength + 7)
	{
		uint8_t crc = 0;
		for (int i = 2; i < configMsgLength + 7; i++)
			crc ^= configSerialBuffer[i];
		if (crc != 0)
		{
			configSerialBufferIndex = 0;
			Serial.printf("CRC error");
			return;
		}
		handleConfigCmd();
		configSerialBufferIndex = 0;
	}
}