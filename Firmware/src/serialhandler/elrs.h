#pragma once
#include "elapsedMillis.h"
#include <Arduino.h>
#include <vector>
using std::vector;

// protocol wiki of CRSF, made by ExpressLRS
// https://github.com/crsf-wg/crsf/wiki

extern RingBuffer<u8> elrsBuffer;
class ExpressLRS {
public:
	ExpressLRS(SerialUART &elrsSerial, u32 baudrate, u8 pinTX = -1, u8 pinRX = -1);
	~ExpressLRS();
	void loop();
	u32 channels[16]	 = {0};
	u32 lastChannels[16] = {0};
	void getSmoothChannels(u16 smoothChannels[4]); // calculates the sticks' smooth position
	elapsedMicros sinceLastRCMessage;
	elapsedMicros sinceLastMessage;
	bool isLinkUp							  = false;
	bool isReceiverUp						  = false;
	u32 msgCount							  = 0;
	u32 rcMsgCount							  = 0;
	u32 errorCount							  = 0;
	bool errorFlag							  = false;
	u8 lastError							  = 0;
	static const u8 NO_ERROR				  = 0x00;
	static const u8 ERROR_CRC				  = 0x01;
	static const u8 ERROR_BUFFER_OVERFLOW	  = 0x02;
	static const u8 ERROR_UNSUPPORTED_COMMAND = 0x03;
	static const u8 ERROR_INVALID_PREFIX	  = 0x04;
	static const u8 ERROR_INVALID_LENGTH	  = 0x05;
	i16 uplinkRssi[2]						  = {0};
	u8 uplinkLinkQuality					  = 0;
	i8 uplinkSNR							  = 0;
	u8 antennaSelection						  = 0;
	u8 packetRate							  = 0;
	u8 txPower								  = 0;
	i16 downlinkRssi						  = 0;
	u8 downlinkLinkQuality					  = 0;
	i8 downlinkSNR							  = 0;
	u32 consecutiveArmedCycles				  = 0;
	u32 newPacketFlag						  = 0;

private:
	const u16 powerStates[9]				 = {0, 10, 25, 100, 500, 1000, 2000, 50, 250};
	static const u8 RX_PREFIX				 = 0xC8;
	static const u8 TX_PREFIX				 = 0xEC;
	static const u8 LINK_STATISTICS			 = 0x14;
	static const u8 RC_CHANNELS_PACKED		 = 0x16;
	static const u8 DEVICE_PING				 = 0x28;
	static const u8 DEVICE_INFO				 = 0x29;
	static const u8 PARAMETER_SETTINGS_ENTRY = 0x2B;
	static const u8 PARAMETER_READ			 = 0x2C;
	static const u8 PARAMETER_WRITE			 = 0x2D;
	static const u8 COMMAND					 = 0x32;
	static const u8 MSP_REQ					 = 0x7A;
	u8 msgBuffer[64]						 = {0};
	u8 msgBufIndex							 = 0;
	SerialUART &elrsSerial;
	const u8 pinTX;
	const u8 pinRX;
	const u32 baudrate;
	elapsedMillis frequencyTimer;
	u32 rcPacketRateCounter = 0;
	u32 packetRateCounter	= 0;
	u32 lastMsgCount		= 0;
	u32 crc					= 0;
	void processMessage();
};