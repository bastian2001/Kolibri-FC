#pragma once
#include <Arduino.h>

#define MOTOR_POLES 14

typedef enum {
	DSHOT_CMD_MOTOR_STOP = 0,
	DSHOT_CMD_BEACON1,
	DSHOT_CMD_BEACON2,
	DSHOT_CMD_BEACON3,
	DSHOT_CMD_BEACON4,
	DSHOT_CMD_BEACON5,
	DSHOT_CMD_ESC_INFO, // V2 includes settings
	DSHOT_CMD_SPIN_DIRECTION_1,
	DSHOT_CMD_SPIN_DIRECTION_2,
	DSHOT_CMD_3D_MODE_OFF,
	DSHOT_CMD_3D_MODE_ON,
	DSHOT_CMD_SETTINGS_REQUEST, // Currently not implemented
	DSHOT_CMD_SAVE_SETTINGS,
	DSHOT_CMD_EXTENDED_TELEMETRY_ENABLE,
	DSHOT_CMD_EXTENDED_TELEMETRY_DISABLE,
	DSHOT_CMD_SPIN_DIRECTION_NORMAL   = 20,
	DSHOT_CMD_SPIN_DIRECTION_REVERSED = 21,
	DSHOT_CMD_LED0_ON,                       // BLHeli32 only
	DSHOT_CMD_LED1_ON,                       // BLHeli32 only
	DSHOT_CMD_LED2_ON,                       // BLHeli32 only
	DSHOT_CMD_LED3_ON,                       // BLHeli32 only
	DSHOT_CMD_LED0_OFF,                      // BLHeli32 only
	DSHOT_CMD_LED1_OFF,                      // BLHeli32 only
	DSHOT_CMD_LED2_OFF,                      // BLHeli32 only
	DSHOT_CMD_LED3_OFF,                      // BLHeli32 only
	DSHOT_CMD_AUDIO_STREAM_MODE_ON_OFF = 30, // KISS audio Stream mode on/Off
	DSHOT_CMD_SILENT_MODE_ON_OFF       = 31, // KISS silent Mode on/Off
	DSHOT_CMD_MAX                      = 47
} ESCCommand;

extern PIO escPio;                    // pio block used for the ESC communication
extern volatile u32 erpmEdges[4][32]; // edge detection durations for each motor
extern volatile u32 escRpm[4];        // decoded RPM values
extern const u32 escDecodeLut[32];    // lookup table for GCR decoding
extern u8 escErpmFail;                // flags for failed RPM decoding
extern u8 escDmaChannel[4];           // DMA channels for the ESC communication
extern u8 escClearDmaChannel;         // DMA channel for clearing the erpm edges, essentially memset(0) for the erpmEdges array
extern u32 enableDShot;               // set to 0 to disable DShot output, e.g. for 4Way
extern u32 escPioOffset;              // offset at which the DShot program is stored

/// @brief Initializes the ESC communication
void initESCs();

/**
 * @brief Sends throttles to all four ESCs
 *
 * @details Telemetry bit is not set
 *
 * @param throttles Array of four throttle values (0-2000)
 */
void sendThrottles(const i16 throttles[4]);

/**
 * @brief Sends raw values to all four ESCs (useful for special commands)
 *
 * @details Telemetry bit always set
 *
 * @param raw Array of four raw values (0-2047, with 1-47 being the special commands, and the others being the throttle values)
 */
void sendRaw11Bit(const u16 raw[4]);

/**
 * @brief Sends raw values to all four ESCs (useful for special commands)
 *
 * @param raw Array of four raw values, including the telemetry bits and checksum
 */
void sendRaw16Bit(const u16 raw[4]);