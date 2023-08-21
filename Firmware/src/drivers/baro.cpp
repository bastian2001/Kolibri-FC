#include "global.h"

elapsedMillis baroStarted = 0;
bool baroMeasuring = false;

int16_t baroASL = 0;
float baroPres = 0;
uint8_t baroTemp = 0;
bool baroDataReady = false;

void initBaro()
{
	i2c_init(BARO_I2C, 1700000);
	gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
	gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
	gpio_pull_up(PIN_SDA);
	gpio_pull_up(PIN_SCL);
}

void startBaroMeasure()
{
	uint8_t data;
	data = 0xAC;
	i2c_write_blocking(BARO_I2C, BARO_I2C_ADDR, &data, 1, false);
	baroStarted = 0;
	baroMeasuring = true;
}

bool checkBaroFinished()
{
	if (!baroMeasuring || baroStarted < 203)
		return false;
	uint8_t data;
	i2c_read_blocking(BARO_I2C, BARO_I2C_ADDR, &data, 1, false);
	baroDataReady = data == 4;
	baroMeasuring = !baroDataReady;
	return baroDataReady;
}

void baroUpdate()
{
	if (!baroDataReady)
		return;
	uint8_t data[6];
	if (i2c_read_blocking(BARO_I2C, BARO_I2C_ADDR, data, 6, false) == PICO_ERROR_GENERIC)
		return;
	int32_t presRaw = data[1] << 16 | data[2] << 8 | data[3];
	int32_t tempRaw = data[4] << 8 | data[5];
	baroPres = 10 * (.0000059605f * ((float)presRaw - 1677722.f) + 20);
	baroTemp = (double)tempRaw / 65536. * 190 - 40;
	if (baroPres > 845.f)
	{
		baroASL = -8.994f * baroPres + 9114.f; // linearised to avoid the logarithms on the MCU
	}
	else if (baroPres > 633.f)
	{
		baroASL = -11.565f * baroPres + 11284.f;
	}
	else
	{
		baroASL = -16.087f * baroPres + 14146.f;
	}
	baroDataReady = false;
}