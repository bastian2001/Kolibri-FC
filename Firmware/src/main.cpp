#include "global.h"
#include "hardware/structs/qmi.h"
#include "hardware/vreg.h"

volatile u8 setupDone = 0b00;
static elapsedMicros taskTimer0;

void setup() {
	Serial.begin(115200);
	vreg_disable_voltage_limit();
	vreg_set_voltage(VREG_VOLTAGE_1_40);
	set_sys_clock_khz(360000, false);

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

	initPid();
	initControl();
	rtcInit();
	osdInit();
	inFlightTuningInit();
	initMag();
	imuInit();
	initADC();
	modesInit();
	initSerial();
	trampInit();
	initGPS();

	u8 elrsNum = 0;
	for (int i = 0; i < SERIAL_COUNT; i++) {
		if (serials[i].functions & SERIAL_CRSF) {
			elrsNum = i;
			break;
		}
	}
	ELRS = new ExpressLRS(elrsNum);

	// init LEDs
	p.recalculateClock();
	p.neoPixelFill(0, 0, 255, true);
	for (int i = 0; i < 10; i++) {
		p.neoPixelFill(0, 0, 0, true);
		sleep_ms(30);
		p.neoPixelFill(0, 0, 255, true);
		sleep_ms(30);
	}

#ifdef BLACKBOX_STORAGE
	initBlackbox();
#endif
	initSpeaker();

	// sleep_ms(4000);

	// for (int i = 0; i < 2048; i++) {
	// 	bbFs.eraseBlock(i);
	// 	u8 feat = bbFs.getFeature();
	// 	Serial.printf("Block %d feat 0x%02X\n", i, feat);
	// 	// Serial.flush();
	// 	// sleep_ms(50);
	// }

	// u8 *buf = (u8 *)malloc(2048);
	// // u8 *buf2 = (u8 *)malloc(2048);
	// for (int i = 0; i < 2048; i++) {
	// 	for (int j = 0; j < 2048; j++) {
	// 		// 	// buf[j] = rp2040.hwrand32();
	// 		buf[j] = 0x55;
	// 	}
	// 	// u16 block = rp2040.hwrand32() % 2048;
	// 	// bbFs.eraseBlock(i);
	// 	// sleep_ms(20);
	// 	// u8 page = rp2040.hwrand32() % 8;
	// 	for (int j = 0; j < 64; j++) {
	// 		bbFs.programLoad(i, 0, 2048, buf);
	// 		bbFs.programExecute(i, j);
	// 	}
	// 	bool blockOk = true;
	// 	for (int j = 0; j < 64; j++) {
	// 		bbFs.getData(i, j, 0, 2048, buf);
	// 		// if (memcmp(buf, buf2, 2048)) {
	// 		bool pageOk = true;
	// 		for (int k = 0; k < 2048; k++) {
	// 			if (buf[k] != 0x55) {
	// 				pageOk = false;
	// 				blockOk = false;
	// 			}
	// 		}
	// 		// if (!pageOk) {
	// 		// 	Serial.printf("\n\n============================\nfound error at block %d page %d\n", i, j);
	// 		// 	Serial.print("                   wanted                                                   got");
	// 		// 	for (int k = 0; k < 2048; k += 16) {
	// 		// 		Serial.printf("\n%4d 0x%02X   ", k, k);
	// 		// 		// Serial.print(memcmp(buf + k, buf2 + k, 16) ? 'x' : ' ');
	// 		// 		Serial.print("  ");
	// 		// 		for (int l = 0; l < 16; l++) {
	// 		// 			if (l % 8 == 0) Serial.print(' ');
	// 		// 			Serial.printf("%02X ", 0xFF);
	// 		// 		}
	// 		// 		Serial.print("     ");
	// 		// 		for (int l = 0; l < 16; l++) {
	// 		// 			if (l % 8 == 0) Serial.print(' ');
	// 		// 			Serial.printf("%02X ", buf[k + l]);
	// 		// 		}
	// 		// 	}
	// 		// 	Serial.println();
	// 		// 	sleep_ms(50);
	// 		// }
	// 	}
	// 	// if ((i % 100) == 0) Serial.printf("%d done\n", i);
	// 	// if (blockOk)
	// 	// 	Serial.printf("Block %d ok\n", i);
	// 	// else
	// 	// 	Serial.printf("Block %d not ok.\n", i);
	// 	// Serial.flush();
	// 	// sleep_ms(blockOk ? 50 : 50);
	// }
	// free(buf);
	// // free(buf2);

	// // for (; true;) {
	// // 	if (Serial.available() >= 5) {
	// // 		u32 block = Serial.parseInt();
	// // 		while (Serial.read() != -1)
	// // 			;
	// // 		Serial.printf("\nReading block %d, enter page\n", block);
	// // 		Serial.flush();
	// // 		for (; Serial.available() < 3;)
	// // 			;
	// // 		u32 page = Serial.parseInt();
	// // 		while (Serial.read() != -1)
	// // 			;
	// // 		Serial.printf("Reading page %d, enter from and length\n", page);
	// // 		Serial.flush();
	// // 		for (; Serial.available() < 5;)
	// // 			;
	// // 		u32 from = Serial.parseInt();
	// // 		while (Serial.read() != -1)
	// // 			;
	// // 		Serial.printf("Reading from %d\n", from);
	// // 		Serial.flush();
	// // 		for (; Serial.available() < 5;)
	// // 			;
	// // 		u32 len = Serial.parseInt();
	// // 		while (Serial.read() != -1)
	// // 			;
	// // 		Serial.printf("Reading %d bytes\n", len);
	// // 		Serial.flush();
	// // u8 *buf = (u8 *)malloc(2176);
	// // 		bbFs.getData(block, page, from, len, buf);
	// // 		for (int i = from; i < from + len; i++) {
	// // 			int c = buf[i - from];
	// // 			if (i % 16 == 0) Serial.printf("\n%4d 0x%02X ", i, i);
	// // 			if (i % 8 == 0) Serial.print(' ');
	// // 			Serial.printf("%02X ", c);
	// // 		}
	// // free(buf);
	// // 	}
	// // }

	rp2040.wdt_begin(200);

	Serial.println("Setup complete");
	taskTimer0 = 0;
	setupDone |= 0b01;
	while (!(setupDone & 0b10)) {
		rp2040.wdt_reset();
	}
	closeSettingsFile();
	rom_flash_flush_cache();
}

