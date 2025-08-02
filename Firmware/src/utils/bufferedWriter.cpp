#include "global.h"

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
	}
	return false;
}
