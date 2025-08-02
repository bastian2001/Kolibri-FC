#include "global.h"
#include "hardware/interp.h"

/* Prevent long execution time in loop() if there is a lot of data in the buffer.
 * - Lower values: more consistent execution time
 * - Higher values: more throughput (better for large MSP requests)
 * 30 works well enough, but if DMA is possible for receiving, the value can be decreased because the buffer will not fill up as quickly.
 */
#define ELRS_MAX_SCAN 30
#define ELRS_BUFFER_SIZE 600 // need to accept large bursts of MSP data

RingBuffer<u8> elrsBuffer(ELRS_BUFFER_SIZE);
interp_config ExpressLRS::interpConfig0, ExpressLRS::interpConfig1, ExpressLRS::interpConfig2;

ExpressLRS::ExpressLRS(SerialUART &elrsSerial, u32 baudrate, u8 pinTX, u8 pinRX)
	: elrsSerial(elrsSerial),
	  pinTX(pinTX),
	  pinRX(pinRX),
	  baudrate(baudrate) {
	elrsStream = &elrsSerial;
	for (int i = 0; i < 3; i++) {
		if (elrsStream == serials[i].stream) {
			serialNum = i;
			break;
		}
	}
	elrsSerial.end();
	elrsSerial.setTX(pinTX);
	elrsSerial.setRX(pinRX);
	elrsSerial.setCTS(UART_PIN_NOT_DEFINED);
	elrsSerial.setRTS(UART_PIN_NOT_DEFINED);
	elrsSerial.setFIFOSize(256);
	elrsSerial.begin(baudrate, SERIAL_8N1);
	interpConfig0 = interp_default_config();
	interp_config_set_blend(&interpConfig0, 1);
	interpConfig1 = interp_default_config();
	interpConfig2 = interp_default_config();
	interp_config_set_clamp(&interpConfig2, 1);
}

ExpressLRS::~ExpressLRS() {
	elrsSerial.end();
}

