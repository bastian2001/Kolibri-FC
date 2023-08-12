#include <Arduino.h>
#include "pico/stdlib.h"
#include "pioasm/dshotx4.pio.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "pins.h"
#include "elapsedMillis.h"
#include "drivers/gyro.h"
#include <vector>
#include "elapsedMillis.h"
#include "drivers/elrs.h"
#include "drivers/speaker.h"
#include "pid.h"
#include "drivers/esc.h"
#include "drivers/osd.h"
#include "adc.h"
#include "hardware/adc.h"

#define SPI_GYRO spi0
#define SPI_OSD spi1

extern ExpressLRS *ELRS;
