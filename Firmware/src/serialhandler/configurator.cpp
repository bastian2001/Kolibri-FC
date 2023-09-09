#include "global.h"

uint8_t configSerialBuffer[256] = {0};
uint8_t configSerialBufferIndex = 0;
uint8_t configMsgLength = 0;
uint16_t configMsgCommand = 0;

elapsedMillis configTimer = 0;

elapsedMillis configOverrideMotors = 1001;

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
		sendCommand(configMsgCommand | 0x4000, "Status report not available", strlen("Status report not available"));
		break;
	case ConfigCmd::TASK_STATUS:
		len = snprintf(buf, 256, "PID loop counter: %d", pidLoopCounter);
		sendCommand(configMsgCommand | 0x4000, buf, len);
		break;
	case ConfigCmd::GET_SETTING:
		break;
	case ConfigCmd::SET_SETTING:
		break;
	case ConfigCmd::PLAY_SOUND:
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
		printLogBin(fileNum);
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
		break;
	case ConfigCmd::SET_MOTORS:
		throttles[(uint8_t)MOTOR::RR] = (uint16_t)configSerialBuffer[CONFIG_BUFFER_DATA + 0] + ((uint16_t)configSerialBuffer[CONFIG_BUFFER_DATA + 1] << 8);
		throttles[(uint8_t)MOTOR::RL] = (uint16_t)configSerialBuffer[CONFIG_BUFFER_DATA + 2] + ((uint16_t)configSerialBuffer[CONFIG_BUFFER_DATA + 3] << 8);
		throttles[(uint8_t)MOTOR::FR] = (uint16_t)configSerialBuffer[CONFIG_BUFFER_DATA + 4] + ((uint16_t)configSerialBuffer[CONFIG_BUFFER_DATA + 5] << 8);
		throttles[(uint8_t)MOTOR::FL] = (uint16_t)configSerialBuffer[CONFIG_BUFFER_DATA + 6] + ((uint16_t)configSerialBuffer[CONFIG_BUFFER_DATA + 7] << 8);
		configOverrideMotors = 0;
		break;
	case ConfigCmd::BB_FILE_DOWNLOAD_RAW:
		printLogBinRaw(configSerialBuffer[CONFIG_BUFFER_DATA]);
		break;
	default:
		Serial.printf("Unknown command: %d\n", configMsgCommand);
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