#include "typedefs.h"

#define PIN_TX0 0
#define PIN_RX0 1
#define PIN_GYRO_CS 6
#define PIN_SPEAKER 7
#define PIN_TX1 8
#define PIN_RX1 9
#define PIN_LED_DEBUG 10
#define PIN_LED_ACTIVITY 11
#define PIN_OSD_CS 13
enum DEFAULT_SPI : u8 {
	PIN_DEFAULT_MISO = 16,
	PIN_DEFAULT_SCK  = 18,
	PIN_DEFAULT_MOSI,
};
#define PIN_MOTORS 20
enum class MOTOR : u8 {
	RR = 0,
	FR,
	RL,
	FL,
};
#define PIN_SDA0 24
#define PIN_SCL0 25
#define PIN_GYRO_INT1 26
#define PIN_BARO_CS 27
#define PIN_ADC_CURRENT 28
#define PIN_ADC_VOLTAGE 29
enum SD_SPI : u8 {
	PIN_SD_MISO = 12,
	PIN_SD_SCK  = 14,
	PIN_SD_MOSI,
	PIN_SD_CS = 17
};