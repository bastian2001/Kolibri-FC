#pragma once
#include "Arduino.h"
#include "typedefs.h"

typedef struct serialPioConfig {
	i8 rxPio;
	i8 txPio;
} SerialPioConfig;

class SerialPio : public HardwareSerial {
public:
	SerialPio() = delete;
	SerialPio(const SerialPio &) = delete;
	SerialPio(PIO pioTx, PIO pioRx, i8 smTx = -1, i8 smRx = -1);
	~SerialPio();

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
	/// @brief Same as begin(baudrate), _config is UNUSED!
	virtual void begin(unsigned long baudrate, uint16_t _config) override;
	virtual void end() override;

	bool setPinout(u8 tx, u8 rx);
	bool setFIFOSize(size_t size);

	operator bool() override;

private:
	void begin();
	u8 pinTx = 255;
	u8 pinRx = 255;
	u32 baudrate = 0;
	PIO pioTx = nullptr;
	PIO pioRx = nullptr;
	i8 beginSmTx = -1;
	i8 beginSmRx = -1;
	i8 smTx = -1;
	i8 smRx = -1;
	bool running = false;
	u8 pioIndexTx = 255;
	u8 pioIndexRx = 255;
	i32 peekVal = -1;
	SerialPioConfig pioConfig;
	size_t rxFifoSize = 32;
	i8 *rxBuf = nullptr;
	u8 rxDmaChan = 255;
};