void ExpressLRS::loop() {
	elapsedMicros taskTimer;
	if (frequencyTimer >= 1000) {
		if (rcPacketRateCounter > 20 && rcMsgCount > 300)
			isLinkUp = true;
		else
			isLinkUp = false;
		if (msgCount) isReceiverUp = true;
		actualPacketRate = rcPacketRateCounter;
		packetRateCounter = 0;
		rcPacketRateCounter = 0;
		frequencyTimer = 0;
		if (!pinged) {
			// somehow ADDRESS_BROADCAST gets no device info in return so using CRSF_RECEIVER here
			this->sendExtPacket(FRAMETYPE_DEVICE_PING, ADDRESS_CRSF_RECEIVER, ADDRESS_FLIGHT_CONTROLLER, nullptr, 0);
		}
	}
	int maxScan = elrsBuffer.itemCount();
	if (maxScan > ELRS_BUFFER_SIZE - ELRS_MAX_SCAN) {
		elrsBuffer.clear();
		msgBufIndex = 0;
		crc = 0;
		lastError = ERROR_BUFFER_OVERFLOW;
		tasks[TASK_ELRS].errorCount++;
		tasks[TASK_ELRS].lastError = ERROR_BUFFER_OVERFLOW;
		errorFlag = true;
		errorCount++;
		return;
	}
	if ((!maxScan && heartbeatTimer >= 300) || heartbeatTimer >= 500) {
		// try to send every 300 ms, and force send after reaching 500ms
		u8 buf[2] = {0, ADDRESS_FLIGHT_CONTROLLER}; // u16 address, big endian
		this->sendPacket(FRAMETYPE_HEARTBEAT, (char *)buf, 2);
		heartbeatTimer = 0;
	} else if (!maxScan && telemetryTimer >= 15) {
		// if there is no data to read and the last sensor was transmitted more than 15ms ago, send one telemetry sensor at a time
		telemetryTimer = 0;
		switch (currentTelemSensor++) {
		case 0: {
			// GPS (0x02)
			telemBuffer[0] = gpsMotion.lat >> 24;
			telemBuffer[1] = gpsMotion.lat >> 16;
			telemBuffer[2] = gpsMotion.lat >> 8;
			telemBuffer[3] = gpsMotion.lat;
			telemBuffer[4] = gpsMotion.lon >> 24;
			telemBuffer[5] = gpsMotion.lon >> 16;
			telemBuffer[6] = gpsMotion.lon >> 8;
			telemBuffer[7] = gpsMotion.lon;
			u16 data = gpsMotion.gSpeed * 10 / 278; // mm/s to km/h / 10
			telemBuffer[8] = data >> 8;
			telemBuffer[9] = data;
			data = gpsMotion.headMot / 1000; // 10^-5deg to 10^-2deg
			telemBuffer[10] = data >> 8;
			telemBuffer[11] = data;
			data = (gpsMotion.alt + 500) / 1000 + 1000; // mm to m + 1000
			telemBuffer[12] = data >> 8;
			telemBuffer[13] = data;
			telemBuffer[14] = gpsStatus.satCount;
			this->sendPacket(FRAMETYPE_GPS, (char *)telemBuffer, 15);
		} break;
		case 1: {
			// Vario (0x07)
			i16 data = -gpsMotion.velD / 10; // mm/s to cm/s
			telemBuffer[0] = data >> 8;
			telemBuffer[1] = data;
			this->sendPacket(FRAMETYPE_VARIO, (char *)telemBuffer, 2);
		} break;
		case 2: {
			// Battery (0x08)
			i32 data = adcVoltage / 10; // cV to dV
			telemBuffer[0] = data >> 8;
			telemBuffer[1] = data;
			data = adcCurrent * 10; // A to dA
			telemBuffer[2] = data >> 8;
			telemBuffer[3] = data;
			telemBuffer[4] = 0;
			telemBuffer[5] = 0;
			telemBuffer[6] = 0;
			telemBuffer[7] = 0;
			this->sendPacket(FRAMETYPE_BATTERY, (char *)telemBuffer, 8);
		} break;
		case 3: {
			// Baro Altitude (0x09)
			i32 data = combinedAltitude.raw / 6554; // dm;
			data += 10000; // dm + 10000
			telemBuffer[0] = data >> 8;
			telemBuffer[1] = data;
			data = vVel.raw / 655; // cm/s
			telemBuffer[2] = data >> 8;
			telemBuffer[3] = data;
			this->sendPacket(FRAMETYPE_BARO_ALT, (char *)telemBuffer, 4);
		} break;
		case 4: {
			// Attitude (0x1E)
			i16 data = (pitch * 10000).geti32(); // 10^-5 rad;
			telemBuffer[0] = data >> 8;
			telemBuffer[1] = data;
			data = (roll * 10000).geti32(); // 10^-5 rad;
			telemBuffer[2] = data >> 8;
			telemBuffer[3] = data;
			data = (yaw * 10000).geti32(); // 10^-5 rad;
			telemBuffer[4] = data >> 8;
			telemBuffer[5] = data;
		} break;
		case 5:
			// Flight Mode (0x21)
			switch (flightMode) {
			case FlightMode::ACRO:
				this->sendPacket(FRAMETYPE_FLIGHTMODE, "Acro", 5);
				break;
			case FlightMode::ANGLE:
				this->sendPacket(FRAMETYPE_FLIGHTMODE, "Angle", 6);
				break;
			case FlightMode::ALT_HOLD:
				this->sendPacket(FRAMETYPE_FLIGHTMODE, "Altitude Hold", 14);
				break;
			case FlightMode::GPS_VEL:
				this->sendPacket(FRAMETYPE_FLIGHTMODE, "GPS Velocity", 13);
				break;
			case FlightMode::GPS_POS:
				this->sendPacket(FRAMETYPE_FLIGHTMODE, "GPS Position", 13);
				break;
			}
			break;
		}
		if (currentTelemSensor > 5) currentTelemSensor = 0;
		return;
	}
	taskTimer = 0;
	if (maxScan > ELRS_MAX_SCAN) maxScan = ELRS_MAX_SCAN; // limit to max scan size
	if (maxScan + msgBufIndex > 64) maxScan = 64 - msgBufIndex; // limit to packet length
	for (int i = 0; i < maxScan; i++) {
		msgBuffer[msgBufIndex] = elrsBuffer.pop();
		if (msgBufIndex >= 2) {
			crc ^= msgBuffer[msgBufIndex];
			crc = crcLutD5[crc];
		} else {
			crc = 0;
		}
		msgBufIndex++;
		if (msgBufIndex >= 2 + msgBuffer[1]) break; // if the message is complete, stop scanning, so we don't corrupt the crc
	}
	if (msgBufIndex > 0 && msgBuffer[0] != CRSF_SYNC_BYTE) {
		msgBufIndex = 0;
		crc = 0;
		lastError = ERROR_INVALID_PREFIX;
		tasks[TASK_ELRS].errorCount++;
		tasks[TASK_ELRS].lastError = ERROR_INVALID_PREFIX;
		errorFlag = true;
		errorCount++;
	}
	if (msgBufIndex >= 2 + msgBuffer[1]) {
		processMessage();
	}
	u32 duration = taskTimer;
	tasks[TASK_ELRS].totalDuration += duration;
	if (duration < tasks[TASK_ELRS].minDuration) {
		tasks[TASK_ELRS].minDuration = duration;
	}
	if (duration > tasks[TASK_ELRS].maxDuration) {
		tasks[TASK_ELRS].maxDuration = duration;
	}
}

