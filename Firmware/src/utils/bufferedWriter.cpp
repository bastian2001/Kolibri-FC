#include "global.h"

char BufferedWriter::serialTypeNames[4][8] = {
	"USB    ",
	"UART   ",
	"PIO    ",
	"PIO HDx",
};
elapsedMicros BufferedWriter::sinceReset = 0;

void BufferedWriter::begin(unsigned long baudrate) {
	switch (serialType) {
	case SerialType::USB:
		usbStream->begin(baudrate);
		break;
	case SerialType::UART:
		uartStream->begin(baudrate);
		break;
	case SerialType::PIO:
		pioStream->begin(baudrate);
		break;
	case SerialType::PIO_HDX:
		hdxStream->begin(baudrate);
		break;
	}
}
void BufferedWriter::begin(unsigned long baudrate, uint16_t config) {
	switch (serialType) {
	case SerialType::USB:
		usbStream->begin(baudrate, config);
		break;
	case SerialType::UART:
		uartStream->begin(baudrate, config);
		break;
	case SerialType::PIO:
		pioStream->begin(baudrate, config);
		break;
	case SerialType::PIO_HDX:
		hdxStream->begin(baudrate, config); // config unused
		break;
	}
}
void BufferedWriter::end() {
	switch (serialType) {
	case SerialType::USB:
		usbStream->end();
		break;
	case SerialType::UART:
		uartStream->end();
		break;
	case SerialType::PIO:
		pioStream->end();
		break;
	case SerialType::PIO_HDX:
		hdxStream->end();
		break;
	}
}

BufferedWriter::operator bool() {
	switch (serialType) {
	case SerialType::USB:
		return (bool)(*usbStream);
	case SerialType::UART:
		return (bool)(*uartStream);
	case SerialType::PIO:
		return (bool)(*pioStream);
	case SerialType::PIO_HDX:
		return (bool)(*hdxStream);
	}
	return false;
}
