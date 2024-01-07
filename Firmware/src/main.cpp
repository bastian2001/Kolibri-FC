#include "global.h"

volatile uint8_t setupDone = 0b00;

void setup() {
	set_sys_clock_khz(264000, true);
	Serial.begin(115200);
	EEPROM.begin(4096);
	Serial.println("Setup started");
	readEEPROM();
	// save crash info to EEPROM
	if (crashInfo[0] == 255) {
		Serial.println("Crash detected");
		for (int i = 0; i < 256; i++) {
			EEPROM.write(4096 - 256 + i, (uint8_t)crashInfo[i]);
		}
		EEPROM.commit();
	}
	for (int i = 0; i < 256; i++) {
		crashInfo[i] = 0;
	}
	initDefaultSpi();
	gyroInit();
	imuInit();
	osdInit();
	initBaro();
	initGPS();
	initADC();
	modesInit();

	// init ELRS on pins 0 and 1 using Serial1 (UART0)
	ELRS = new ExpressLRS(Serial1, 420000, PIN_TX0, PIN_RX0);

	// init LEDs
	gpio_init(PIN_LED_ACTIVITY);
	gpio_set_dir(PIN_LED_ACTIVITY, GPIO_OUT);
	gpio_init(PIN_LED_DEBUG);
	gpio_set_dir(PIN_LED_DEBUG, GPIO_OUT);

	initSpeaker();

	initESCs();
	initBlackbox();
	rp2040.wdt_begin(200);

	Serial.println("Setup complete");
	setupDone |= 0b01;
	while (!(setupDone & 0b10)) {
		rp2040.wdt_reset();
	}
	startBootupSound(); // annoying during development
}

elapsedMillis activityTimer;

void loop() {
	baroLoop();
	speakerLoop();
	blackboxLoop();
	ELRS->loop();
	modesLoop();
	adcLoop();
	serialLoop();
	configuratorLoop();
	gpsLoop();
	rp2040.wdt_reset();
	if (activityTimer > 500) {
		gpio_put(PIN_LED_ACTIVITY, !gpio_get(PIN_LED_ACTIVITY));
		activityTimer = 0;
	}
}

void setup1() {
	setupDone |= 0b10;
	while (!(setupDone & 0b01)) {
	}
}
void loop1() {
	uint32_t times[2];
	elapsedMicros timer;
	gyroLoop();
	times[0] = timer;
	osdLoop();
	times[1] = timer;
	if (times[1] > 300) {
		Serial.printf("Gyro: %3d, OSD: %3d\n", times[0], times[1] - times[0]);
	}
}