void ExpressLRS::processMessage() {
	int size = msgBuffer[1] + 2; // total CRSF packet length, including everything
	if (crc & 0xFF) // if the crc is not 0, then the message is invalid
	{
		crc = 0;
		msgBufIndex -= size;
		// shift all the bytes in the buffer to the left by size
		for (int i = 0; i < msgBufIndex; i++)
			msgBuffer[i] = msgBuffer[i + size];
		lastError = ERROR_CRC;
		errorFlag = true;
		errorCount++;
		tasks[TASK_ELRS].errorCount++;
		tasks[TASK_ELRS].lastError = ERROR_CRC;
		return;
	}

	msgCount++;
	sinceLastMessage = 0;
	packetRateCounter++;
	tasks[TASK_ELRS].runCounter++;

	switch (msgBuffer[2]) {
	case FRAMETYPE_RC_CHANNELS_PACKED: {
		if (size != 26) // 16 channels * 11 bits + 3 bytes header + 1 byte crc
		{
			lastError = ERROR_INVALID_LENGTH;
			errorFlag = true;
			errorCount++;
			tasks[TASK_ELRS].errorCount++;
			tasks[TASK_ELRS].lastError = ERROR_INVALID_LENGTH;
			break;
		}
		crsf_channels_11 *chs = (crsf_channels_11 *)(&msgBuffer[3]);
		u32 pChannels[16] = {chs->ch0, chs->ch1, chs->ch2, chs->ch3, chs->ch4, chs->ch5, chs->ch6, chs->ch7, chs->ch8, chs->ch9, chs->ch10, chs->ch11, chs->ch12, chs->ch13, chs->ch14, chs->ch15};
		// map pChannels to 989-2012 (?)
		for (u8 i = 0; i < 16; i++) {
			pChannels[i] = 1500 + (1023 * ((i32)pChannels[i] - 992) / 1636);
			pChannels[i] = constrain(pChannels[i], 988, 2012);
		}

		// update as fast as possible
		fix32 smooth[4];
		getSmoothChannels(smooth);
		u32 smooth2[4];
		for (int i = 0; i < 4; i++) {
			smooth2[i] = smooth[i].geti32();
		}
		memcpy(lastChannels, smooth2, 4 * sizeof(u32));
		memcpy(&lastChannels[4], &channels[4], 12 * sizeof(u32));
		sinceLastRCMessage = 0;
		memcpy(channels, pChannels, 16 * sizeof(u32));
		newPacketFlag = 0xFFFFFFFF;
		rcPacketRateCounter++;
		rcMsgCount++;
	} break;
	case FRAMETYPE_SUBSET_RC_CHANNELS_PACKED: {
		u8 cfg = msgBuffer[3];
		u8 firstChannel = cfg & 0x1F;
		u8 res = (cfg >> 5) & 0x03;
		u8 channelCount = (size - 5) * 8 / (res + 10);
		if (firstChannel + channelCount > 16) {
			lastError = ERROR_INVALID_LENGTH;
			errorFlag = true;
			errorCount++;
			tasks[TASK_ELRS].errorCount++;
			tasks[TASK_ELRS].lastError = ERROR_INVALID_LENGTH;
			break;
		}
		u32 pChannels[16] = {0};
		switch (res) {
		case 0b00: {
			// 10 bits
			crsf_channels_10 chs = {0};
			memcpy(&chs, &msgBuffer[4], size - 5);
			pChannels[0] = chs.ch0;
			pChannels[1] = chs.ch1;
			pChannels[2] = chs.ch2;
			pChannels[3] = chs.ch3;
			pChannels[4] = chs.ch4;
			pChannels[5] = chs.ch5;
			pChannels[6] = chs.ch6;
			pChannels[7] = chs.ch7;
			pChannels[8] = chs.ch8;
			pChannels[9] = chs.ch9;
			pChannels[10] = chs.ch10;
			pChannels[11] = chs.ch11;
			pChannels[12] = chs.ch12;
			pChannels[13] = chs.ch13;
			pChannels[14] = chs.ch14;
			pChannels[15] = chs.ch15;
		} break;
		case 0b01: {
			// 11 bits
			crsf_channels_11 chs = {0};
			memcpy(&chs, &msgBuffer[4], size - 5);
			pChannels[0] = chs.ch0;
			pChannels[1] = chs.ch1;
			pChannels[2] = chs.ch2;
			pChannels[3] = chs.ch3;
			pChannels[4] = chs.ch4;
			pChannels[5] = chs.ch5;
			pChannels[6] = chs.ch6;
			pChannels[7] = chs.ch7;
			pChannels[8] = chs.ch8;
			pChannels[9] = chs.ch9;
			pChannels[10] = chs.ch10;
			pChannels[11] = chs.ch11;
			pChannels[12] = chs.ch12;
			pChannels[13] = chs.ch13;
			pChannels[14] = chs.ch14;
			pChannels[15] = chs.ch15;
		} break;
		case 0b10: {
			// 12 bits
			crsf_channels_12 chs = {0};
			memcpy(&chs, &msgBuffer[4], size - 5);
			pChannels[0] = chs.ch0;
			pChannels[1] = chs.ch1;
			pChannels[2] = chs.ch2;
			pChannels[3] = chs.ch3;
			pChannels[4] = chs.ch4;
			pChannels[5] = chs.ch5;
			pChannels[6] = chs.ch6;
			pChannels[7] = chs.ch7;
			pChannels[8] = chs.ch8;
			pChannels[9] = chs.ch9;
			pChannels[10] = chs.ch10;
			pChannels[11] = chs.ch11;
			pChannels[12] = chs.ch12;
			pChannels[13] = chs.ch13;
			pChannels[14] = chs.ch14;
			pChannels[15] = chs.ch15;
		} break;
		case 0b11: {
			// 13 bits
			crsf_channels_13 chs = {0};
			memcpy(&chs, &msgBuffer[4], size - 5);
			pChannels[0] = chs.ch0;
			pChannels[1] = chs.ch1;
			pChannels[2] = chs.ch2;
			pChannels[3] = chs.ch3;
			pChannels[4] = chs.ch4;
			pChannels[5] = chs.ch5;
			pChannels[6] = chs.ch6;
			pChannels[7] = chs.ch7;
			pChannels[8] = chs.ch8;
			pChannels[9] = chs.ch9;
			pChannels[10] = chs.ch10;
			pChannels[11] = chs.ch11;
			pChannels[12] = chs.ch12;
			pChannels[13] = chs.ch13;
			pChannels[14] = chs.ch14;
			pChannels[15] = chs.ch15;
		} break;
		default:
			return;
		}
		// shift pChannels by firstChannel
		for (u32 i = channelCount - 1; i; i--) {
			pChannels[i + firstChannel] = pChannels[i];
		}
		for (u32 i = 0; i < firstChannel; i++) {
			pChannels[i] = this->channels[i];
		}
		for (u32 i = firstChannel + channelCount; i < 16; i++) {
			pChannels[i] = this->channels[i];
		}
		// map pChannels to 988-2012 (?)
		for (u8 i = 0; i < 16; i++) {
			pChannels[i] = 1500 + (1023 * ((i32)pChannels[i] - 992) / 1636);
			pChannels[i] = constrain(pChannels[i], 988, 2012);
		}

		// update as fast as possible
		fix32 smooth[4];
		getSmoothChannels(smooth);
		u32 smooth2[4];
		for (int i = 0; i < 4; i++) {
			smooth2[i] = smooth[i].geti32();
		}
		memcpy(lastChannels, smooth2, 4 * sizeof(u32));
		memcpy(&lastChannels[4], &channels[4], 12 * sizeof(u32));
		sinceLastRCMessage = 0;
		memcpy(channels, pChannels, 16 * sizeof(u32));
		newPacketFlag = 0xFFFFFFFF;
		rcPacketRateCounter++;
		rcMsgCount++;
	} break;
	case FRAMETYPE_LINK_STATISTICS: {
		if (size != 14) // 10 info bytes + 3 bytes header + 1 byte crc
		{
			lastError = ERROR_INVALID_LENGTH;
			errorFlag = true;
			errorCount++;
			tasks[TASK_ELRS].errorCount++;
			tasks[TASK_ELRS].lastError = ERROR_INVALID_LENGTH;
			break;
		}
		uplinkRssi[0] = -msgBuffer[3];
		uplinkRssi[1] = -msgBuffer[4];
		uplinkLinkQuality = msgBuffer[5];
		uplinkSNR = msgBuffer[6];
		antennaSelection = msgBuffer[7];
		packetRateIdx = msgBuffer[8];
		targetPacketRate = packetRates[packetRateIdx];
		txPower = powerStates[msgBuffer[9]];
		downlinkRssi = -msgBuffer[10];
		downlinkLinkQuality = msgBuffer[11];
		downlinkSNR = msgBuffer[12];
	} break;
	case FRAMETYPE_DEVICE_PING: {
		char buf[32] = FIRMWARE_NAME " " FIRMWARE_VERSION_STRING;
		u8 pos = strlen(buf) + 1;
		memcpy(&buf[pos], rp2040.getChipID(), 4);
		pos += 4;
		buf[pos++] = 0; // hardware version
		buf[pos++] = 0; // hardware version
		buf[pos++] = 0; // hardware version
		buf[pos++] = 0; // hardware version
		buf[pos++] = 0; // software version
		buf[pos++] = FIRMWARE_VERSION_MAJOR;
		buf[pos++] = FIRMWARE_VERSION_MINOR;
		buf[pos++] = FIRMWARE_VERSION_PATCH;
		buf[pos++] = 0; // config parameter count
		buf[pos++] = 0; // parameter protocol version (0)
		this->sendExtPacket(FRAMETYPE_DEVICE_INFO, msgBuffer[4], ADDRESS_FLIGHT_CONTROLLER, buf, pos);
	} break;
	case FRAMETYPE_DEVICE_INFO: {
		pinged = true;
	} break;
	case FRAMETYPE_PARAMETER_SETTINGS_ENTRY: {
		Serial.printf("FRAMETYPE_PARAMETER_SETTINGS_ENTRY with ext dest %02X, ext src %02X, and %d more bytes\n", msgBuffer[3], msgBuffer[4], size - 6);
		for (int i = 0; i < size - 6; i++) {
			Serial.printf("%02X ", msgBuffer[5 + i]);
		}
		Serial.println();
	} break;
	case FRAMETYPE_PARAMETER_READ: {
		Serial.printf("FRAMETYPE_PARAMETER_READ with ext dest %02X, ext src %02X, and %d more bytes\n", msgBuffer[3], msgBuffer[4], size - 6);
		for (int i = 0; i < size - 6; i++) {
			Serial.printf("%02X ", msgBuffer[5 + i]);
		}
		Serial.println();
	} break;
	case FRAMETYPE_PARAMETER_WRITE: {
		Serial.printf("FRAMETYPE_PARAMETER_WRITE with ext dest %02X, ext src %02X, and %d more bytes\n", msgBuffer[3], msgBuffer[4], size - 6);
		for (int i = 0; i < size - 6; i++) {
			Serial.printf("%02X ", msgBuffer[5 + i]);
		}
		Serial.println();
	} break;
	case FRAMETYPE_COMMAND: {
		Serial.printf("FRAMETYPE_COMMAND with ext dest %02X, ext src %02X, and %d more bytes\n", msgBuffer[3], msgBuffer[4], size - 6);
		for (int i = 0; i < size - 6; i++) {
			Serial.printf("%02X ", msgBuffer[5 + i]);
		}
		Serial.println();
	} break;
	case FRAMETYPE_MSP_REQ:
	case FRAMETYPE_MSP_WRITE: {
		this->processMspReq(size);
	} break;
	default:
		lastError = ERROR_UNSUPPORTED_COMMAND;
		tasks[TASK_ELRS].errorCount++;
		tasks[TASK_ELRS].lastError = ERROR_UNSUPPORTED_COMMAND;
		errorFlag = true;
		errorCount++;
		msgBufIndex -= size;
		crc = 0;
		// shift all the bytes in the buffer to the left by size
		for (int i = 0; i < msgBufIndex; i++)
			msgBuffer[i] = msgBuffer[i + size];
		return;
	}

	msgBufIndex = 0;
	crc = 0;
}

