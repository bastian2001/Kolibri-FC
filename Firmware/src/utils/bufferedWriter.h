#pragma once
#include "ringbuffer.h"
#include "typedefs.h"
#include <Arduino.h>

//! When updating this, also update the serialTypeNames array
enum class SerialType {
	USB,
	UART,
	PIO
};

class BufferedWriter : public HardwareSerial {
public:
	Stream *const stream;
#ifdef USE_TINYUSB
	Adafruit_USBD_CDC *const usbStream;
#else
	SerialUSB *const usbStream;
#endif
	SerialUART *const uartStream;
	SerialPIO *const pioStream;
	const SerialType serialType;

#ifdef USE_TINYUSB
	BufferedWriter(Adafruit_USBD_CDC *const usbStream, int fifoSize)
		: stream(usbStream),
		  usbStream(usbStream),
		  uartStream(nullptr),
		  pioStream(nullptr),
		  serialType(SerialType::USB),
		  writeBuffer(fifoSize) {
		mutex_init(&writeMutex);
	};
#else
	BufferedWriter(SerialUSB *const usbStream, int fifoSize)
		: stream(usbStream),
		  usbStream(usbStream),
		  uartStream(nullptr),
		  pioStream(nullptr),
		  serialType(SerialType::USB),
		  writeBuffer(fifoSize) {
		mutex_init(&writeMutex);
	};
#endif

	BufferedWriter(SerialUART *const uartStream, size_t fifoSize)
		: stream(uartStream),
		  usbStream(nullptr),
		  uartStream(uartStream),
		  pioStream(nullptr),
		  serialType(SerialType::UART),
		  writeBuffer(fifoSize) {
		mutex_init(&writeMutex);
	};

	BufferedWriter(SerialPIO *const pioStream, size_t fifoSize)
		: stream(uartStream),
		  usbStream(nullptr),
		  uartStream(nullptr),
		  pioStream(pioStream),
		  serialType(SerialType::PIO),
		  writeBuffer(fifoSize) {
		mutex_init(&writeMutex);
	};

	virtual void begin(unsigned long baudrate = 115200) override;
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
			if (!c) return mutex_exit(&writeMutex);
			while (c--) {
				if (!stream->availableForWrite())
					return mutex_exit(&writeMutex);
				uartStream->write(writeBuffer.pop());
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

	operator bool();

	volatile u32 totalRx = 0;
	volatile u32 totalTx = 0;
	static elapsedMicros sinceReset;
	static char serialTypeNames[3][5];

private:
	RingBuffer<u8> writeBuffer;
	mutex_t writeMutex;
};
