#include "global.h"

bool pinIsAllowed(u8 pin) {
#ifdef PIN_ADC_CURRENT
	if (pin == PIN_ADC_CURRENT) return false;
#endif
#ifdef PIN_ADC_VOLTAGE
	if (pin == PIN_ADC_VOLTAGE) return false;
#endif
#ifdef PIN_OSD_MISO
	if (pin == PIN_OSD_MISO) return false;
#endif
#ifdef PIN_OSD_CS
	if (pin == PIN_OSD_CS) return false;
#endif
#ifdef PIN_OSD_SCLK
	if (pin == PIN_OSD_SCLK) return false;
#endif
#ifdef PIN_OSD_MOSI
	if (pin == PIN_OSD_MOSI) return false;
#endif
#ifdef PIN_GYRO_INT1
	if (pin == PIN_GYRO_INT1) return false;
#endif
#ifdef PIN_GYRO_CS
	if (pin == PIN_GYRO_CS) return false;
#endif
#ifdef PIN_GYRO_SCLK
	if (pin == PIN_GYRO_SCLK) return false;
#endif
#ifdef PIN_GYRO_SDX
	if (pin == PIN_GYRO_SDX) return false;
#endif
#ifdef PIN_GYRO_MISO
	if (pin == PIN_GYRO_MISO) return false;
#endif
#ifdef PIN_GYRO_MOSI
	if (pin == PIN_GYRO_MOSI) return false;
#endif
#ifdef PIN_GYRO_CLKIN
	if (pin == PIN_GYRO_CLKIN) return false;
#endif
#ifdef PIN_SD_DAT
	if (pin >= PIN_SD_DAT && pin < PIN_SD_DAT + 4) return false;
#endif
#ifdef PIN_SD_CMD
	if (pin == PIN_SD_CMD) return false;
#endif
#ifdef PIN_SD_SCLK
	if (pin == PIN_SD_SCLK) return false;
#endif
#ifdef PIN_FLASH_SCLK
	if (pin == PIN_FLASH_SCLK) return false;
#endif
#ifdef PIN_FLASH_IO_BASE
	if (pin >= PIN_FLASH_IO_BASE && pin < PIN_FLASH_IO_BASE + 2) return false;
#endif
#ifdef PIN_FLASH_CS
	if (pin == PIN_FLASH_CS) return false;
#endif
#ifdef PIN_SPEAKER
	if (pin == PIN_SPEAKER) return false;
#endif
#ifdef PIN_LEDS
	if (pin == PIN_LEDS) return false;
#endif
	return true;
}

bool pinIsRecommended(u8 pin) {
	if (!pinIsAllowed(pin)) return false;
	if (pin >= PIN_MOTORS && pin < PIN_MOTORS + 4) return false;
#ifdef PIN_DCDC_EN
	if (pin == PIN_DCDC_EN) return false;
#endif
#ifdef PIN_SDA0
	if (pin == PIN_SDA0) return false;
#endif
#ifdef PIN_SCL0
	if (pin == PIN_SCL0) return false;
#endif
#ifdef PIN_SDA1
	if (pin == PIN_SDA1) return false;
#endif
#ifdef PIN_SCL1
	if (pin == PIN_SCL1) return false;
#endif
	return true;
}

bool pinHasHwUart(u8 pin, u8 uart, bool tx) {
	if (uart >= NUM_UARTS) return false;

	if (uart == 1) pin += 8;
	pin += 4;
	pin %= 16;
	if (pin >= 8) return false;
	if (!tx) pin++;
	pin %= 2;
	if (!pin) return true;

	return false;
}

bool sendIoConstraints(u8 page, char *buf, u16 &len) {
	switch (page) {
	case 0: {
		// page 0: list of pins and their functions
		buf[0] = page;
		for (int pin = 0; pin < 30; pin++) {
			// exactly 11 bytes per pin, describing their capabilities
			u64 funcs = 0;
			// 6 bytes for functions
			// 0: HSTX
			// 1: SPI (irrelevant what exactly)
			// 2-9: T0/R0/T1/R1...R3 (hardware UART)
			// 10-15: SDA0/SCL0...SCL2
			// 16-19: PIO0...PIO3(?)
			// 20: PWM (exactly how is up to the FC – it may fire an error)
			// rest: reserved (more UART, PIO, whatever)
			// 46: recommended for different use (e.g. OSD is not, but could be)
			// 47: allowed for different use (e.g. gyro is critical => not allowed)
			if (pin >= 12 && pin < 20) funcs |= 1 << 0;
			funcs |= 1 << 1; // all pins have something SPI
			for (int i = 0; i < 4; i++) {
				if (pinHasHwUart(pin, i, true)) funcs |= 1 << (2 + (i * 2));
				if (pinHasHwUart(pin, i, false)) funcs |= 1 << (2 + (i * 2 + 1));
			}
			// only 2 HW uarts
			if (pin % 4 == 0) funcs |= 1 << 10;
			if (pin % 4 == 1) funcs |= 1 << 11;
			if (pin % 4 == 2) funcs |= 1 << 12;
			if (pin % 4 == 3) funcs |= 1 << 13;
			// only 2 I2Cs
			funcs |= 1 << 16;
			funcs |= 1 << 17;
			funcs |= 1 << 18;
			// only 3 PIOs
			funcs |= 1 << 20;
			if (pinIsRecommended(pin)) funcs |= 1ULL << 46;
			if (pinIsAllowed(pin)) funcs |= 1ULL << 47;
			memcpy(&buf[1 + pin * 11], &funcs, 6);
			// then 5 bytes for label
			memcpy(&buf[1 + pin * 11 + 6], pinLabels[pin], 5);
		}
		len = 1 + 30 * 11;
		return true;
	}
	case 5: {
		int ioMin = page * 32;
		int ioMax = (page + 1) * 32;
		buf[len++] = page;

		for (int pin = ioMin; pin < ioMax; pin++) {
			buf[len++] = pin;
			buf[len++] = 0; // relative position on board, board side, size, shape, preferred for this pin
			buf[len++] = 0; // relative position on board, board side, size, shape, preferred for this pin
			buf[len++] = 0; // relative position on board, board side, size, shape, preferred for this pin
			buf[len++] = 0; // relative position on board, board side, size, shape, preferred for this pin
		}
		return true;
	}
	case 6:
		// no more pages for pads
		buf[0] = page;
		len = 1;
		return true;
	case 10:
		// page 10: info about possible serial combinations
		buf[0] = page;
		buf[1] = NUM_UARTS; // hardware UARTs
		buf[2] = SERIAL_COUNT - 1; // maximum number of externally available serials (just the software/performance limit part)
		len = 3;
		return true;
	case 11:
		// page 11: PIO parameters: available PIOs and space
		buf[len++] = page;
		buf[len++] = PICO_PIO_VERSION;
		buf[len++] = NUM_PIOS;
		buf[len++] = halfduplex_uart_program.length;
		buf[len++] = uart_tx_program.length;
		buf[len++] = uart_rx_program.length;
		len = 16; // reserved
		for (int i = 0; i < NUM_PIOS; i++) {
			buf[len++] = getFreeSms(i);
			u32 freeInstructions = getFreeInstructions(i);
			memcpy(&buf[len], &freeInstructions, 4);
			len += 4;
		}
		return true;
	}
	return false;
}
