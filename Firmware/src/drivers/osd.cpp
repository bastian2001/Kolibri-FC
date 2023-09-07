#include "global.h"

int regRead(spi_inst_t *spi, const uint cs, const uint8_t reg, uint8_t *buf, const uint16_t nbytes = 1, const uint16_t delay = 0, uint8_t dummy = true);
int regWrite(spi_inst_t *spi, const uint cs, const uint8_t reg, const uint8_t *buf, const uint16_t nbytes = 1, const uint16_t delay = 0);

uint8_t osdReady = false;

elapsedMillis osdTimer = 0;

uint8_t drawIterator = 0;

uint8_t elemPositions[OSD_MAX_ELEM][2] = {0}; // up to OSD_MAX_ELEM elements can be shown, each element has 2 bytes for x and y position, the MSB of x is used as an updated flag, the MSB of y is used as a visible flag

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

void disableOSD()
{
	uint8_t data;
	regRead(SPI_OSD, PIN_OSD_CS, (uint8_t)OSDReg::VM0, &data, 1, 0, 0);
	data &= ~0b00001000;
	regWrite(SPI_OSD, PIN_OSD_CS, (uint8_t)OSDReg::VM0, &data);
}

void enableOSD()
{
	uint8_t data;
	regRead(SPI_OSD, PIN_OSD_CS, (uint8_t)OSDReg::VM0, &data, 1, 0, 0);
	data |= 0b00001000;
	regWrite(SPI_OSD, PIN_OSD_CS, (uint8_t)OSDReg::VM0, &data);
}

void updateCharacter(uint8_t cmAddr, uint8_t data[54])
{
	disableOSD();
	delayMicroseconds(100);
	regWrite(SPI_OSD, PIN_OSD_CS, (uint8_t)OSDReg::CMAH, &cmAddr);
	for (int i = 0; i < 54; i++)
	{
		regWrite(SPI_OSD, PIN_OSD_CS, (uint8_t)OSDReg::CMAL, (uint8_t *)&i);
		regWrite(SPI_OSD, PIN_OSD_CS, (uint8_t)OSDReg::CMDI, &data[i]);
	}
	uint8_t scratch = 0b10100000;
	regWrite(SPI_OSD, PIN_OSD_CS, (uint8_t)OSDReg::CMM, &scratch);
	delay(15);
	enableOSD();
}

void placeElem(OSDElem elem, uint8_t x, uint8_t y)
{
	elemPositions[(uint8_t)elem][0] = (elemPositions[(uint8_t)elem][0] & 0x80) | (x & 0x7F);
	elemPositions[(uint8_t)elem][1] = (elemPositions[(uint8_t)elem][1] & 0x80) | (y & 0x7F);
}

void updateElem(OSDElem elem, const char *str)
{
	uint8_t len = strlen(str);
	if (len > 16)
		len = 16;
	memcpy(elemData[(uint8_t)elem], str, len);
	elemPositions[(uint8_t)elem][0] |= 0x80;
}

void enableElem(OSDElem elem)
{
	elemPositions[(uint8_t)elem][1] |= 0x80;
}
void disableElem(OSDElem elem)
{
	elemPositions[(uint8_t)elem][1] &= ~0x80;
}

void drawElem(uint8_t elem)
{
	elemPositions[elem][0] &= ~0x80; // clear updated flag
	// draw element using the elemData array
	uint16_t pos = (uint16_t)(elemPositions[elem][0] & 0x7F) + (uint16_t)(elemPositions[elem][1] & 0x7F) * OSD_WIDTH;
	for (int i = 0; i < 16; i++)
	{
		if (!elemData[elem][i])
			break;
		pos &= 0x1FF;
		uint8_t posLow = pos & 0xFF;
		uint8_t posHigh = pos >> 8;
		regWrite(SPI_OSD, PIN_OSD_CS, (uint8_t)OSDReg::DMAH, &posHigh);
		regWrite(SPI_OSD, PIN_OSD_CS, (uint8_t)OSDReg::DMAL, &posLow);
		regWrite(SPI_OSD, PIN_OSD_CS, (uint8_t)OSDReg::DMDI, &elemData[elem][i]);
		pos++;
	}
}

void osdLoop()
{
	if (osdReady && (gyroUpdateFlag & 1))
	{
		gyroUpdateFlag &= ~1;
		// cycle through all drawn elements, and update one per gyro cycle
		uint8_t drawIteratorStart = drawIterator;
		while (1)
		{
			drawIterator++;
			if (drawIterator >= OSD_MAX_ELEM)
				drawIterator = 0;
			if ((elemPositions[drawIterator][0] & 0x80) && (elemPositions[drawIterator][1] & 0x80)) // if updated and enabled
			{
				drawElem(drawIterator);
				break;
			}
			if (drawIterator == drawIteratorStart)
				break;
		}
	}
	else if (!osdReady && (gyroUpdateFlag & 1))
	{
		gyroUpdateFlag &= ~1;
		if (osdTimer > 1000)
		{
			// gyro likely ready, check registers
			uint8_t data = 0;
			osdTimer = 0;
			regRead(SPI_OSD, PIN_OSD_CS, (uint8_t)OSDReg::STAT, &data, 1, 0, 0);
			if (data && !(data & 0b01100000))
			{
				osdReady = true;
			}
			if (data & 1)
			{
				data = 0b01001100; // dont care, pal, autosync (2 bits), enable osd, sync at next vsync, don't reset, enable output
				regWrite(SPI_OSD, PIN_OSD_CS, (uint8_t)OSDReg::VM0, &data);
			}
			if (data & 2)
			{
				data = 0b00001100; // dont care, ntsc, autosync (2 bits), enable osd, sync at next vsync, don't reset, enable output
				regWrite(SPI_OSD, PIN_OSD_CS, (uint8_t)OSDReg::VM0, &data);
			}
		}
	}
}