void ExpressLRS::getSmoothChannels(fix32 smoothChannels[4]) {
	// one new RC message every 4ms = 4000µs, ELRS 250Hz
	int sinceLast = sinceLastRCMessage;
	if (sinceLast > 8000) {
		sinceLast = 8000;
	}
	sinceLast = 255 * sinceLast / 8000;
	interp_set_config(interp0, 0, &interpConfig0);
	interp_set_config(interp0, 1, &interpConfig1);
	interp_set_config(interp1, 0, &interpConfig2);
	interp0->accum[1] = sinceLast;
	interp1->base[0] = 988 << 16;
	interp1->base[1] = 2012 << 16;
	for (int i = 0; i < 4; i++) {
		interp0->base[0] = lastChannels[i] << 16;
		interp0->base[1] = (channels[i] * 2 - lastChannels[i]) << 16;
		interp1->accum[0] = interp0->peek[1];
		smoothChannels[i].setRaw(interp1->peek[0]);
	}
}

void ExpressLRS::sendPacket(u8 cmd, const char *payload, u8 payloadLen) {
	if (payloadLen > 60 || (payload == nullptr && payloadLen)) return;
	u8 packet[64] = {CRSF_SYNC_BYTE, (u8)(payloadLen + 2), cmd};
	if (payloadLen)
		memcpy(&packet[3], payload, payloadLen);
	u32 crc = 0;
	for (int i = 2; i < 3 + payloadLen; i++) {
		crc ^= packet[i];
		crc = crcLutD5[crc];
	}
	packet[3 + payloadLen] = crc;
	elrsSerial.write(packet, 4 + payloadLen);
}

