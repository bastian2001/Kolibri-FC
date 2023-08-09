#pragma once
#include <Arduino.h>
#include "elapsedMillis.h"

typedef struct crsf_channels_s
{
	unsigned ch0 : 11;
	unsigned ch1 : 11;
	unsigned ch2 : 11;
	unsigned ch3 : 11;
	unsigned ch4 : 11;
	unsigned ch5 : 11;
	unsigned ch6 : 11;
	unsigned ch7 : 11;
	unsigned ch8 : 11;
	unsigned ch9 : 11;
	unsigned ch10 : 11;
	unsigned ch11 : 11;
	unsigned ch12 : 11;
	unsigned ch13 : 11;
	unsigned ch14 : 11;
	unsigned ch15 : 11;
} crsf_channels_t;

class ExpressLRS
{

public:
	ExpressLRS(SerialUART &elrsSerial, uint32_t baudrate, uint8_t pinTX = -1, uint8_t pinRX = -1);
	~ExpressLRS();
	void loop();
	uint16_t channels[16] = {0};
	elapsedMicros sinceLastRCMessage;
	elapsedMicros sinceLastMessage;
	bool isLinkUp = false;
	bool isReceiverUp = false;
	uint32_t msgCount = 0;
	uint32_t errorCount = 0;
	bool errorFlag = false;
	uint8_t lastError = 0;
	static const uint8_t NO_ERROR = 0x00;
	static const uint8_t ERROR_CRC = 0x01;
	static const uint8_t ERROR_BUFFER_OVERFLOW = 0x02;
	static const uint8_t ERROR_UNSUPPORTED_COMMAND = 0x03;
	static const uint8_t ERROR_INVALID_PREFIX = 0x04;
	static const uint8_t ERROR_INVALID_LENGTH = 0x05;

private:
	static const uint8_t RX_PREFIX = 0xC8;
	static const uint8_t TX_PREFIX = 0xEC;
	static const uint8_t LINK_STATISTICS = 0x14;
	static const uint8_t RC_CHANNELS_PACKED = 0x16;
	static const uint8_t DEVICE_PING = 0x28;
	static const uint8_t DEVICE_INFO = 0x29;
	static const uint8_t PARAMETER_SETTINGS_ENTRY = 0x2B;
	static const uint8_t PARAMETER_READ = 0x2C;
	static const uint8_t PARAMETER_WRITE = 0x2D;
	static const uint8_t COMMAND = 0x32;
	uint8_t msgBuffer[64] = {0};
	uint8_t msgBufIndex = 0;
	SerialUART &elrsSerial;
	const uint8_t pinTX;
	const uint8_t pinRX;
	const uint32_t baudrate;
	void processMessage();
};