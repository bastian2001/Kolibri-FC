#pragma once
#include <Arduino.h>

extern bool configuratorConnected; // true if the configurator is connected
extern u8 accelCalDone;            // accel calibration flag to send a message to the configurator

/**
 * @brief Kolibri Serial commands
 *
 * @details These commands are used to communicate with the configurator. Each command has a corresponding response. The command is sent as a 16-bit value, followed by the data. The response is the same 16-bit value, followed by the data. If the command fails, the response is the command with the 0x8000 bit set, followed by the error message. A successful command is responded to with the command | 0x4000, followed by the data. Indicators are commands starting with the 0xC000 bit set.
 */
enum class ConfigCmd { // commands/responses/errors
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
	ESC_PASSTHROUGH,
	GET_CRASH_DUMP,
	CLEAR_CRASH_DUMP,
	CALIBRATE_ACCELEROMETER,
	GET_MAG_DATA,
	MAG_CALIBRATE,

	// indicators
	IND_MESSAGE = 0xC000,
};

enum class MspState {
	IDLE,         // waiting for $
	PACKET_START, // receiving M or X
	TYPE_V1,      // got M, receiving type byte (<, >, !)
	LEN_V1,       // if 255 is received in this step, inject jumbo len bytes
	JUMBO_LEN_LO_V1,
	JUMBO_LEN_HI_V1,
	CMD_V1,
	PAYLOAD_V1,
	FLAG_V2_OVER_V1,
	CMD_LO_V2_OVER_V1,
	CMD_HI_V2_OVER_V1,
	LEN_LO_V2_OVER_V1,
	LEN_HI_V2_OVER_V1,
	PAYLOAD_V2_OVER_V1,
	CHECKSUM_V2_OVER_V1,
	CHECKSUM_V1,
	TYPE_V2, // got X, receiving type byte (<, >, !)
	FLAG_V2,
	CMD_LO_V2,
	CMD_HI_V2,
	LEN_LO_V2,
	LEN_HI_V2,
	PAYLOAD_V2,
	CHECKSUM_V2,
};
enum class MspMsgType {
	REQUEST  = '<',
	RESPONSE = '>',
	ERROR    = '!',
};

/**
 * @brief Send a command to the configurator
 *
 * @param command command to issue
 * @param data data buffer, default is nullptr for no data
 * @param len length of the data buffer, default is 0
 */
void sendCommand(u16 command, const char *data = nullptr, u16 len = 0);

/**
 * @brief Process a byte from the configurator
 *
 * @param c data byte
 * @param serialNum serial port number to send the response to
 */
void configuratorHandleByte(u8 c, u8 serialNum);

/// @brief counter for the motor override timeout
/// @details If the configurator is connected, it can override the motor values when this is < 1000, and it is possible to arm with an attached configurator
extern elapsedMillis configOverrideMotors;

/// @brief handles configurator pings and asynchronous operations
void configuratorLoop();