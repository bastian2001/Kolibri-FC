#include "global.h"

void setup()
{
	delay(10000);
	set_sys_clock_khz(132000, true);
	Serial.begin(115200);
	Serial.println("Starting up");

	// initialize the BMI270
	gyroInit();

	// init ELRS
	ELRS = new ExpressLRS(Serial2, 420000, 8, 9);

	// init LEDs
	gpio_init(PIN_LED_ACTIVITY);
	gpio_set_dir(PIN_LED_ACTIVITY, GPIO_OUT);
	gpio_init(PIN_LED_DEBUG);
	gpio_set_dir(PIN_LED_DEBUG, GPIO_OUT);

	// init speaker
	initSpeaker();

	// init gyro/accel pointer
	gyroData = bmiData;
	accelData = bmiData + 3;

	// init ESCs
	initESCs();

	Serial.println("Setup complete");
	// startBootupSound(); //annoying during development
}

elapsedMillis activityTimer;
elapsedMillis gyroTimer;
uint8_t gyroLastState = 0;
void loop()
{
	if (activityTimer > 500)
	{
		gpio_put(PIN_LED_ACTIVITY, !gpio_get(PIN_LED_ACTIVITY));
		activityTimer = 0;
	}
	ELRS->loop();
	speakerLoop();
	uint8_t gpio_state = digitalRead(PIN_GYRO_INT1);
	// actual interrupts might interrupt the code at a bad time, so we just poll the pin
	// latched interrupts have the disadvantage of having to read multiple registers, thus taking longer
	if (gpio_state != gyroLastState)
	{
		gyroLastState = gpio_state;
		if (gpio_state == 1)
		{
			pidLoop();
		}
	}
}