#pragma once
#include "elapsedMillis.h"
#include "hardware/interp.h"
#include "msp.h"
#include <Arduino.h>
#include <vector>
using std::vector;

// protocol wiki of CRSF, made by ExpressLRS
// https://github.com/crsf-wg/crsf/wiki

extern RingBuffer<u8> elrsBuffer;

class ExpressLRS {
public:
	/**
	 * @brief New ExpressLRS instance
	 *
	 * @param elrsSerial Serial port used for telemetry output
	 * @param baudrate Baudrate that is used to set up the serial port
	 * @param pinTX TX pin for the serial port
	 * @param pinRX RX pin for the serial port
	 */
	ExpressLRS(SerialUART &elrsSerial, u32 baudrate, u8 pinTX = -1, u8 pinRX = -1);
	~ExpressLRS();
	/// @brief Called regularly to process incoming bytes and send telemetry
	void loop();
	u32 channels[16] = {0}; // raw RC channels (1000-2000 for throttle and switches, 988-2012 for other sticks)
	u32 lastChannels[16] = {0}; // RC channels from the last packet (1000-2000 for throttle and switches, 988-2012 for other sticks)
	/**
	 * @brief Calculate the smoothed RC channels
	 *
	 * @details uses linear interpolation over 4ms / extrapolation up to 8ms and clamps the values to 1000-2000 / 988-2012
	 * @param smoothChannels Array to store the smoothed channels
	 */
	void getSmoothChannels(fix32 smoothChannels[4]);
	elapsedMicros sinceLastRCMessage; // time (µs) since the last valid RC message was received
	elapsedMicros sinceLastMessage; // time (µs) since the last valid message was received
	bool isLinkUp = false; // true if the link is up (300 RC messages received and 20 in the last second)
	bool isReceiverUp = false; // true if the receiver recognized (1 message in the last second)
	u32 msgCount = 0; // total count of any received message
	u32 rcMsgCount = 0; // total count of received RC messages
	u32 errorCount = 0; // total count of errors (CRC, buffer overflow, unsupported command, invalid prefix, invalid length)
	bool errorFlag = false; // set to true whenever an error occurs
	u8 lastError = NO_ERROR; // last error code
	static const u8 NO_ERROR = 0x00;
	static const u8 ERROR_CRC = 0x01;
	static const u8 ERROR_BUFFER_OVERFLOW = 0x02;
	static const u8 ERROR_UNSUPPORTED_COMMAND = 0x03;
	static const u8 ERROR_INVALID_PREFIX = 0x04;
	static const u8 ERROR_INVALID_LENGTH = 0x05;
	i16 uplinkRssi[2] = {0}; // RX RSSI values of both antennas (signed, more negative = worse)
	u8 uplinkLinkQuality = 0; // RX packet success rate 0-100 [%]
	i8 uplinkSNR = 0; // RX SNR in dB
	u8 antennaSelection = 0; // used antenna (0 = antenna 1, 1 = antenna 2)
	u8 packetRateIdx = 0; // packet rate index
	u16 txPower = 0; // current TX power in mW
	i16 downlinkRssi = 0; // telemetry RSSI (signed, more negative = worse)
	u8 downlinkLinkQuality = 0; // telemetry packet success rate 0-100 [%]
	i8 downlinkSNR = 0; // telemetry SNR in dB
	u16 targetPacketRate = 0; // in Hz
	u16 actualPacketRate = 0; // in Hz
	u32 consecutiveArmedCycles = 0; // number of cycles the switch is in the armed position, reset to 0 when disarmed
	u32 newPacketFlag = 0; // flags for new RC packets (set to 0xFFFFFFFF when a new packet is received)
	void sendPacket(u8 cmd, const char *payload, u8 payloadLen);
	void sendExtPacket(u8 cmd, u8 destAddr, u8 srcAddr, const char *extPayload, u8 extPayloadLen);
	void sendMspMsg(MspMsgType type, u8 mspVersion, const char *payload, u16 payloadLen);

