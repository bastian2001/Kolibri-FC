#pragma once
#include "Arduino.h"
#include "typedefs.h"

class SerialPioHdx : public HardwareSerial {
public:
	SerialPioHdx() = delete;
	SerialPioHdx(PIO pio, i8 sm = -1);
	~SerialPioHdx();

	// from Stream
	virtual int available() override;
	int read();
	int peek();

	// from Print
	virtual size_t write(uint8_t c) override;
	virtual size_t write(const uint8_t *buffer, size_t size) override;
	void flush();
	virtual int availableForWrite() override;

	// Serial-alike
	virtual void begin(unsigned long baudrate) override;
	/**
	 * @brief Same as begin(baudrate), config is UNUSED!
	 *
	 * @param baudrate
	 * @param _config
	 */
	virtual void begin(unsigned long baudrate, uint16_t _config) override;
	virtual void end() override;
	int getPc();

	bool setPin(u8 pin);

	operator bool() override;

private:
	void begin();
	u8 pin = 255;
	u32 baudrate = 0;
	PIO pio = nullptr;
	i8 beginSm = -1;
	i8 sm = -1;
	bool running = false;
	pio_sm_config pioConfig;
	u8 pioIndex = 255;
	i32 peekVal = -1;

	static u8 programOffsets[NUM_PIOS];
	static bool offsetsSet;
};