void ExpressLRS::sendExtPacket(u8 cmd, u8 destAddr, u8 srcAddr, const char *extPayload, u8 extPayloadLen) {
	if (extPayloadLen > 58 || (extPayload == nullptr && extPayloadLen)) return;
	u8 packet[64] = {CRSF_SYNC_BYTE, (u8)(extPayloadLen + 4), cmd, destAddr, srcAddr};
	if (extPayloadLen)
		memcpy(&packet[5], extPayload, extPayloadLen);
	u32 crc = 0;
	for (int i = 2; i < 5 + extPayloadLen; i++) {
		crc ^= packet[i];
		crc = crcLutD5[crc];
	}
	packet[5 + extPayloadLen] = crc;
	elrsSerial.write(packet, 6 + extPayloadLen);
}

void ExpressLRS::sendMspMsg(MspMsgType type, u8 mspVersion, const char *payload, u16 payloadLen) {
	if (payload == nullptr && payloadLen) return;
	u8 chunkCount = (payloadLen) / 57 + 1;
	u8 firstPacket = 0b10000;
	for (u8 chunk = 0; chunk < chunkCount; chunk++) {
		u8 chunkSize = 57;
		if (chunk == chunkCount - 1) {
			chunkSize = payloadLen - chunk * 57;
		}
		u8 stat = 0;
		stat |= this->mspTelemSeq;
		stat |= firstPacket;
		stat |= mspVersion << 5;
		stat |= (type == MspMsgType::ERROR) << 7;

		this->mspTelemSeq++;
		this->mspTelemSeq &= 0xF;
		u8 packet[60] = {this->mspExtSrcAddr, ADDRESS_FLIGHT_CONTROLLER, stat};
		firstPacket = 0;
		if (chunkSize)
			memcpy(&packet[3], &payload[chunk * 57], chunkSize);
		if (type != MspMsgType::REQUEST)
			sendPacket(FRAMETYPE_MSP_RESP, (char *)packet, chunkSize + 3);
		else
			sendPacket(FRAMETYPE_MSP_REQ, (char *)packet, chunkSize + 3);
	}
}

