#include "global.h"

u32 crcLutD5[256] = {};

char const serialFunctionNames[SERIAL_FUNCTION_COUNT][20] = {
	"CRSF",
	"MSP",
	"GPS",
	"4Way",
	"Tramp",
	"Smartaudio",
	"ESC Telem",
	"MSP Displayport",
};

std::optional<KoliSerial> serials[SERIAL_COUNT];
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

	serials[0].emplace(&Serial, 2048, SERIAL_MSP);
	serials[1].emplace(&Serial1, 2048, SERIAL_CRSF);
	serials[2].emplace(&Serial2, 2048, SERIAL_GPS);
	// serials[3].emplace(pio1, pio1, -1, -1, 2048, SERIAL_MSP | SERIAL_MSP_DISPLAYPORT);
	serials[4].emplace(pio2, -1, 2048, SERIAL_IRC_TRAMP);

	for (int i = 0; i < SERIAL_COUNT; i++) {
		if (!serials[i]) continue;
		auto &serial = *serials[i];
		u32 rxFifo = 4;
		u32 baud = 115200;
		u16 config = SERIAL_8N1;

		if (serial.functions & SERIAL_CRSF) {
			rxFifo = 256;
			baud = 420000;
		} else if (serial.functions & SERIAL_MSP) {
			rxFifo = 256;
		} else if (serial.functions & SERIAL_GPS) {
			rxFifo = 128;
			baud = 38400;
		} else if (serial.functions & SERIAL_4WAY) {
			rxFifo = 256;
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
		} else if (serial.functions & SERIAL_MSP_DISPLAYPORT) {
			rxFifo = 256;
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
		case 4:
			rxPin = txPin = PIN_TX2;
			break;
		}
		serial.setPinout(txPin, rxPin);
		serial.setRxFifoSize(rxFifo);
		serial.begin(baud, config);
	}
}

void serialLoop() {
	TASK_START(TASK_SERIAL);

	if (++currentSerial >= SERIAL_COUNT) currentSerial = 0;
	if (!serials[currentSerial]) return;

	KoliSerial &serial = *serials[currentSerial];
	u32 &functions = serial.functions;

	if (!functions) {
		while (serial.read() != -1) { // empty RX buf
			tight_loop_contents();
		}
		TASK_END(TASK_SERIAL);
		return;
	}

	// max 16 chars per loop, stop when no char to read
	for (int i = 16; i; i--) {
		int c = serial.read();
		if (c == -1) break;

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
			if (!trampRxBuffer.isFull())
				trampRxBuffer.push(c);
		}
		if (functions & SERIAL_SMARTAUDIO) {
		}
		if (functions & SERIAL_ESC_TELEM) {
		}
		if (functions & SERIAL_MSP_DISPLAYPORT) {
		}
	}
	serial.loop();
	TASK_END(TASK_SERIAL);
}
