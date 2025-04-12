#include "global.h"

u8 osdReady = false;

elapsedMillis osdTimer = 0;

u8 drawIterator = 0;

u8 elemPositions[OSD_MAX_ELEM][2] = {0}; // up to OSD_MAX_ELEM elements can be shown, each element has 2 bytes for x and y position, the MSB of x is used as an updated flag, the MSB of y is used as a visible flag

u8 elemData[OSD_MAX_ELEM][16] = {0}; // up to OSD_MAX_ELEM elements can be shown, each element has 16 bytes for data

void osdInit() {
	osdTimer = 0;

	spi_init(SPI_OSD, 8000000);

	spi_set_format(SPI_OSD, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
	gpio_set_function(PIN_OSD_MOSI, GPIO_FUNC_SPI);
	gpio_set_function(PIN_OSD_MISO, GPIO_FUNC_SPI);
	gpio_set_function(PIN_OSD_SCLK, GPIO_FUNC_SPI);
	gpio_init(PIN_OSD_CS);
	gpio_set_dir(PIN_OSD_CS, GPIO_OUT);
	gpio_put(PIN_OSD_CS, 1);
	sleep_ms(2);
}

void disableOSD() {
	u8 data;
	regRead(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::VM0, &data, 1, 0, 0);
	data &= ~0b00001000;
	regWrite(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::VM0, &data);
}

void enableOSD() {
	u8 data;
	regRead(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::VM0, &data, 1, 0, 0);
	data |= 0b00001000;
	regWrite(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::VM0, &data);
}

void updateCharacter(u8 cmAddr, u8 data[54]) {
	disableOSD();
	sleep_us(100);
	regWrite(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::CMAH, &cmAddr);
	for (int i = 0; i < 54; i++) {
		regWrite(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::CMAL, (u8 *)&i);
		regWrite(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::CMDI, &data[i]);
	}
	u8 scratch = 0b10100000;
	regWrite(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::CMM, &scratch);
	sleep_ms(15);
	enableOSD();
}

void placeElem(OSDElem elem, u8 x, u8 y) {
	elemPositions[(u8)elem][0] = (elemPositions[(u8)elem][0] & 0x80) | (x & 0x7F);
	elemPositions[(u8)elem][1] = (elemPositions[(u8)elem][1] & 0x80) | (y & 0x7F);
}

void updateElem(OSDElem elem, const char *str) {
	u8 len = strlen(str);
	if (len > 16)
		len = 16;
	memcpy(elemData[(u8)elem], str, len);
	elemPositions[(u8)elem][0] |= 0x80;
}

void enableElem(OSDElem elem) {
	elemPositions[(u8)elem][1] |= 0x80;
}
void disableElem(OSDElem elem) {
	elemPositions[(u8)elem][1] &= ~0x80;
}

void drawElem(u8 elem) {
	elemPositions[elem][0] &= ~0x80; // clear updated flag
	// draw element using the elemData array
	u16 pos = (u16)(elemPositions[elem][0] & 0x7F) + (u16)(elemPositions[elem][1] & 0x7F) * OSD_WIDTH;
	for (int i = 0; i < 16; i++) {
		if (!elemData[elem][i])
			break;
		pos &= 0x1FF;
		u8 posLow = pos & 0xFF;
		u8 posHigh = pos >> 8;
		regWrite(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::DMAH, &posHigh);
		regWrite(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::DMAL, &posLow);
		regWrite(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::DMDI, &elemData[elem][i]);
		pos++;
	}
}

void osdLoop() {
	if (osdReady) {
		elapsedMicros taskTimer = 0;
		tasks[TASK_OSD].runCounter++;
		// cycle through all drawn elements, and update one per gyro cycle
		u8 drawIteratorStart = drawIterator;
		while (1) {
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
		u32 duration = taskTimer;
		tasks[TASK_OSD].totalDuration += duration;
		if (duration < tasks[TASK_OSD].minDuration) {
			tasks[TASK_OSD].minDuration = duration;
		}
		if (duration > tasks[TASK_OSD].maxDuration) {
			tasks[TASK_OSD].maxDuration = duration;
		}
	}
	if (osdTimer > 1000 && osdReady != 1) {
		// gyro likely ready, check registers
		u8 data = 0;
		osdTimer = 0;
		regRead(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::STAT, &data, 1, 0, 0);
		if (data && !(data & 0b01100000)) {
			osdReady = 2;
		}
		if (!osdReady) return;
		if (data & 1) {
			data = 0b01001100; // dont care, pal, autosync (2 bits), enable osd, sync at next vsync, don't reset, enable output
			regWrite(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::VM0, &data);
		} else {
			data = 0b00001100; // dont care, ntsc, autosync (2 bits), enable osd, sync at next vsync, don't reset, enable output
			regWrite(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::VM0, &data);
		}
		osdReady = data & 0b00000011 ? 1 : 2;
	}
}