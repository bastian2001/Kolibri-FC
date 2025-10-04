#include "global.h"

u8 osdReady = 1;

static elapsedMicros osdTimer = 0;

u8 drawIterator = 0;

u8 elemPositions[(u8)OSDElem::LENGTH][2] = {0}; // up to LENGTH elements can be shown, each element has 2 bytes for x and y position, the 3 MSBs of x is used as an updated flag + reserved, the 3 MSBs of y is used as a visible flag + blinking flag + reserved

u8 elemData[(u8)OSDElem::LENGTH][16] = {0}; // up to LENGTH elements can be shown, each element has 16 bytes for data

void osdInit() {
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
	elemPositions[(u8)elem][0] = (elemPositions[(u8)elem][0] & 0xC0) | (x & 0x3F);
	elemPositions[(u8)elem][1] = (elemPositions[(u8)elem][1] & 0xC0) | (y & 0x3F);
}

void enableBlinking(OSDElem elem) {
	elemPositions[(u8)elem][1] |= 0x40;
}
void disableBlinking(OSDElem elem) {
	elemPositions[(u8)elem][1] &= ~0x40;
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
	u16 pos = (u16)(elemPositions[elem][0] & 0x3F) + (u16)(elemPositions[elem][1] & 0x3F) * OSD_WIDTH;
	bool blinking = elemPositions[elem][1] & 0x40;
	bool isOff = blinking ? (osdTimer % 500000 < 250000) : false; // blink every 500ms
	for (int i = 0; i < 16; i++) {
		if (!elemData[elem][i])
			break;
		char data = elemData[elem][i];
		if (isOff) data = ' ';
		pos &= 0x1FF;
		u8 posLow = pos & 0xFF;
		u8 posHigh = pos >> 8;
		regWrite(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::DMAH, &posHigh);
		regWrite(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::DMAL, &posLow);
		regWrite(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::DMDI, (u8 *)&data);
		pos++;
	}
	dpWriteString(elemPositions[elem][1] & 0x3F, elemPositions[elem][0] & 0x3F, blinking ? 0x40 : 0x00, (const char *)elemData[elem]);
}

void osdLoop() {
	return;
	static bool lastPbActive = false;
	if (pidBoostActive != lastPbActive) {
		lastPbActive = !lastPbActive;
		if (lastPbActive)
			updateElem(OSDElem::PIDBOOST_INDICATOR, "PB");
		else
			updateElem(OSDElem::PIDBOOST_INDICATOR, "  ");
	}
	if (osdReady) {
		TASK_START(TASK_OSD);
		// cycle through all drawn elements, and update one per gyro cycle
		u8 drawIteratorStart = drawIterator;
		while (1) {
			drawIterator++;
			if (drawIterator >= (u8)OSDElem::LENGTH)
				drawIterator = 0;
			if ((elemPositions[drawIterator][0] & 0x80) && (elemPositions[drawIterator][1] & 0x80)) // if updated and enabled
			{
				drawElem(drawIterator);
				break;
			}
			if (drawIterator == drawIteratorStart)
				break;
		}
		TASK_END(TASK_OSD);
	}
	if (osdTimer > 1000000 && osdReady != 1) {
		TASK_START(TASK_OSD);
		u8 data = 0;
		osdTimer = 0;
		regRead(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::STAT, &data, 1, 0, 0);
		if (data && !(data & 0b01100000)) {
			osdReady = 2;
		}
		if (!osdReady) return;
		if (data & 1) {
			u8 data2 = 0b01001100; // dont care, pal, autosync (2 bits), enable osd, sync at next vsync, don't reset, enable output
			regWrite(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::VM0, &data2);
		} else {
			u8 data2 = 0b00001100; // dont care, ntsc, autosync (2 bits), enable osd, sync at next vsync, don't reset, enable output
			regWrite(SPI_OSD, PIN_OSD_CS, (u8)OSDReg::VM0, &data2);
		}
		osdReady = (data & 0b00000011) ? 1 : 2;
		TASK_END(TASK_OSD);
	}
	mspDisplayportLoop();
}

// TODO Make OSD Great again (for digital)
