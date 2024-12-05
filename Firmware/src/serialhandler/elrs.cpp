#include "global.h"
#include "hardware/interp.h"
RingBuffer<u8> elrsBuffer(260);
interp_config ExpressLRS::interpConfig0, ExpressLRS::interpConfig1, ExpressLRS::interpConfig2;

ExpressLRS::ExpressLRS(SerialUART &elrsSerial, u32 baudrate, u8 pinTX, u8 pinRX)
	: elrsSerial(elrsSerial),
	  pinTX(pinTX),
	  pinRX(pinRX),
	  baudrate(baudrate) {
	elrsStream = &elrsSerial;
	for (int i = 0; i < 3; i++) {
		if (elrsStream == serials[i]) {
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
	if (maxScan > 250) {
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
	if (maxScan > 10) maxScan = 10;
	if (msgBufIndex > 54) maxScan = 64 - msgBufIndex;
	for (int i = 0; i < maxScan; i++) {
		msgBuffer[msgBufIndex] = elrsBuffer.pop();
		if (msgBufIndex >= 2) {
			crc ^= msgBuffer[msgBufIndex];
			crc = crcLutD5[crc];
		} else
			crc = 0;
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
	int size = msgBuffer[1] + 2;
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
		// crsf_channels_t *crsfChannels = (crsf_channels_t *)(&msgBuffer[3]); // somehow conversion through bit-fields does not work, so manual conversion
		u64 decoder, decoder2;
		memcpy(&decoder, &msgBuffer[3], 8);
		u32 pChannels[16];
		pChannels[0] = decoder & 0x7FF; // 0...10
		pChannels[1] = (decoder >> 11) & 0x7FF; // 11...21
		pChannels[2] = (decoder >> 22) & 0x7FF; // 22...32
		pChannels[3] = (decoder >> 33) & 0x7FF; // 33...43
		pChannels[4] = (decoder >> 44) & 0x7FF; // 44...54
		decoder >>= 55; // 55, 9 bits left
		memcpy(&decoder2, &msgBuffer[11], 6);
		decoder |= (decoder2 << 9); // 57 bits left
		pChannels[5] = decoder & 0x7FF; // 55...65
		pChannels[6] = (decoder >> 11) & 0x7FF; // 66...76
		pChannels[7] = (decoder >> 22) & 0x7FF; // 77...87
		pChannels[8] = (decoder >> 33) & 0x7FF; // 88...98
		pChannels[9] = (decoder >> 44) & 0x7FF; // 99...109
		decoder >>= 55; // 55, 2 bits left
		memcpy(&decoder2, &msgBuffer[17], 7);
		decoder |= (decoder2 << 2); // 58 bits left
		pChannels[10] = decoder & 0x7FF; // 110...120
		pChannels[11] = (decoder >> 11) & 0x7FF; // 121...131
		pChannels[12] = (decoder >> 22) & 0x7FF; // 132...142
		pChannels[13] = (decoder >> 33) & 0x7FF; // 143...153
		pChannels[14] = (decoder >> 44) & 0x7FF; // 154...164
		decoder >>= 55; // 55, 3 bits left
		pChannels[15] = decoder | (msgBuffer[24] << 3);
		// map pChannels (switches) to 1000-2000 and joysticks to 988-2011
		for (u8 i = 0; i < 16; i++) {
			if (i == 2)
				continue;
			pChannels[i] -= 174;
			pChannels[i] *= 1024;
			pChannels[i] /= 1636;
			pChannels[i] += 988;
			pChannels[i] = constrain(pChannels[i], 988, 2012);
		}
		// map pChannels (throttle) to 1000-2000
		pChannels[2] -= 174;
		pChannels[2] *= 1000;
		pChannels[2] /= 1636;
		pChannels[2] += 1000; // keep radio commands within 1000-2000
		pChannels[2] = constrain(pChannels[2], 1000, 2000);

		newPacketFlag = 0xFFFFFFFF;
		if (pChannels[4] > 1500)
			consecutiveArmedCycles++;
		else
			consecutiveArmedCycles = 0;

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
		rcPacketRateCounter++;
		rcMsgCount++;
		break;
	}
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
		break;
	}
	case FRAMETYPE_DEVICE_PING: {
		char buf[32];
		strncpy(buf, "Kolibri FC", 16);
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
	case FRAMETYPE_PARAMETER_READ:
	} break;
		Serial.printf("FRAMETYPE_PARAMETER_READ with ext dest %02X, ext src %02X, and %d more bytes\n", msgBuffer[3], msgBuffer[4], size - 6);
		for (int i = 0; i < size - 6; i++) {
			Serial.printf("%02X ", msgBuffer[5 + i]);
		}
		Serial.println();
		break;
	case FRAMETYPE_PARAMETER_WRITE:
		Serial.printf("FRAMETYPE_PARAMETER_WRITE with ext dest %02X, ext src %02X, and %d more bytes\n", msgBuffer[3], msgBuffer[4], size - 6);
		for (int i = 0; i < size - 6; i++) {
			Serial.printf("%02X ", msgBuffer[5 + i]);
		}
		Serial.println();
		break;
	case FRAMETYPE_COMMAND:
		Serial.printf("FRAMETYPE_COMMAND with ext dest %02X, ext src %02X, and %d more bytes\n", msgBuffer[3], msgBuffer[4], size - 6);
		for (int i = 0; i < size - 6; i++) {
			Serial.printf("%02X ", msgBuffer[5 + i]);
		}
		Serial.println();
		break;
	case FRAMETYPE_MSP_REQ:
	case FRAMETYPE_MSP_WRITE: {
		char *extPayload = (char *)&msgBuffer[5];
		i8 packetSize = size - 4;
		u8 minSize = 2;
		if (packetSize < minSize) break;
		char &stat = *extPayload++;
		this->lastExtSrcAddr = msgBuffer[4];
		if (stat & 0x10) {
			// new MSP request
			this->mspRxSeq = (stat + 1) & 0x0F;
			this->mspRxPos = 0;
			this->mspVersion = (stat >> 5) == 1 ? MspVersion::V1_OVER_CRSF : MspVersion::V2_OVER_CRSF;
			if (this->mspVersion == MspVersion::V1_OVER_CRSF) {
				minSize = 3;
				if (packetSize < minSize) break;
				this->mspRxPayloadLen = *extPayload++;
				if (this->mspRxPayloadLen == 255) {
					minSize += 2;
					if (packetSize < minSize) break;
					this->mspRxPayloadLen = *extPayload++;
					this->mspRxPayloadLen |= (*extPayload++) << 8;
					if (this->mspRxPayloadLen > 512) {
						this->mspRxPayloadLen = 0;
						break;
					}
					this->mspVersion = MspVersion::V1_JUMBO_OVER_CRSF;
				}
				this->mspRxCmd = *extPayload++;
				if (this->mspRxCmd == (u8)MspFn::MSP_V2_FRAME) {
					minSize += 5;
					if (packetSize < minSize) break;
					this->mspRxPayloadLen = *extPayload++;
					this->mspRxPayloadLen |= (*extPayload++) << 8;
					if (this->mspRxPayloadLen > 512) {
						this->mspRxPayloadLen = 0;
						break;
					}
					if (this->mspVersion == MspVersion::V1_JUMBO_OVER_CRSF)
						this->mspVersion = MspVersion::V2_OVER_V1_JUMBO_OVER_CRSF;
					else
						this->mspVersion = MspVersion::V2_OVER_V1_OVER_CRSF;
				}
				memcpy(this->mspRxPayload, extPayload, packetSize - minSize);
				this->mspRxPos += packetSize;
				this->mspRecording = true;
			} else {
				minSize = 6;
				if (packetSize < minSize) break;
				this->mspRxFlag = *extPayload++;
				this->mspRxCmd = *extPayload++;
				this->mspRxCmd |= (*extPayload++) << 8;
				this->mspRxPayloadLen = *extPayload++;
				this->mspRxPayloadLen |= (*extPayload++) << 8;
				if (this->mspRxPayloadLen > 512) {
					this->mspRxPayloadLen = 0;
					break;
				}
				memcpy(this->mspRxPayload, extPayload, packetSize - minSize);
				this->mspRxPos += packetSize;
				this->mspRecording = true;
			}
		} else if (mspRecording) {
		}
		if (this->mspRxPos >= this->mspRxPayloadLen) {
			processMspCmd(this->serialNum, MspMsgType::REQUEST, (MspFn)this->mspRxCmd, this->mspVersion, (char *)this->mspRxPayload, this->mspRxPayloadLen);
			this->mspRecording = false;
		}
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
	interp1->base[0] = 1000 << 16;
	interp1->base[1] = 2000 << 16;
	interp1->accum[0] = smoothChannels[2].raw;
	smoothChannels[2].setRaw(interp1->peek[0]);
}

void ExpressLRS::sendPacket(u8 cmd, const char *payload, u8 payloadLen) {
	if (payloadLen > 60) return;
	u8 packet[64] = {CRSF_SYNC_BYTE, (u8)(payloadLen + 2), cmd};
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
	if (extPayloadLen > 58) return;
	u8 packet[64] = {CRSF_SYNC_BYTE, (u8)(extPayloadLen + 4), cmd, destAddr, srcAddr};
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
	u8 chunkCount = (payloadLen) / 57 + 1;
	u8 firstPacket = 1;
	for (u8 chunk = 0; chunk < chunkCount; chunk++) {
		u8 chunkSize = 57;
		if (chunk == chunkCount - 1) {
			chunkSize = payloadLen - chunk * 57;
		}
		++mspTelemSeq &= 0xF;
		u8 stat = 0;
		stat |= this->mspTelemSeq;
		stat |= (firstPacket << 4);
		stat |= mspVersion << 5;
		stat |= (type == MspMsgType::ERROR) << 7;
		u8 packet[60] = {this->lastExtSrcAddr, ADDRESS_FLIGHT_CONTROLLER, stat};
		firstPacket = 0;
		memcpy(&packet[3], &payload[chunk * 57], chunkSize);
		if (type != MspMsgType::REQUEST)
			sendPacket(FRAMETYPE_MSP_RESP, (char *)packet, chunkSize + 3);
		else
			sendPacket(FRAMETYPE_MSP_REQ, (char *)packet, chunkSize + 3);
	}
}