#include "global.h"

// u8 readChar = 0;
u32 crcLutD5[256] = {0};

char serialFunctionNames[SERIAL_FUNCTION_COUNT][11] = {
	"CRSF",
	"MSP",
	"GPS",
	"4Way",
	"Tramp",
	"Smartaudio",
	"ESC Telem",
};

KoliSerial serials[SERIAL_COUNT] = {
	{new BufferedWriter(&Serial, 2048), SERIAL_MSP},
	{new BufferedWriter(&Serial1, 2048), SERIAL_CRSF},
	{new BufferedWriter(&Serial2, 2048), SERIAL_GPS},
};
static u8 currentSerial = 0;

void initSerial() {
	for (u32 i = 0; i < 256; i++) {
		u32 crc = i;
		for (u32 j = 0; j < 8; j++) {
			if (crc & 0x80)
				crc = (crc << 1) ^ 0xD5;
			else
				crc <<= 1;
		}
		crcLutD5[i] = crc & 0xFF;
	}

	for (int i = 0; i < SERIAL_COUNT; i++) {
		auto &serial = serials[i];
		u32 rxFifo = 0;
		u32 baud = 115200;
		u16 config = SERIAL_8N1;
		if (serial.functions & SERIAL_CRSF) {
			rxFifo = 256;
			baud = 420000;
		} else if (serial.functions & SERIAL_MSP) {
			rxFifo = 256;
			baud = 115200;
		} else if (serial.functions & SERIAL_GPS) {
			rxFifo = 128;
			baud = 38400;
		} else if (serial.functions & SERIAL_4WAY) {
			rxFifo = 256;
			baud = 115200;
		} else if (serial.functions & SERIAL_IRC_TRAMP) {
			rxFifo = 32;
			baud = 9600;
		} else if (serial.functions & SERIAL_SMARTAUDIO) {
			config = SERIAL_8N2;
			rxFifo = 32;
			baud = 4800;
		} else if (serial.functions & SERIAL_ESC_TELEM) {
			rxFifo = 64;
			baud = 115200;
		}
		u8 rxPin = 0;
		u8 txPin = 0;
		switch (i) {
		case 1:
			rxPin = PIN_RX0;
			txPin = PIN_TX0;
			break;
		case 2:
			rxPin = PIN_RX1;
			txPin = PIN_TX1;
			break;
		case 3:
			rxPin = PIN_RX2;
			txPin = PIN_TX2;
			break;
		}
		switch (serial.stream->serialType) {
		case SerialType::USB:
			break;
		case SerialType::UART:
			serial.stream->uartStream->setPinout(txPin, rxPin);
			serial.stream->uartStream->setFIFOSize(rxFifo);
			break;
		case SerialType::PIO:
			if (serial.stream->pioStream)
				delete serial.stream->pioStream;
			delete serial.stream;
			serial.stream = new BufferedWriter(new SerialPIO(txPin, rxPin, rxFifo), 2048);
			break;
		}
		serial.stream->begin(baud, config);
	}
}

void serialLoop() {
	TASK_START(TASK_SERIAL);

	if (++currentSerial == SERIAL_COUNT) currentSerial = 0;
	u32 functions = serials[currentSerial].functions;
	if (!functions) return;

	Stream *serial = serials[currentSerial].stream;
	for (int c = serial->read(), i = 16; c != -1 && i; c = serial->read(), i--) {
		if (functions & SERIAL_CRSF) {
			if (!elrsBuffer.isFull())
				elrsBuffer.push(c);
		}
		if (functions & SERIAL_MSP) {
			rp2040.wdt_reset();
			elapsedMicros timer = 0;
			mspHandleByte(c, currentSerial);
			taskTimerTASK_SERIAL -= timer;
		}
		if (functions & SERIAL_GPS) {
			if (!gpsBuffer.isFull())
				gpsBuffer.push(c);
		}
		if (functions & SERIAL_4WAY) {
			process4Way(c);
		}
		if (functions & SERIAL_IRC_TRAMP) {
		}
		if (functions & SERIAL_SMARTAUDIO) {
		}
		if (functions & SERIAL_ESC_TELEM) {
		}
	}
	serials[currentSerial].stream->loop();
	TASK_END(TASK_SERIAL);
}
