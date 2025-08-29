#pragma once
#include "hardware/interp.h"
#include "msp.h"
#include <Arduino.h>
#include <elapsedMillis.h>
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
	 * @param serialNum serial number to use for sending
	 */
	ExpressLRS(u8 serialNum);

	/// @brief Called regularly to process incoming bytes and send telemetry
	void loop();

	/**
	 * @brief Calculate the smoothed RC channels
	 *
	 * @details uses linear interpolation over 4ms / extrapolation up to 8ms and clamps the values to 988-2012
	 * @param smoothChannels Array to store the smoothed channels
	 */
	void getSmoothChannels(fix32 smoothChannels[4]);

	u32 channels[16] = {0}; // raw RC channels (1000-2000 for switches, 988-2012 for other sticks)
	u32 lastChannels[16] = {0}; // RC channels from the last packet (1000-2000 for switches, 988-2012 for other sticks)
	u32 newPacketFlag = 0; // flags for new RC packets (set to 0xFFFFFFFF when a new packet is received)
	u32 newLinkStatsFlag = 0; // flags for new link stats (set to 0xFFFFFFFF when a new link stats are available)

	// custom link info
	elapsedMicros sinceLastRCMessage; // time (µs) since the last valid RC message was received
	elapsedMicros sinceLastMessage; // time (µs) since the last valid message was received
	bool isLinkUp = false; // true if the link is up (300 RC messages received and 20 in the last second)
	bool isReceiverUp = false; // true if the receiver recognized (1 message in the last second)
	u32 msgCount = 0; // total count of any received message
	u32 rcMsgCount = 0; // total count of received RC messages
	u16 actualPacketRate = 0; // in Hz

	// ELRS provided link stats
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

	// communication errors
	u32 errorCount = 0; // total count of errors (CRC, buffer overflow, unsupported command, invalid prefix, invalid length)
	bool errorFlag = false; // set to true whenever an error occurs
	enum {
		NO_ERROR,
		ERROR_CRC,
		ERROR_BUFFER_OVERFLOW,
		ERROR_UNSUPPORTED_COMMAND,
		ERROR_INVALID_LENGTH,
		ERROR_INVALID_PKT_LEN,
		ERROR_MSP_OVER_CRSF,
	};
	u8 lastError = NO_ERROR; // last error code

	/**
	 * @brief Send a packet to the receiver
	 *
	 * @param cmd packet type
	 * @param payload pointer to payload, may be nullptr for zero-length packets
	 * @param payloadLen length of the actual payload, max 60
	 */
	void sendPacket(u8 cmd, const char *payload, u8 payloadLen);
	/**
	 * @brief Send an extended packet to another device
	 *
	 * @param cmd packet type
	 * @param destAddr to which device
	 * @param srcAddr from which device
	 * @param extPayload pointer to payload, may be nullptr for zero-length packets
	 * @param extPayloadLen length of the actual payload, max 58
	 */
	void sendExtPacket(u8 cmd, u8 destAddr, u8 srcAddr, const char *extPayload, u8 extPayloadLen);
	/**
	 * @brief Send an MSP message to another device
	 *
	 * ext src = FC, ext dest defined by who last requested anything from the FC.
	 *
	 * For Jumbo, report a V1 packet with the header stating 255 length, and then after the MSP function append the actual size.
	 *
	 * @param type MspMsgType:: object, either REQUEST, RESPONSE or ERROR
	 * @param mspVersion 1 or 2 for V1 or V2. See description for Jumbo
	 * @param payload pointer to fully prepared payload ((2, 5, 4 bits) header + data)
	 * @param payloadLen length of header + data
	 */
	void sendMspMsg(MspMsgType type, u8 mspVersion, const char *payload, u16 payloadLen);

	// possible frametypes
	static constexpr u8 FRAMETYPE_GPS = 0x02;
	static constexpr u8 FRAMETYPE_VARIO = 0x07;
	static constexpr u8 FRAMETYPE_BATTERY = 0x08;
	static constexpr u8 FRAMETYPE_BARO_ALT = 0x09;
	static constexpr u8 FRAMETYPE_HEARTBEAT = 0x0B;
	static constexpr u8 FRAMETYPE_LINK_STATISTICS = 0x14;
	static constexpr u8 FRAMETYPE_RC_CHANNELS_PACKED = 0x16;
	static constexpr u8 FRAMETYPE_SUBSET_RC_CHANNELS_PACKED = 0x17;
	// static constexpr u8 FRAMETYPE_LINK_RX_ID = 0x1C; // unclear usage, ignore
	// static constexpr u8 FRAMETYPE_LINK_TX_ID = 0x1D; // unclear usage, ignore
	static constexpr u8 FRAMETYPE_ATTITUDE = 0x1E;
	static constexpr u8 FRAMETYPE_FLIGHTMODE = 0x21;
	static constexpr u8 FRAMETYPE_DEVICE_PING = 0x28;
	static constexpr u8 FRAMETYPE_DEVICE_INFO = 0x29;
	static constexpr u8 FRAMETYPE_PARAMETER_SETTINGS_ENTRY = 0x2B;
	static constexpr u8 FRAMETYPE_PARAMETER_READ = 0x2C;
	static constexpr u8 FRAMETYPE_PARAMETER_WRITE = 0x2D;
	// static constexpr u8 FRAMETYPE_ELRS_STATUS = 0x2E; // only on transmitter side
	static constexpr u8 FRAMETYPE_COMMAND = 0x32;
	// static constexpr u8 FRAMETYPE_RADIO_ID = 0x3A; // only on transmitter side
	// static constexpr u8 FRAMETYPE_KISS_REQ = 0x78; // non-standard
	// static constexpr u8 FRAMETYPE_KISS_RESP = 0x79; // non-standard
	static constexpr u8 FRAMETYPE_MSP_REQ = 0x7A;
	static constexpr u8 FRAMETYPE_MSP_RESP = 0x7B;
	static constexpr u8 FRAMETYPE_MSP_WRITE = 0x7C;
	static constexpr u8 FRAMETYPE_DISPLAYPORT_CMD = 0x7D;
	// static constexpr u8 FRAMETYPE_ARDUPILOT_RESP = 0x80; // non-standard

	// possible addresses (extended messages)
	static constexpr u8 ADDRESS_FLIGHT_CONTROLLER = 0xC8;
	static constexpr u8 ADDRESS_RADIO_TRANSMITTER = 0xEA;
	static constexpr u8 ADDRESS_CRSF_RECEIVER = 0xEC;
	static constexpr u8 ADDRESS_CRSF_TRANSMITTER = 0xEE;

