#include "global.h"

void setup()
{
	// delay(10000);
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
	gyroDataRaw = bmiDataRaw + 3;
	accelDataRaw = bmiDataRaw;

	// init ESCs
	initESCs();

	// init PID constants
	initPID();

	Serial.println("Setup complete");
	// startBootupSound(); //annoying during development
}

elapsedMillis activityTimer;
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
	uint8_t gpioState = gpio_get(PIN_GYRO_INT1);
	// actual interrupts might interrupt the code at a bad time, so we just poll the pin
	// latched interrupts have the disadvantage of having to read multiple registers, thus taking longer
	if (gpioState != gyroLastState)
	{
		gyroLastState = gpioState;
		if (gpioState == 1)
		{
			pidLoop();
		}
	}
}