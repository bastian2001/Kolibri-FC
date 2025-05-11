#include "global.h"

u16 adcVoltage = 0, pVoltage = 0; // centivolts
u16 emptyVoltage = 0;
f32 adcCurrent = 0;
elapsedMillis adcTimer = 0;
f32 temperature = 0;

void initADC() {
	addSetting(SETTING_EMPTY_VOLTAGE, &emptyVoltage, 1400);

	adc_gpio_init(PIN_ADC_VOLTAGE);
	adc_gpio_init(PIN_ADC_CURRENT);
	adc_init();
	enableElem(OSDElem::TOT_VOLTAGE);
	placeElem(OSDElem::TOT_VOLTAGE, 1, 1);
	// enableElem(OSDElem::CURRENT);
	// placeElem(OSDElem::CURRENT, 1, 2);
}

u8 adcType = 0; // 1 = voltage, 0 = current

void adcLoop() {
	if (adcTimer >= 50) {
		elapsedMicros taskTimer = 0;
		tasks[TASK_ADC].runCounter++;
		adcTimer = 0;
		if (adcType) {
			adc_select_input(PIN_ADC_VOLTAGE - 26);
			u32 raw = adc_read();
			adcVoltage = (raw * 3630U) / 4096U; // 36.3V full deflection, voltage divider is 11:1, and 4096 is 3.3V
			if ((adcVoltage > emptyVoltage && pVoltage <= emptyVoltage) || (adcVoltage < 400 && pVoltage >= 400)) {
				stopSound();
				disableBlinking(OSDElem::TOT_VOLTAGE);
			} else if (pVoltage > emptyVoltage && adcVoltage <= emptyVoltage) {
				makeSound(3000, 65535, 300, 300);
				enableBlinking(OSDElem::TOT_VOLTAGE);
			}
			u8 voltageStr[16] = {0};
			snprintf((char *)voltageStr, 16, "%.2f\x06", adcVoltage / 100.f);
			updateElem(OSDElem::TOT_VOLTAGE, (char *)voltageStr);
			pVoltage = adcVoltage;
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
		u32 duration = taskTimer;
		tasks[TASK_ADC].totalDuration += duration;
		if (duration < tasks[TASK_ADC].minDuration) {
			tasks[TASK_ADC].minDuration = duration;
		}
		if (duration > tasks[TASK_ADC].maxDuration) {
			tasks[TASK_ADC].maxDuration = duration;
		}
	}
}
