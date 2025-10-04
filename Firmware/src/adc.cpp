#include "global.h"

u16 adcVoltage = 0, cellVoltage = 0; // centivolts
bool batBlinkingAndBeeping = false;
u16 emptyVoltageSetting = 0;
static u16 emptyVoltage = 0;
f32 adcCurrent = 0;
elapsedMicros adcTimer = 0;
f32 temperature = 0;
u8 cellCountSetting = 0;
u8 batCells = 1;
u8 batState = 0;
elapsedMicros batTimer = 0;
u32 adcFlag = 0;

// up to 4.35V per cell + 0.1V (inclusive) for auto detection, e.g. 1S = 0-4.45V, 5S = 17.5x-21.85V, 6S = 21.85x-26.2V

void initADC() {
	addSetting(SETTING_EMPTY_VOLTAGE, &emptyVoltageSetting, 350);
	addSetting(SETTING_CELL_COUNT, &cellCountSetting, 0);

	if (cellCountSetting) batCells = cellCountSetting;

	adc_gpio_init(PIN_ADC_VOLTAGE);
	adc_gpio_init(PIN_ADC_CURRENT);
	adc_init();
	// enableElem(OSDElem::TOT_VOLTAGE);
	// placeElem(OSDElem::TOT_VOLTAGE, 1, 1);
	// enableElem(OSDElem::CELL_VOLTAGE);
	// placeElem(OSDElem::CELL_VOLTAGE, 8, 1);
	// enableElem(OSDElem::CURRENT);
	// placeElem(OSDElem::CURRENT, 1, 2);

	batTimer = 0;
}

u8 adcType = 0; // 1 = voltage, 0 = current

void adcLoop() {
	if (adcTimer >= 50000) {
		TASK_START(TASK_ADC);
		adcTimer = 0;
		if (adcType) {
			adc_select_input(PIN_ADC_VOLTAGE - 26);
			u32 raw = adc_read();
			adcVoltage = (raw * 3630U) / 4096U; // 36.3V full deflection, voltage divider is 11:1, and 4096 is 3.3V
			// int elemIndex = osdHandler.find(elemType::BATTERY_VOLTAGE);
			// if (elemIndex >= 0) {
			// 	osdHandler.elements[elemIndex]->setRawDataPtr(&adcVoltage);
			// 	osdHandler.elements[elemIndex]->updated = true;
			// }
			adcFlag |= 0xFFFF;
			switch (batState) {
			case 0: // no battery (USB or startup)
				if (adcVoltage < 250) { // stay in setup as long as battery is below 2.5V
					batTimer = 0;
				} else if (batTimer > 1000000) { // accept a new battery after 1 second
					if (!cellCountSetting) batCells = ((adcVoltage - 11) / 435 + 1);
					emptyVoltage = batCells * emptyVoltageSetting;
					batState = 1;
				}
				break;
			case 1: // battery detected
				if (adcVoltage > 150) {
					batTimer = 0;
				} else if (batTimer > 500000) { // reset cell count after 500ms
					if (!cellCountSetting) {
						batCells = 1;
					}
					emptyVoltage = 0;
					batState = 0;
				}
				break;
			}
			cellVoltage = adcVoltage / batCells;
			if (batBlinkingAndBeeping && (adcVoltage >= emptyVoltage || adcVoltage < 50)) {
				stopSound();
				disableBlinking(OSDElem::TOT_VOLTAGE);
				disableBlinking(OSDElem::CELL_VOLTAGE);
				batBlinkingAndBeeping = false;
			} else if (adcVoltage < emptyVoltage && !batBlinkingAndBeeping) {
				makeSound(3000, 65535, 300, 300);
				enableBlinking(OSDElem::TOT_VOLTAGE);
				enableBlinking(OSDElem::CELL_VOLTAGE);
				batBlinkingAndBeeping = true;
			}
			u8 voltageStr[16] = {0};
			snprintf((char *)voltageStr, 16, "%.2f\x06 ", adcVoltage / 100.f);
			updateElem(OSDElem::TOT_VOLTAGE, (char *)voltageStr);
			snprintf((char *)voltageStr, 16, "%.2f\x06 ", cellVoltage / 100.f);
			updateElem(OSDElem::CELL_VOLTAGE, (char *)voltageStr);
		} else {
			// adc_select_input(PIN_ADC_CURRENT - 26);
			// u16 raw           = adc_read();
			// adcCurrent             = raw * (300.f / 4096.f); // 300A full deflection
			// u8 currentStr[16] = {0};
			// u8 len            = snprintf((char *)currentStr, 16, "%.2f", adcCurrent);
			// currentStr[len]        = 0x9A;
			// currentStr[len + 1]    = '\0';
			// updateElem(OSDElem::CURRENT, (char *)currentStr);

			// read temperature
			// adc_select_input(4);
			// temperature = analogReadTemp();
		}
		adcType = !adcType;
		TASK_END(TASK_ADC);
	}
}
