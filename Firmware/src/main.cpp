#include "esc_passthrough.h"
#include "global.h"
#include "hardware/structs/xip_ctrl.h"

volatile u8 setupDone = 0b00;

void setup() {
	Serial.begin(115200);

	if (powerOnResetMagicNumber == 0xdeadbeefdeadbeef)
		bootReason = rebootReason;
	else
		bootReason = BootReason::POR;
	powerOnResetMagicNumber = 0xdeadbeefdeadbeef;
	rebootReason            = BootReason::WATCHDOG;

	if (connectEscPassthrough && bootReason == BootReason::CMD_ESC_PASSTHROUGH) { // 0 if disabled, pin + 1 if enabled
		beginPassthrough(connectEscPassthrough - 1);
		connectEscPassthrough = 0;
		for (u8 breakout = 0; !breakout;) {
			breakout = processPassthrough();
		}
		endPassthrough();
	}
	connectEscPassthrough = 0;
	initPID();
	EEPROM.begin(4096);
	Serial.println("Setup started");
	readEEPROM();
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
	initDefaultSpi();
	gyroInit();
	imuInit();
	osdInit();
	initBaro();
	initGPS();
	initADC();
	modesInit();
	initMag();

	// init ELRS on pins 0 and 1 using Serial1 (UART0)
	ELRS = new ExpressLRS(Serial1, 420000, PIN_TX0, PIN_RX0);

	// init LEDs
	gpio_init(PIN_LED_ACTIVITY);
	gpio_set_dir(PIN_LED_ACTIVITY, GPIO_OUT);
	gpio_init(PIN_LED_DEBUG);
	gpio_set_dir(PIN_LED_DEBUG, GPIO_OUT);

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
	xip_ctrl_hw->flush = 1;
}

elapsedMillis activityTimer;

elapsedMicros taskTimer0;
void loop() {
	tasks[TASK_LOOP0].runCounter++;
	u32 duration0 = taskTimer0;
	if (duration0 > tasks[TASK_LOOP0].maxGap) {
		tasks[TASK_LOOP0].maxGap = duration0;
	}
	taskTimer0 = 0;
	speakerLoop();
	evalBaroLoop();
	blackboxLoop();
	ELRS->loop();
	modesLoop();
	adcLoop();
	serialLoop();
	configuratorLoop();
	gpsLoop();
	magLoop();
	taskManagerLoop();
	rp2040.wdt_reset();
	if (activityTimer >= 500) {
		gpio_put(PIN_LED_ACTIVITY, !gpio_get(PIN_LED_ACTIVITY));
		activityTimer = 0;
	}
	duration0 = taskTimer0;
	tasks[TASK_LOOP0].totalDuration += duration0;
	if (duration0 > tasks[TASK_LOOP0].maxDuration) {
		tasks[TASK_LOOP0].maxDuration = duration0;
	}
	if (duration0 < tasks[TASK_LOOP0].minDuration) {
		tasks[TASK_LOOP0].minDuration = duration0;
	}
	taskTimer0 = 0;
}

u32 *speakerRxPacket;
void setup1() {
	while (!(setupDone & 0b01)) {
	}
	initESCs();
	setupDone |= 0b10;
	while (!(setupDone & 0b01)) {
	}
}
elapsedMicros taskTimer = 0;
u32 taskState           = 0;

extern PIO speakerPio;
extern u8 speakerSm;
void loop1() {
	tasks[TASK_LOOP1].runCounter++;
	u32 duration = taskTimer;
	if (duration > tasks[TASK_LOOP1].maxGap) {
		tasks[TASK_LOOP1].maxGap = duration;
	}
	taskTimer = 0;
	gyroLoop();
	if (gyroUpdateFlag & 1) {
		switch (taskState++) {
		case 0:
			osdLoop(); // slow, but both need to be on this core, due to SPI collision
			break;
		case 1:
			readBaroLoop();
			break;
		}
		if (taskState == 2) taskState = 0;
		gyroUpdateFlag &= ~1;
	}
	duration = taskTimer;
	tasks[TASK_LOOP1].totalDuration += duration;
	if (duration > tasks[TASK_LOOP1].maxDuration) {
		tasks[TASK_LOOP1].maxDuration = duration;
	}
	if (duration < tasks[TASK_LOOP1].minDuration) {
		tasks[TASK_LOOP1].minDuration = duration;
	}
	taskTimer = 0;
}