	static const u8 FRAMETYPE_GPS = 0x02;
	static const u8 FRAMETYPE_VARIO = 0x07;
	static const u8 FRAMETYPE_BATTERY = 0x08;
	static const u8 FRAMETYPE_BARO_ALT = 0x09;
	static const u8 FRAMETYPE_HEARTBEAT = 0x0B;
	static const u8 FRAMETYPE_LINK_STATISTICS = 0x14;
	static const u8 FRAMETYPE_RC_CHANNELS_PACKED = 0x16;
	static const u8 FRAMETYPE_SUBSET_RC_CHANNELS_PACKED = 0x17;
	// static const u8 FRAMETYPE_LINK_RX_ID = 0x1C; // unclear usage, ignore
	// static const u8 FRAMETYPE_LINK_TX_ID = 0x1D; // unclear usage, ignore
	static const u8 FRAMETYPE_ATTITUDE = 0x1E;
	static const u8 FRAMETYPE_FLIGHTMODE = 0x21;
	static const u8 FRAMETYPE_DEVICE_PING = 0x28;
	static const u8 FRAMETYPE_DEVICE_INFO = 0x29;
	static const u8 FRAMETYPE_PARAMETER_SETTINGS_ENTRY = 0x2B;
	static const u8 FRAMETYPE_PARAMETER_READ = 0x2C;
	static const u8 FRAMETYPE_PARAMETER_WRITE = 0x2D;
	// static const u8 FRAMETYPE_ELRS_STATUS = 0x2E; // only on transmitter side
	static const u8 FRAMETYPE_COMMAND = 0x32;
	// static const u8 FRAMETYPE_RADIO_ID = 0x3A; // only on transmitter side
	// static const u8 FRAMETYPE_KISS_REQ = 0x78; // non-standard
	// static const u8 FRAMETYPE_KISS_RESP = 0x79; // non-standard
	static const u8 FRAMETYPE_MSP_REQ = 0x7A;
	static const u8 FRAMETYPE_MSP_RESP = 0x7B;
	static const u8 FRAMETYPE_MSP_WRITE = 0x7C;
	static const u8 FRAMETYPE_DISPLAYPORT_CMD = 0x7D;
	// static const u8 FRAMETYPE_ARDUPILOT_RESP = 0x80; // non-standard

private:
	const u16 powerStates[9] = {0, 10, 25, 100, 500, 1000, 2000, 250, 50};
	const u16 packetRates[20] = {
		4, 25, 50, 100, 100, 150, 200, 250, 333, 500, 250, 500, 500, 1000, 50, 200, 500, 1000, 1000, 1000};
	static const u8 CRSF_SYNC_BYTE = 0xC8;
	static const u8 ADDRESS_FLIGHT_CONTROLLER = 0xC8;
	static const u8 ADDRESS_RADIO_TRANSMITTER = 0xEA;
	static const u8 ADDRESS_CRSF_RECEIVER = 0xEC;
	static const u8 ADDRESS_CRSF_TRANSMITTER = 0xEE;
	static interp_config interpConfig0; // used to interpolate for smooth sticks
	static interp_config interpConfig1; // used to interpolate for smooth sticks
	static interp_config interpConfig2; // used to clamp smoothed values
	u8 msgBuffer[64] = {0};
	u8 telemBuffer[30] = {0};
	u32 currentTelemSensor = 0;
	u8 msgBufIndex = 0;
	u8 mspTelemSeq = 0;
	SerialUART &elrsSerial;
	Stream *elrsStream;
	u8 serialNum = 255;
	const u8 pinTX;
	const u8 pinRX;
	const u32 baudrate;
	elapsedMillis frequencyTimer;
	elapsedMillis telemetryTimer;
	elapsedMillis heartbeatTimer;
	u32 rcPacketRateCounter = 0;
	u32 packetRateCounter = 0;
	u32 crc = 0;
	u8 lastExtSrcAddr = 0;
	u8 mspRxPayload[512] = {0};
	u16 mspRxPayloadLen = 0;
	u16 mspRxCmd = 0;
	u8 mspRxSeq = 0;
	u16 mspRxPos = 0;
	u8 mspRxFlag = 0;
	bool pinged = false;
	bool mspRecording = false;
	MspVersion mspVersion = MspVersion::V2_OVER_CRSF;
	void processMessage();
};

struct __attribute__((packed)) crsf_channels_10 {
	unsigned ch0 : 10;
	unsigned ch1 : 10;
	unsigned ch2 : 10;
	unsigned ch3 : 10;
	unsigned ch4 : 10;
	unsigned ch5 : 10;
	unsigned ch6 : 10;
	unsigned ch7 : 10;
	unsigned ch8 : 10;
	unsigned ch9 : 10;
	unsigned ch10 : 10;
	unsigned ch11 : 10;
	unsigned ch12 : 10;
	unsigned ch13 : 10;
	unsigned ch14 : 10;
	unsigned ch15 : 10;
};

struct __attribute__((packed)) crsf_channels_11 {
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
};

struct __attribute__((packed)) crsf_channels_12 {
	unsigned ch0 : 12;
	unsigned ch1 : 12;
	unsigned ch2 : 12;
	unsigned ch3 : 12;
	unsigned ch4 : 12;
	unsigned ch5 : 12;
	unsigned ch6 : 12;
	unsigned ch7 : 12;
	unsigned ch8 : 12;
	unsigned ch9 : 12;
	unsigned ch10 : 12;
	unsigned ch11 : 12;
	unsigned ch12 : 12;
	unsigned ch13 : 12;
	unsigned ch14 : 12;
	unsigned ch15 : 12;
};

struct __attribute__((packed)) crsf_channels_13 {
	unsigned ch0 : 13;
	unsigned ch1 : 13;
	unsigned ch2 : 13;
	unsigned ch3 : 13;
	unsigned ch4 : 13;
	unsigned ch5 : 13;
	unsigned ch6 : 13;
	unsigned ch7 : 13;
	unsigned ch8 : 13;
	unsigned ch9 : 13;
	unsigned ch10 : 13;
	unsigned ch11 : 13;
	unsigned ch12 : 13;
	unsigned ch13 : 13;
	unsigned ch14 : 13;
	unsigned ch15 : 13;
};
