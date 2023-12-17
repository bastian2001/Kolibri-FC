#include "global.h"

uint8_t setupDone = 0b00;

void setup() {
	// delay(5000);
	set_sys_clock_khz(132000, true);
	Serial.begin(115200);
	Serial.println("Starting up");
	EEPROM.begin(4096);
	readEEPROM();

	gyroInit();
	imuInit();
	osdInit();
	initBaro();

	initADC();

	// init ELRS on pins 8 and 9 using Serial2 (UART1)
	ELRS = new ExpressLRS(Serial2, 420000, 8, 9);

	// init LEDs
	gpio_init(PIN_LED_ACTIVITY);
	gpio_set_dir(PIN_LED_ACTIVITY, GPIO_OUT);
	gpio_init(PIN_LED_DEBUG);
	gpio_set_dir(PIN_LED_DEBUG, GPIO_OUT);
	gpio_put(PIN_LED_DEBUG, 1);

	initSpeaker();

	// init gyro/accel pointer
	gyroDataRaw	 = bmiDataRaw + 3;
	accelDataRaw = bmiDataRaw;

	initESCs();
	initBlackbox();
	rp2040.wdt_begin(200);

	Serial.println("Setup complete");
	setupDone |= 0b01;
	while (!(setupDone & 0b10)) {
		rp2040.wdt_reset();
	}
	startBootupSound(); //annoying during development
}

elapsedMillis activityTimer;

void loop() {
	// baroLoop();
	speakerLoop();
	ELRS->loop();
	adcLoop();
	osdLoop();
	serialLoop();
	configuratorLoop();
	rp2040.wdt_reset();
	if (activityTimer > 500) {
		gpio_put(PIN_LED_ACTIVITY, !gpio_get(PIN_LED_ACTIVITY));
		activityTimer = 0;
	}
}

void setup1() {
	setupDone |= 0b10;
	while (!(setupDone & 0b01)) {
		rp2040.wdt_reset();
	}
}
void loop1() {
	gyroLoop();
}