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
#include "PIO_DShot.h"
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
#include "git_version.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/resets.h"
#include "hardware/rtc.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "imu.h"
#include "modes.h"
#include "pico/stdlib.h"
#include "pid.h"
#include "pins.h"
#include "pioasm/speaker8bit.pio.h"
#include "ringbuffer.h"
#include "rtc.h"
#include "serial.h"
#include "serialhandler/4way.h"
#include "serialhandler/elrs.h"
#include "serialhandler/gps.h"
#include "serialhandler/msp.h"
#include "taskManager.h"
#include "unittest.h"
#include "utils/filters.h"
#include "utils/fixedPointInt.h"
#include "utils/quaternion.h"

#define SPI_GYRO spi0 // SPI for gyro
#define SPI_OSD spi0 // SPI for OSD
#define SPI_BARO spi0 // SPI for baro
#define SPI_SD spi1 // SPI for SD card
#define I2C_MAG i2c0 // I2C for magnetometer
#define PROPS_OUT

#define DEBUG_ON gpio_put(PIN_LED_DEBUG, 1);
#define DEBUG_OFF gpio_put(PIN_LED_DEBUG, 0);

#define ARRAYLEN(arr) (sizeof(arr) / sizeof(arr[0])) // Get the length of an array

extern ExpressLRS *ELRS; // global ELRS instance
#define DECODE_U2(buf) ((*(buf) & 0xFF) + (*((u8 *)(buf) + 1) << 8)) // Decode 2 bytes from a buffer into a 16-bit unsigned integer
#define DECODE_I2(buf) ((*(buf) & 0xFF) + ((*((u8 *)(buf) + 1)) << 8)) // Decode 2 bytes from a buffer into a 16-bit signed integer
u32 DECODE_U4(const u8 *buf); // Decode 4 bytes from a buffer into a 32-bit unsigned integer
i32 DECODE_I4(const u8 *buf); // Decode 4 bytes from a buffer into a 32-bit signed integer
f32 DECODE_R4(const u8 *buf); // Decode 4 bytes from a buffer into a 32-bit float
i64 DECODE_I8(const u8 *buf); // Decode 8 bytes from a buffer into a 64-bit signed integer
f64 DECODE_R8(const u8 *buf); // Decode 8 bytes from a buffer into a 64-bit float / double

enum class BootReason {
	POR, // Power-on reset
	CMD_REBOOT,
	CMD_BOOTLOADER,
	WATCHDOG
};
enum MspRebootMode {
	MSP_REBOOT_FIRMWARE = 0,
	MSP_REBOOT_BOOTLOADER_ROM,
	MSP_REBOOT_MSC,
	MSP_REBOOT_MSC_UTC,
	MSP_REBOOT_BOOTLOADER_FLASH
};

extern volatile u32 crashInfo[256]; // Crash info buffer (arbitrary data to be saved to EEPROM in case of a crash)
extern BootReason bootReason; // Reason for booting
extern BootReason rebootReason; // Reason for rebooting (can be set right before an intentional reboot, WATCHDOG otherwise)
extern u64 powerOnResetMagicNumber; // Magic number to detect power-on reset (0xdeadbeefdeadbeef)

#define FIRMWARE_NAME "Kolibri"
#define FIRMWARE_VERSION_MAJOR 0
#define FIRMWARE_VERSION_MINOR 1
#define FIRMWARE_VERSION_PATCH 0
#define RELEASE_SUFFIX "-dev"
#define xstr(a) str(a)
#define str(a) #a
#define FIRMWARE_VERSION_STRING xstr(FIRMWARE_VERSION_MAJOR) "." xstr(FIRMWARE_VERSION_MINOR) "." xstr(FIRMWARE_VERSION_PATCH) RELEASE_SUFFIX
