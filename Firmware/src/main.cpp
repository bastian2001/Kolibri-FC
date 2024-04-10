#include "global.h"
#include "hardware/structs/xip_ctrl.h"
#include "onewire_receive.pio.h"
#include "onewire_transmit.pio.h"
#include "serial_comm.h"

volatile u8 setupDone = 0b00;

PIO testPio;
u32 offsetPioReceive, offsetPioTransmit;
u8 testSm = 0;
pio_sm_config configPioReceive, configPioTransmit;

void delayWhileRead(u16 ms);
void pioEnableTx(bool enable);
u8 pioAvailable();
u8 pioRead();
void pio_set_program(uint offset, pio_sm_config c, int waitForInst = -1);

// void serialWrite(u8 *data, u16 len) {
// 	for (u16 i = 0; i < len; i++) {
// 		pio_sm_put_blocking(testPio, testSm, data[i]);
// 	}
// }
void setup() {
	Serial.begin(115200);
	Serial2.begin(250000);
	gpio_set_function(4, GPIO_FUNC_UART);
	gpio_set_function(5, GPIO_FUNC_UART);
	{
		gpio_init(PIN_LED_ACTIVITY);
		gpio_set_dir(PIN_LED_ACTIVITY, GPIO_OUT);
		gpio_init(PIN_LED_DEBUG);
		gpio_set_dir(PIN_LED_DEBUG, GPIO_OUT);
		for (int i = 0; i < 5; i++) {
			digitalWrite(PIN_LED_ACTIVITY, HIGH);
			digitalWrite(PIN_LED_DEBUG, HIGH);
			delay(100);
			digitalWrite(PIN_LED_ACTIVITY, LOW);
			digitalWrite(PIN_LED_DEBUG, LOW);
			delay(100);
		}
		digitalWrite(PIN_LED_ACTIVITY, HIGH);
		digitalWrite(PIN_LED_DEBUG, HIGH);
		testPio           = pio0;
		offsetPioReceive  = pio_add_program(testPio, &onewire_receive_program);
		offsetPioTransmit = pio_add_program(testPio, &onewire_transmit_program);
		pio_claim_sm_mask(testPio, 0b1);
#define TEST_PIN PIN_MOTORS
		pio_gpio_init(testPio, TEST_PIN);
		gpio_set_pulls(TEST_PIN, true, false);
		configPioReceive = onewire_receive_program_get_default_config(offsetPioReceive);
		sm_config_set_set_pins(&configPioReceive, TEST_PIN, 1);
		sm_config_set_out_pins(&configPioReceive, TEST_PIN, 1);
		sm_config_set_in_pins(&configPioReceive, TEST_PIN);
		sm_config_set_jmp_pin(&configPioReceive, TEST_PIN);
		sm_config_set_out_shift(&configPioReceive, true, false, 32);
		sm_config_set_in_shift(&configPioReceive, true, false, 32);
		configPioTransmit = onewire_transmit_program_get_default_config(offsetPioTransmit);
		sm_config_set_set_pins(&configPioTransmit, TEST_PIN, 1);
		sm_config_set_out_pins(&configPioTransmit, TEST_PIN, 1);
		sm_config_set_in_pins(&configPioTransmit, TEST_PIN);
		sm_config_set_jmp_pin(&configPioTransmit, TEST_PIN);
		sm_config_set_out_shift(&configPioTransmit, true, false, 32);
		sm_config_set_in_shift(&configPioTransmit, true, false, 32);
		pio_sm_init(testPio, testSm, offsetPioReceive, &configPioReceive);
		pio_sm_set_consecutive_pindirs(testPio, testSm, TEST_PIN, 1, false);
		pio_sm_set_enabled(testPio, testSm, true);
		pio_sm_set_clkdiv_int_frac(testPio, testSm, 429, 176);
	}
	while (true) {
		// if (pio_sm_get_rx_fifo_level(testPio, testSm))
		// 	Serial.write(pio_sm_get(testPio, testSm) >> 24);
		// if (Serial.available()) {
		// 	u8 data = Serial.read();
		// 	serialWrite(&data, 1);
		// }
		process_serial();
		delayWhileRead(0);
		// while (pioAvailable()) {
		// 	u8 data = pioRead();
		// 	Serial.write(data);
		// }
		// if (Serial.available()) {
		// 	u8 data = Serial.read();
		// 	pioEnableTx(true);
		// 	serialWrite(&data, 1);
		// 	pioEnableTx(false);
		// }
	}
	// EEPROM.begin(4096);
	// Serial.println("Setup started");
	// readEEPROM();
	// // save crash info to EEPROM
	// if (crashInfo[0] == 255) {
	// 	Serial.println("Crash detected");
	// 	for (int i = 0; i < 256; i++) {
	// 		EEPROM.write(4096 - 256 + i, (u8)crashInfo[i]);
	// 	}
	// 	EEPROM.commit();
	// }
	// for (int i = 0; i < 256; i++) {
	// 	crashInfo[i] = 0;
	// }
	// initDefaultSpi();
	// gyroInit();
	// imuInit();
	// osdInit();
	// initBaro();
	// initGPS();
	// initADC();
	// modesInit();
	// initMag();

	// // init ELRS on pins 0 and 1 using Serial1 (UART0)
	// ELRS = new ExpressLRS(Serial1, 420000, PIN_TX0, PIN_RX0);

	// // init LEDs
	// gpio_init(PIN_LED_ACTIVITY);
	// gpio_set_dir(PIN_LED_ACTIVITY, GPIO_OUT);
	// gpio_init(PIN_LED_DEBUG);
	// gpio_set_dir(PIN_LED_DEBUG, GPIO_OUT);

	// initBlackbox();
	// initSpeaker();
	// rp2040.wdt_begin(200);

	// Serial.println("Setup complete");
	// extern elapsedMicros taskTimer0;
	// taskTimer0 = 0;
	// setupDone |= 0b01;
	// while (!(setupDone & 0b10)) {
	// 	rp2040.wdt_reset();
	// }
	// xip_ctrl_hw->flush = 1;
}

