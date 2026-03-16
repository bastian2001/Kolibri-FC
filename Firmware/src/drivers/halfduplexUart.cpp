#include "drivers/halfduplexUart.h"
#include "global.h"

static u8 programOffsets[NUM_PIOS];
static bool offsetsSet;

static std::list<SerialPioHdxConfig *> configs;

SerialPioHdx::SerialPioHdx(PIO pio, i8 sm)
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

SerialPioHdx::~SerialPioHdx() {
	if (running) {
		end();
	}
}

int SerialPioHdx::available() {
	return pio_sm_get_rx_fifo_level(pio, sm);
}
int SerialPioHdx::read() {
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
int SerialPioHdx::peek() {
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

size_t SerialPioHdx::write(uint8_t c) {
	if (!running) {
		return 0;
	}
	pio_sm_put_blocking(pio, sm, c);
	return 1;
}
size_t SerialPioHdx::write(const uint8_t *buffer, size_t size) {
	if (!running) {
		return 0;
	}
	for (size_t i = 0; i < size; i++) {
		pio_sm_put_blocking(pio, sm, buffer[i]);
		rp2040.wdt_reset();
	}
	return size;
}
void SerialPioHdx::flush() {
	while (!pio_sm_is_tx_fifo_empty(pio, sm)) {
		tight_loop_contents();
	}
	return;
}
int SerialPioHdx::availableForWrite() {
	return 4 - pio_sm_get_tx_fifo_level(pio, sm);
}

void SerialPioHdx::begin() {
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
	pio_sm_config cfg = halfduplex_uart_program_get_default_config(programOffsets[pioIndex]);
	sm_config_set_set_pins(&cfg, pin, 1);
	sm_config_set_out_pins(&cfg, pin, 1);
	sm_config_set_in_pins(&cfg, pin);
	sm_config_set_jmp_pin(&cfg, pin);
	sm_config_set_in_shift(&cfg, true, false, 8);
	sm_config_set_out_shift(&cfg, true, false, 8);
	sm_config_set_clkdiv(&cfg, clkdiv);

	// set up state machine
	pio_sm_init(pio, sm, programOffsets[pioIndex], &cfg);
	pio_sm_set_enabled(pio, sm, true);
	pio_sm_set_clkdiv(pio, sm, clkdiv);

	pioConfig.pio = pioIndex;
	configs.push_back(&pioConfig);

	running = true;
}
void SerialPioHdx::begin(unsigned long baudrate) {
	if (running) return;

	this->baudrate = baudrate;
	begin();
}
void SerialPioHdx::begin(unsigned long baudrate, uint16_t _config) {
	begin(baudrate);
}
void SerialPioHdx::end() {
	running = false;

	// erase this serial from the configs
	for (auto it = configs.begin(); it != configs.end();) {
		if ((*it) == &pioConfig) {
			it = configs.erase(it);
		} else {
			++it;
		}
	}

	// stop state machines
	pio_sm_set_enabled(pio, sm, false);
	pio_sm_clear_fifos(pio, sm);
	pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
	gpio_set_function(pin, GPIO_FUNC_NULL);

	// search for others, and if another serial on the same pio still exists, don't delete program
	bool del = true;
	for (auto it = configs.begin(); it != configs.end(); ++it) {
		if ((*it)->pio == pioIndex) del = false;
	}
	if (del) {
		pio_remove_program(pio, &halfduplex_uart_program, programOffsets[pioIndex]);
		programOffsets[pioIndex] = 255;
	}

	pio_sm_unclaim(pio, sm);
}

SerialPioHdx::operator bool() {
	return running;
}

int SerialPioHdx::getPc() {
	return pio_sm_get_pc(pio, sm) - programOffsets[pioIndex];
}

bool SerialPioHdx::setPinout(u8 pin, u8 _) {
	if (running) return false;
	this->pin = pin;
	return true;
}

bool SerialPioHdx::setFIFOSize(size_t size) {
	if (running) return false;
	// only accept powers of 2, don't accept zero size
	if (!size || (size & (size - 1)) != 0) return false;
	rxBufSize = size;
	// TODO actually do something with this
	return true;
}
