#include "global.h"

int regRead(spi_inst_t *spi, const uint cs, const uint8_t reg, uint8_t *buf, const uint16_t nbytes = 1, const uint16_t delay = 0, uint8_t dummy = true);
int regWrite(spi_inst_t *spi, const uint cs, const GyroReg reg, const uint8_t *buf, const uint16_t nbytes = 1, const uint16_t delay = 0);

uint8_t osdReady = false;

elapsedMillis osdTimer = 0;

uint8_t elemPositions[OSD_MAX_ELEM][2] = {0}; // up to OSD_MAX_ELEM elements can be shown, each element has 2 bytes for x and y position, the MSB of x is used as an updated flag

uint8_t elemData[OSD_MAX_ELEM][16] = {0}; // up to OSD_MAX_ELEM elements can be shown, each element has 16 bytes for data

void osdInit()
{
	spi_init(SPI_OSD, 8000000);
	spi_set_format(SPI_OSD, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
	gpio_set_function(PIN_OSD_MISO, GPIO_FUNC_SPI);
	gpio_set_function(PIN_OSD_MOSI, GPIO_FUNC_SPI);
	gpio_set_function(PIN_OSD_SCK, GPIO_FUNC_SPI);
	gpio_init(PIN_OSD_CS);
	gpio_set_dir(PIN_OSD_CS, GPIO_OUT);

	uint8_t data = 0;
	osdTimer = 0;
	Serial.println("OSD inited");
}

void placeElem(OSDElem elem, uint8_t x, uint8_t y)
{
	elemPositions[(uint8_t)elem][0] = x;
	elemPositions[(uint8_t)elem][1] = y;
}

void updateElem(OSDElem elem, const char *str)
{
	uint8_t len = strlen(str);
	if (len > 16)
		len = 16;
	memcpy(elemData[(uint8_t)elem], str, len);
	elemData[(uint8_t)elem][len] = 0;
	elemPositions[(uint8_t)elem][0] |= 0x80;
}

void osdLoop()
{
	if (osdReady && (gyroUpdateFlag & 1))
	{
		gyroUpdateFlag &= ~1;
		// always update one element at a time to not block the gyro/PID
	}
	else if (!osdReady && (gyroUpdateFlag & 1))
	{
		gyroUpdateFlag &= ~1;
		if (osdTimer > 60)
		{
			// gyro likely ready, check registers
			uint8_t data = 0;
			osdTimer = 0;
			regRead(SPI_OSD, PIN_OSD_CS, (uint8_t)OSDReg::VM1, &data, 1, 0, 0);
			if (data && !(data & 0b01100000))
			{
				osdReady = true;
			}
		}
	}
}