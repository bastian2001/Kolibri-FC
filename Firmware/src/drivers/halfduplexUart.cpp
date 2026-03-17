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
	rxDmaChan = dma_claim_unused_channel(false);
}

SerialPioHdx::~SerialPioHdx() {
	if (running) {
		end();
		if (rxBuf != nullptr) free(rxBuf);
		if (rxDmaChan != 255) dma_channel_unclaim(rxDmaChan);
	}
}

int SerialPioHdx::available() {
	u8 *dmaPtr = (u8 *)dma_hw->ch[rxDmaChan].write_addr;
	if (dmaPtr >= rxPtr) return dmaPtr - rxPtr;
	return rxBufSize - (rxPtr - dmaPtr);
}
int SerialPioHdx::read() {
	if (!running) return -1;
	volatile u8 *dmaPtr = (volatile u8 *)dma_hw->ch[rxDmaChan].write_addr;
	if (dmaPtr == rxPtr) return -1;
	int res = *rxPtr++;
	if (rxPtr >= rxBuf + rxBufSize)
		rxPtr = rxBuf;
	return res;
}
int SerialPioHdx::peek() {
	if (!running || !available()) return -1;
	return *rxPtr;
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
	if (!pio || !baudrate || pin == 255 || rxBuf == nullptr || rxDmaChan == 255) {
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

	// set up RX DMA channel
	dma_channel_config_t dmaCfg = dma_channel_get_default_config(rxDmaChan);
	channel_config_set_read_increment(&dmaCfg, false);
	channel_config_set_write_increment(&dmaCfg, true);
	channel_config_set_dreq(&dmaCfg, pio_get_dreq(pio, sm, false));
	channel_config_set_transfer_data_size(&dmaCfg, DMA_SIZE_8);
	channel_config_set_ring(&dmaCfg, true, 31 - __builtin_clz(rxBufSize));
	dma_channel_set_config(rxDmaChan, &dmaCfg, false);
	dma_channel_set_read_addr(rxDmaChan, ((u8 *)&pio->rxf[sm]) + 3, false);
	dma_channel_set_write_addr(rxDmaChan, rxBuf, false);
	dma_channel_set_transfer_count(rxDmaChan, 0xFFFFFFFFUL, true);
	rxPtr = rxBuf;

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

	// stop state machine
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

bool SerialPioHdx::setPinout(u8 pin, u8 _) {
	if (running) return false;
	this->pin = pin;
	return true;
}

bool SerialPioHdx::setFIFOSize(size_t size) {
	if (running) return false;
	// only accept powers of 2, don't accept zero size
	if (!size || (size & (size - 1)) != 0) return false;
	if (rxBuf != nullptr) free(rxBuf);
	rxBuf = (u8 *)aligned_alloc(size, size);
	if (rxBuf == nullptr) return false;
	rxBufSize = size;
	return true;
}
