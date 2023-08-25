#include "global.h"

ExpressLRS::ExpressLRS(SerialUART &elrsSerial, uint32_t baudrate, uint8_t pinTX, uint8_t pinRX)
	: elrsSerial(elrsSerial),
	  pinTX(pinTX),
	  pinRX(pinRX),
	  baudrate(baudrate)
{
	elrsSerial.end();
	elrsSerial.setTX(pinTX);
	elrsSerial.setRX(pinRX);
	elrsSerial.setCTS(UART_PIN_NOT_DEFINED);
	elrsSerial.setRTS(UART_PIN_NOT_DEFINED);
	elrsSerial.begin(baudrate, SERIAL_8N1);
}

ExpressLRS::~ExpressLRS()
{
	elrsSerial.end();
}

void ExpressLRS::loop()
{
	if (elrsSerial.available() > 28) // 32 byte UART buffer, 4 bytes for overhead
	{
		elrsSerial.flush();
		msgBufIndex = 0;
		lastError = ERROR_BUFFER_OVERFLOW;
		errorFlag = true;
		errorCount++;
	}
	while (elrsSerial.available())
	{
		msgBuffer[msgBufIndex++] = elrsSerial.read();
		if (msgBufIndex > 64)
			break;
	}
	if (msgBufIndex > 0 && msgBuffer[0] != RX_PREFIX)
	{
		msgBufIndex = 0;
		lastError = ERROR_INVALID_PREFIX;
		errorFlag = true;
		errorCount++;
	}
	if (msgBufIndex >= 2 + msgBuffer[1] && msgBuffer[0] == RX_PREFIX)
		processMessage();
	if (sinceLastRCMessage > 500000)
	{ // 500ms
		armed = false;
	}
}

// from https://github.com/catphish/openuav/blob/master/firmware/src/elrs.c
//  Append a byte to a CRC-8
void crc8_append(uint8_t data, uint8_t &crc)
{
	crc ^= data;
	for (uint8_t i = 0; i < 8; i++)
	{
		if (crc & 0x80)
		{
			crc = (crc << 1) ^ 0xD5;
		}
		else
		{
			crc <<= 1;
		}
	}
}

