#pragma once
#include <Arduino.h>

typedef enum
{
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
	DSHOT_CMD_SPIN_DIRECTION_NORMAL = 20,
	DSHOT_CMD_SPIN_DIRECTION_REVERSED = 21,
	DSHOT_CMD_LED0_ON,						 // BLHeli32 only
	DSHOT_CMD_LED1_ON,						 // BLHeli32 only
	DSHOT_CMD_LED2_ON,						 // BLHeli32 only
	DSHOT_CMD_LED3_ON,						 // BLHeli32 only
	DSHOT_CMD_LED0_OFF,						 // BLHeli32 only
	DSHOT_CMD_LED1_OFF,						 // BLHeli32 only
	DSHOT_CMD_LED2_OFF,						 // BLHeli32 only
	DSHOT_CMD_LED3_OFF,						 // BLHeli32 only
	DSHOT_CMD_AUDIO_STREAM_MODE_ON_OFF = 30, // KISS audio Stream mode on/Off
	DSHOT_CMD_SILENT_MODE_ON_OFF = 31,		 // KISS silent Mode on/Off
	DSHOT_CMD_MAX = 47
} ESCCommand;

extern PIO escPio;
extern uint8_t escSm;
extern uint32_t motorPacket[2];

void initESCs();

// 11 bit value without checksum and telemetry bit
void sendThrottles(const int16_t throttles[4]);
void sendRaw11Bit(const uint16_t raw[4]);
void sendRaw16Bit(const uint16_t raw[4]);