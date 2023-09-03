#include "global.h"

float adcVoltage = 0, pVoltage = 0, adcCurrent = 0;
elapsedMillis adcTimer = 0;

void initADC()
{
	adc_gpio_init(PIN_ADC_VOLTAGE);
	adc_gpio_init(PIN_ADC_CURRENT);
	adc_init();
	enableElem(OSDElem::TOT_VOLTAGE);
	placeElem(OSDElem::TOT_VOLTAGE, 1, 1);
	enableElem(OSDElem::CURRENT);
	placeElem(OSDElem::CURRENT, 1, 2);
}

uint8_t adcType = 0; // 1 = voltage, 0 = current

void adcLoop()
{
	if (adcTimer > 50)
	{
		adcTimer = 0;
		if (adcType)
		{
			adc_select_input(PIN_ADC_VOLTAGE - 26);
			uint16_t raw = adc_read();
			adcVoltage = raw * (3.3f * 11.f / 4096.f);
			if (pVoltage > 14 && adcVoltage <= 14)
			{
				makeSound(5000, 65535, 300, 300);
			}
			else if (adcVoltage > 14 && pVoltage <= 14)
			{
				stopSound();
			}
			uint8_t voltageStr[16] = {0};
			uint8_t len = snprintf((char *)voltageStr, 16, "%.2f", adcVoltage);
			voltageStr[len] = 0x06;
			voltageStr[len + 1] = '\0';
			updateElem(OSDElem::TOT_VOLTAGE, (char *)voltageStr);
			pVoltage = adcVoltage;
		}
		else
		{
			adc_select_input(PIN_ADC_CURRENT - 26);
			uint16_t raw = adc_read();
			adcCurrent = raw * (300.f / 4096.f); // 300A full deflection
			uint8_t currentStr[16] = {0};
			uint8_t len = snprintf((char *)currentStr, 16, "%.2f", adcCurrent);
			currentStr[len] = 0x9A;
			currentStr[len + 1] = '\0';
			updateElem(OSDElem::CURRENT, (char *)currentStr);
		}
		adcType = !adcType;
	}
}