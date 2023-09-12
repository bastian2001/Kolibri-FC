#pragma once
#include <Arduino.h>

#define CONFIG_BUFFER_PREFIX 0
#define CONFIG_BUFFER_LENGTH 2
#define CONFIG_BUFFER_COMMAND 4
#define CONFIG_BUFFER_DATA 6

// responses to commands have the command | 0x4000, followed by the data
// errors have the command | 0x8000, followed by the error
// indicators have a command starting with the 0x2000 bit set
enum class ConfigCmd
{
	STATUS,
	TASK_STATUS,
	GET_SETTING,
	SET_SETTING,
	PLAY_SOUND,
	BB_FILE_LIST,
	BB_FILE_INFO,
	BB_FILE_DOWNLOAD,
	BB_FILE_DELETE,
	BB_FORMAT,
	WRITE_OSD_FONT_CHARACTER,
	SET_MOTORS,
	BB_FILE_DOWNLOAD_RAW,
	SET_DEBUG_LED,
	CONFIGURATOR_PING,
};

void sendCommand(uint16_t command, const char *data = nullptr, uint16_t len = 0);
void configuratorHandleByte(uint8_t c, uint8_t serialNum);
extern elapsedMillis configOverrideMotors;
void configuratorLoop();