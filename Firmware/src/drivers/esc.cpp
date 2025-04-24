#include "global.h"

u32 enableDShot = 0;

u32 escRawTelemetry[4] = {0};
u32 escRpm[4] = {0};
u32 escTemp[4] = {0};
fix32 escVoltage[4] = {0};
u32 escCurrent[4] = {0};
u8 escErpmFail = 0;
u16 dshotBeepTone = 1;

BidirDShotX1 *escs[4];

void initESCs() {
	for (int i = 0; i < 4; i++) {
		escs[3 - i] = new BidirDShotX1(PIN_MOTORS + i, 600, PIO_ESC);
	}
	enableDShot = 1;
}

void deinitESCs() {
	for (int i = 0; i < 4; i++) {
		delete escs[i];
	}
	enableDShot = 0;
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
	tasks[TASK_ESC_RPM].runCounter++;
	elapsedMicros taskTimer = 0;
	for (int m = 0; m < 4; m++) {
		BidirDshotTelemetryType telemType = escs[m]->getTelemetryRaw((uint32_t *)&escRawTelemetry[m]);
		u32 telemVal = BidirDShotX1::convertFromRaw(escRawTelemetry[m], telemType);
		switch (telemType) {
		case BidirDshotTelemetryType::ERPM:
			escRpm[m] = telemVal;
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
			escRawTelemetry[m] = 0;
			break;
		case BidirDshotTelemetryType::CHECKSUM_ERROR:
			tasks[TASK_ESC_RPM].errorCount++;
			tasks[TASK_ESC_RPM].lastError = 2;
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
	u32 duration = taskTimer;
	tasks[TASK_ESC_RPM].totalDuration += duration;
	if (duration > tasks[TASK_ESC_RPM].maxDuration)
		tasks[TASK_ESC_RPM].maxDuration = duration;
	if (duration < tasks[TASK_ESC_RPM].minDuration)
		tasks[TASK_ESC_RPM].minDuration = duration;
}
