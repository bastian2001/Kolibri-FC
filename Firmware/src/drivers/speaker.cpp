#include "global.h"

elapsedMillis soundStart;
uint16_t soundDuration = 0;
bool typeSweep = false;
uint16_t sweepStartFrequency = 0;
uint16_t sweepEndFrequency = 0;
uint16_t onTime = 0;
uint16_t offTime = 0;
uint16_t currentWrap = 400;

int freqToWrap(uint16_t frequency)
{
	return 1000000 / frequency;
}

void initSpeaker()
{
	gpio_set_function(PIN_SPEAKER, GPIO_FUNC_PWM);
	uint8_t sliceNum = pwm_gpio_to_slice_num(PIN_SPEAKER);
	pwm_set_clkdiv_int_frac(sliceNum, 132, 0); // 1MHz, therefore a wrap of 50000 for 20Hz, and a wrap of 200 for 5kHz
	// default: 2.5kHz
	pwm_set_wrap(sliceNum, 400);
	pwm_set_gpio_level(PIN_SPEAKER, 0);
	pwm_set_enabled(sliceNum, true);
}

void startBootupSound()
{
	makeSweepSound(1000, 4000, 2000, 300, 100);
}

void speakerLoop()
{
	if (soundDuration > 0)
	{
		uint32_t sinceStart = soundStart;
		if (sinceStart > soundDuration)
		{
			stopSound();
		}
		else if (typeSweep)
		{
			uint32_t thisCycle = sinceStart % (onTime + offTime);
			if (thisCycle > onTime)
			{
				pwm_set_gpio_level(PIN_SPEAKER, 0);
			}
			else
			{
				uint32_t thisFreq = sweepStartFrequency + ((sweepEndFrequency - sweepStartFrequency) * thisCycle) / onTime;
				currentWrap = freqToWrap(thisFreq);
				pwm_set_wrap(pwm_gpio_to_slice_num(PIN_SPEAKER), currentWrap);
				pwm_set_gpio_level(PIN_SPEAKER, currentWrap >> 1);
			}
		}
		else
		{
			uint32_t thisCycle = sinceStart % (onTime + offTime);
			if (thisCycle > onTime)
			{
				pwm_set_gpio_level(PIN_SPEAKER, 0);
			}
			else
			{
				pwm_set_gpio_level(PIN_SPEAKER, currentWrap >> 1);
			}
		}
	}
}

void makeSound(uint16_t frequency, uint16_t duration, uint16_t tOnMs, uint16_t tOffMs)
{
	typeSweep = false;
	currentWrap = freqToWrap(frequency);
	pwm_set_wrap(pwm_gpio_to_slice_num(PIN_SPEAKER), currentWrap);
	onTime = tOnMs;
	offTime = tOffMs;
	soundDuration = duration;
	soundStart = 0;
}

void stopSound()
{
	soundDuration = 0;
	pwm_set_gpio_level(PIN_SPEAKER, 0);
}

// sweep from startFrequency to endFrequency over tOnMs, then stop for tOffMs, repeat for duration
void makeSweepSound(uint16_t startFrequency, uint16_t endFrequency, uint16_t duration, uint16_t tOnMs, uint16_t tOffMs)
{
	typeSweep = true;
	sweepStartFrequency = startFrequency;
	sweepEndFrequency = endFrequency;
	onTime = tOnMs;
	offTime = tOffMs;
	soundDuration = duration;
	soundStart = 0;
}