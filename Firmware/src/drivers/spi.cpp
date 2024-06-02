#include "global.h"

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

void initDefaultSpi() {
	spi_init(SPI_GYRO, 8000000);

	spi_set_format(SPI_GYRO, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
	gpio_set_function(PIN_DEFAULT_MOSI, GPIO_FUNC_SPI);
	gpio_set_function(PIN_DEFAULT_MISO, GPIO_FUNC_SPI);
	gpio_set_function(PIN_DEFAULT_SCK, GPIO_FUNC_SPI);
	gpio_init(PIN_GYRO_CS);
	gpio_set_dir(PIN_GYRO_CS, GPIO_OUT);
	gpio_put(PIN_GYRO_CS, 1);
	gpio_init(PIN_OSD_CS);
	gpio_set_dir(PIN_OSD_CS, GPIO_OUT);
	gpio_put(PIN_OSD_CS, 1);
	gpio_init(PIN_BARO_CS);
	gpio_set_dir(PIN_BARO_CS, GPIO_OUT);
	gpio_put(PIN_BARO_CS, 1);
	sleep_ms(2);
}