#include "global.h"

void setup()
{
	delay(10000);
	set_sys_clock_khz(132000, true);
	Serial.begin(115200);
	Serial.println("Starting up");
	Serial.printf("Clock speed %d\n", clock_get_hz(clk_sys));

	// initialize the BMI270
	gyroInit();
}

void loop()
{
	if (Serial.available())
	{
		Serial.print(Serial.read());
	}
}