#include "global.h"

const char KoliSerial::SERIAL_TYPE_NAMES[4][8] = {
	"USB    ",
	"UART   ",
	"PIO    ",
	"PIO HDx",
};
elapsedMicros KoliSerial::sinceReset = 0;

void KoliSerial::begin(unsigned long baudrate) {
	return begin(baudrate, SERIAL_8N1);
}
void KoliSerial::begin(unsigned long baudrate, uint16_t config) {
	this->baudrate = baudrate;
	switch (serialType) {
	case SerialType::USB:
		static_cast<UsbSerialClass *>(stream)->begin(baudrate, config);
		break;
	case SerialType::UART:
		static_cast<SerialUART *>(stream)->begin(baudrate, config);
		break;
	case SerialType::PIO:
		static_cast<SerialPio *>(stream)->begin(baudrate, config);
		break;
	case SerialType::PIO_HDX:
		static_cast<SerialPioHdx *>(stream)->begin(baudrate, config); // config unused
		break;
	}
}
void KoliSerial::end() {
	switch (serialType) {
	case SerialType::USB:
		static_cast<UsbSerialClass *>(stream)->end();
		break;
	case SerialType::UART:
		static_cast<SerialUART *>(stream)->end();
		break;
	case SerialType::PIO:
		static_cast<SerialPio *>(stream)->end();
		break;
	case SerialType::PIO_HDX:
		static_cast<SerialPioHdx *>(stream)->end();
		break;
	}
}

KoliSerial::operator bool() {
	switch (serialType) {
	case SerialType::USB:
		return (bool)(*static_cast<UsbSerialClass *>(stream));
	case SerialType::UART:
		return (bool)(*static_cast<SerialUART *>(stream));
	case SerialType::PIO:
		return (bool)(*static_cast<SerialPio *>(stream));
	case SerialType::PIO_HDX:
		return (bool)(*static_cast<SerialPioHdx *>(stream));
	}
	return false;
}

KoliSerial::~KoliSerial() {
	end();

	switch (serialType) {
	case SerialType::USB:
	case SerialType::UART:
		// The USB and UART streams are not owned by this instance (pointer transferred to constructor)
		break;
	case SerialType::PIO:
		delete static_cast<SerialPio *>(stream);
		break;
	case SerialType::PIO_HDX:
		delete static_cast<SerialPioHdx *>(stream);
		break;
	}

	if (msp != nullptr) delete msp;
	if (dpOutput != nullptr) delete dpOutput;
}

bool KoliSerial::setRxFifoSize(size_t size) {
	switch (serialType) {
	case SerialType::USB:
		// FIFO size is set at compile time in TinyUSB config
		return false;
	case SerialType::UART:
		return static_cast<SerialUART *>(stream)->setFIFOSize(size);
	case SerialType::PIO:
		return static_cast<SerialPio *>(stream)->setFIFOSize(size);
	case SerialType::PIO_HDX:
		return static_cast<SerialPioHdx *>(stream)->setFIFOSize(size);
	}
	return false;
}

bool KoliSerial::setPinout(pin_size_t tx, pin_size_t rx) {
	txPin = tx;
	rxPin = rx;
	switch (serialType) {
	case SerialType::USB:
		// cannot set pinout on USB interface
		return false;
	case SerialType::UART:
		return static_cast<SerialUART *>(stream)->setPinout(tx, rx);
	case SerialType::PIO:
		return static_cast<SerialPio *>(stream)->setPinout(tx, rx);
	case SerialType::PIO_HDX:
		// halfduplex => use "tx" pin for all IO
		rxPin = tx;
		return static_cast<SerialPioHdx *>(stream)->setPinout(tx, tx);
	}
	return false;
}

void KoliSerial::setFunctions(u32 newFunctions) {
	if (newFunctions & (SERIAL_MSP | SERIAL_MSP_DISPLAYPORT) && msp == nullptr) {
		msp = new MspParser(*this);
	} else if (!(newFunctions & (SERIAL_MSP | SERIAL_MSP_DISPLAYPORT)) && msp != nullptr) {
		delete msp;
		msp = nullptr;
	}
	if (newFunctions & SERIAL_MSP_DISPLAYPORT && dpOutput == nullptr) {
		dpOutput = new MspOsdOutput(*this);
	} else if (!(newFunctions & SERIAL_MSP_DISPLAYPORT) && dpOutput != nullptr) {
		delete dpOutput;
		dpOutput = nullptr;
	}
	funcs = newFunctions;
}
