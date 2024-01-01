#include "global.h"

uint16_t adcVoltage = 0, pVoltage = 0; // centivolts
uint16_t emptyVoltage  = 0;
float adcCurrent	   = 0;
elapsedMillis adcTimer = 0;

void initADC() {
	adc_gpio_init(PIN_ADC_VOLTAGE);
	adc_gpio_init(PIN_ADC_CURRENT);
	adc_init();
	enableElem(OSDElem::TOT_VOLTAGE);
	placeElem(OSDElem::TOT_VOLTAGE, 1, 1);
	enableElem(OSDElem::CURRENT);
	placeElem(OSDElem::CURRENT, 1, 2);
	EEPROM.get((uint16_t)EEPROM_POS::BATTERY_EMPTY_THRESHOLD, emptyVoltage);
	if (!emptyVoltage) {
		emptyVoltage = 1400;
		EEPROM.put((uint16_t)EEPROM_POS::BATTERY_EMPTY_THRESHOLD, emptyVoltage);
	}
}

uint8_t adcType = 0; // 1 = voltage, 0 = current

void adcLoop() {
		if (adcTimer > 50) {
				adcTimer	 = 0;
		if (adcType) {
						adc_select_input(PIN_ADC_VOLTAGE - 26);
						uint32_t raw = adc_read();
			adcVoltage	 = (raw * 3630U) / 4096U; // 36.3V full deflection, voltage divider is 11:1, and 4096 is 3.3V
						if ((adcVoltage > emptyVoltage && pVoltage <= emptyVoltage) || (adcVoltage < 400 && pVoltage >= 400)) {
								stopSound();
			} else if (pVoltage > emptyVoltage && adcVoltage <= emptyVoltage) {
								makeSound(5000, 65535, 300, 300);
			}
						uint8_t voltageStr[16] = {0};
			snprintf((char *)voltageStr, 16, "%.2f\x06", adcVoltage / 100.f);
						updateElem(OSDElem::TOT_VOLTAGE, (char *)voltageStr);
						pVoltage	 = adcVoltage;
		} else {
						// adc_select_input(PIN_ADC_CURRENT - 26);
			// uint16_t raw           = adc_read();
			// adcCurrent             = raw * (300.f / 4096.f); // 300A full deflection
			// uint8_t currentStr[16] = {0};
			// uint8_t len            = snprintf((char *)currentStr, 16, "%.2f", adcCurrent);
			// currentStr[len]        = 0x9A;
			// currentStr[len + 1]    = '\0';
			// updateElem(OSDElem::CURRENT, (char *)currentStr);
		}
		adcType = !adcType;
	}
}