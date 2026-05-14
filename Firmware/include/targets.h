/**
 * @file targets.h
 * @brief Pinouts for targets, as well as some other target-specific definitions.
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

#pragma once

#define SD_BB 0
#define FLASH_BB 1

#define HW_V5 5
#define HW_V6 6

#define GYRO_BMI270 1
#define GYRO_ICM42688P 2
#define GYRO_LSM6DSV 3

#define BARO_SPL006 1
#define BARO_LPS22 2

#define MAG_HMC5883L 1
#define MAG_QMC5883L 2

#if HW_VARIANT == HW_V5

#define PIN_TX0 0
#define PIN_RX0 1
#define PIN_TX1 8
#define PIN_RX1 9
#define PIN_TX2 10
#define PIN_RX2 11

#define PIN_OSD_MISO 12
#define PIN_OSD_CS 13
#define PIN_OSD_SCLK 14
#define PIN_OSD_MOSI 15

#define PIN_GYRO_INT1 16
#define PIN_GYRO_CS 17
#define PIN_GYRO_SCLK 18
#define PIN_GYRO_SDX 19

#define PIN_SD_DAT 2
#define PIN_SD_CMD 6
#define PIN_SD_SCLK 7

#define PIN_SDA0 24
#define PIN_SCL0 25

#define PIN_ADC_CURRENT 28
#define PIN_ADC_VOLTAGE 29

#define PIN_MOTORS 20 // pin at which the motor outputs start

#define PIN_SPEAKER 26
#define PIN_LEDS 27

#define BLACKBOX_STORAGE SD_BB

#define I2C_MAG i2c0 // I2C for magnetometer

#define SPI_OSD spi1 // SPI for OSD

#define GYRO_HALFDUPLEX_SPI

#define PIO_ESC pio0 // uses all 4 SMs
#define PIO_SDIO pio1 // automatically assigned, claims all 4 SMs and 32 instructions
#define PIO_LED pio2 // 1 SM, 4 instructions
#define PIO_GYRO_SPI pio2 // 1 SM, 5 instructions

#define HW_MAG MAG_QMC5883L
#define HW_BARO BARO_LPS22
#define HW_GYRO GYRO_BMI270

#define TARGET_IDENTIFIER "KD05"
#define TARGET_FULL_NAME "Kolibri Dev v0.5"

inline constexpr char pinLabels[30][6]{"T0", "R0", "", "", "", "", "", "", "T1", "R1", "T2", "R2", "", "", "", "", "", "", "", "", "M1", "M2", "M3", "M4", "SDA", "SCL", "Spk", "LEDx", "", ""};

#define PID_FREQ 3200

#elif HW_VARIANT == HW_V6

#define PIN_TX0 0
#define PIN_RX0 1
#define PIN_TX1 8
#define PIN_RX1 9
#define PIN_TX2 10
#define PIN_RX2 11

#define PIN_OSD_MISO 12
#define PIN_OSD_CS 13
#define PIN_OSD_SCLK 14
#define PIN_OSD_MOSI 15

#define PIN_GYRO_MISO 4
#define PIN_GYRO_CS 5
#define PIN_GYRO_SCLK 2
#define PIN_GYRO_MOSI 3
#define PIN_GYRO_INT1 6
#define PIN_GYRO_CLKIN 7

#define PIN_FLASH_IO_BASE 17
#define PIN_FLASH_SCLK 16
#define PIN_FLASH_CS 19

#define PIN_SDA0 24
#define PIN_SCL0 25

#define PIN_ADC_VOLTAGE 29

#define PIN_MOTORS 20 // pin at which the motor outputs start

#define PIN_SPEAKER 26
#define PIN_LEDS 27
#define PIN_DCDC_EN 28

#define BLACKBOX_STORAGE FLASH_BB
#define I2C_MAG i2c0 // I2C for magnetometer
#define SPI_OSD spi1 // SPI for OSD
#define SPI_GYRO spi0

#define PIO_ESC pio0 // uses all 4 SMs
#define PIO_EXT_SPI_BB pio1 // 1 SM, 2 instructions, at least right now (may become more if parallel transfer is wanted in the future)
#define PIO_LED pio1 // 1 SM, 4 instructions

#define HW_MAG MAG_QMC5883L
#define HW_BARO BARO_LPS22
#define HW_GYRO GYRO_ICM42688P

#define TARGET_IDENTIFIER "KD06"
#define TARGET_FULL_NAME "Kolibri Dev v0.6"

inline constexpr char pinLabels[30][6]{"T0", "R0", "", "", "", "", "", "", "T1", "R1", "T2", "R2", "", "", "", "", "", "", "", "", "M1", "M2", "M3", "M4", "SDA", "SCL", "Spk", "LEDx", "10V", ""};

#define PID_FREQ 8000

#elif HW_VARIANT == HW_OPENFC_ECO

#define PIN_TX0 0
#define PIN_RX0 1
#define PIN_TX1 22
#define PIN_RX1 21
#define PIN_TX2 2
#define PIN_RX2 3

#define PIN_OSD_MISO 8
#define PIN_OSD_CS 9
#define PIN_OSD_SCLK 10
#define PIN_OSD_MOSI 27

#define PIN_GYRO_MISO 20
#define PIN_GYRO_CS 14
#define PIN_GYRO_SCLK 18
#define PIN_GYRO_MOSI 19
#define PIN_GYRO_INT1 13
#define PIN_GYRO_CLKIN 15

#define PIN_FLASH_IO_BASE 43
#define PIN_FLASH_SCLK 42
#define PIN_FLASH_CS 46

#define PIN_SDA0 16
#define PIN_SCL0 17

#define PIN_ADC_VOLTAGE 41

#define PIN_MOTORS 28 // pin at which the motor outputs start

#define PIN_SPEAKER 7
#define PIN_LEDS 23
#define PIN_DCDC_EN 11

#define BLACKBOX_STORAGE FLASH_BB
#define I2C_MAG i2c0 // I2C for magnetometer
#define SPI_OSD spi1 // SPI for OSD
#define SPI_GYRO spi0

#define PIO_ESC pio0 // uses all 4 SMs
#define PIO_EXT_SPI_BB pio1 // 1 SM, 2 instructions, at least right now (may become more if parallel transfer is wanted in the future)
#define PIO_LED pio1 // 1 SM, 4 instructions

#define HW_MAG MAG_QMC5883L
#define HW_BARO BARO_LPS22
#define HW_GYRO GYRO_LSM6DSV

#define TARGET_IDENTIFIER "OFEC"
#define TARGET_FULL_NAME "OpenFC Eco"

inline constexpr char pinLabels[30][6]{"T0", "R0", "", "", "", "", "", "", "T1", "R1", "T2", "R2", "", "", "", "", "", "", "", "", "M1", "M2", "M3", "M4", "SDA", "SCL", "Spk", "LEDx", "10V", ""};

#define PID_FREQ 7680

#endif

#ifdef SPI_GYRO
#define SPI_GYRO_HW spi_get_hw(SPI_GYRO)
#endif
