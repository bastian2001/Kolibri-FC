#include <Arduino.h>

#define PIN_TX0 0
#define PIN_RX0 1
#define PIN_SDA 2
#define PIN_SCL 3
#define PIN_SPEAKER 7
#define PIN_TX1 8
#define PIN_RX1 9
#define PIN_LED_DEBUG 10
#define PIN_LED_ACTIVITY 11
enum OSD_SPI : uint8_t
{
	PIN_OSD_MISO = 12,
	PIN_OSD_CS,
	PIN_OSD_SCK,
	PIN_OSD_MOSI,
};
enum GYRO_SPI : uint8_t
{
	PIN_GYRO_MISO = 16,
	PIN_GYRO_CS,
	PIN_GYRO_SCK,
	PIN_GYRO_MOSI,
};
#define PIN_MOTORS 20
enum class MOTOR : uint8_t
{
	RR = 0,
	RL,
	FR,
	FL,
};
#define PIN_GYRO_INT1 26
#define PIN_GYRO_INT2 27
#define PIN_ADC_CURRENT 28
#define PIN_ADC_VOLTAGE 29