elapsedMillis activityTimer;

elapsedMicros taskTimer0;
void loop() {
	// tasks[TASK_LOOP0].runCounter++;
	// u32 duration0 = taskTimer0;
	// if (duration0 > tasks[TASK_LOOP0].maxGap) {
	// 	tasks[TASK_LOOP0].maxGap = duration0;
	// }
	// taskTimer0 = 0;
	// speakerLoop();
	// evalBaroLoop();
	// blackboxLoop();
	// ELRS->loop();
	// modesLoop();
	// adcLoop();
	// serialLoop();
	// configuratorLoop();
	// gpsLoop();
	// magLoop();
	// taskManagerLoop();
	// rp2040.wdt_reset();
	// if (activityTimer >= 500) {
	// 	gpio_put(PIN_LED_ACTIVITY, !gpio_get(PIN_LED_ACTIVITY));
	// 	activityTimer = 0;
	// }
	// duration0 = taskTimer0;
	// tasks[TASK_LOOP0].totalDuration += duration0;
	// if (duration0 > tasks[TASK_LOOP0].maxDuration) {
	// 	tasks[TASK_LOOP0].maxDuration = duration0;
	// }
	// if (duration0 < tasks[TASK_LOOP0].minDuration) {
	// 	tasks[TASK_LOOP0].minDuration = duration0;
	// }
	// taskTimer0 = 0;
}

u32 *speakerRxPacket;
void setup1() {
	for (;;) {
		u8 read = !digitalRead(TEST_PIN);
		digitalWrite(PIN_LED_DEBUG, read);
		if (read) delayMicroseconds(100);
	}
	// initESCs();
	// setupDone |= 0b10;
	// while (!(setupDone & 0b01)) {
	// }
}
elapsedMicros taskTimer = 0;
u32 taskState           = 0;

extern PIO speakerPio;
extern u8 speakerSm;
void loop1() {
	// tasks[TASK_LOOP1].runCounter++;
	// u32 duration = taskTimer;
	// if (duration > tasks[TASK_LOOP1].maxGap) {
	// 	tasks[TASK_LOOP1].maxGap = duration;
	// }
	// taskTimer = 0;
	// gyroLoop();
	// if (gyroUpdateFlag & 1) {
	// 	switch (taskState++) {
	// 	case 0:
	// 		osdLoop(); // slow, but both need to be on this core, due to SPI collision
	// 		break;
	// 	case 1:
	// 		readBaroLoop();
	// 		break;
	// 	}
	// 	if (taskState == 2) taskState = 0;
	// 	gyroUpdateFlag &= ~1;
	// }
	// duration = taskTimer;
	// tasks[TASK_LOOP1].totalDuration += duration;
	// if (duration > tasks[TASK_LOOP1].maxDuration) {
	// 	tasks[TASK_LOOP1].maxDuration = duration;
	// }
	// if (duration < tasks[TASK_LOOP1].minDuration) {
	// 	tasks[TASK_LOOP1].minDuration = duration;
	// }
	// taskTimer = 0;
}