uint8_t counter = 0;
void ExpressLRS::processMessage()
{
	int size = msgBuffer[1] + 2;
	uint8_t crc = 0;
	for (int i = 2; i < size; i++)
		crc8_append(msgBuffer[i], crc);
	if (crc) // if the crc is not 0, then the message is invalid
	{
		msgBufIndex -= size;
		// shift all the bytes in the buffer to the left by size
		for (int i = 0; i < msgBufIndex; i++)
			msgBuffer[i] = msgBuffer[i + size];
		lastError = ERROR_CRC;
		errorFlag = true;
		errorCount++;
		return;
	}

	msgCount++;
	sinceLastMessage = 0;

	switch (msgBuffer[2])
	{
	case LINK_STATISTICS:
	{
		if (size != 14) // 10 info bytes + 3 bytes header + 1 byte crc
		{
			lastError = ERROR_INVALID_LENGTH;
			errorFlag = true;
			errorCount++;
			break;
		}
		uplinkRssi[0] = -msgBuffer[3];
		uplinkRssi[1] = -msgBuffer[4];
		uplinkLinkQuality = msgBuffer[5];
		uplinkSNR = msgBuffer[6];
		antennaSelection = msgBuffer[7];
		packetRate = msgBuffer[8];
		txPower = powerStates[msgBuffer[9]];
		downlinkRssi = -msgBuffer[10];
		downlinkLinkQuality = msgBuffer[11];
		downlinkSNR = msgBuffer[12];
		break;
	}
	case RC_CHANNELS_PACKED:
	{
		if (size != 26) // 16 channels * 11 bits + 3 bytes header + 1 byte crc
		{
			lastError = ERROR_INVALID_LENGTH;
			errorFlag = true;
			errorCount++;
			break;
		}
		sinceLastRCMessage = 0;
		// crsf_channels_t *crsfChannels = (crsf_channels_t *)(&msgBuffer[3]); // somehow conversion through bit-fields does not work, so manual conversion
		uint32_t decoder = msgBuffer[3] | (msgBuffer[4] << 8) | (msgBuffer[5] << 16) | (msgBuffer[6] << 24);
		uint16_t pChannels[16];
		pChannels[0] = (decoder >> 0) & 0x7FF;	// 0...10
		pChannels[1] = (decoder >> 11) & 0x7FF; // 11...21
		decoder >>= 16;							// 16
		decoder |= (msgBuffer[7] << 16);
		decoder |= (msgBuffer[8] << 24);
		pChannels[2] = (decoder >> 6) & 0x7FF;	// 22...32
		pChannels[3] = (decoder >> 17) & 0x7FF; // 33...43
		decoder >>= 24;							// 40
		decoder |= (msgBuffer[9] << 8);
		decoder |= (msgBuffer[10] << 16);
		decoder |= (msgBuffer[11] << 24);
		pChannels[4] = (decoder >> 4) & 0x7FF;	// 44...54
		pChannels[5] = (decoder >> 15) & 0x7FF; // 55...65
		decoder >>= 16;							// 56
		decoder |= (msgBuffer[12] << 16);
		decoder |= (msgBuffer[13] << 24);
		pChannels[6] = (decoder >> 10) & 0x7FF; // 66...76
		pChannels[7] = (decoder >> 21) & 0x7FF; // 77...87
		decoder = 0;							// 88, aka shift right by 32 bits
		decoder |= (msgBuffer[14] << 0);
		decoder |= (msgBuffer[15] << 8);
		decoder |= (msgBuffer[16] << 16);
		decoder |= (msgBuffer[17] << 24);
		pChannels[8] = (decoder >> 0) & 0x7FF;	// 88...98
		pChannels[9] = (decoder >> 11) & 0x7FF; // 99...109
		decoder >>= 16;							// 104
		decoder |= (msgBuffer[18] << 16);
		decoder |= (msgBuffer[19] << 24);
		pChannels[10] = (decoder >> 6) & 0x7FF;	 // 110...120
		pChannels[11] = (decoder >> 17) & 0x7FF; // 121...131
		decoder >>= 24;							 // 128
		decoder |= (msgBuffer[20] << 8);
		decoder |= (msgBuffer[21] << 16);
		decoder |= (msgBuffer[22] << 24);
		pChannels[12] = (decoder >> 4) & 0x7FF;	 // 132...142
		pChannels[13] = (decoder >> 15) & 0x7FF; // 143...153
		decoder >>= 16;							 // 144
		decoder |= (msgBuffer[23] << 16);
		decoder |= (msgBuffer[24] << 24);
		pChannels[14] = (decoder >> 10) & 0x7FF; // 154...164
		pChannels[15] = (decoder >> 21) & 0x7FF; // 165...175

		// map pChannels (switches) to 1000-2000 and joysticks to 988-2011
		for (uint8_t i = 0; i < 16; i++)
		{
			if (i == 2)
				continue;
			pChannels[i] -= 174;
			pChannels[i] *= 0.626f; //(2012-988)/(1811-174), scaled so that center = 1500
			pChannels[i] += 988;
			pChannels[i] = constrain(pChannels[i], 988, 2012);
		}
		// map pChannels (throttle) to 1000-2000
		pChannels[2] -= 174;
		pChannels[2] *= 0.610874f; //(2000-1000)/(1811-174)
		pChannels[2] += 1000;	   // keep radio commands within 1000-2000
		pChannels[2] = constrain(pChannels[2], 1000, 2000);

		// check arming
		// arming switch and already armed, or arming switch and throttle down (and not armed on boot)
		if (pChannels[4] > 1500 && ((channels[4] < 1500 && channels[4] > 0 && pChannels[2] < 1020) || armed))
		{
			if (!armed)
			{
				startLogging();
			}
			armed = true;
		}
		else if (pChannels[4] > 1500 && channels[4] < 1500)
		{
			Serial.println(pChannels[2]);
			armed = false;
			makeSound(2500, 599, 70, 50);
		}
		else
			armed = false;
		if (pChannels[4] < 1500 && channels[4] > 1500)
		{
			// disarming
			endLogging();
		}

		// update as fast as possible
		for (uint8_t i = 0; i < 16; i++)
		{
			this->lastChannels[i] = this->channels[i];
			this->channels[i] = pChannels[i];
		}
		break;
	}
	case DEVICE_PING:
		break;
	case DEVICE_INFO:
		break;
	case PARAMETER_SETTINGS_ENTRY:
		break;
	case PARAMETER_READ:
		break;
	case PARAMETER_WRITE:
		break;
	case COMMAND:
		break;
	case FRAMETYPE_WHATEVER:
		// Seems like this is not an error, but idk what it is
		break;
	default:
		lastError = ERROR_UNSUPPORTED_COMMAND;
		errorFlag = true;
		errorCount++;
		msgBufIndex -= size;
		// shift all the bytes in the buffer to the left by size
		for (int i = 0; i < msgBufIndex; i++)
			msgBuffer[i] = msgBuffer[i + size];
		return;
	}

	msgBufIndex = 0;
}

void ExpressLRS::getSmoothChannels(uint16_t smoothChannels[4])
{
	// one new RC message every 4ms = 4000µs, ELRS 250Hz
	static uint32_t sum[4];
	int sinceLast = sinceLastRCMessage;
	if (sinceLast > 4000)
		sinceLast = 4000;
	for (int i = 0; i < 4; i++)
	{
		sum[i] = sinceLast * channels[i] + (4000 - sinceLast) * lastChannels[i];
		sum[i] /= 4000;
		smoothChannels[i] = sum[i];
	}
}