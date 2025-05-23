#include "global.h"

RingBuffer<u8> gpsBuffer(1024);
elapsedMillis gpsInitTimer;
bool gpsInitAck = false;
GpsAccuracy gpsAcc;
struct tm gpsTime;
GpsStatus gpsStatus;
GpsMotion gpsMotion;
fix64 gpsLatitudeFiltered, gpsLongitudeFiltered;
char olcString[14] = "AABBCCDD+EEFG";
char olcAlphabet[] = "23456789CFGHJMPQRVWX";
u8 currentPvtMsg[92];
u32 newPvtMessageFlag = 0;
u32 gpsUpdateRate;
fix32 gpsVelocityFilterCutoff;

int gpsSerialSpeed = 38400;
u8 retryCounter = 0;
elapsedMillis lastPvtMessage = 0;

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
	placeElem(OSDElem::PLUS_CODE, 6, 12);
	enableElem(OSDElem::LATITUDE);
	enableElem(OSDElem::LONGITUDE);
	enableElem(OSDElem::ALTITUDE);
	enableElem(OSDElem::GROUND_SPEED);
	enableElem(OSDElem::HEADING);
	enableElem(OSDElem::HOME_DISTANCE);
	enableElem(OSDElem::GPS_STATUS);
	enableElem(OSDElem::PLUS_CODE);
}

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
	if (lastPvtMessage > 1000) {
		// no PVT message received for 1 second
		gpsStatus.fixType = fixTypes::FIX_NONE;
		if (gpsStatus.gpsInited) {
			gpsStatus.gpsInited = false;
			lastPvtMessage = 0;
		} else if (lastPvtMessage > 30000) {
			// no PVT message received for 30 seconds
			// reinit GPS
			gpsStatus.initStep = 0;
			gpsInitAck = false;
			gpsInitTimer = 0;
			gpsStatus.gpsInited = false;
			lastPvtMessage = 0;
		}
	}
	// UBX implementation
	if (!gpsStatus.gpsInited && (gpsInitAck || gpsInitTimer > 1000)) {
		switch (gpsStatus.initStep) {
		case 0: {
			if (retryCounter++ % 2 == 0) {
				gpsSerialSpeed = 153600 - gpsSerialSpeed;
				Serial2.end();
				Serial2.begin(gpsSerialSpeed);
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
			Serial2.write(msgSetupUart, 28);
			gpsInitAck = false;
			gpsInitTimer = 0;
		} break;
		case 1: {
			Serial2.end();
			Serial2.begin(115200);
			u8 msgDisableGxGGA[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG,
									0x03, 0x00, // length 3
									NMEA_CLASS_STANDARD, // message class U1
									NMEA_ID_GGA, // message ID U1
									0, // rate (0 = disable, 1+ = divider) U1
									0, 0};
			gpsChecksum(&msgDisableGxGGA[2], 7, &msgDisableGxGGA[9], &msgDisableGxGGA[10]);
			Serial2.write(msgDisableGxGGA, 11);
			gpsInitAck = false;
			gpsInitTimer = 0;
		} break;
		case 2: {
			u8 msgDisableGxGSA[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG, 0x03, 0x00, NMEA_CLASS_STANDARD, NMEA_ID_GSA, 0, 0, 0};
			gpsChecksum(&msgDisableGxGSA[2], 7, &msgDisableGxGSA[9], &msgDisableGxGSA[10]);
			Serial2.write(msgDisableGxGSA, 11);
			gpsInitAck = false;
			gpsInitTimer = 0;
		} break;
		case 3: {
			u8 msgDisableGxGSV[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG, 0x03, 0x00, NMEA_CLASS_STANDARD, NMEA_ID_GSV, 0, 0, 0};
			gpsChecksum(&msgDisableGxGSV[2], 7, &msgDisableGxGSV[9], &msgDisableGxGSV[10]);
			Serial2.write(msgDisableGxGSV, 11);
			gpsInitAck = false;
			gpsInitTimer = 0;
		} break;
		case 4: {
			u8 msgDisableGxRMC[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG, 0x03, 0x00, NMEA_CLASS_STANDARD, NMEA_ID_RMC, 0, 0, 0};
			gpsChecksum(&msgDisableGxRMC[2], 7, &msgDisableGxRMC[9], &msgDisableGxRMC[10]);
			Serial2.write(msgDisableGxRMC, 11);
			gpsInitAck = false;
			gpsInitTimer = 0;
		} break;
		case 5: {
			u8 msgDisableGxVTG[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG, 0x03, 0x00, NMEA_CLASS_STANDARD, NMEA_ID_VTG, 0, 0, 0};
			gpsChecksum(&msgDisableGxVTG[2], 7, &msgDisableGxVTG[9], &msgDisableGxVTG[10]);
			Serial2.write(msgDisableGxVTG, 11);
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
			Serial2.write(msgEnableNavPvt, 11);
			gpsInitAck = false;
			gpsInitTimer = 0;
		} break;
		case 7: {
			u8 msgDisableGxGLL[] = {UBX_SYNC1, UBX_SYNC2, UBX_CLASS_CFG, UBX_ID_CFG_MSG, 0x03, 0x00, NMEA_CLASS_STANDARD, NMEA_ID_GLL, 0, 0, 0};
			gpsChecksum(&msgDisableGxGLL[2], 7, &msgDisableGxGLL[9], &msgDisableGxGLL[10]);
			Serial2.write(msgDisableGxGLL, 11);
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
			Serial2.write(msgSetNavRate, 14);
			gpsInitAck = false;
			gpsInitTimer = 0;
		} break;
		case 9:
			gpsStatus.gpsInited = true;
			break;
		}
	}
	if (gpsBuffer.itemCount() >= 8) {
		elapsedMicros taskTimer = 0;
		if (gpsBuffer[0] != UBX_SYNC1 || gpsBuffer[1] != UBX_SYNC2) {
			gpsBuffer.pop();
			return;
		}
		int len = gpsBuffer[4] | (gpsBuffer[5] << 8);
		if (len > GPS_BUF_LEN - 8) {
			gpsBuffer.pop();
			return;
		}
		if (gpsBuffer.itemCount() < len + 8) {
			return;
		}
		tasks[TASK_GPS].runCounter++;
		u8 msg[len + 8];
		gpsBuffer.copyToArray(msg, 2, len + 6);
		u8 ck_a, ck_b;
		gpsChecksum(msg, len + 4, &ck_a, &ck_b);
		if (ck_a != msg[len + 4] || ck_b != msg[len + 5]) {
			gpsBuffer.pop();
			return;
		}
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
				eVel.update(fix32(0.001f) * gpsMotion.velE);
				nVel.update(fix32(0.001f) * gpsMotion.velN);
				static fix64 lastLat = fix64(gpsMotion.lat) / 10000000;
				static fix64 lastLon = fix64(gpsMotion.lon) / 10000000;
				gpsLatitudeFiltered = (gpsLatitudeFiltered * 3 + fix64(gpsMotion.lat) / 10000000) / 4;
				gpsLongitudeFiltered = (gpsLongitudeFiltered * 3 + fix64(gpsMotion.lon) / 10000000) / 4;
				u8 buf[16];
				snprintf((char *)buf, 16, "\x89%.7f", gpsMotion.lat / 10000000.f);
				updateElem(OSDElem::LATITUDE, (char *)buf);
				snprintf((char *)buf, 16, "\x98%.7f", gpsMotion.lon / 10000000.f);
				updateElem(OSDElem::LONGITUDE, (char *)buf);
				snprintf((char *)buf, 16, "\x7F%d\x0C ", combinedAltitude.geti32());
				updateElem(OSDElem::ALTITUDE, (char *)buf);
				fix32 gVel = fix32(3.6f) * sqrtf(((fix32)eVel * (fix32)eVel + (fix32)nVel * (fix32)nVel).getf32());
				snprintf((char *)buf, 16, "%d\x9E ", (gVel + fix32(0.5f)).geti32());
				updateElem(OSDElem::GROUND_SPEED, (char *)buf);
				snprintf((char *)buf, 16, "%dD ", (combinedHeading * FIX_RAD_TO_DEG).geti32());
				updateElem(OSDElem::HEADING, (char *)buf);
				f32 toRadians = 1.745329251e-9f;
				f32 sin1 = sinf((gpsMotion.lat - startPointLat) * (toRadians / 2));
				f32 sin2 = sinf((gpsMotion.lon - startPointLon) * (toRadians / 2));

				int dist = 2 * 6371000 *
						   asinf(sqrtf(sin1 * sin1 + cosf(gpsMotion.lat * toRadians) * cosf(startPointLat * toRadians) * sin2 * sin2));
				snprintf((char *)buf, 16, "\x11%d\x0C  ", dist);
				updateElem(OSDElem::HOME_DISTANCE, (char *)buf);
				snprintf((char *)buf, 16, "\x1E\x1F%d  ", gpsStatus.satCount);
				updateElem(OSDElem::GPS_STATUS, (char *)buf);
				fillOpenLocationCode();
				updateElem(OSDElem::PLUS_CODE, olcString);
				if (gpsStatus.fixType == FIX_3D && gpsStatus.satCount >= 6) {
					gpsBaroAlt.setRaw(((i64)gpsMotion.alt << 16) / 1000);
					// armingDisableFlags &= ~0x04;
				}
				// else
				// 	armingDisableFlags |= 0x04;
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
