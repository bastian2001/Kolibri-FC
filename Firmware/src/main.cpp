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

	Serial.println("Setup complete");
	delay(10);
}

elapsedMillis timer;
elapsedMillis activityTimer;
void loop()
{
	if (activityTimer > 500)
	{
		gpio_put(PIN_LED_ACTIVITY, !gpio_get(PIN_LED_ACTIVITY));
		activityTimer = 0;
	}
	ELRS->loop();
	if (timer > 10)
	{
		// print all ELRS channels
		for (int i = 0; i < 12; i++)
		{
			Serial.printf("%d ", ELRS->channels[i]);
		}
		Serial.printf("\t%d %d", ELRS->errorCount, ELRS->msgCount);
		Serial.println();
		timer = 0;
	}
}