#include "global.h"

u8 SerialPioHd::programOffsets[NUM_PIOS];
bool SerialPioHd::offsetsSet;

SerialPioHd::SerialPioHd(PIO pio, i8 sm)
	: pio(pio),
	  beginSm(sm) {
	if (!offsetsSet) {
		// somehow "static u8 programOffsets[] = {[0 ...(NUM_PIOS - 1)] = 255};" does not work...
		for (int i = 0; i < NUM_PIOS; i++) {
			programOffsets[i] = 255;
		}
		offsetsSet = true;
	}
}

SerialPioHd::~SerialPioHd() {
	if (running) {
		end();
	}
}

int SerialPioHd::available() {
	return pio_sm_get_rx_fifo_level(pio, sm);
}
int SerialPioHd::read() {
	if (!running) {
		return -1;
	}
	if (peekVal != -1) {
		int i = peekVal;
		peekVal = -1;
		return i;
	}
	if (pio_sm_is_rx_fifo_empty(pio, sm)) {
		return -1;
	}
	return pio_sm_get(pio, sm) >> 24;
}
int SerialPioHd::peek() {
	if (!running) {
		return -1;
	}
	if (peekVal != -1) {
		return peekVal;
	}
	if (pio_sm_is_rx_fifo_empty(pio, sm)) {
		return -1;
	}
	return peekVal = pio_sm_get(pio, sm) >> 24;
}

size_t SerialPioHd::write(uint8_t c) {
	if (!running) {
		return 0;
	}
	pio_sm_put_blocking(pio, sm, c);
	return 1;
}
size_t SerialPioHd::write(const uint8_t *buffer, size_t size) {
	if (!running) {
		return 0;
	}
	for (size_t i = 0; i < size; i++) {
		pio_sm_put_blocking(pio, sm, buffer[i]);
		rp2040.wdt_reset();
	}
	return size;
}
void SerialPioHd::flush() {
	while (!pio_sm_is_tx_fifo_empty(pio, sm)) {
		tight_loop_contents();
	}
	return;
}
int SerialPioHd::availableForWrite() {
	return 4 - pio_sm_get_tx_fifo_level(pio, sm);
}

void SerialPioHd::begin() {
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

	// 23 ticks per bit
	float clkdiv = (float)clock_get_hz(clk_sys) / (baudrate * 23);
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

	// check and load program
	if (programOffsets[pioIndex] == 255) {
		if (!pio_can_add_program(pio, &halfduplex_uart_program)) {
			Serial.println("No free instruction memory available");
			return;
		}
		programOffsets[pioIndex] = pio_add_program(pio, &halfduplex_uart_program);
	}

	// set up GPIO
	gpio_set_pulls(pin, true, false);
	gpio_init(pin);
	gpio_put(pin, true); // set high for once the output is enabled
	pio_gpio_init(pio, pin);

	// set up configs
	pioConfig = halfduplex_uart_program_get_default_config(programOffsets[pioIndex]);
	sm_config_set_set_pins(&pioConfig, pin, 1);
	sm_config_set_out_pins(&pioConfig, pin, 1);
	sm_config_set_in_pins(&pioConfig, pin);
	sm_config_set_jmp_pin(&pioConfig, pin);
	sm_config_set_in_shift(&pioConfig, true, false, 8);
	sm_config_set_out_shift(&pioConfig, true, false, 8);
	sm_config_set_clkdiv(&pioConfig, clkdiv);

	// set up state machine
	pio_sm_init(pio, sm, programOffsets[pioIndex], &pioConfig);
	pio_sm_set_enabled(pio, sm, true);
	pio_sm_set_clkdiv(pio, sm, clkdiv);

	running = true;
}
void SerialPioHd::begin(unsigned long baudrate) {
	if (running) return;

	this->baudrate = baudrate;
	begin();
}
void SerialPioHd::begin(unsigned long baudrate, uint16_t _config) {
	begin(baudrate);
}
void SerialPioHd::end() {}

SerialPioHd::operator bool() {
	return running;
}

int SerialPioHd::getPc() {
	return pio_sm_get_pc(pio, sm) - programOffsets[pioIndex];
}

bool SerialPioHd::setPin(u8 pin) {
	if (running) return false;
	this->pin = pin;
	return true;
}
