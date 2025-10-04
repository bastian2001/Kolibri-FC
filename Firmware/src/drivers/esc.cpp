#include "global.h"

u32 enableDShot = 0;

u32 escRawTelemetry[4] = {0};
u32 escRpm[4] = {0};
u32 escTemp[4] = {0};
fix32 escVoltage[4] = {0};
u32 escCurrent[4] = {0};
u8 escErpmFail = 0;
u32 escErpmFailCounter = 0;
u16 dshotBeepTone = 1;
static u8 motorPins[4] = {0};

BidirDShotX1 *escs[4];

void initMotorPins() {
	for (int i = 0; i < 4; i++) {
		motorPins[i] = PIN_MOTORS + 3 - i;
	}
}

void initESCs() {
	addSetting(SETTING_BEEP_TONE, &dshotBeepTone, 2);
	addArraySetting(SETTING_MOTOR_PINS, motorPins, initMotorPins);

	for (int i = 0; i < 4; i++) {
		escs[i] = new BidirDShotX1(motorPins[i], 600, PIO_ESC);
	}
	enableDShot = 1;
}

void deinitESCs() {
	for (int i = 0; i < 4; i++) {
		delete escs[i];
	}
	enableDShot = 0;
}

bool updateMotorPins(const u8 newPins[4]) {
	for (int i = 0; i < 4; i++) {
		for (int j = i + 1; j < 4; j++) {
			if (newPins[i] == newPins[j]) return false;
		}
	}
	for (int i = 0; i < 4; i++) {
		motorPins[i] = newPins[i];
	}
	for (int i = 0; i < 4; i++) {
		delete escs[i];
	}
	for (int i = 0; i < 4; i++) {
		escs[i] = new BidirDShotX1(motorPins[i], 600, PIO_ESC);
	}
	getSetting(SETTING_MOTOR_PINS)->updateSettingInFile();
	return true;
}

void getMotorPins(u8 pins[4]) {
	for (int i = 0; i < 4; i++) {
		pins[i] = motorPins[i];
	}
}

void sendRaw11Bit(const u16 raw[4]) {
	if (!enableDShot) return;
	for (int i = 0; i < 4; i++) {
		escs[i]->sendRaw11Bit(raw[i]);
	}
}

void sendThrottles(const i16 throttles[4]) {
	if (!enableDShot) return;
	for (int i = 0; i < 4; i++) {
		i16 t = throttles[i];
		if (t < 0) t = 0;
		escs[i]->sendThrottle(t);
	}
}

void decodeErpm() {
	if (!enableDShot) return;
	TASK_START(TASK_ESC_RPM);
	escErpmFail = 0;
	for (int m = 0; m < 4; m++) {
		BidirDshotTelemetryType telemType = escs[m]->getTelemetryRaw((uint32_t *)&escRawTelemetry[m]);
		u32 telemVal = BidirDShotX1::convertFromRaw(escRawTelemetry[m], telemType);
		switch (telemType) {
		case BidirDshotTelemetryType::ERPM:
			if (escRpm[m] >= 0)
				escRpm[m] = telemVal / (MOTOR_POLES / 2);
			else
				escErpmFail |= 1 << m;
			break;
		case BidirDshotTelemetryType::TEMPERATURE:
			escTemp[m] = telemVal;
			break;
		case BidirDshotTelemetryType::VOLTAGE:
			escVoltage[m] = fix32(telemVal) >> 2;
			break;
		case BidirDshotTelemetryType::CURRENT:
			escCurrent[m] = telemVal;
			break;
		case BidirDshotTelemetryType::NO_PACKET:
			tasks[TASK_ESC_RPM].errorCount++;
			tasks[TASK_ESC_RPM].lastError = 1;
			escErpmFail |= 1 << m;
			escRawTelemetry[m] = 0;
			break;
		case BidirDshotTelemetryType::CHECKSUM_ERROR:
			tasks[TASK_ESC_RPM].errorCount++;
			tasks[TASK_ESC_RPM].lastError = 2;
			escErpmFail |= 1 << m;
			escRawTelemetry[m] = 0;
			break;
		case BidirDshotTelemetryType::STATUS:
		case BidirDshotTelemetryType::STRESS:
		case BidirDshotTelemetryType::DEBUG_FRAME_1:
		case BidirDshotTelemetryType::DEBUG_FRAME_2:
			escRawTelemetry[m] = 0;
		default:
			break;
		}
	}
	if (escErpmFail)
		escErpmFailCounter++;
	else
		escErpmFailCounter = 0;
	TASK_END(TASK_ESC_RPM);
}
