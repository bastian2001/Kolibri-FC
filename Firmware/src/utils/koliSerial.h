#pragma once
#include "drivers/halfduplexUart.h"
#include "elapsedMillis.h"
#include "ringbuffer.h"
#include "typedefs.h"
#include <Arduino.h>

//! When updating this, also update the serialTypeNames array
enum class SerialType {
	USB,
	UART,
	PIO,
	PIO_HDX,
};

#ifdef USE_TINYUSB
typedef Adafruit_USBD_CDC UsbSerialClass;
#else
typedef SerialUSB UsbSerialClass;
#endif

class KoliSerial : public HardwareSerial {
public:
	const SerialType serialType;

	KoliSerial() = delete;
	KoliSerial(const KoliSerial &) = delete;
	KoliSerial &operator=(const KoliSerial &) = delete;

	KoliSerial(UsbSerialClass *const usbStream, int txfSize, int functions)
		: stream(usbStream),
		  serialType(SerialType::USB),
		  writeBuffer(txfSize),
		  functions(functions) {
		mutex_init(&writeMutex);
	};

	KoliSerial(SerialUART *const uartStream, int txfSize, int functions)
		: stream(uartStream),
		  serialType(SerialType::UART),
		  writeBuffer(txfSize),
		  functions(functions) {
		mutex_init(&writeMutex);
	};

	KoliSerial(PIO pioTx, PIO pioRx, u8 smTx, u8 smRx, size_t txfSize, int functions)
		: stream(new SerialPio(pioTx, pioRx, smTx, smRx)),
		  serialType(SerialType::PIO),
		  writeBuffer(txfSize),
		  functions(functions) {
		mutex_init(&writeMutex);
	};

	KoliSerial(PIO pio, u8 sm, size_t txfSize, int functions)
		: stream(new SerialPioHdx(pio, sm)),
		  serialType(SerialType::PIO_HDX),
		  writeBuffer(txfSize),
		  functions(functions) {
		mutex_init(&writeMutex);
	};

	~KoliSerial();

	virtual void begin(unsigned long baudrate) override;
	virtual void begin(unsigned long baudrate, uint16_t config) override;
	virtual void end() override;
	virtual int available() override {
		return stream->available();
	}
	virtual int availableForWrite() override {
		mutex_enter_blocking(&writeMutex);
		u32 writable = writeBuffer.freeSpace();
		mutex_exit(&writeMutex);
		return writable;
	}
	int peek() { return stream->peek(); }
	int read() {
		int i = stream->read();
		if (i != -1) totalRx++;
		return i;
	}
	void loop(i32 maxWrite = 64) {
		// write the lowest of
		// - maxWrite
		// - available space in peripheral
		// - available data
		i32 c = writeBuffer.itemCount();
		if (c > maxWrite) c = maxWrite;
		if (!mutex_try_enter(&writeMutex, nullptr)) return;

		if (serialType == SerialType::UART) {
			// special treatment: UART cannot tell how many bytes it can still send, only _that_ it can still send at least one
			SerialUART &s = *static_cast<SerialUART *>(stream);
			if (!c) return mutex_exit(&writeMutex);
			while (c--) {
				if (!s.availableForWrite())
					return mutex_exit(&writeMutex);
				s.write(writeBuffer.pop());
				totalTx++;
			}
			return mutex_exit(&writeMutex);
		}

		maxWrite = stream->availableForWrite();
		if (c > maxWrite) c = maxWrite;
		if (!c) return mutex_exit(&writeMutex);

		u8 buf[c];
		writeBuffer.copyToArray(buf, 0, c);
		writeBuffer.erase(c);
		mutex_exit(&writeMutex);
		stream->write(buf, c);
		totalTx += c;
	};
	virtual void flush() override {
		mutex_enter_blocking(&writeMutex);
		while (!writeBuffer.isEmpty()) {
			i32 c = stream->availableForWrite();
			i32 maxWrite = writeBuffer.itemCount();
			if (c > maxWrite) c = maxWrite;
			if (!c) continue;
			u8 buf[c];
			writeBuffer.copyToArray(buf, 0, c);
			writeBuffer.erase(c);
			stream->write(buf, c);
			totalTx += c;
			// USB can take advantage of larger chunks, let it free the TX buffer completely before retrying any new
			if (serialType == SerialType::USB) {
				stream->flush();
			}
		}
		mutex_exit(&writeMutex);
		stream->flush();
	};
	virtual size_t write(uint8_t c) override {
		mutex_enter_blocking(&writeMutex);
		if (writeBuffer.isFull()) {
			stream->write(writeBuffer.pop());
			totalTx++;
		}
		writeBuffer.push(c);
		mutex_exit(&writeMutex);
		return 1;
	};
	virtual size_t write(const uint8_t *p, size_t len) override {
		mutex_enter_blocking(&writeMutex);
		u32 free = writeBuffer.freeSpace();
		if (free >= len) {
			while (len--) {
				writeBuffer.push(*p++);
			}
		} else {
			len -= free;
			// first fill as many into the buffer as possible
			while (free--) {
				writeBuffer.push(*p++);
			}
			// then empty the buf as much as needed into the peripheral
			while (len > 0) {
				i32 c = stream->availableForWrite();
				i32 maxWrite = writeBuffer.itemCount();
				if (c > maxWrite) c = maxWrite;
				if (c > len) c = len;
				if (!c) continue;
				u8 buf[c];
				// pull from internal buffer
				writeBuffer.copyToArray(buf, 0, c);
				writeBuffer.erase(c);
				// write to peri
				stream->write(buf, c);
				totalTx += c;
				len -= c;
				// refill internal buffer
				while (c--) {
					writeBuffer.push(*p++);
				}
			}
		}
		mutex_exit(&writeMutex);
		return len;
	};

	bool setRxFifoSize(size_t size);
	bool setPinout(pin_size_t tx, pin_size_t rx);
	pin_size_t getRxPin() { return rxPin; };
	pin_size_t getTxPin() { return txPin; };
	bool setBaudrate(u32 baud);
	const u32 &getBaurate() { return baudrate; };

	operator bool();

	volatile u32 totalRx = 0;
	volatile u32 totalTx = 0;

	static elapsedMicros sinceReset;
	u32 functions = 0; // OR of SERIAL_ defines, e.g. SERIAL_MSP
	static char serialTypeNames[4][8];

private:
	RingBuffer<u8> writeBuffer;
	mutex_t writeMutex;
	u32 baudrate = 0;
	pin_size_t txPin, rxPin;

	Stream *const stream;
};
