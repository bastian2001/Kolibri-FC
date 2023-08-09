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

	switch (msgBuffer[2])
	{
	case LINK_STATISTICS:
		break;
	case RC_CHANNELS_PACKED:
	{
		if (size != 26) // 16 channels * 11 bits + 3 bytes header + 1 byte crc
		{
			Serial.println("ERROR_INVALID_LENGTH");
			lastError = ERROR_INVALID_LENGTH;
			errorFlag = true;
			errorCount++;
			break;
		}
		// crsf_channels_t *crsfChannels = (crsf_channels_t *)(&msgBuffer[3]); // somehow conversion through bit-fields does not work, so manual conversion
		uint32_t decoder = msgBuffer[3] | (msgBuffer[4] << 8) | (msgBuffer[5] << 16) | (msgBuffer[6] << 24);
		channels[0] = (decoder >> 0) & 0x7FF;  // 0...10
		channels[1] = (decoder >> 11) & 0x7FF; // 11...21
		decoder >>= 16;						   // 16
		decoder |= (msgBuffer[7] << 16);
		decoder |= (msgBuffer[8] << 24);
		channels[2] = (decoder >> 6) & 0x7FF;  // 22...32
		channels[3] = (decoder >> 17) & 0x7FF; // 33...43
		decoder >>= 24;						   // 40
		decoder |= (msgBuffer[9] << 8);
		decoder |= (msgBuffer[10] << 16);
		decoder |= (msgBuffer[11] << 24);
		channels[4] = (decoder >> 4) & 0x7FF;  // 44...54
		channels[5] = (decoder >> 15) & 0x7FF; // 55...65
		decoder >>= 16;						   // 56
		decoder |= (msgBuffer[12] << 16);
		decoder |= (msgBuffer[13] << 24);
		channels[6] = (decoder >> 10) & 0x7FF; // 66...76
		channels[7] = (decoder >> 21) & 0x7FF; // 77...87
		decoder = 0;						   // 88, aka shift right by 32 bits
		decoder |= (msgBuffer[14] << 0);
		decoder |= (msgBuffer[15] << 8);
		decoder |= (msgBuffer[16] << 16);
		decoder |= (msgBuffer[17] << 24);
		channels[8] = (decoder >> 0) & 0x7FF;  // 88...98
		channels[9] = (decoder >> 11) & 0x7FF; // 99...109
		decoder >>= 16;						   // 104
		decoder |= (msgBuffer[18] << 16);
		decoder |= (msgBuffer[19] << 24);
		channels[10] = (decoder >> 6) & 0x7FF;	// 110...120
		channels[11] = (decoder >> 17) & 0x7FF; // 121...131
		decoder >>= 24;							// 128
		decoder |= (msgBuffer[20] << 8);
		decoder |= (msgBuffer[21] << 16);
		decoder |= (msgBuffer[22] << 24);
		channels[12] = (decoder >> 4) & 0x7FF;	// 132...142
		channels[13] = (decoder >> 15) & 0x7FF; // 143...153
		decoder >>= 16;							// 144
		decoder |= (msgBuffer[23] << 16);
		decoder |= (msgBuffer[24] << 24);
		channels[14] = (decoder >> 10) & 0x7FF; // 154...164
		channels[15] = (decoder >> 21) & 0x7FF; // 165...175

		// if (++counter == 30)
		// {
		// 	counter = 0;
		// 	for (int i = 3; i < 25; i++)
		// 	{
		// 		for (int j = 7; j >= 0; j--)
		// 		{
		// 			// Serial.printf("%d", (msgBuffer[i] >> j) & 1);
		// 		}
		// 		// Serial.print(" ");
		// 	}
		// 	// Serial.println();
		// }
		// map channels to 1000-2000
		for (uint8_t i = 0; i < 16; i++)
			channels[i] = map(channels[i], 172, 1811, 988, 2012);
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