private:
	// constants needed for encoding/decoding packets
	static constexpr u16 powerStates[9] = {0, 10, 25, 100, 500, 1000, 2000, 250, 50};
	static constexpr u16 packetRates[20] = {
		4, 25, 50, 100, 100, 150, 200, 250, 333, 500, 250, 500, 500, 1000, 50, 200, 500, 1000, 1000, 1000};
	static constexpr u8 CRSF_SYNC_BYTE = 0xC8;

	// incoming packets
	enum {
		CRSF_STATE_SYNC,
		CRSF_STATE_LEN,
		CRSF_STATE_TYPE,
		CRSF_STATE_EXT_DEST,
		CRSF_STATE_EXT_SRC,
		CRSF_STATE_PAYLOAD,
		CRSF_STATE_CRC
	};
	u8 inMsgState = CRSF_STATE_SYNC;
	u8 inPayloadIndex = 0;
	u8 inType = 0;
	u8 inMsgLen = 0;
	u8 inActualLen = 0;
	u8 inExtDest = 0;
	u8 inExtSrc = 0;
	u32 inCrc = 0;
	bool inMsgIsExtended = false;
	u8 inPayload[60]; // only actual payload, not ext src/dest
	void processMessage();
	bool parseChar(u8 c); // returns true if a packet is done

	// stick smoothing
	static interp_config interpConfig0; // used to interpolate for smooth sticks
	static interp_config interpConfig1; // used to interpolate for smooth sticks
	static interp_config interpConfig2; // used to clamp smoothed values

	// telemetry and link maintenance
	u32 currentTelemSensor = 0;
	const u8 serialNum;
	elapsedMicros frequencyTimer;
	elapsedMicros telemetryTimer;
	elapsedMicros heartbeatTimer;
	u32 rcPacketRateCounter = 0;
	u32 packetRateCounter = 0;
	bool pinged = false;

	// MSP packets (in/out)
	u8 mspExtSrcAddr = 0;
	u8 mspRxPayload[512] = {0};
	u8 mspTelemSeq = 0;
	u16 mspRxPayloadLen = 0;
	u16 mspRxCmd = 0;
	u8 mspRxSeq = 0;
	u16 mspRxPos = 0;
	bool mspRecording = false;
	u8 mspRxFlag = 0;
	MspVersion mspVersion = MspVersion::V2_OVER_CRSF;
	void resetMsp(bool setError = false);
	void processMspReq();
};
