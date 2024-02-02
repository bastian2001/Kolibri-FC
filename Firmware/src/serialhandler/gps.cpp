#include "global.h"

RingBuffer<u8> gpsBuffer(1024);
elapsedMillis gpsInitTimer;
bool gpsInitAck = false;
GpsAccuracy gpsAcc;
GpsTime gpsTime;
GpsStatus gpsStatus;
GpsMotion gpsMotion;
i32 headingAdjustment = 0;
/*
 * 0-5: seconds
 * 6-11: minutes
 * 12-16: hours
 * 17-21: days
 * 22-25: months
 * 26-31: years, since 2020
 * This is valid till 2084, which is enough
 */
u32 timestamp = 0; // since unix epoch is hard to calculate, the 32 bits are distributed as described above

void gpsChecksum(const u8 *buf, int len, u8 *ck_a, u8 *ck_b) {
	*ck_a = 0;
	*ck_b = 0;
	for (int i = 0; i < len; i++) {
		*ck_a += buf[i];
		*ck_b += *ck_a;
	}
}

void initGPS() {
	Serial2.setFIFOSize(1024);
	Serial2.begin(38400);

	placeElem(OSDElem::LATITUDE, 1, 13);
	placeElem(OSDElem::LONGITUDE, 13, 13);
	placeElem(OSDElem::ALTITUDE, 1, 14);
	placeElem(OSDElem::GROUND_SPEED, 10, 14);
	placeElem(OSDElem::HEADING, 15, 14);
	placeElem(OSDElem::HOME_DISTANCE, 23, 14);
	placeElem(OSDElem::GPS_STATUS, 1, 12);
	enableElem(OSDElem::LATITUDE);
	enableElem(OSDElem::LONGITUDE);
	enableElem(OSDElem::ALTITUDE);
	enableElem(OSDElem::GROUND_SPEED);
	enableElem(OSDElem::HEADING);
	enableElem(OSDElem::HOME_DISTANCE);
	enableElem(OSDElem::GPS_STATUS);
}

int gpsSerialSpeed			 = 38400;
u8 retryCounter				 = 0;
elapsedMillis lastPvtMessage = 0;

