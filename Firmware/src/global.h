#include <Arduino.h>
#include "pico/stdlib.h"
#include "pioasm/dshotx4.pio.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "pins.h"
#include "elapsedMillis.h"
#include "drivers/gyro.h"

#define SPI_GYRO spi0
