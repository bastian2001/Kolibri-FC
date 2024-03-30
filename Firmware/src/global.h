#define LITTLEFS_BB 0
#define SD_BB 1
#define BLACKBOX_STORAGE SD_BB
#include "typedefs.h"
#ifdef USE_TINYUSB
#include <Adafruit_TinyUSB.h>
#endif
#include <Arduino.h>

#if BLACKBOX_STORAGE == LITTLEFS_BB
#include "LittleFS.h"
#elif BLACKBOX_STORAGE == SD_BB
#include "SDFS.h"
#endif
#include "EEPROM.h"
#include "EEPROMImpl.h"
#include "adc.h"
#include "blackbox.h"
#include "drivers/baro.h"
#include "drivers/esc.h"
#include "drivers/gyro.h"
#include "drivers/mag.h"
#include "drivers/osd.h"
#include "drivers/speaker.h"
#include "drivers/spi.h"
#include "elapsedMillis.h"
#include "filters.h"
#include "fixedPointInt.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
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
#include "pioasm/bidir_dshot_x1.pio.h"
#include "pioasm/dshotx4.pio.h"
#include "pioasm/speaker8bit.pio.h"
#include "quaternion.h"
#include "ringbuffer.h"
#include "serial.h"
#include "serialhandler/configurator.h"
#include "serialhandler/elrs.h"
#include "serialhandler/gps.h"
#include "taskManager.h"

#define SPI_GYRO spi0
#define SPI_OSD spi0
#define SPI_BARO spi0
#define SPI_SD spi1
#define I2C_MAG i2c0
#define PROPS_OUT

extern ExpressLRS *ELRS;
#define DECODE_RU1_3(buf) ((*(buf) & 0x1F) << ((*(u8 *)(buf)) >> 5))
#define DECODE_U2(buf) ((*(buf) & 0xFF) + (*((u8 *)(buf) + 1) << 8))
#define DECODE_RU2_5(buf) ((*(buf) + ((*((buf) + 1) & 0x7) * << 8)) << ((*((u8 *)(buf) + 1)) >> 3))
#define DECODE_I2(buf) ((*(buf) & 0xFF) + ((*((u8 *)(buf) + 1)) << 8))
u32 DECODE_U4(const u8 *buf);
i32 DECODE_I4(const u8 *buf);
f32 DECODE_R4(const u8 *buf);
i64 DECODE_I8(const u8 *buf);
f64 DECODE_R8(const u8 *buf);

extern volatile u32 crashInfo[256];
