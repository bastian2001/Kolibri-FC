#include <Arduino.h>

#include "../../../src/pins.h"
#include "../../../src/typedefs.h"
#include "ESC_Serial.h" // ESC Serial Header
#include "Global.h"     // Global variables
#include "elapsedMillis.h"
#include <deque>

extern u32 offsetPioReceive, offsetPioTransmit;
extern pio_sm_config configPioReceive, configPioTransmit;
extern PIO testPio;
extern u8 testSm;

std::deque<uint8_t> escRxBuf;

uint16_t esc_crc = 0;

void pio_set_program(uint offset, pio_sm_config c) {
	pio_sm_set_config(testPio, testSm, &c);
	pio_sm_exec(testPio, testSm, pio_encode_jmp(offset));
}

void pioResetESC() {
	pio_set_program(offsetPioTransmit, configPioTransmit);
	delay(1);
	pio_sm_exec_wait_blocking(testPio, testSm, pio_encode_set(pio_pins, 0));
	delay(300);
	pio_sm_exec_wait_blocking(testPio, testSm, pio_encode_set(pio_pins, 1));
	pio_set_program(offsetPioReceive, configPioReceive);
}

void pioEnableTx(bool enable) {
	if (enable) {
		pio_set_program(offsetPioTransmit, configPioTransmit);
	} else {
		while (!pio_sm_is_tx_fifo_empty(testPio, testSm)) {
		}
		while (pio_sm_get_pc(testPio, testSm) != offsetPioTransmit + 2) {
		}
		pio_set_program(offsetPioReceive, configPioReceive);
	}
}

void InitSerialOutput() {
	Enable4Way = true;
}

void DeinitSerialOutput() {
	Enable4Way = false;
}

void pioWrite(u8 data) {
	pio_sm_put_blocking(testPio, testSm, data);
}
u8 pioAvailableRaw() {
	u8 level = pio_sm_get_rx_fifo_level(testPio, testSm);
	return level;
}
u8 pioReadRaw() {
	return pio_sm_get(testPio, testSm) >> 24;
}

u32 pioAvailable() {
	return escRxBuf.size();
}
u8 pioRead() {
	u8 data = escRxBuf.front();
	escRxBuf.pop_front();
	return data;
}

void delayWhileRead(u16 ms) {
	elapsedMillis x = 0;
	do {
		if (pioAvailableRaw()) {
			escRxBuf.push_back(pioReadRaw());
		}
	} while (x < ms);
}
void delayMicrosWhileRead(u16 us) {
	elapsedMicros x = 0;
	do {
		if (pioAvailableRaw()) {
			escRxBuf.push_back(pioReadRaw());
		}
	} while (x < us);
}

void SendESC(uint8_t tx_buf[], uint16_t buf_size, bool CRC) {
	uint16_t i = 0;
	esc_crc    = 0;
	if (buf_size == 0) {
		buf_size = 256;
	}
	pioEnableTx(true);
	for (i = 0; i < buf_size; i++) {
		pioWrite(tx_buf[i]);
		esc_crc = ByteCrc(tx_buf[i], esc_crc);
	}
	if (CRC) {
		pioWrite(esc_crc & 0xff);
		pioWrite((esc_crc >> 8) & 0xff);
		buf_size = buf_size + 2;
	}
	pioEnableTx(false);
}

uint16_t GetESC(uint8_t rx_buf[], uint16_t wait_ms) {
	uint16_t i      = 0;
	esc_crc         = 0;
	bool timeout    = false;
	u32 y           = 0;
	elapsedMillis x = 0;
	while ((!pioAvailable()) && (!timeout)) {
		delayWhileRead(1);
		i++;
		if (i >= wait_ms) {
			timeout = true;
			return 0;
		}
	}
	y = i;
	i = 0;
	while (pioAvailable()) {
		rx_buf[i] = pioRead();
		i++;
		delayMicrosWhileRead(500);
	}
	u32 m = x;
	return i;
}

uint16_t ByteCrc(uint8_t data, uint16_t crc) {
	uint8_t xb = data;
	for (uint8_t i = 0; i < 8; i++) {
		if (((xb & 0x01) ^ (crc & 0x0001)) != 0) {
			crc = crc >> 1;
			crc = crc ^ 0xA001;
		} else {
			crc = crc >> 1;
		}
		xb = xb >> 1;
	}
	return crc;
}
