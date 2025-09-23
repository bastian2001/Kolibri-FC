#include "global.h"

char BufferedWriter::serialTypeNames[4][12] = {
	"USB        ",
	"UART       ",
	"PIO        ",
	"PIO Half-D.",
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
	case SerialType::PIO_HD:
		hdStream->begin(baudrate);
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
	case SerialType::PIO_HD:
		hdStream->begin(baudrate, config); // config unused
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
	case SerialType::PIO_HD:
		hdStream->end();
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
	case SerialType::PIO_HD:
		return (bool)(*hdStream);
	}
	return false;
}
