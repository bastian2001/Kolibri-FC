/**
 * @file gps.cpp
 * @brief GPS (UBX) serial parser functions
 *
 * Copyright (c) 2026 Kolibri-FC contributors
 *
 * This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
 *
 * Kolibri-FC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kolibri-FC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.
 */

#include "global.h"

RingBuffer<u8> gpsBuffer(1024);
static elapsedMicros gpsInitTimer;
static bool gpsInitAck = false;
GpsAccuracy gpsAcc;
struct tm gpsTime;
GpsStatus gpsStatus;
GpsMotion gpsMotion;
fix64 gpsLatitudeFiltered, gpsLongitudeFiltered;
static char olcString[14] = "AABBCCDD+EEFG";
static char olcAlphabet[] = "23456789CFGHJMPQRVWX";
u8 currentPvtMsg[92];
u32 newPvtMessageFlag = 0;
u32 gpsUpdateRate;
fix32 gpsVelocityFilterCutoff;
static KoliSerial *gpsSerial = nullptr;
bool gpsGoodQuality = false;
static bool firstGoodQuality = true;

int gpsSerialSpeed = 38400;
u8 retryCounter = 0;
elapsedMicros lastPvtMessage = 0;

void gpsChecksum(const u8 *buf, int len, u8 *ck_a, u8 *ck_b) {
	*ck_a = 0;
	*ck_b = 0;
	for (int i = 0; i < len; i++) {
		*ck_a += buf[i];
		*ck_b += *ck_a;
	}
}

void setGpsSerial(KoliSerial *g) {
	gpsSerial = g;
}

void initGPS() {}

void fillOpenLocationCode() {
	u32 lat = gpsMotion.lat / 1250 + 720000;
	u32 lon = gpsMotion.lon / 1250 + 1440000;
	olcString[10] = olcAlphabet[lon % 20];
	olcString[9] = olcAlphabet[lat % 20];
	for (int i = 7; i > 0; i -= 2) {
		lat /= 20;
		lon /= 20;
		olcString[i] = olcAlphabet[lon % 20];
		olcString[i - 1] = olcAlphabet[lat % 20];
	}
	// writing to characters 12 and 11 to enhance precision (10 is +)
	lat = gpsMotion.lat / 50;
	lon = ((i64)gpsMotion.lon * 8) / 625;
	olcString[12] = olcAlphabet[(lat % 5) * 4 + (lon % 4)];
	lat /= 5;
	lon /= 4;
	olcString[11] = olcAlphabet[(lat % 5) * 4 + (lon % 4)];
}

