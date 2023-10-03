
#include "EEPROM.h"
#include "EEPROMImpl.h"
#include "LittleFS.h"
#include "adc.h"
#include "blackbox.h"
#include "drivers/baro.h"
#include "drivers/elrs.h"
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
#include "pico/stdlib.h"
#include "pid.h"
#include "pins.h"
#include "pioasm/dshotx4.pio.h"
#include "serial.h"
#include "serialhandler/configurator.h"
#include <Arduino.h>
#include <vector>

#define SPI_GYRO spi0
#define SPI_OSD spi1
#define BARO_I2C i2c1
#define PROPS_OUT

extern ExpressLRS *ELRS;