void ExpressLRS::resetMsp(bool setError) {
	this->mspRxPos = 0;
	this->mspRecording = false;
	this->mspRxPayloadLen = 0;
	this->mspVersion = MspVersion::V2_OVER_CRSF;
	this->mspExtSrcAddr = 0;
	this->mspRxCmd = 0;
	this->mspRxSeq = 0;
	if (setError) {
		this->errorCount++;
		this->errorFlag = true;
		this->lastError = ExpressLRS::ERROR_MSP_OVER_CRSF;
	}
}

void ExpressLRS::processMspReq(int size) {
	const u8 *const crsfPayload = &msgBuffer[3];
	const u8 *const extPayload = &crsfPayload[2];
	const u8 *const mspData = &extPayload[1];
	const i8 crsfPacketSize = size - 4; // regular payload size = extended payload size + 2 (ext src and dest)
	u8 minSize = 3; // ext src, dest and status byte
	if (crsfPacketSize < minSize) return;

	const u8 status = *extPayload; // first byte of payload is the status byte
	const u8 isError = (status >> 7);
	const bool isNewFrame = (status >> 4) & 0b1;
	const u8 sequenceNo = status & 0xF;
	u8 readPos = 0; // read position for MSP payload

	const u8 expectedSequence = (this->mspRxSeq + 1) & 0xF;
	const bool sequenceError = sequenceNo != expectedSequence;

	if (isError || (!isNewFrame && sequenceError)) return this->resetMsp(true);

	if (isNewFrame) {
		this->resetMsp(); // if a new frame comes in before the previous one is complete we need to reset the rxPos

		// set MSP version
		const u8 headerVersion = (status >> 5) & 0b11; // 1 = V1 or V1_JUMBO, 2 = V2
		switch (headerVersion) {
		case 1:
			if (extPayload[1] == 0xFF)
				this->mspVersion = MspVersion::V1_JUMBO_OVER_CRSF;
			else
				this->mspVersion = MspVersion::V1_OVER_CRSF;
			break;
		case 2:
			this->mspVersion = MspVersion::V2_OVER_CRSF;
			break;
		default:
			Serial.println("unsupported MSP version: " + String(headerVersion));
			this->resetMsp(true);
			return;
		}

		// save return address for once packet is complete
		this->mspExtSrcAddr = msgBuffer[4];

		// calculate frame length
		switch (this->mspVersion) {
		case MspVersion::V1_OVER_CRSF:
			minSize += 2; // 1 payload length, 1 command
			readPos = 2;
			if (crsfPacketSize < minSize) {
				Serial.println("Too small V1");
				this->resetMsp(true);
				return; // broken packet
			}
			this->mspRxPayloadLen = mspData[0];
			this->mspRxCmd = mspData[1];
			break;
		case MspVersion::V1_JUMBO_OVER_CRSF:
			minSize += 4; // 1+2 payload length, 1 command
			readPos = 4;
			if (crsfPacketSize < minSize) {
				Serial.println("Too small V1 Jumbo");
				this->resetMsp(true);
				return; // broken packet
			}
			this->mspRxCmd = mspData[1];
			this->mspRxPayloadLen = mspData[2] | (mspData[3] << 8);
			if (this->mspRxPayloadLen > 512) {
				Serial.println("Too large V1 Jumbo");
				this->resetMsp(true);
				return; // invalid payload length
			}
			break;
		case MspVersion::V2_OVER_CRSF:
			minSize += 5; // 1 flags, 2 payload length, 2 command
			readPos = 5;
			if (crsfPacketSize < minSize) {
				Serial.println("Too small V2");
				this->resetMsp(true);
				return;
			}
			this->mspRxFlag = mspData[0];
			this->mspRxCmd = mspData[1] | (mspData[2] << 8);
			this->mspRxPayloadLen = mspData[3] | (mspData[4] << 8);
			if (this->mspRxPayloadLen > 512) {
				Serial.println("Too large V2");
				this->resetMsp(true);
				return; // invalid payload length
			}
		}
		this->mspRecording = true;
	}
	if (!this->mspRecording) {
		Serial.println("MSP recording not started, but got a packet, probably packet loss");
		this->resetMsp(true);
		return;
	}
	this->mspRxSeq = sequenceNo; // save the sequence number for next frame

	i8 thisMspPayloadLen = size - 7 - readPos; // how much MSP payload is in this CRSF frame = total packet size minus sync, len, type, ext src, ext dest, status and CRSF CRC, then deduct start of MSP payload (readPos)

	// something is wrong
	if (thisMspPayloadLen < 0) return this->resetMsp(true);

	i16 mspNeedsPayloadBytes = this->mspRxPayloadLen - this->mspRxPos; // how many bytes we still need to read for the MSP payload
	i16 readFromThisPacket = MIN(thisMspPayloadLen, mspNeedsPayloadBytes); // how much MSP payload we can read from this packet

	// something is wrong
	if (readFromThisPacket < 0) return this->resetMsp(true);

	memcpy(this->mspRxPayload + this->mspRxPos, &mspData[readPos], readFromThisPacket);

	this->mspRxPos += readFromThisPacket; // update the position in the MSP payload

	// if we have a full packet
	if (this->mspRxPos >= this->mspRxPayloadLen) {
		processMspCmd(this->serialNum, MspMsgType::REQUEST, (MspFn)this->mspRxCmd, this->mspVersion, (char *)this->mspRxPayload, this->mspRxPayloadLen);
		this->resetMsp();
	}
}