void gpsLoop() {
	// UBX implementation
	if (!gpsStatus.gpsInited && (gpsInitAck || gpsInitTimer > 1000)) {
		switch (gpsStatus.initStep) {
		case 0: {
			if (retryCounter++ % 2 == 0) {
				gpsSerialSpeed = 153600 - gpsSerialSpeed;
				Serial2.end();
				Serial2.begin(gpsSerialSpeed);
			}
			u8 msgSetupUart[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_PRT, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0xD0, 0x08, 0x00, 0x00, 0x00, 0xC2, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
			gpsChecksum(&msgSetupUart[2], 24, &msgSetupUart[26], &msgSetupUart[27]);
			Serial2.write(msgSetupUart, 28);
			gpsInitAck	 = false;
			gpsInitTimer = 0;
		} break;
		case 1: {
			Serial2.end();
			Serial2.begin(115200);
			u8 msgDisableGxGGA[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG, 0x03, 0x00, 0xF0, 0x00, 0x00, 0, 0};
			gpsChecksum(&msgDisableGxGGA[2], 7, &msgDisableGxGGA[9], &msgDisableGxGGA[10]);
			Serial2.write(msgDisableGxGGA, 11);
			gpsInitAck	 = false;
			gpsInitTimer = 0;
		} break;
		case 2: {
			u8 msgDisableGxGSA[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG, 0x03, 0x00, 0xF0, 0x02, 0x00, 0, 0};
			gpsChecksum(&msgDisableGxGSA[2], 7, &msgDisableGxGSA[9], &msgDisableGxGSA[10]);
			Serial2.write(msgDisableGxGSA, 11);
			gpsInitAck	 = false;
			gpsInitTimer = 0;
		} break;
		case 3: {
			u8 msgDisableGxGSV[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG, 0x03, 0x00, 0xF0, 0x03, 0x00, 0, 0};
			gpsChecksum(&msgDisableGxGSV[2], 7, &msgDisableGxGSV[9], &msgDisableGxGSV[10]);
			Serial2.write(msgDisableGxGSV, 11);
			gpsInitAck	 = false;
			gpsInitTimer = 0;
		} break;
		case 4: {
			u8 msgDisableGxRMC[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG, 0x03, 0x00, 0xF0, 0x04, 0x00, 0, 0};
			gpsChecksum(&msgDisableGxRMC[2], 7, &msgDisableGxRMC[9], &msgDisableGxRMC[10]);
			Serial2.write(msgDisableGxRMC, 11);
			gpsInitAck	 = false;
			gpsInitTimer = 0;
		} break;
		case 5: {
			u8 msgDisableGxVTG[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG, 0x03, 0x00, 0xF0, 0x05, 0x00, 0, 0};
			gpsChecksum(&msgDisableGxVTG[2], 7, &msgDisableGxVTG[9], &msgDisableGxVTG[10]);
			Serial2.write(msgDisableGxVTG, 11);
			gpsInitAck	 = false;
			gpsInitTimer = 0;
		} break;
		case 6: {
			u8 msgEnableNavPvt[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG, 0x03, 0x00, 0x01, 0x07, 0x01, 0, 0};
			gpsChecksum(&msgEnableNavPvt[2], 7, &msgEnableNavPvt[9], &msgEnableNavPvt[10]);
			Serial2.write(msgEnableNavPvt, 11);
			gpsInitAck	 = false;
			gpsInitTimer = 0;
		} break;
		case 7: {
			u8 msgDisableGxGLL[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG, 0x03, 0x00, 0xF0, 0x01, 0x00, 0, 0};
			gpsChecksum(&msgDisableGxGLL[2], 7, &msgDisableGxGLL[9], &msgDisableGxGLL[10]);
			Serial2.write(msgDisableGxGLL, 11);
			gpsInitAck	 = false;
			gpsInitTimer = 0;
		} break;
		case 8: {
			u8 msgSetNavRate[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_RATE, 0x06, 0x00, 0x64, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00};
			gpsChecksum(&msgSetNavRate[2], 10, &msgSetNavRate[12], &msgSetNavRate[13]);
			Serial2.write(msgSetNavRate, 14);
			gpsInitAck	 = false;
			gpsInitTimer = 0;
		} break;
		case 9:
			gpsStatus.gpsInited = true;
			break;
		}
	}
	if (gpsBuffer.itemCount() >= 8) {
		elapsedMicros taskTimer = 0;
		int len					= gpsBuffer[4] + gpsBuffer[5] * 256;
		if (gpsBuffer[0] != UBX_SYNC1 || gpsBuffer[1] != UBX_SYNC2) {
			gpsBuffer.pop();
			return;
		}
		if (len > GPS_BUF_LEN - 8) {
			gpsBuffer.pop();
			return;
		}
		if (gpsBuffer.itemCount() < len + 8) {
			return;
		}
		tasks[TASK_GPS].runCounter++;
		u8 ck_a, ck_b;
		gpsChecksum(&(gpsBuffer[2]), len + 4, &ck_a, &ck_b);
		if (ck_a != gpsBuffer[len + 6] || ck_b != gpsBuffer[len + 7]) {
			gpsBuffer.pop();
			return;
		}
		// ensured that the packet is valid
		u32 id = gpsBuffer[3], classId = gpsBuffer[2];

		u8 msgData[len];
		gpsBuffer.copyToArray(msgData, 6, len);
		switch (classId) {
		case UBX_CLASS_ACK: {
			switch (id) {
			case UBX_ID_ACK_ACK:
				if (gpsStatus.initStep < 9 && len == 2 && msgData[0] == UBX_CLASS_CFG && (msgData[1] == UBX_ID_CFG_MSG || msgData[1] == UBX_ID_CFG_PRT || msgData[1] == UBX_ID_CFG_RATE)) {
					gpsStatus.initStep++;
					gpsInitAck = true;
				}
				break;
			}
		} break;
		case UBX_CLASS_NAV: {
			switch (id) {
			case UBX_ID_NAV_PVT: {
				lastPvtMessage				= 0;
				gpsTime.year				= DECODE_U2(&msgData[4]);
				gpsTime.month				= msgData[6];
				gpsTime.day					= msgData[7];
				gpsTime.hour				= msgData[8];
				gpsTime.minute				= msgData[9];
				gpsTime.second				= msgData[10];
				gpsStatus.timeValidityFlags = msgData[11];
				gpsAcc.tAcc					= DECODE_U4(&msgData[12]);
				gpsStatus.fixType			= msgData[20];
				gpsStatus.flags				= msgData[21];
				gpsStatus.flags2			= msgData[22];
				gpsStatus.satCount			= msgData[23];
				gpsMotion.lat				= DECODE_I4(&msgData[24]);
				gpsMotion.lon				= DECODE_I4(&msgData[28]);
				gpsMotion.alt				= DECODE_I4(&msgData[36]);
				gpsAcc.hAcc					= DECODE_U4(&msgData[40]);
				gpsAcc.vAcc					= DECODE_U4(&msgData[44]);
				gpsMotion.velN				= DECODE_I4(&msgData[48]);
				gpsMotion.velE				= DECODE_I4(&msgData[52]);
				gpsMotion.velD				= DECODE_I4(&msgData[56]);
				gpsMotion.gSpeed			= DECODE_I4(&msgData[60]);
				gpsMotion.headMot			= DECODE_I4(&msgData[64]);
				gpsAcc.sAcc					= DECODE_U4(&msgData[68]);
				gpsAcc.headAcc				= DECODE_U4(&msgData[72]);
				gpsAcc.pDop					= DECODE_U2(&msgData[76]);
				gpsStatus.flags3			= DECODE_U2(&msgData[78]);
				timestamp					= gpsTime.second & 0x3F | (gpsTime.minute & 0x3F) << 6 | (gpsTime.hour & 0x1F) << 12 | (gpsTime.day & 0x1F) << 17 | (gpsTime.month & 0x0F) << 22 | (gpsTime.year - 2020) << 26;
				static i32 lastHeadMot		= 0;
				if (gpsStatus.fixType == fixTypes::FIX_3D && gpsMotion.gSpeed > 5000 && lastHeadMot != gpsMotion.headMot && gpsAcc.headAcc < 200000 && gpsStatus.satCount >= 6) {
					// speed > 18 km/h, thus the heading is likely valid
					// heading changed (if not, then that means the GPS module just took the old heading, which is probably inaccurate), and the heading accuracy is good (less than +-2°)
					// gps acts as a LPF to bring the heading into the right direction, while the gyro acts as a HPF to adjust the heading quickly
					i32 diff = gpsMotion.headMot - combinedHeadMot;
					if (diff > 18000000) diff -= 36000000;
					if (diff < -18000000) diff += 36000000;
					diff /= 20;
					headingAdjustment += diff;
					if (headingAdjustment > 18000000) headingAdjustment -= 36000000;
					if (headingAdjustment < -18000000) headingAdjustment += 36000000;
				}
				lastHeadMot = gpsMotion.headMot;
				if (gpsStatus.fixType == fixTypes::FIX_3D && gpsStatus.satCount >= 6) {
					vVel			 = fix32(0.9f) * vVel + fix32(0.0001f) * (int)gpsMotion.velD;
					combinedAltitude = fix32(0.9f) * combinedAltitude + fix32(0.0001f) * (int)gpsMotion.alt;
				}
				eVel = fix32(0.8f) * eVel + fix32(0.0002f) * (int)gpsMotion.velE;
				nVel = fix32(0.8f) * nVel + fix32(0.0002f) * (int)gpsMotion.velN;
				u8 buf[16];
				snprintf((char *)buf, 16, "\x89%.7f", gpsMotion.lat / 10000000.f);
				updateElem(OSDElem::LATITUDE, (char *)buf);
				snprintf((char *)buf, 16, "\x98%.7f", gpsMotion.lon / 10000000.f);
				updateElem(OSDElem::LONGITUDE, (char *)buf);
				// snprintf((char *)buf, 16, "\x7F%d\x0C ", gpsMotion.alt / 1000);
				// updateElem(OSDElem::ALTITUDE, (char *)buf);
				snprintf((char *)buf, 16, "%d\x9E ", gpsMotion.gSpeed / 278);
				updateElem(OSDElem::GROUND_SPEED, (char *)buf);
				snprintf((char *)buf, 16, "%dD ", combinedHeading / 100000);
				updateElem(OSDElem::HEADING, (char *)buf);
				f32 toRadians = 1.745329251e-9f;
				f32 sin1	  = sinf((gpsMotion.lat - startPointLat) * (toRadians / 2));
				f32 sin2	  = sinf((gpsMotion.lon - startPointLon) * (toRadians / 2));

				int dist = 2 * 6371000 *
						   asinf(sqrtf(sin1 * sin1 + cosf(gpsMotion.lat * toRadians) * cosf(startPointLat * toRadians) * sin2 * sin2));
				snprintf((char *)buf, 16, "\x11%d\x0C  ", dist);
				updateElem(OSDElem::HOME_DISTANCE, (char *)buf);
				snprintf((char *)buf, 16, "\x1E\x1F%d  ", gpsStatus.satCount);
				updateElem(OSDElem::GPS_STATUS, (char *)buf);
				// if (gpsStatus.fixType >= FIX_3D && gpsStatus.satCount >= 6)
				// 	armingDisableFlags &= 0xFFFFFFFB;
				// else
				// 	armingDisableFlags |= 0x00000004;
			} break;
			}
		}
		}
		// pop the packet
		gpsBuffer.erase(len + 8);
		u32 duration = taskTimer;
		tasks[TASK_GPS].totalDuration += duration;
		if (duration < tasks[TASK_GPS].minDuration) {
			tasks[TASK_GPS].minDuration = duration;
		}
		if (duration > tasks[TASK_GPS].maxDuration) {
			tasks[TASK_GPS].maxDuration = duration;
		}
	}
}