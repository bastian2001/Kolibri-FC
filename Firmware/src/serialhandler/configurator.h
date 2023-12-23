#pragma once
#include <Arduino.h>

#define CONFIG_BUFFER_PREFIX 0
#define CONFIG_BUFFER_LENGTH 2
#define CONFIG_BUFFER_COMMAND 4
#define CONFIG_BUFFER_DATA 6

// responses to commands have the command | 0x4000, followed by the data, no separate response codes
// When a command fails, the response code is command | 0x8000, followed by the error, no separate error codes.
// indicators have a command starting with the 0xC000 bit set
enum class ConfigCmd {
	// commands/responses/errors
	STATUS,
	TASK_STATUS,
	REBOOT,
	SAVE_SETTINGS,
	PLAY_SOUND,
	BB_FILE_LIST,
	BB_FILE_INFO,
	BB_FILE_DOWNLOAD,
	BB_FILE_DELETE,
	BB_FORMAT,
	WRITE_OSD_FONT_CHARACTER,
	SET_MOTORS,
	GET_MOTORS,
	BB_FILE_DOWNLOAD_RAW,
	SET_DEBUG_LED,
	CONFIGURATOR_PING,
	REBOOT_TO_BOOTLOADER,
	GET_NAME,
	SET_NAME,
	GET_PIDS,
	SET_PIDS,
	GET_RATES,
	SET_RATES,
	GET_BB_SETTINGS,
	SET_BB_SETTINGS,
	GET_ROTATION,
	SERIAL_PASSTHROUGH,
	GET_GPS_STATUS,
	GET_GPS_ACCURACY,
	GET_GPS_TIME,
	GET_GPS_MOTION,
	REBOOT_BY_WATCHDOG,
	GET_CRASH_DUMP,
	CLEAR_CRASH_DUMP,

	// indicators
	IND_MESSAGE = 0xC000,
};

void sendCommand(uint16_t command, const char *data = nullptr, uint16_t len = 0);
void configuratorHandleByte(uint8_t c, uint8_t serialNum);
extern elapsedMillis configOverrideMotors;
void configuratorLoop();