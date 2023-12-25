#include "global.h"

uint8_t setupDone = 0b00;

void setup() {
	// delay(5000);
	set_sys_clock_khz(132000, true);
	Serial.begin(115200);
	EEPROM.begin(4096);
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
	crashInfo[0] = 255;

	gyroInit();
	imuInit();
	osdInit();
	initBaro();
	initGPS();
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
	startBootupSound(); // annoying during development
}

elapsedMillis activityTimer;

void loop() {
	while (true) {
		crashInfo[1] = 1;
		// baroLoop();
		speakerLoop();
		crashInfo[1] = 2;
		ELRS->loop();
		modesLoop();
		crashInfo[1] = 3;
		adcLoop();
		crashInfo[1] = 4;
		osdLoop();
		crashInfo[1] = 5;
		serialLoop();
		crashInfo[1] = 6;
		configuratorLoop();
		crashInfo[1] = 7;
		gpsLoop();
		crashInfo[1] = 8;
		rp2040.wdt_reset();
		crashInfo[1] = 9;
		if (activityTimer > 500) {
			crashInfo[1] = 10;
			gpio_put(PIN_LED_ACTIVITY, !gpio_get(PIN_LED_ACTIVITY));
			activityTimer = 0;
		}
		crashInfo[1] = 11;
	}
}

void setup1() {
	setupDone |= 0b10;
	while (!(setupDone & 0b01)) {
		rp2040.wdt_reset();
	}
	crashInfo[127] = 255;
}
void loop1() {
	crashInfo[128] = 1;
	gyroLoop();
	crashInfo[128] = 2;
}