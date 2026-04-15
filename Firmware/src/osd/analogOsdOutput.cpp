#include "global.h"

void AnalogOsdOutput::begin() {
	OsdCanvas::get().addOutput(this);
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

void AnalogOsdOutput::loop() {
	if (isReady) {
		TASK_START(TASK_OSD);
		if (!OsdOutput::fullyTransmitted) {
			// TODO revise, way too (unnecessarily) long transmission
			for (int i = 0; i < width; i++) {
				u16 pos = i + drawingLine * width;

				char data = frameBuffer[pos];
				u8 posLow = pos & 0xFF;
				u8 posHigh = (pos >> 8) & 0x01;
				regWrite(SPI_OSD, PIN_OSD_CS, REG_DMAH, &posHigh);
				regWrite(SPI_OSD, PIN_OSD_CS, REG_DMAL, &posLow);
				regWrite(SPI_OSD, PIN_OSD_CS, REG_DMDI, (u8 *)&data);
				pos++;
			}

			drawingLine++;
			if (drawingLine >= height) {
				drawingLine = 0;
				fullyTransmitted = true;
			}
		}
		TASK_END(TASK_OSD);
	}
	if (checkTimer > 1000000 && isReady != 1) {
		TASK_START(TASK_OSD);
		u8 data = 0;
		checkTimer = 0;
		regRead(SPI_OSD, PIN_OSD_CS, REG_STAT, &data, 1, 0);
		if (data && !(data & 0b01100000)) {
			isReady = 2;
		}
		if (!isReady) return;
		if (data & 1) {
			u8 data2 = 0b01001100; // dont care, pal, autosync (2 bits), enable osd, sync at next vsync, don't reset, enable output
			setSize(30, 16);
			isPalOutput = true;
			regWrite(SPI_OSD, PIN_OSD_CS, REG_VM0, &data2);
		} else {
			u8 data2 = 0b00001100; // dont care, ntsc, autosync (2 bits), enable osd, sync at next vsync, don't reset, enable output
			setSize(30, 13);
			isNtscOutput = true;
			regWrite(SPI_OSD, PIN_OSD_CS, REG_VM0, &data2);
		}
		isReady = (data & 0b00000011) ? 1 : 2;
		TASK_END(TASK_OSD);
	}
}

void AnalogOsdOutput::disableOutput() {
	u8 data;
	regRead(SPI_OSD, PIN_OSD_CS, REG_VM0, &data, 1, 0);
	data &= ~0b00001000;
	regWrite(SPI_OSD, PIN_OSD_CS, REG_VM0, &data);
}

void AnalogOsdOutput::enableOutput() {
	u8 data;
	regRead(SPI_OSD, PIN_OSD_CS, REG_VM0, &data, 1, 0);
	data |= 0b00001000;
	regWrite(SPI_OSD, PIN_OSD_CS, REG_VM0, &data);
}

void AnalogOsdOutput::updateCharacter(u8 cmAddr, u8 data[54]) {
	disableOutput();
	sleep_us(100);
	regWrite(SPI_OSD, PIN_OSD_CS, REG_CMAH, &cmAddr);
	for (int i = 0; i < 54; i++) {
		regWrite(SPI_OSD, PIN_OSD_CS, REG_CMAL, (u8 *)&i);
		regWrite(SPI_OSD, PIN_OSD_CS, REG_CMDI, &data[i]);
	}
	u8 scratch = 0b10100000;
	regWrite(SPI_OSD, PIN_OSD_CS, REG_CMM, &scratch);
	sleep_ms(15);
	enableOutput();
}

void AnalogOsdOutput::setSize(u8 width, u8 height) {
	fullyTransmitted = false;
	drawingLine = 0;
	OsdCanvas::get().setSize(width, height, 0);
	OsdOutput::setSize(width, height);
}
