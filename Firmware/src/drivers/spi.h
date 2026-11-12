/**
 * @file spi.h
 * @brief SPI helper function declarations for synchronous read/write operations (setup)
 *
 * Copyright (c) 2026 Kolibri-FC contributors
 *
 * This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 *
 * Kolibri-FC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kolibri-FC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
 */

#include "hardware/spi.h"
#include <Arduino.h>

/**
 * @brief Read a register from a SPI device
 *
 * @param spi SPI instance, spi0 or spi1
 * @param cs Chip select pin
 * @param reg Register address
 * @param buf Buffer to store the read data
 * @param nbytes Number of bytes to read, default is 1
 * @param dummy Send a dummy byte before reading, default is false
 * @return int
 */
int regRead(spi_inst_t *spi, const uint cs, const u8 reg, u8 *buf, const u16 nbytes = 1, u8 dummy = false);
/**
 * @brief Read a register from a halfduplex PIO SPI device
 *
 * @param pio PIO instance, pio0, pio1 or pio2
 * @param sm State machine number
 * @param cs Chip select pin
 * @param reg Register address
 * @param buf Buffer to store the read data
 * @param nbytes Number of bytes to read, default is 1
 * @param dummy Send a dummy byte before reading, default is false
 * @return int
 */
int regRead(PIO pio, u8 sm, const uint cs, const u8 reg, u8 *buf, const u16 nbytes = 1, u8 dummy = false);
/**
 * @brief Writes literally one byte to an SPI device. CS low => write => CS high
 *
 * @param spi SPI instance, spi0 or spi1
 * @param cs Chip select pin
 * @param data Byte to write
 */
void spiWriteByte(spi_inst_t *spi, const uint cs, const u8 data);
/**
 * @brief Writes multiple bytes to an SPI device, one byte per transfer. CS low => write => CS high for each byte
 *
 * @param spi SPI instance, spi0 or spi1
 * @param cs Chip select pin
 * @param data Buffer with the bytes to write
 * @param count Number of bytes to write
 */
void spiWriteSingleBytes(spi_inst_t *spi, const uint cs, const u8 *data, size_t count);
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
 * @brief Write a register to a halfduplex PIO SPI device
 *
 * @param pio PIO instance, pio0, pio1 or pio2
 * @param sm State machine number
 * @param cs Chip select pin
 * @param reg Register address
 * @param buf Buffer with the data to write
 * @param nbytes Number of bytes to write, default is 1
 * @param delay Delay in microseconds after the write, default is 0
 * @return int
 */
int regWrite(PIO pio, u8 sm, const uint cs, const u8 reg, const u8 *buf, const u16 nbytes = 1, const u16 delay = 0);
