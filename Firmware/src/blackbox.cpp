#include "global.h"

uint64_t bbFlags = 0;
uint64_t currentBBFlags = 0;

bool bbLogging = false;
bool lfsReady = false;

FSInfo64 fsInfo;
int currentLogNum = 0;

File blackboxFile;

int32_t maxFileSize = 0;

void initBlackbox()
{
	lfsReady = LittleFS.begin();
	lfsReady = lfsReady && LittleFS.info64(fsInfo);
	bbFlags = LOG_MOTOR_OUTPUTS | LOG_ROLL_GYRO_RAW | LOG_ROLL_PID_P | LOG_ROLL_PID_I | LOG_ROLL_PID_D | LOG_ROLL_PID_FF | LOG_ROLL_PID_S | LOG_PITCH_GYRO_RAW | LOG_PITCH_PID_P | LOG_PITCH_PID_I | LOG_PITCH_PID_D | LOG_PITCH_PID_FF | LOG_PITCH_PID_S | LOG_YAW_GYRO_RAW | LOG_YAW_PID_P | LOG_YAW_PID_I | LOG_YAW_PID_D | LOG_YAW_PID_FF | LOG_YAW_PID_S;
	// bbFlags = LOG_ROLL_ELRS_RAW | LOG_ROLL_SETPOINT;
	// bbFlags=LOG_MOTOR_OUTPUTS ;
}

bool clearBlackbox()
{
	if (!lfsReady || bbLogging)
		return false;
	LittleFS.format();
	return true;
}

void printLogBinRaw(uint8_t logNum)
{
	char path[32];
	logNum %= 100;
	snprintf(path, 32, "/logs%01d/%01d.kbb", logNum / 10, logNum % 10);
	rp2040.wdt_reset();
	File logFile = LittleFS.open(path, "r");
	if (!logFile)
		return;
	uint32_t fileSize = logFile.size();
	for (uint32_t i = 0; i < fileSize; i++)
	{
		Serial.write(logFile.read());
		if ((i % 1024) == 0)
		{
			rp2040.wdt_reset();
			Serial.flush();
		}
	}
	logFile.close();
}

void printLogBin(uint8_t logNum)
{
	char path[32];
	snprintf(path, 32, "/logs%01d/%01d.kbb", logNum / 10, logNum % 10);
	File logFile = LittleFS.open(path, "r");
	if (!logFile)
	{
		sendCommand(((uint16_t)ConfigCmd::BB_FILE_DOWNLOAD) | 0x8000, "File not found", strlen("File not found"));
		return;
	}
	uint8_t buffer[1027];
	buffer[0] = logNum;
	uint16_t chunkNum = 0;
	size_t bytesRead = 1;
	while (bytesRead > 0)
	{
		rp2040.wdt_reset();
		gpio_put(PIN_LED_ACTIVITY, !gpio_get(PIN_LED_ACTIVITY));
		bytesRead = logFile.read(buffer + 3, 1024);
		buffer[1] = chunkNum & 0xFF;
		buffer[2] = chunkNum >> 8;
		sendCommand(((uint16_t)ConfigCmd::BB_FILE_DOWNLOAD) | 0x4000, (char *)buffer, bytesRead + 3);
		Serial.flush();
		chunkNum++;
	}
	logFile.close();
	// finish frame includes 0xFFFF as chunk number, and then the actual max chunk number
	buffer[0] = logNum;
	buffer[1] = 0xFF;
	buffer[2] = 0xFF;
	buffer[3] = chunkNum & 0xFF;
	buffer[4] = chunkNum >> 8;
	sendCommand(((uint16_t)ConfigCmd::BB_FILE_DOWNLOAD) | 0x4000, (char *)buffer, 5);
}

