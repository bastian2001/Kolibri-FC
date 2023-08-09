#pragma once
#include <Arduino.h>
#include "elapsedMillis.h"

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
	int16_t uplinkRssi[2] = {0};
	uint8_t uplinkLinkQuality = 0;
	int8_t uplinkSNR = 0;
	uint8_t antennaSelection = 0;
	uint8_t packetRate = 0;
	uint8_t txPower = 0;
	int16_t downlinkRssi = 0;
	uint8_t downlinkLinkQuality = 0;
	int8_t downlinkSNR = 0;
	bool armed = false;

private:
	const uint16_t powerStates[9] = {0, 10, 25, 100, 500, 1000, 2000, 50, 250};
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
	static const uint8_t FRAMETYPE_WHATEVER = 0x7A;
	uint8_t msgBuffer[64] = {0};
	uint8_t msgBufIndex = 0;
	SerialUART &elrsSerial;
	const uint8_t pinTX;
	const uint8_t pinRX;
	const uint32_t baudrate;
	void processMessage();
};