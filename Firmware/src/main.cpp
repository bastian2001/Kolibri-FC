#include "global.h"
#include "hardware/vreg.h"

volatile u8 setupDone = 0b00;

void setup() {
	vreg_set_voltage(VREG_VOLTAGE_1_30);
	Serial.begin(115200);

	initFixMath();

	runUnitTests();

	if (powerOnResetMagicNumber == 0xdeadbeefdeadbeef)
		bootReason = rebootReason;
	else
		bootReason = BootReason::POR;
	powerOnResetMagicNumber = 0xdeadbeefdeadbeef;
	rebootReason = BootReason::WATCHDOG;

	Serial.println("Setup started");
	initLittleFs();
	openSettingsFile();
	addSetting(SETTING_UAV_NAME, &uavName, "Kolibri UAV");

	initPid();

	EEPROM.begin(4096);
	// save crash info to EEPROM
	if (crashInfo[0] == 255) {
		Serial.println("Crash detected");
		for (int i = 0; i < 256; i++) {
			EEPROM.write(4096 - 256 + i, (u8)crashInfo[i]);
		}
		EEPROM.commit();
	}
	for (int i = 0; i < 256; i++) {
		crashInfo[i] = 0;
	}
	rtcInit();
	osdInit();
	initMag();
	imuInit();
	initADC();
	modesInit();
	initSerial();
	initGPS();

	// init ELRS on pins 0 and 1 using Serial1 (UART0)
	u8 elrsNum = 0;
	for (int i = 0; i < SERIAL_COUNT; i++) {
		if (serials[i].functions & SERIAL_CRSF) {
			elrsNum = i;
			break;
		}
	}
	ELRS = new ExpressLRS(elrsNum);

	// init LEDs
	sleep_ms(1);
	p.recalculateClock();
	p.neoPixelFill(0, 0, 255, true);

	initBlackbox();
	initSpeaker();
	rp2040.wdt_begin(200);

	Serial.println("Setup complete");
	extern elapsedMicros taskTimer0;
	taskTimer0 = 0;
	setupDone |= 0b01;
	while (!(setupDone & 0b10)) {
		rp2040.wdt_reset();
	}
	closeSettingsFile();
	rom_flash_flush_cache();
}

elapsedMillis activityTimer;

elapsedMicros taskTimer0;
void loop() {
	u32 duration0 = taskTimer0;
	if (duration0 > tasks[TASK_LOOP0].maxGap) {
		tasks[TASK_LOOP0].maxGap = duration0;
	}
	TASK_START(TASK_LOOP0);
	speakerLoop();
	baroLoop();
	blackboxLoop();
	ELRS->loop();
	modesLoop();
	adcLoop();
	serialLoop();
	configuratorLoop();
	gpsLoop();
	magLoop();
	osdLoop();
	taskManagerLoop();
	rp2040.wdt_reset();
	if (activityTimer >= 500) {
		static bool on = false;
		if (on) {
			p.neoPixelSetValue(0, 0, 0, 0, true);
			on = false;
		} else {
			p.neoPixelSetValue(0, 255, 255, 255, true);
			on = true;
		}
		activityTimer = 0;
	}
	TASK_END(TASK_LOOP0);
	taskTimer0 = 0;
}

u32 *speakerRxPacket;
void setup1() {
	while (!(setupDone & 0b01)) {
	}
	initESCs();
	gyroInit();
	setupDone |= 0b10;
	while (!(setupDone & 0b01)) {
	}
}
elapsedMicros taskTimer = 0;

extern PIO speakerPio;
extern u8 speakerSm;
void loop1() {
	u32 duration = taskTimer;
	if (duration > tasks[TASK_LOOP1].maxGap) {
		tasks[TASK_LOOP1].maxGap = duration;
	}
	TASK_START(TASK_LOOP1);
	gyroLoop();

	if (gyroUpdateFlag & 0x01) {
		gyroUpdateFlag &= ~0x01;
		imuUpdate();
		decodeErpm();
		pidLoop();
	}
	TASK_END(TASK_LOOP1);
	taskTimer = 0;
}
