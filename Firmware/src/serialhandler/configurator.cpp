#include "global.h"

uint8_t configSerialBuffer[256] = {0};
uint8_t configSerialBufferIndex = 0;
uint8_t configMsgLength = 0;
uint16_t configMsgCommand = 0;

elapsedMillis configTimer = 0;

void sendCommand(uint16_t command, const char *data, const uint16_t len)
{
	Serial.write('_');
	Serial.write('K');
	Serial.write(len & 0xFF);
	Serial.write(len >> 8);
	Serial.write(command & 0xFF);
	Serial.write(command >> 8);
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
			snprintf(shortbuf, 16, "/logs%01d/%01d.rpbb", i / 10, i % 10);
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
		char path[32];
		uint8_t fileNum = configSerialBuffer[CONFIG_BUFFER_DATA];
		snprintf(path, 32, "/logs%01d/%01d.rpbb", fileNum / 10, fileNum % 10);
		File logFile = LittleFS.open(path, "r");
		if (!logFile)
		{
			sendCommand(configMsgCommand | 0x8000, "File not found", strlen("File not found"));
			break;
		}
		uint8_t buffer[1024];
		buffer[0] = fileNum;
		uint16_t chunkNum = 0;
		buffer[1] = chunkNum & 0xFF;
		buffer[2] = chunkNum >> 8;
		size_t bytesRead = logFile.read(buffer + 3, 1024 - 3);
		while (bytesRead > 0)
		{
			chunkNum++;
			buffer[1] = chunkNum & 0xFF;
			buffer[2] = chunkNum >> 8;
			rp2040.wdt_reset();
			sendCommand(configMsgCommand | 0x4000, (char *)buffer, bytesRead + 3);
			Serial.flush();
			bytesRead = logFile.read(buffer + 3, 1024 - 3);
		}
		logFile.close();
	}
	break;
	case ConfigCmd::BB_FILE_DELETE:
	{
		// data just includes one byte of file number
		uint8_t fileNum = configSerialBuffer[CONFIG_BUFFER_DATA];
		char path[32];
		snprintf(path, 32, "/logs%01d/%01d.rpbb", fileNum / 10, fileNum % 10);
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
			snprintf(path, 32, "/logs%01d/%01d.rpbb", fileNum / 10, fileNum % 10);
			File logFile = LittleFS.open(path, "r");
			if (!logFile)
				continue;
			buffer[index++] = fileNum;
			buffer[index++] = logFile.size() & 0xFF;
			buffer[index++] = (logFile.size() >> 8) & 0xFF;
			buffer[index++] = (logFile.size() >> 16) & 0xFF;
			buffer[index++] = (logFile.size() >> 24) & 0xFF;
			logFile.seek(4, SeekSet);
			// version, timestamp, pid and divider can directly be read from the file
			for (int i = 0; i < 9; i++)
				buffer[index++] = logFile.read();
			logFile.seek(145, SeekCur);
			// flags
			for (int i = 0; i < 8; i++)
				buffer[index++] = logFile.read();
		}
		sendCommand(configMsgCommand | 0x4000, (char *)buffer, index);
	}
	break;
	case ConfigCmd::WRITE_OSD_FONT_CHARACTER:
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
	}
}