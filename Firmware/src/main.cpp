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
elapsedMillis gyroTimer;
uint8_t gyroLastState = 0;
uint8_t escPassthroughPin = 255;
elapsedMillis printGyroTimer;
void loop()
{
	if (printGyroTimer > 50 && false)
	{
		printGyroTimer = 0;
		Serial.printf("%d %d %d\n", imuData[0], imuData[1], imuData[2]);
	}
	if (activityTimer > 500)
	{
		gpio_put(PIN_LED_ACTIVITY, !gpio_get(PIN_LED_ACTIVITY));
		activityTimer = 0;
	}
	if (escPassthroughPin < PIN_MOTORS + 4 && escPassthroughPin >= PIN_MOTORS)
	{
		processPassthrough(escPassthroughPin);
		return;
	}
	ELRS->loop();
	speakerLoop();
	if (Serial.available())
	{
		if (Serial.read() == 'C') // ESC Configuration, parse pin next
		{
			delay(1);
			String input = Serial.readStringUntil('\n');
			Serial.println(input);
			if (input.startsWith("RR"))
				escPassthroughPin = PIN_MOTORS + (int)MOTOR::RR;
			else if (input.startsWith("RL"))
				escPassthroughPin = PIN_MOTORS + (int)MOTOR::RL;
			else if (input.startsWith("FR"))
				escPassthroughPin = PIN_MOTORS + (int)MOTOR::FR;
			else if (input.startsWith("FL"))
				escPassthroughPin = PIN_MOTORS + (int)MOTOR::FL;
			else
				Serial.println("Invalid motor");
			Serial.printf("Going into ESC Configuration mode with motor %s on pin %d\n", input.substring(0, 2).c_str(), escPassthroughPin);
		}
	}
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