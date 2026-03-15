#include "global.h"
#include "pioasm/uart_rx.pio.h"
#include "pioasm/uart_tx.pio.h"

static u8 programOffsetsRx[NUM_PIOS];
static u8 programOffsetsTx[NUM_PIOS];
static bool offsetsSet;

static std::list<SerialPioConfig *> configs;

SerialPio::SerialPio(PIO pioTx, PIO pioRx, i8 smTx, i8 smRx)
	: pioTx(pioTx),
	  pioRx(pioRx),
	  beginSmTx(smTx),
	  beginSmRx(smRx) {
	if (!offsetsSet) {
		// somehow "static u8 programOffsets[] = {[0 ...(NUM_PIOS - 1)] = 255};" does not work...
		for (int i = 0; i < NUM_PIOS; i++) {
			programOffsetsTx[i] = 255;
			programOffsetsRx[i] = 255;
		}
		offsetsSet = true;
	}
}

SerialPio::~SerialPio() {
	if (running) {
		end();
	}
}

int SerialPio::available() {
	return pio_sm_get_rx_fifo_level(pioRx, smRx);
}
int SerialPio::read() {
	if (!running) {
		return -1;
	}
	if (peekVal != -1) {
		int i = peekVal;
		peekVal = -1;
		return i;
	}
	if (pio_sm_is_rx_fifo_empty(pioRx, smRx)) {
		return -1;
	}
	return pio_sm_get(pioRx, smRx) >> 24;
}
int SerialPio::peek() {
	if (!running) {
		return -1;
	}
	if (peekVal != -1) {
		return peekVal;
	}
	if (pio_sm_is_rx_fifo_empty(pioRx, smRx)) {
		return -1;
	}
	return peekVal = pio_sm_get(pioRx, smRx) >> 24;
}

size_t SerialPio::write(uint8_t c) {
	if (!running) {
		return 0;
	}
	u32 data = 0x100 | c;
	data <<= 1;
	pio_sm_put_blocking(pioTx, smTx, c);
	return 1;
}
size_t SerialPio::write(const uint8_t *buffer, size_t size) {
	if (!running) {
		return 0;
	}
	for (size_t i = 0; i < size; i++) {
		u32 data = 0x100 | buffer[i];
		data <<= 1;
		pio_sm_put_blocking(pioTx, smTx, data);
		rp2040.wdt_reset();
	}
	return size;
}
void SerialPio::flush() {
	while (!pio_sm_is_tx_fifo_empty(pioTx, smTx)) {
		tight_loop_contents();
	}
	return;
}
int SerialPio::availableForWrite() {
	return 8 - pio_sm_get_tx_fifo_level(pioTx, smTx);
}

void SerialPio::begin() {
	if (running) return;
	if (!pioTx || !pioRx || !baudrate || pinRx == 255 || pinTx == 255) {
		Serial.println("Assign values to UART first");
		return;
	}

	pioIndexTx = pio_get_index(pioTx);
	pioIndexRx = pio_get_index(pioRx);
	if (pioIndexTx >= NUM_PIOS || pioIndexRx >= NUM_PIOS) {
		Serial.println("Invalid PIO instance");
		return;
	}

	// claim the state machines
	if (beginSmTx < 0) {
		smTx = pio_claim_unused_sm(pioTx, false);
		if (smTx < 0) {
			Serial.println("No free TX state machine available");
			return;
		}
	} else {
		if (pio_sm_is_claimed(pioTx, beginSmTx)) {
			Serial.println("TX state machine already claimed");
			return;
		}
		pio_sm_claim(pioTx, beginSmTx);
		smTx = beginSmTx;
	}
	if (beginSmRx < 0) {
		smRx = pio_claim_unused_sm(pioRx, false);
		if (smRx < 0) {
			Serial.println("No free RX state machine available");
			pio_sm_unclaim(pioTx, smTx);
			return;
		}
	} else {
		if (pio_sm_is_claimed(pioRx, beginSmRx)) {
			Serial.println("RX state machine already claimed");
			pio_sm_unclaim(pioTx, smTx);
			return;
		}
		pio_sm_claim(pioRx, beginSmRx);
		smRx = beginSmRx;
	}

	// check and load program
	bool addTx = false;
	bool addRx = false;
	if (programOffsetsTx[pioIndexTx] == 255) {
		if (!pio_can_add_program(pioTx, &uart_tx_program)) {
			Serial.println("TX instruction memory");
			pio_sm_unclaim(pioTx, smTx);
			pio_sm_unclaim(pioRx, smRx);
			return;
		}
		addRx = true;
	}
	if (programOffsetsRx[pioIndexRx] == 255) {
		if (!pio_can_add_program(pioRx, &uart_rx_program)) {
			Serial.println("RX instruction memory");
			pio_sm_unclaim(pioTx, smTx);
			pio_sm_unclaim(pioRx, smRx);
			return;
		}
		addRx = true;
	}
	if (addTx) {
		programOffsetsTx[pioIndexTx] = pio_add_program(pioTx, &uart_tx_program);
	}
	if (addRx) {
		programOffsetsRx[pioIndexRx] = pio_add_program(pioRx, &uart_rx_program);
	}

	// start state machines
	uart_tx_program_init(pioTx, smTx, programOffsetsTx[pioIndexTx], pinTx, baudrate);
	uart_rx_program_init(pioRx, smRx, programOffsetsRx[pioIndexRx], pinRx, baudrate);

	pioConfig.txPio = pioIndexTx;
	pioConfig.rxPio = pioIndexRx;
	configs.push_back(&pioConfig);

	running = true;
}
void SerialPio::begin(unsigned long baudrate) {
	if (running) return;

	this->baudrate = baudrate;
	begin();
}
void SerialPio::begin(unsigned long baudrate, uint16_t _config) {
	begin(baudrate);
}
void SerialPio::end() {
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
	uart_rx_program_end(pioRx, smRx, pinRx);
	uart_tx_program_end(pioTx, smTx, pinTx);

	// search for others, and if another serial on the same pio still exists, don't delete programs
	bool delRx = true;
	bool delTx = true;
	for (auto it = configs.begin(); it != configs.end();) {
		if ((*it)->rxPio == pioIndexRx) delRx = false;
		if ((*it)->txPio == pioIndexTx) delTx = false;
	}
	if (delRx) {
		pio_remove_program(pioRx, &uart_rx_program, programOffsetsRx[pioIndexRx]);
		programOffsetsRx[pioIndexRx] = 255;
	}
	if (delTx) {
		pio_remove_program(pioTx, &uart_tx_program, programOffsetsTx[pioIndexTx]);
		programOffsetsTx[pioIndexTx] = 255;
	}

	// unclaim the state machines
	pio_sm_unclaim(pioRx, smRx);
	pio_sm_unclaim(pioTx, smTx);
}

SerialPio::operator bool() {
	return running;
}

bool SerialPio::setPinout(u8 tx, u8 rx) {
	if (running) return false;
	this->pinTx = tx;
	this->pinRx = rx;
	return true;
}

bool SerialPio::setFIFOSize(size_t size) {
	if (running) return false;
	// only accept powers of 2, don't accept zero size
	if (!size || (size & (size - 1)) != 0) return false;
	rxFifoSize = size;
	// TODO actually do something with this
	return true;
}