void startLogging()
{
	if (!bbFlags || !lfsReady || bbLogging)
		return;
	currentBBFlags = bbFlags;
	if (!(LittleFS.info64(fsInfo)))
		return;
	maxFileSize = fsInfo.totalBytes - fsInfo.usedBytes - 50000;
	if (maxFileSize < 20000)
	{
		Serial.println("Not enough space for blackbox, cleared");
		return;
	}
	char path[32];
	for (int i = 0; i < 100; i++)
	{
		rp2040.wdt_reset();
		snprintf(path, 32, "/logs%01d/%01d.kbb", ((i + currentLogNum) % 100) / 10, (i + currentLogNum) % 10);
		if (!LittleFS.exists(path))
		{
			currentLogNum += i + 1;
			break;
		}
		if (i == 99)
		{
			clearBlackbox();
			return;
		}
	}
	blackboxFile = LittleFS.open(path, "a");
	if (!blackboxFile)
		return;
	bbLogging = true;
	const uint8_t data[] = {
		0x20, 0x27, 0xA1, 0x99, 0, 0, 0 // magic bytes, version
	};
	blackboxFile.write(data, 7);
	uint32_t time = millis();
	blackboxFile.write((uint8_t *)&time, 4); // timestamp unknown
	blackboxFile.write((uint8_t)0);			 // 3200Hz gyro
	blackboxFile.write((uint8_t)BB_FREQ_DIVIDER);
	blackboxFile.write((uint8_t)3); // 2000deg/sec and 16g
	blackboxFile.write((uint8_t *)rateFactors, 60);
	for (int i = 0; i < 3; i++)
	{
		blackboxFile.write((uint8_t *)&kP, 4);
		blackboxFile.write((uint8_t *)&kI, 4);
		blackboxFile.write((uint8_t *)&kD, 4);
		blackboxFile.write((uint8_t *)&kFF, 4);
		blackboxFile.write((uint8_t *)&kS, 4);
		blackboxFile.write((uint8_t *)&iFalloff, 4);
		blackboxFile.write((uint8_t *)&iFalloff, 4);
	}
	blackboxFile.write((uint8_t *)&bbFlags, 8);
	// 166 bytes header
}

void endLogging()
{
	// bbFlags <<= 1;
	// if (!bbFlags)
	// 	bbFlags = 1 << 0;
	if (!lfsReady)
		return;
	rp2040.wdt_reset();
	if (bbLogging)
		blackboxFile.close();
	bbLogging = false;
}

