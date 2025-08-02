#pragma once
#include "ringbuffer.h"
#include "typedefs.h"
#include <Arduino.h>

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
	virtual int available() override { return stream->available(); }
	virtual int availableForWrite() override {
		mutex_enter_blocking(&writeMutex);
		u32 writable = writeBuffer.freeSpace();
		mutex_exit(&writeMutex);
		return writable;
	}
	int peek() { return stream->peek(); }
	int read() { return stream->read(); }
	void loop(i32 maxWrite = 64) {
		// write the lowest of
		// - maxWrite
		// - available space in peripheral
		// - available data
		i32 c = stream->availableForWrite();
		if (c > maxWrite) c = maxWrite;
		if (!mutex_try_enter(&writeMutex, nullptr)) return;
		maxWrite = writeBuffer.itemCount();
		if (c > maxWrite) c = maxWrite;
		if (!c) {
			mutex_exit(&writeMutex);
			return;
		}
		u8 buf[c];
		writeBuffer.copyToArray(buf, 0, c);
		writeBuffer.erase(c);
		mutex_exit(&writeMutex);
		stream->write(buf, c);
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
			// USB can take advantage of larger chunks, let it free the TX buffer completely before retrying any new
			if (serialType == SerialType::USB){
				stream->flush();
			}
		}
		mutex_exit(&writeMutex);
		stream->flush();
	};
	virtual size_t write(uint8_t c) override {
		mutex_enter_blocking(&writeMutex);
		if (writeBuffer.isFull())
			stream->write(writeBuffer.pop());
		writeBuffer.push(c);
		mutex_exit(&writeMutex);
		return 1;
	};
	virtual size_t write(const uint8_t *p, size_t len) override {
		mutex_enter_blocking(&writeMutex);
		u32 free = writeBuffer.freeSpace();
		if (free >= len) {
			for (; len; --len) {
				writeBuffer.push(*p++);
			}
		} else {
			len -= free;
			for (; free; --free) {
				writeBuffer.push(*p++);
			}
			while (len > 0) {
				i32 c = stream->availableForWrite();
				i32 maxWrite = writeBuffer.itemCount();
				if (c > maxWrite) c = maxWrite;
				if (c > len) c = len;
				if (!c) continue;
				u8 buf[c];
				writeBuffer.copyToArray(buf, 0, c);
				writeBuffer.erase(c);
				stream->write(buf, c);
				len -= c;
				for (; c; --c) {
					writeBuffer.push(*p++);
				}
			}
		}
		mutex_exit(&writeMutex);
		return len;
	};

	operator bool();

private:
	RingBuffer<u8> writeBuffer;
	mutex_t writeMutex;
};
