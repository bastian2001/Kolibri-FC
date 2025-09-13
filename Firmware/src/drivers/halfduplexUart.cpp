#include "global.h"
#include "pioasm/onewire_receive.pio.h"
#include "pioasm/onewire_transmit.pio.h"

u8 SerialOnewire::transmitOffsets[NUM_PIOS];
u8 SerialOnewire::receiveOffsets[NUM_PIOS];
bool SerialOnewire::offsetsSet;

SerialOnewire::SerialOnewire(u8 pin, u32 baudrate, PIO pio, i8 sm)
	: pin(pin),
	  baudrate(baudrate),
	  pio(pio),
	  beginSm(sm) {
	if (!offsetsSet) {
		// somehow "static u8 transmitOffsets[] = {[0 ...(NUM_PIOS - 1)] = 255};" does not work...
		for (int i = 0; i < NUM_PIOS; i++) {
			transmitOffsets[i] = 255;
			receiveOffsets[i] = 255;
		}
		offsetsSet = true;
	}
}

SerialOnewire::~SerialOnewire() {
	if (running) {
		end();
	}
}

int SerialOnewire::available() { return 0; }
int SerialOnewire::read() {
	if (!running) {
		return -1;
	}
	if (pio_sm_is_rx_fifo_empty(pio, sm)) {
		return -1;
	}
	return pio_sm_get(pio, sm);
}
int SerialOnewire::peek() {
	// cannot peek into PIO fifo -> maybe for later with DMA?
	return -1;
}

size_t SerialOnewire::write(uint8_t c) {
	if (!running) {
		return 0;
	}
	startTx();
	pio_sm_put_blocking(pio, sm, c);
	endTx();
	return 1;
}
size_t SerialOnewire::write(const uint8_t *buffer, size_t size) {
	if (!running) {
		return 0;
	}
	startTx();
	for (size_t i = 0; i < size; i++) {
		pio_sm_put_blocking(pio, sm, buffer[i]);
		rp2040.wdt_reset();
	}
	endTx();
	return size;
}
void SerialOnewire::flush() {
	return; // always flushed with each write
}
int SerialOnewire::availableForWrite() {
	return 0; // always flushed with each write, no byte can be buffered
}

void SerialOnewire::begin() {
	if (running) return;
	if (!pio || !baudrate || pin == 255) {
		Serial.println("Assign values to halfduplex UART first");
		return;
	}

	pioIndex = pio_get_index(pio);
	if (pioIndex >= NUM_PIOS) {
		Serial.println("Invalid PIO instance");
		return;
	}

	// 16 ticks per bit
	float clkdiv = (float)clock_get_hz(clk_sys) / (baudrate * 16);
	if (clkdiv < 1 || clkdiv >= 65536) {
		Serial.println("Invalid baudrate");
		return;
	}

	// claim the state machine
	if (beginSm < 0) {
		sm = pio_claim_unused_sm(pio, false);
		if (sm < 0) {
			Serial.println("No free state machine available");
			return;
		}
	} else {
		if (pio_sm_is_claimed(pio, beginSm)) {
			Serial.println("State machine already claimed");
			return;
		}
		pio_sm_claim(pio, beginSm);
		sm = beginSm;
	}

	// check and load programs
	if (receiveOffsets[pioIndex] == 255) {
		if (!pio_can_add_program(pio, &onewire_receive_program)) {
			Serial.println("No free instruction memory available");
			return;
		}
		receiveOffsets[pioIndex] = pio_add_program(pio, &onewire_receive_program);
	}
	if (transmitOffsets[pioIndex] == 255) {
		if (!pio_can_add_program(pio, &onewire_transmit_program)) {
			Serial.println("No free instruction memory available");
			return;
		}
		transmitOffsets[pioIndex] = pio_add_program(pio, &onewire_transmit_program);
	}

	// set up GPIO
	gpio_set_pulls(pin, true, false);
	pio_gpio_init(pio, pin);

	// set up configs
	rxConfig = onewire_receive_program_get_default_config(receiveOffsets[pioIndex]);
	sm_config_set_set_pins(&rxConfig, pin, 1);
	sm_config_set_in_pins(&rxConfig, pin);
	sm_config_set_jmp_pin(&rxConfig, pin);
	sm_config_set_in_shift(&rxConfig, true, false, 32);
	sm_config_set_clkdiv(&rxConfig, clkdiv);
	txConfig = onewire_transmit_program_get_default_config(transmitOffsets[pioIndex]);
	sm_config_set_set_pins(&txConfig, pin, 1);
	sm_config_set_out_pins(&txConfig, pin, 1);
	sm_config_set_out_shift(&txConfig, true, false, 8);
	sm_config_set_clkdiv(&txConfig, clkdiv);

	// set up state machine
	pio_sm_init(pio, sm, receiveOffsets[pioIndex], &rxConfig);
	pio_sm_set_enabled(pio, sm, true);
}
void SerialOnewire::begin(u8 pin, u32 baudrate, PIO pio, i8 sm) {
	if (running) return;

	this->pin = pin;
	this->baudrate = baudrate;
	this->pio = pio;
	this->sm = sm;
	begin();
}
void SerialOnewire::end() {}

void SerialOnewire::startTx() {
	pio_sm_set_config(PIO_ESC, 0, &txConfig);
	pio_sm_exec(PIO_ESC, 0, pio_encode_jmp(transmitOffsets[pioIndex]));
}
void SerialOnewire::endTx() {
	while (pio_sm_is_tx_fifo_empty(pio, sm)) {
		tight_loop_contents();
	}
	while (pio_sm_get_pc(PIO_ESC, 0) != transmitOffsets[pioIndex] + 2) {
	}
	pio_sm_set_config(PIO_ESC, 0, &rxConfig);
	pio_sm_exec(PIO_ESC, 0, pio_encode_jmp(receiveOffsets[pioIndex]));
}
