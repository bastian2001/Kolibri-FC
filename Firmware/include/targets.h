#include "typedefs.h"

#define SD_BB 0
#define FLASH_BB 1

#define HW_V5 5
#define HW_V6 6

#if HW_VARIANT == HW_V5

#define PIN_TX0 0 // UART0 / Serial1
#define PIN_RX0 1 // UART0 / Serial1
#define PIN_TX1 8 // UART1 / Serial2
#define PIN_RX1 9 // UART1 / Serial2
#define PIN_TX2 10 // SoftwareSerial
#define PIN_RX2 11 // SoftwareSerial

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
#define PIO_SDIO pio0 // uses 2 SMs but basically all instructions
#define I2C_MAG i2c0 // I2C for magnetometer
#define SPI_OSD spi1 // SPI for OSD
#define GYRO_HALFDUPLEX_SPI
#define PIO_GYRO_SPI pio2 // 1 SM, 5 instructions
#define GYRO_BMI270

#elif HW_VARIANT == HW_V6

#define PIN_TX0 0 // UART0 / Serial1
#define PIN_RX0 1 // UART0 / Serial1
#define PIN_TX1 8 // UART1 / Serial2
#define PIN_RX1 9 // UART1 / Serial2
#define PIN_TX2 10 // SoftwareSerial
#define PIN_RX2 11 // SoftwareSerial

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
#define PIO_EXT_SPI_BB pio0
#define I2C_MAG i2c0 // I2C for magnetometer
#define SPI_OSD spi1 // SPI for OSD
#define SPI_GYRO spi0
#define GYRO_ICM42688P
#endif

#ifdef SPI_GYRO
#define SPI_GYRO_HW spi_get_hw(SPI_GYRO)
#endif