void gpsLoop() {
	if (gpsSerial == nullptr) return;
	TASK_START(TASK_GPS);
	if (lastPvtMessage > 2000000) {
		// no PVT message received for 1 second
		gpsStatus.fixType = fixTypes::FIX_NONE;
		if (gpsStatus.gpsInited) {
			gpsStatus.gpsInited = false;
			lastPvtMessage = 0;
			gpsStatus.initStep = 0;
			gpsInitAck = false;
			gpsInitTimer = 0;
			lastPvtMessage = 0;
		}
	}
	// UBX implementation
	if (!gpsStatus.gpsInited && (gpsInitAck || gpsInitTimer > 1000000)) {
		switch (gpsStatus.initStep) {
		case 0: {
			if (retryCounter++ % 2 == 0) {
				gpsSerialSpeed = 153600 - gpsSerialSpeed;
				gpsSerial->end();
				gpsSerial->begin(gpsSerialSpeed);
			}
			u8 msgSetupUart[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_PRT,
								 0x14, 0x00, // length 20
								 0x01, 0x00, 0x00, 0x00, // port identifier U1, reserved U1, txReady pin config X2
								 0xD0, 0x08, 0x00, 0x00, // UART mode (8N1) X4
								 0x00, 0xC2, 0x01, 0x00, // baudrate (115200) U4
								 0x01, 0x00, 0x01, 0x00, // in proto X2, out proto X2 (UBX only)
								 0x00, 0x00, 0x00, 0x00, // flags X2, reserved U1[2]
								 0, 0}; // checksum X1[2]
			gpsChecksum(&msgSetupUart[2], 24, &msgSetupUart[26], &msgSetupUart[27]);
			gpsSerial->write(msgSetupUart, 28);
			gpsInitAck = false;
			gpsInitTimer = 0;
		} break;
		case 1: {
			gpsSerial->end();
			gpsSerial->begin(115200);
			u8 msgDisableGxGGA[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG,
									0x03, 0x00, // length 3
									NMEA_CLASS_STANDARD, // message class U1
									NMEA_ID_GGA, // message ID U1
									0, // rate (0 = disable, 1+ = divider) U1
									0, 0};
			gpsChecksum(&msgDisableGxGGA[2], 7, &msgDisableGxGGA[9], &msgDisableGxGGA[10]);
			gpsSerial->write(msgDisableGxGGA, 11);
			gpsInitAck = false;
			gpsInitTimer = 0;
		} break;
		case 2: {
			u8 msgDisableGxGSA[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG, 0x03, 0x00, NMEA_CLASS_STANDARD, NMEA_ID_GSA, 0, 0, 0};
			gpsChecksum(&msgDisableGxGSA[2], 7, &msgDisableGxGSA[9], &msgDisableGxGSA[10]);
			gpsSerial->write(msgDisableGxGSA, 11);
			gpsInitAck = false;
			gpsInitTimer = 0;
		} break;
		case 3: {
			u8 msgDisableGxGSV[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG, 0x03, 0x00, NMEA_CLASS_STANDARD, NMEA_ID_GSV, 0, 0, 0};
			gpsChecksum(&msgDisableGxGSV[2], 7, &msgDisableGxGSV[9], &msgDisableGxGSV[10]);
			gpsSerial->write(msgDisableGxGSV, 11);
			gpsInitAck = false;
			gpsInitTimer = 0;
		} break;
		case 4: {
			u8 msgDisableGxRMC[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG, 0x03, 0x00, NMEA_CLASS_STANDARD, NMEA_ID_RMC, 0, 0, 0};
			gpsChecksum(&msgDisableGxRMC[2], 7, &msgDisableGxRMC[9], &msgDisableGxRMC[10]);
			gpsSerial->write(msgDisableGxRMC, 11);
			gpsInitAck = false;
			gpsInitTimer = 0;
		} break;
		case 5: {
			u8 msgDisableGxVTG[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG, 0x03, 0x00, NMEA_CLASS_STANDARD, NMEA_ID_VTG, 0, 0, 0};
			gpsChecksum(&msgDisableGxVTG[2], 7, &msgDisableGxVTG[9], &msgDisableGxVTG[10]);
			gpsSerial->write(msgDisableGxVTG, 11);
			gpsInitAck = false;
			gpsInitTimer = 0;
		} break;
		case 6: {
			u8 msgEnableNavPvt[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG,
									0x03, 0x00, // length 3
									UBX_CLASS_NAV, // message class U1
									UBX_ID_NAV_PVT, // message ID U1
									1, // rate (0 = disable, 1+ = divider) U1
									0, 0};
			gpsChecksum(&msgEnableNavPvt[2], 7, &msgEnableNavPvt[9], &msgEnableNavPvt[10]);
			gpsSerial->write(msgEnableNavPvt, 11);
			gpsInitAck = false;
			gpsInitTimer = 0;
		} break;
		case 7: {
			u8 msgDisableGxGLL[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG, 0x03, 0x00, NMEA_CLASS_STANDARD, NMEA_ID_GLL, 0, 0, 0};
			gpsChecksum(&msgDisableGxGLL[2], 7, &msgDisableGxGLL[9], &msgDisableGxGLL[10]);
			gpsSerial->write(msgDisableGxGLL, 11);
			gpsInitAck = false;
			gpsInitTimer = 0;
		} break;
		case 8: {
			u16 milliseconds = 1000 / gpsUpdateRate;
			u8 msgSetNavRate[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_RATE,
								  0x06, 0x00, // length 6
								  (u8)(milliseconds & 0xFF), (u8)(milliseconds >> 8), // measurement interval in ms U2
								  0x01, 0x00, // navRate divider U2
								  0x01, 0x00, // time system alignment (1 = GPS time) U2
								  0x00, 0x00};
			gpsChecksum(&msgSetNavRate[2], 10, &msgSetNavRate[12], &msgSetNavRate[13]);
			gpsSerial->write(msgSetNavRate, 14);
			gpsInitAck = false;
			gpsInitTimer = 0;
		} break;
		case 9:
			gpsStatus.gpsInited = true;
			break;
		}
	}
	if (gpsBuffer.itemCount() >= 8) {
		if (gpsBuffer[0] != UBX_SYNC1 || gpsBuffer[1] != UBX_SYNC2) {
			gpsBuffer.pop();
			tasks[TASK_GPS].errorCount++;
			tasks[TASK_GPS].lastError = 1;
			TASK_END(TASK_GPS);
			return;
		}
		int len = gpsBuffer[4] | (gpsBuffer[5] << 8);
		if (len > GPS_BUF_LEN - 8) {
			gpsBuffer.pop();
			tasks[TASK_GPS].errorCount++;
			tasks[TASK_GPS].lastError = 2;
			TASK_END(TASK_GPS);
			return;
		}
		if (gpsBuffer.itemCount() < len + 8) {
			TASK_END(TASK_GPS);
			return;
		}
		u8 msg[len + 8];
		gpsBuffer.copyToArray(msg, 2, len + 6);
		u8 ck_a, ck_b;
		gpsChecksum(msg, len + 4, &ck_a, &ck_b);
		if (ck_a != msg[len + 4] || ck_b != msg[len + 5]) {
			gpsBuffer.pop();
			tasks[TASK_GPS].errorCount++;
			tasks[TASK_GPS].lastError = 3;
			TASK_END(TASK_GPS);
			return;
		}
		TASK_START(TASK_GPS_MSG);
		u8 *msgData = &msg[4];
		// ensured that the packet is valid
		u32 id = msg[1], classId = msg[0];

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
				if (len != 92) {
					gpsBuffer.pop();
					return;
				}
				static u32 goodTimes = 0;
				memcpy(currentPvtMsg, msgData, 92);
				lastPvtMessage = 0;
				newPvtMessageFlag = 0xFFFFFFFF;
				gpsTime.tm_year = DECODE_U2(&msgData[4]);
				gpsTime.tm_mon = msgData[6];
				gpsTime.tm_mday = msgData[7];
				gpsTime.tm_hour = msgData[8];
				gpsTime.tm_min = msgData[9];
				gpsTime.tm_sec = msgData[10];
				gpsStatus.timeValidityFlags = msgData[11];
				gpsAcc.tAcc = DECODE_U4(&msgData[12]);
				gpsStatus.fixType = msgData[20];
				gpsStatus.flags = msgData[21];
				gpsStatus.flags2 = msgData[22];
				bool fullyResolved = (gpsStatus.timeValidityFlags & 0x04) == 0x04;
				bool valid = (gpsStatus.timeValidityFlags & 0x03) == 0x03;
				bool confirmed = (gpsStatus.flags2 & 0xC0) == 0xC0;
				u8 thisQuality = TIME_QUALITY_NONE;
				if (confirmed) {
					thisQuality = TIME_QUALITY_CONFIRMED;
				} else if (valid) {
					thisQuality = TIME_QUALITY_VALID;
				} else if (fullyResolved) {
					thisQuality = TIME_QUALITY_FULLY_RESOLVED;
				}
				if (thisQuality >= rtcTimeQuality) {
					// refresh from gpsTime every 1200 frames, typically 1 min at 20Hz, as long as the quality is not decreasing
					if (++goodTimes == 1200 || thisQuality > rtcTimeQuality) {
						goodTimes = (thisQuality > rtcTimeQuality) * 1100; // already update time 10s after the quality has settled
						struct timespec gpsTimespec;
						rtcConvertToTimespec(&gpsTime, &gpsTimespec);
						gpsTimespec.tv_nsec = DECODE_U4(&msgData[16]);
						rtcSetTime(&gpsTimespec, thisQuality);
					}
				}
				gpsStatus.satCount = msgData[23];
				gpsMotion.lon = DECODE_I4(&msgData[24]);
				gpsMotion.lat = DECODE_I4(&msgData[28]);
				gpsMotion.alt = DECODE_I4(&msgData[36]);
				gpsAcc.hAcc = DECODE_U4(&msgData[40]);
				gpsAcc.vAcc = DECODE_U4(&msgData[44]);
				gpsMotion.velN = DECODE_I4(&msgData[48]);
				gpsMotion.velE = DECODE_I4(&msgData[52]);
				gpsMotion.velD = DECODE_I4(&msgData[56]);
				gpsMotion.gSpeed = DECODE_I4(&msgData[60]);
				gpsMotion.headMot = DECODE_I4(&msgData[64]);
				gpsAcc.sAcc = DECODE_U4(&msgData[68]);
				gpsAcc.headAcc = DECODE_U4(&msgData[72]);
				gpsAcc.pDop = DECODE_U2(&msgData[76]);
				gpsStatus.flags3 = DECODE_U2(&msgData[78]);
				fix64 lat64 = fix64(gpsMotion.lat) / 10000000;
				fix64 lon64 = fix64(gpsMotion.lon) / 10000000;

				gpsGoodQuality = gpsStatus.fixType == FIX_3D &&
								 gpsStatus.satCount >= 6 &&
								 gpsAcc.hAcc < 20000 &&
								 gpsAcc.vAcc < 20000;
				if (gpsGoodQuality && firstGoodQuality) {
					firstGoodQuality = false;
					eVelFilter.set(fix32(0.001f) * gpsMotion.velE);
					nVelFilter.set(fix32(0.001f) * gpsMotion.velN);
					gpsLatitudeFiltered = lat64;
					gpsLongitudeFiltered = lon64;
					if (altInitState < 2) altInitState = 2;
				} else {
					eVelFilter.update(fix32(0.001f) * gpsMotion.velE);
					nVelFilter.update(fix32(0.001f) * gpsMotion.velN);
					gpsLatitudeFiltered = (gpsLatitudeFiltered * 3 + lat64) / 4;
					gpsLongitudeFiltered = (gpsLongitudeFiltered * 3 + lon64) / 4;
				}
				if (gpsGoodQuality) {
					gpsBaroAlt.setRaw(((i64)gpsMotion.alt << 16) / 1000);
					// armingDisableFlags &= ~0x04;
				} // else {
				// armingDisableFlags |= 0x04;
				// }
			} break;
			}
		}
		}
		// pop the packet
		gpsBuffer.erase(len + 8);
		TASK_END(TASK_GPS_MSG);
	}
	TASK_END(TASK_GPS);
}