static elapsedMicros activityTimer;

void loop() {
	u32 duration0 = taskTimer0;
	if (duration0 > tasks[TASK_LOOP0].maxGap) {
		tasks[TASK_LOOP0].maxGap = duration0;
	}
	TASK_START(TASK_LOOP0);
	speakerLoop();
	baroLoop();
#ifdef BLACKBOX_STORAGE
	blackboxLoop();
#endif
	ELRS->loop();
	modesLoop();
	adcLoop();
	inFlightTuningLoop();
	serialLoop();
	configuratorLoop();
	gpsLoop();
	magLoop();
	osdLoop();
	taskManagerLoop();
	trampLoop();
	rp2040.wdt_reset();
	if (activityTimer >= 500000) {
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

void loop1() {
	u32 duration = taskTimer;
	if (duration > tasks[TASK_LOOP1].maxGap) {
		tasks[TASK_LOOP1].maxGap = duration;
	}
	TASK_START(TASK_LOOP1);
	gyroLoop();

	if (gyroUpdateFlag & 0x01) {
		gyroUpdateFlag &= ~0x01;

		static u8 imuUpdateCycle = 0;
		TASK_START(TASK_IMU);
		TASK_START(TASK_IMU_GYRO);
		imuGyroUpdate();
		TASK_END(TASK_IMU_GYRO);

		switch (imuUpdateCycle) {
		case 0: {
			TASK_START(TASK_IMU_ACCEL1);
			imuAccelUpdate1();
			TASK_END(TASK_IMU_ACCEL1);
		} break;
		case 1: {
			TASK_START(TASK_IMU_ACCEL2);
			imuAccelUpdate2();
			TASK_END(TASK_IMU_ACCEL2);
		} break;
		case 2: {
			TASK_START(TASK_IMU_ANGLE);
			imuUpdatePitchRoll();
			TASK_END(TASK_IMU_ANGLE);
		} break;
		case 3: {
			TASK_START(TASK_IMU_SPEEDS);
			imuUpdateSpeeds();
			TASK_END(TASK_IMU_SPEEDS);
		} break;
		}
		if (++imuUpdateCycle >= 8) imuUpdateCycle = 0;
		TASK_END(TASK_IMU);

		if (armed) {
			controlLoop();
			decodeErpm();
			pidLoop();
		} else {
			controlDisarmedLoop();
			decodeErpm();
			pidDisarmedLoop();
		}
	}
	TASK_END(TASK_LOOP1);
	taskTimer = 0;
}
