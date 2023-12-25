
#include "EEPROM.h"
#include "EEPROMImpl.h"
#include "LittleFS.h"
#include "adc.h"
#include "blackbox.h"
#include "drivers/baro.h"
#include "drivers/esc.h"
#include "drivers/gyro.h"
#include "drivers/osd.h"
#include "drivers/speaker.h"
#include "elapsedMillis.h"
#include "fixedPointInt.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "imu.h"
#include "modes.h"
#include "pico/stdlib.h"
#include "pid.h"
#include "pins.h"
#include "pioasm/dshotx4.pio.h"
#include "quaternion.h"
#include "ringbuffer.h"
#include "serial.h"
#include "serialhandler/configurator.h"
#include "serialhandler/elrs.h"
#include "serialhandler/gps.h"
#include <Arduino.h>
// #include <Adafruit_TinyUSB.h>

#define SPI_GYRO spi0
#define SPI_OSD spi1
#define BARO_I2C i2c1
#define PROPS_OUT

extern ExpressLRS *ELRS;
#define DECODE_RU1_3(buf) ((*(buf) & 0x1F) << ((*(uint8_t *)(buf)) >> 5))
#define DECODE_U2(buf) ((*(buf) & 0xFF) + (*((uint8_t *)(buf) + 1) << 8))
#define DECODE_RU2_5(buf) ((*(buf) + ((*((buf) + 1) & 0x7) * << 8)) << ((*((uint8_t *)(buf) + 1)) >> 3))
#define DECODE_I2(buf) ((*(buf) & 0xFF) + ((*((int8_t *)(buf) + 1)) << 8))
uint32_t DECODE_U4(const uint8_t *buf);
int32_t DECODE_I4(const uint8_t *buf);
float DECODE_R4(const uint8_t *buf);
int64_t DECODE_I8(const uint8_t *buf);
double DECODE_R8(const uint8_t *buf);

extern uint32_t crashInfo[256];