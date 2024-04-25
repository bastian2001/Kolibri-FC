/**
 * MSP Guidelines, emphasis is used to clarify.
 *
 * Each FlightController (FC, Server) MUST change the API version when any MSP command is added, deleted, or changed.
 *
 * If you fork the FC source code and release your own version, you MUST change the Flight Controller Identifier.
 *
 * NEVER release a modified copy of this code that shares the same Flight controller IDENT and API version
 * if the API doesn't match EXACTLY.
 *
 * Consumers of the API (API clients) SHOULD first attempt to get a response from the MspFn::API_VERSION command.
 * If no response is obtained then client MAY try the legacy MSP_IDENT (100) command.
 *
 * API consumers should ALWAYS handle communication failures gracefully and attempt to continue
 * without the information if possible.  Clients MAY log/display a suitable message.
 *
 * API clients should NOT attempt any communication if they can't handle the returned API MAJOR VERSION.
 *
 * API clients SHOULD attempt communication if the API MINOR VERSION has increased from the time
 * the API client was written and handle command failures gracefully.  Clients MAY disable
 * functionality that depends on the commands while still leaving other functionality intact.
 * that the newer API version may cause problems before using API commands that change FC state.
 *
 * It is for this reason that each MSP command should be specific as possible, such that changes
 * to commands break as little functionality as possible.
 *
 * API client authors MAY use a compatibility matrix/table when determining if they can support
 * a given command from a given flight controller at a given api version level.
 *
 * Developers MUST NOT create new MSP commands that do more than one thing.
 *
 * Failure to follow these guidelines will likely invoke the wrath of developers trying to write tools
 * that use the API and the users of those tools.
 */

#pragma once
#include <Arduino.h>

extern bool configuratorConnected; // true if the configurator is connected
extern u8 accelCalDone;            // accel calibration flag to send a message to the configurator

/**
 * @brief MSP Serial Functions
 *
 * @details These commands are used to communicate with the configurator and other peripherals. For Kolibri specific functions, the space 0x4000-0x4FFF is used. More details: https://github.com/iNavFlight/inav/wiki/MSP-V2
 */
enum class MspFn {
	API_VERSION              = 1,
	FIRMWARE_VARIANT         = 2,
	FIRMWARE_VERSION         = 3,
	BOARD_INFO               = 4,
	BUILD_INFO               = 5,
	GET_NAME                 = 10,
	SET_NAME                 = 11,
	GET_FEATURE_CONFIG       = 36,
	SET_FEATURE_CONFIG       = 37,
	REBOOT                   = 68,
	GET_ADVANCED_CONFIG      = 90,
	SET_ADVANCED_CONFIG      = 91,
	MSP_STATUS               = 101,
	GET_MOTOR                = 104,
	MSP_ATTITUDE             = 108,
	BOXIDS                   = 119,
	GET_MOTOR_3D_CONFIG      = 124,
	GET_MOTOR_CONFIG         = 131,
	UID                      = 160,
	SET_MOTOR                = 214,
	SET_MOTOR_3D_CONFIG      = 217,
	SET_MOTOR_CONFIG         = 222,
	ENABLE_4WAY_IF           = 245,
	MSP_V2_FRAME             = 255,
	STATUS                   = 0x4000,
	TASK_STATUS              = 0x4001,
	DUMMY_REBOOT             = 0x4002,
	SAVE_SETTINGS            = 0x4003,
	PLAY_SOUND               = 0x4004,
	BB_FILE_LIST             = 0x4005,
	BB_FILE_INFO             = 0x4006,
	BB_FILE_DOWNLOAD         = 0x4007,
	BB_FILE_DELETE           = 0x4008,
	BB_FORMAT                = 0x4009,
	WRITE_OSD_FONT_CHARACTER = 0x400A,
	SET_MOTORS               = 0x400B,
	GET_MOTORS               = 0x400C,
	BB_FILE_DOWNLOAD_RAW     = 0x400D,
	SET_DEBUG_LED            = 0x400E,
	CONFIGURATOR_PING        = 0x400F,
	REBOOT_TO_BOOTLOADER     = 0x4010,
	GET_PIDS                 = 0x4013,
	SET_PIDS                 = 0x4014,
	GET_RATES                = 0x4015,
	SET_RATES                = 0x4016,
	GET_BB_SETTINGS          = 0x4017,
	SET_BB_SETTINGS          = 0x4018,
	GET_ROTATION             = 0x4019,
	SERIAL_PASSTHROUGH       = 0x401A,
	GET_GPS_STATUS           = 0x401B,
	GET_GPS_ACCURACY         = 0x401C,
	GET_GPS_TIME             = 0x401D,
	GET_GPS_MOTION           = 0x401E,
	ESC_PASSTHROUGH          = 0x401F,
	GET_CRASH_DUMP           = 0x4020,
	CLEAR_CRASH_DUMP         = 0x4021,
	CALIBRATE_ACCELEROMETER  = 0x4022,
	GET_MAG_DATA             = 0x4023,
	MAG_CALIBRATE            = 0x4024,
	IND_MESSAGE              = 0x4025,
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

#define MSP_PROTOCOL_VERSION 0
#define API_VERSION_MAJOR 1
#define API_VERSION_MINOR 42

#define FIRMWARE_VERSION_MAJOR 0
#define FIRMWARE_VERSION_MINOR 1
#define FIRMWARE_VERSION_PATCH 0

#define KOLIBRI_IDENTIFIER "BTFL" // Baseflight: BAFL, Betaflight: BTFL, Cleanflight: CLFL, iNav: INAV, MultiWii: MWII, Raceflight: RCFL
#define FIRMWARE_IDENTIFIER_LENGTH 4

enum class McuType : u8 {
	SIMULATOR = 0,
	F40X,
	F411,
	F446,
	F722,
	F745,
	F746,
	F765,
	H750,
	H743_REV_UNKNOWN,
	H743_REV_Y,
	H743_REV_X,
	H743_REV_V,
	H7A3,
	H723_725,
	G474,
	H730,
	AT32,
	RP2040  = 64,
	UNKNOWN = 255,
};

enum class MspMsgType {
	REQUEST  = '<',
	RESPONSE = '>',
	ERROR    = '!',
};

enum class MspVersion {
	V2,
	V1,
	V1_JUMBO,
	V2_OVER_V1,
	V2_OVER_V1_JUMBO,
};

/**
 * @brief Send an MSP packet to the configurator
 *
 * @param type message type (request, response, error)
 * @param fn function to send
 * @param version MSP version to use
 * @param data data buffer, default is nullptr for no data
 * @param len length of the data buffer, default is 0
 */
void sendMsp(MspMsgType type, MspFn fn, MspVersion version = MspVersion::V2, const char *data = nullptr, u16 len = 0);

/**
 * @brief Process a byte from the configurator
 *
 * @param c data byte
 * @param serialNum serial port number to send the response to
 */
void mspHandleByte(u8 c, u8 serialNum);

/// @brief counter for the motor override timeout
/// @details If the configurator is connected, it can override the motor values when this is < 1000, and it is possible to arm with an attached configurator
extern elapsedMillis mspOverrideMotors;

/// @brief handles configurator pings and asynchronous operations
void configuratorLoop();