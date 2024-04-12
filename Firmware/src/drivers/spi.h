#include "Arduino.h"
#include "hardware/spi.h"

/**
 * @brief Read a register from a SPI device
 *
 * @param spi SPI instance, spi0 or spi1
 * @param cs Chip select pin
 * @param reg Register address
 * @param buf Buffer to store the read data
 * @param nbytes Number of bytes to read, default is 1
 * @param delay Delay in microseconds after the read, default is 0
 * @param dummy Send a dummy byte before reading, default is true
 * @return int
 */
int regRead(spi_inst_t *spi, const uint cs, const u8 reg, u8 *buf, const u16 nbytes = 1, const u16 delay = 0, u8 dummy = true);
/**
 * @brief Write a register to a SPI device
 *
 * @param spi SPI instance, spi0 or spi1
 * @param cs Chip select pin
 * @param reg Register address
 * @param buf Buffer with the data to write
 * @param nbytes Number of bytes to write, default is 1
 * @param delay Delay in microseconds after the write, default is 0
 * @return int
 */
int regWrite(spi_inst_t *spi, const uint cs, const u8 reg, const u8 *buf, const u16 nbytes = 1, const u16 delay = 0);

/**
 * @brief Initialize the default SPI bus
 *
 * @details initializes PIN_GYRO_CS, PIN_OSD_CS and PIN_BARO_CS as output pins
 * initializes SPI_GYRO bus with 8 bits, CPOL=0, CPHA=0, MSB first and 8MHz
 */
void initDefaultSpi();