#pragma once
#include "Arduino.h"
#include "typedefs.h"

class SerialOnewire : public Stream {
public:
	SerialOnewire(u8 pin, u32 baudrate, PIO pio, i8 sm = -1);
	~SerialOnewire();

	// from Stream
	int available();
	int read();
	int peek();

	// from Print
	size_t write(uint8_t c);
	size_t write(const uint8_t *buffer, size_t size);
	void flush();
	int availableForWrite();

	// Serial-alike
	void begin();
	void begin(u8 pin, u32 baudrate, PIO pio, i8 sm = -1);
	void end();

private:
	u8 pin = 0;
	u32 baudrate = 0;
	PIO pio = nullptr;
	i8 beginSm = -1;
	i8 sm = -1;
	bool running = false;
	pio_sm_config rxConfig;
	pio_sm_config txConfig;
	u8 pioIndex = 255;

	static u8 transmitOffsets[NUM_PIOS];
	static u8 receiveOffsets[NUM_PIOS];
	static bool offsetsSet;

	void startTx();
	void endTx();
};