uint8_t bbBuffer[128];
void writeSingleFrame()
{
	size_t bufferPos = 0;
	if (!lfsReady || !bbLogging)
		return;
	if (blackboxFile.size() > maxFileSize)
	{
		endLogging();
		return;
	}
	if (currentBBFlags & LOG_ROLL_ELRS_RAW)
	{
		bbBuffer[bufferPos++] = ELRS->channels[0];
		bbBuffer[bufferPos++] = ELRS->channels[0] >> 8;
	}
	if (currentBBFlags & LOG_PITCH_ELRS_RAW)
	{
		bbBuffer[bufferPos++] = ELRS->channels[1];
		bbBuffer[bufferPos++] = ELRS->channels[1] >> 8;
	}
	if (currentBBFlags & LOG_THROTTLE_ELRS_RAW)
	{
		bbBuffer[bufferPos++] = ELRS->channels[2];
		bbBuffer[bufferPos++] = ELRS->channels[2] >> 8;
	}
	if (currentBBFlags & LOG_YAW_ELRS_RAW)
	{
		bbBuffer[bufferPos++] = ELRS->channels[3];
		bbBuffer[bufferPos++] = ELRS->channels[3] >> 8;
	}
	if (currentBBFlags & LOG_ROLL_SETPOINT)
	{
		int16_t setpoint = (int16_t)(rollSetpoint >> 12);
		bbBuffer[bufferPos++] = setpoint;
		bbBuffer[bufferPos++] = setpoint >> 8;
	}
	if (currentBBFlags & LOG_PITCH_SETPOINT)
	{
		int16_t setpoint = (int16_t)(pitchSetpoint >> 12);
		bbBuffer[bufferPos++] = setpoint;
		bbBuffer[bufferPos++] = setpoint >> 8;
	}
	if (currentBBFlags & LOG_THROTTLE_SETPOINT)
	{
		bbBuffer[bufferPos++] = smoothChannels[2];
		bbBuffer[bufferPos++] = smoothChannels[2] >> 8;
	}
	if (currentBBFlags & LOG_YAW_SETPOINT)
	{
		int16_t setpoint = (int16_t)(yawSetpoint >> 12);
		bbBuffer[bufferPos++] = setpoint;
		bbBuffer[bufferPos++] = setpoint >> 8;
	}
	if (currentBBFlags & LOG_ROLL_GYRO_RAW)
	{
		int16_t gyroData = (imuData[AXIS_ROLL] >> 12);
		bbBuffer[bufferPos++] = gyroData;
		bbBuffer[bufferPos++] = gyroData >> 8;
	}
	if (currentBBFlags & LOG_PITCH_GYRO_RAW)
	{
		int16_t gyroData = (imuData[AXIS_PITCH] >> 12);
		bbBuffer[bufferPos++] = gyroData;
		bbBuffer[bufferPos++] = gyroData >> 8;
	}
	if (currentBBFlags & LOG_YAW_GYRO_RAW)
	{
		int16_t gyroData = (imuData[AXIS_YAW] >> 12);
		bbBuffer[bufferPos++] = gyroData;
		bbBuffer[bufferPos++] = gyroData >> 8;
	}
	if (currentBBFlags & LOG_ROLL_PID_P)
	{
		bbBuffer[bufferPos++] = rollP >> 16;
		bbBuffer[bufferPos++] = rollP >> 24;
	}
	if (currentBBFlags & LOG_ROLL_PID_I)
	{
		bbBuffer[bufferPos++] = rollI >> 16;
		bbBuffer[bufferPos++] = rollI >> 24;
	}
	if (currentBBFlags & LOG_ROLL_PID_D)
	{
		bbBuffer[bufferPos++] = rollD >> 16;
		bbBuffer[bufferPos++] = rollD >> 24;
	}
	if (currentBBFlags & LOG_ROLL_PID_FF)
	{
		bbBuffer[bufferPos++] = rollFF >> 16;
		bbBuffer[bufferPos++] = rollFF >> 24;
	}
	if (currentBBFlags & LOG_ROLL_PID_S)
	{
		bbBuffer[bufferPos++] = rollS >> 16;
		bbBuffer[bufferPos++] = rollS >> 24;
	}
	if (currentBBFlags & LOG_PITCH_PID_P)
	{
		bbBuffer[bufferPos++] = pitchP >> 16;
		bbBuffer[bufferPos++] = pitchP >> 24;
	}
	if (currentBBFlags & LOG_PITCH_PID_I)
	{
		bbBuffer[bufferPos++] = pitchI >> 16;
		bbBuffer[bufferPos++] = pitchI >> 24;
	}
	if (currentBBFlags & LOG_PITCH_PID_D)
	{
		bbBuffer[bufferPos++] = pitchD >> 16;
		bbBuffer[bufferPos++] = pitchD >> 24;
	}
	if (currentBBFlags & LOG_PITCH_PID_FF)
	{
		bbBuffer[bufferPos++] = pitchFF >> 16;
		bbBuffer[bufferPos++] = pitchFF >> 24;
	}
	if (currentBBFlags & LOG_PITCH_PID_S)
	{
		bbBuffer[bufferPos++] = pitchS >> 16;
		bbBuffer[bufferPos++] = pitchS >> 24;
	}
	if (currentBBFlags & LOG_YAW_PID_P)
	{
		bbBuffer[bufferPos++] = yawP >> 16;
		bbBuffer[bufferPos++] = yawP >> 24;
	}
	if (currentBBFlags & LOG_YAW_PID_I)
	{
		bbBuffer[bufferPos++] = yawI >> 16;
		bbBuffer[bufferPos++] = yawI >> 24;
	}
	if (currentBBFlags & LOG_YAW_PID_D)
	{
		bbBuffer[bufferPos++] = yawD >> 16;
		bbBuffer[bufferPos++] = yawD >> 24;
	}
	if (currentBBFlags & LOG_YAW_PID_FF)
	{
		bbBuffer[bufferPos++] = yawFF >> 16;
		bbBuffer[bufferPos++] = yawFF >> 24;
	}
	if (currentBBFlags & LOG_YAW_PID_S)
	{
		bbBuffer[bufferPos++] = yawS >> 16;
		bbBuffer[bufferPos++] = yawS >> 24;
	}
	if (currentBBFlags & LOG_MOTOR_OUTPUTS)
	{
		int64_t throttles64 = ((uint64_t)(throttles[(uint8_t)MOTOR::RR])) << 36 | ((uint64_t)throttles[(uint8_t)MOTOR::FR]) << 24 | throttles[(uint8_t)MOTOR::RL] << 12 | throttles[(uint8_t)MOTOR::FL];
		bbBuffer[bufferPos++] = throttles64 >> 40;
		bbBuffer[bufferPos++] = throttles64 >> 32;
		bbBuffer[bufferPos++] = throttles64 >> 24;
		bbBuffer[bufferPos++] = throttles64 >> 16;
		bbBuffer[bufferPos++] = throttles64 >> 8;
		bbBuffer[bufferPos++] = throttles64;
	}
	if (currentBBFlags & LOG_ALTITUDE)
	{
		bbBuffer[bufferPos++] = baroATO;
		bbBuffer[bufferPos++] = baroATO >> 8;
	}
	blackboxFile.write(bbBuffer, bufferPos);
}