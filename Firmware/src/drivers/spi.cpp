#include "global.h"

#define READ_BASE 0x000 // used to indicate to the PIO that this byte is a read
#define WRITE_BASE 0x100 // used to indicate to the PIO that this byte is a write

// adapted from https://www.digikey.de/de/maker/projects/raspberry-pi-pico-rp2040-spi-example-with-micropython-and-cc/9706ea0cf3784ee98e35ff49188ee045
int regRead(spi_inst_t *spi, const uint cs, const u8 reg, u8 *buf, const u16 nbytes, const u16 delay, u8 dummy) {
	// Construct message (set ~W bit high)
	u8 msg = 0x80 | reg;

	// Read from register
	gpio_put(cs, 0);
	spi_write_blocking(spi, &msg, 1);
	if (dummy)
		spi_read_blocking(spi, 0, buf, 1); // +1 for dummy byte
	int num_bytes_read = spi_read_blocking(spi, 0, buf, nbytes);
	gpio_put(cs, 1);

	if (delay > 0)
		sleep_us(delay);
	return num_bytes_read;
}

int regRead(PIO pio, u8 sm, const uint cs, const u8 reg, u8 *buf, const u16 nbytes, const u16 delay, u8 dummy) {
	// Read from register
	gpio_put(cs, 0);
	pio_sm_clear_fifos(pio, sm);
	pio_sm_put_blocking(pio, sm, (WRITE_BASE | 0x80 | reg) << 23);
	pio_sm_get_blocking(pio, sm);

	if (dummy) {
		pio_sm_put_blocking(pio, sm, READ_BASE << 23);
		pio_sm_get_blocking(pio, sm); // drop dummy byte
	}

	int written = 0;
	int read = 0;
	while (read < nbytes) {
		if (!pio_sm_is_tx_fifo_full(pio, sm) && written < nbytes) {
			pio_sm_put_blocking(pio, sm, READ_BASE << 23);
			written++;
		}
		if (!pio_sm_is_rx_fifo_empty(pio, sm)) {
			buf[read++] = pio_sm_get_blocking(pio, sm);
		}
	}
	gpio_put(cs, 1);

	if (delay > 0)
		sleep_us(delay);
	return nbytes;
}

int regWrite(spi_inst_t *spi, const uint cs, const u8 reg, const u8 *buf, const u16 nbytes, const u16 delay) {
	// Write to register
	gpio_put(cs, 0);
	spi_write_blocking(spi, &reg, 1);
	int bytes_written = spi_write_blocking(spi, buf, nbytes);
	gpio_put(cs, 1);
	if (delay > 0)
		sleep_us(delay);
	return bytes_written;
}

int regWrite(PIO pio, u8 sm, const uint cs, const u8 reg, const u8 *buf, const u16 nbytes, const u16 delay) {
	// Write to register
	gpio_put(cs, 0);
	pio_sm_clear_fifos(pio, sm);
	pio_sm_put_blocking(pio, sm, (WRITE_BASE | reg) << 23);
	pio_sm_get_blocking(pio, sm);

	int written = 0;
	int read = 0;
	while (read < nbytes) {
		if (!pio_sm_is_tx_fifo_full(pio, sm) && written < nbytes) {
			pio_sm_put_blocking(pio, sm, (WRITE_BASE | buf[written++]) << 23);
		}
		if (!pio_sm_is_rx_fifo_empty(pio, sm)) {
			pio_sm_get_blocking(pio, sm);
			read++;
		}
	}

	gpio_put(cs, 1);
	if (delay > 0)
		sleep_us(delay);
	return nbytes;
}