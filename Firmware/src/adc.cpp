#include "global.h"

float adcVoltage = 0;
elapsedMillis adcTimer = 0;

void initADC()
{
	adc_gpio_init(PIN_ADC_VOLTAGE);
	adc_gpio_init(PIN_ADC_CURRENT);
	adc_init();
	adc_select_input(PIN_ADC_VOLTAGE - 26);
}

void adcLoop()
{
	if (adcTimer > 50)
	{
		adcTimer = 0;
		uint16_t raw = adc_read();
		adcVoltage = (raw - 1930) * (10.f) / (3041 - 1930) + 10;
		adcVoltage = constrain(adcVoltage, 0, 30);
	}
}