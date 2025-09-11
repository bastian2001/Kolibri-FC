#include "git_version.h"
#include "global.h"

#define SIGNATURE_LENGTH 32
#define TARGET_IDENTIFIER_LENGTH 4

u8 accelCalDone = 0;

elapsedMillis mspOverrideMotors = 1001;

static constexpr char targetIdentifier[] = "KD05";
static constexpr char targetFullName[] = "Kolibri Dev v0.5";

elapsedMicros lastConfigPingRx = 0;
bool configuratorConnected = false;

i16 mspDebugSensors[4] = {0, 0, 0, 0};

u8 lastMspSerial = 0;
MspVersion lastMspVersion = MspVersion::V2;

void configuratorLoop() {
	if (lastConfigPingRx > 1000000)
		configuratorConnected = false;
	if (accelCalDone) {
		accelCalDone = 0;
		char data = 1;
		sendMsp(lastMspSerial, MspMsgType::RESPONSE, MspFn::ACC_CALIBRATION, lastMspVersion, &data, 1);
		openSettingsFile();
		getSetting(SETTING_ACC_CAL)->updateSettingInFile();
		closeSettingsFile();
		sendMsp(lastMspSerial, MspMsgType::RESPONSE, MspFn::SAVE_SETTINGS, lastMspVersion);
	}
}

void sendMsp(u8 serialNum, MspMsgType type, MspFn fn, MspVersion version, const char *data, u16 len) {
	if (data == nullptr && len > 0) return;
	if (serialNum > ARRAYLEN(serials)) return;

	if (version == MspVersion::V1 && len > 254) version = MspVersion::V1_JUMBO;
	if (version == MspVersion::V2_OVER_V1 && len > 248) return;
	if (version == MspVersion::V1_OVER_CRSF && len > 254) version = MspVersion::V1_JUMBO_OVER_CRSF;

	bool versionHasV1 = version == MspVersion::V1 || version == MspVersion::V1_JUMBO || version == MspVersion::V2_OVER_V1 || version == MspVersion::V1_OVER_CRSF || version == MspVersion::V1_JUMBO_OVER_CRSF;
	bool versionHasV2 = version == MspVersion::V2 || version == MspVersion::V2_OVER_V1 || version == MspVersion::V2_OVER_CRSF;
	bool versionHasCrsf = version == MspVersion::V2_OVER_CRSF || version == MspVersion::V1_OVER_CRSF || version == MspVersion::V1_JUMBO_OVER_CRSF;
	if (!versionHasV2 && fn >= MspFn::MSP_V2_FRAME) return;
	Stream *ser = serials[serialNum].stream;

	if (versionHasCrsf) {
		u8 headerSize = 0;
		switch (version) {
		case MspVersion::V1_OVER_CRSF:
			headerSize = 2; // size, cmd
			break;
		case MspVersion::V2_OVER_CRSF:
			headerSize = 5; // flag, cmd (2), size (2)
			break;
		case MspVersion::V1_JUMBO_OVER_CRSF:
			headerSize = 4; // sizeV1=255, cmd, sizeJumbo (2)
			break;
		}
		char buf[len + headerSize] = {0};
		switch (version) {
		case MspVersion::V1_OVER_CRSF:
			buf[0] = len;
			buf[1] = (u8)fn;
			break;
		case MspVersion::V2_OVER_CRSF:
			buf[0] = 0; // flag
			buf[1] = (u8)fn;
			buf[2] = (u16)fn >> 8;
			buf[3] = len & 0xFF;
			buf[4] = len >> 8;
			break;
		case MspVersion::V1_JUMBO_OVER_CRSF:
			buf[0] = 255; // trigger jumbo
			buf[1] = (u8)fn;
			buf[2] = len & 0xFF;
			buf[3] = len >> 8;
			break;
		}
		memcpy(&buf[headerSize], data, len);
		ELRS->sendMspMsg(type, version == MspVersion::V2_OVER_CRSF ? 2 : 1, buf, len + headerSize);
	} else {
		u8 pos = 0;
		u8 header[12];
		u32 crcV1 = 0;
		u32 crcV2 = 0;
		header[pos++] = '$';
		header[pos++] = versionHasV1 ? 'M' : 'X';
		header[pos++] = (u8)type;
		switch (version) {
		case MspVersion::V1:
			header[pos++] = len;
			header[pos++] = (u8)fn;
			break;
		case MspVersion::V1_JUMBO:
			header[pos++] = 255;
			header[pos++] = (u8)fn;
			header[pos++] = len;
			header[pos++] = len >> 8;
			break;
		case MspVersion::V2_OVER_V1:
			header[pos++] = len + 6;
			header[pos++] = (u8)MspFn::MSP_V2_FRAME;
			header[pos++] = 0; // flag
			header[pos++] = (u16)fn;
			header[pos++] = (u16)fn >> 8;
			header[pos++] = len & 0xFF;
			header[pos++] = len >> 8;
			break;
		case MspVersion::V2:
			header[pos++] = 0; // flag
			header[pos++] = (u16)fn;
			header[pos++] = (u16)fn >> 8;
			header[pos++] = len & 0xFF;
			header[pos++] = len >> 8;
			break;
		}
		if (versionHasV2) {
			CRC_LUT_D5_APPLY(crcV2, 0); // flag
			CRC_LUT_D5_APPLY(crcV2, (u16)fn);
			CRC_LUT_D5_APPLY(crcV2, (u16)fn >> 8);
			CRC_LUT_D5_APPLY(crcV2, len & 0xFF);
			CRC_LUT_D5_APPLY(crcV2, len >> 8);
			for (int i = 0; i < len; i++) {
				CRC_LUT_D5_APPLY(crcV2, data[i]);
			}
		}
		if (versionHasV1) {
			for (int i = 3; i < pos; i++) {
				crcV1 ^= header[i];
			}
			for (int i = 0; i < len; i++) {
				crcV1 ^= data[i];
			}
			if (versionHasV2) {
				crcV1 ^= crcV2;
			}
		}
		ser->write(header, pos);
		ser->write(data, len);
		if (versionHasV2)
			ser->write((u8)crcV2);
		if (versionHasV1)
			ser->write((u8)crcV1);
	}
}

void processMspCmd(u8 serialNum, MspMsgType mspType, MspFn fn, MspVersion version, const char *reqPayload, u16 reqLen) {
	char buf[256] = {0};
	u16 len = 0;
	if (mspType == MspMsgType::REQUEST) {
		switch (fn) {
		case MspFn::API_VERSION:
			buf[len++] = MSP_PROTOCOL_VERSION;
			buf[len++] = API_VERSION_MAJOR;
			buf[len++] = API_VERSION_MINOR;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
			break;
		case MspFn::FIRMWARE_VARIANT:
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, KOLIBRI_IDENTIFIER, FIRMWARE_IDENTIFIER_LENGTH);
			break;
		case MspFn::FIRMWARE_VERSION:
			buf[len++] = FIRMWARE_VERSION_MAJOR;
			buf[len++] = FIRMWARE_VERSION_MINOR;
			buf[len++] = FIRMWARE_VERSION_PATCH;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
			break;
		case MspFn::BOARD_INFO: {
			memcpy(&buf[len], targetIdentifier, TARGET_IDENTIFIER_LENGTH);
			len += 4;
			buf[len++] = 0; // board revision
			buf[len++] = 0;
			buf[len++] = 2; // 2 == FC with MAX7456
			u8 targetCapabilities = 0;
			targetCapabilities |= 1 << 0; // VCP / CDC
			targetCapabilities |= 0 << 1; // no soft serial
			buf[len++] = targetCapabilities;
			u8 targetNameLen = strlen(targetFullName);
			buf[len++] = targetNameLen;
			memcpy(&buf[len], targetFullName, targetNameLen);
			len += targetNameLen;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
		} break;
		case MspFn::BUILD_INFO:
			memcpy(&buf[len], __DATE__, 11);
			len += 11;
			memcpy(&buf[len], __TIME__, 8);
			len += 8;
			memcpy(&buf[len], GIT_HASH, 7);
			len += 7;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
			break;
		case MspFn::GET_NAME: {
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, uavName.c_str(), strlen(uavName.c_str()));
		} break;
		case MspFn::SET_NAME: {
			openSettingsFile();
			uavName = string(reqPayload, reqLen);
			getSetting(SETTING_UAV_NAME)->updateSettingInFile();
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
		} break;
		case MspFn::GET_FEATURE_CONFIG: {
			// only exists for compatibility with BLHeliSuite32
			u32 features = 0;
			features |= 1 << 3; // FEATURE_RX_SERIAL
			features |= 1 << 4; // FEATURE_MOTOR_STOP
			features |= 1 << 7; // FEATURE_GPS
			features |= 1 << 10; // FEATURE_TELEMETRY
			features |= 1 << 18; // FEATURE_OSD
			features |= 1 << 22; // FEATURE_AIRMODE
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, (char *)features, len);
		} break;
		case MspFn::REBOOT:
			switch (reqPayload[0]) {
			case MSP_REBOOT_FIRMWARE:
				sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
				serials[serialNum].stream->flush();
				rebootReason = BootReason::CMD_REBOOT;
				sleep_ms(100);
				rp2040.reboot();
				break;
			case MSP_REBOOT_BOOTLOADER_FLASH:
			case MSP_REBOOT_BOOTLOADER_ROM:
				sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
				serials[serialNum].stream->flush();
				rebootReason = BootReason::CMD_BOOTLOADER;
				sleep_ms(100);
				rp2040.rebootToBootloader();
				break;
			default:
				sendMsp(serialNum, MspMsgType::ERROR, fn, version, "Invalid reboot mode");
				break;
			}
			break;
		case MspFn::GET_ADVANCED_CONFIG:
			// only exists for compatibility with BLHeliSuite32
			buf[len++] = 1; // gyro_sync_denom
			buf[len++] = 1; // pid_process_denom
			buf[len++] = 0; // useUnsyncedPwm => true if motors are updated asynchronously from the PID
			buf[len++] = 6; // motorPwmProtocol, 6 = DShot 300
			buf[len++] = 3200 & 0xFF;
			buf[len++] = 3200 >> 8;
			buf[len++] = (idlePermille * 10) & 0xFF;
			buf[len++] = (idlePermille * 10) >> 8;
			buf[len++] = 0; // gyro_use_32kHz
			buf[len++] = 0; // motorPwmInversion
			buf[len++] = 0; // gyro_to_use
			buf[len++] = 0; // gyro_high_fsr (true if > 2000dps)
			buf[len++] = CALIBRATION_TOLERANCE;
			buf[len++] = (CALIBRATION_SAMPLES * 100 / 3200) & 0xFF; // calibration duration in centiseconds
			buf[len++] = (CALIBRATION_SAMPLES * 100 / 3200) >> 8;
			buf[len++] = 0; // gyro_offset_yaw
			buf[len++] = 0;
			buf[len++] = 0; // checkOverflow, no overflow
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
			break;
		case MspFn::SET_ARMING_DISABLED:
			// not used by Kolibri configurator
			if (reqLen < 1) {
				sendMsp(serialNum, MspMsgType::ERROR, fn, version);
				break;
			}
			if (reqPayload[0]) {
				armingDisableFlags |= 0x80;
			} else {
				armingDisableFlags &= ~0x80;
			}
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
			break;
		case MspFn::MSP_STATUS:
			// only exists for compatibility with BLHeliSuite32
			buf[len++] = 312 & 0xFF;
			buf[len++] = 312 >> 8;
			buf[len++] = 0; // I2C error count
			buf[len++] = 0;
			buf[len++] = 0b101111; // gyro, no rangefinder, gps, mag, baro, accel
			buf[len++] = 0; // no other sensors
			buf[len++] = 0; // flight mode flags
			buf[len++] = 0;
			buf[len++] = 0;
			buf[len++] = 0;
			buf[len++] = 0; // PID profile index
			buf[len++] = 0; // CPU load (%)
			buf[len++] = 0; // gyro cycle time
			buf[len++] = 0;
			buf[len++] = 0; // flight mode flags count
			buf[len++] = 7; // arming disable flags count
			buf[len++] = armingDisableFlags;
			buf[len++] = armingDisableFlags >> 8;
			buf[len++] = armingDisableFlags >> 16;
			buf[len++] = armingDisableFlags >> 24;
			buf[len++] = 0; // config state flags, e.g. reboot required
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
			break;
		case MspFn::MSP_RAW_IMU: {
			buf[len++] = accelDataRaw[0] & 0xFF; // accel x
			buf[len++] = accelDataRaw[0] >> 8;
			buf[len++] = accelDataRaw[1] & 0xFF; // accel y
			buf[len++] = accelDataRaw[1] >> 8;
			buf[len++] = accelDataRaw[2] & 0xFF; // accel z
			buf[len++] = accelDataRaw[2] >> 8;
			i16 gyroX = gyroScaled[0].geti32(); // gyro x
			i16 gyroY = gyroScaled[1].geti32(); // gyro y
			i16 gyroZ = gyroScaled[2].geti32(); // gyro z
			buf[len++] = gyroX & 0xFF;
			buf[len++] = gyroX >> 8;
			buf[len++] = gyroY & 0xFF;
			buf[len++] = gyroY >> 8;
			buf[len++] = gyroZ & 0xFF;
			buf[len++] = gyroZ >> 8;
			buf[len++] = magData[0] & 0xFF; // mag x
			buf[len++] = magData[0] >> 8;
			buf[len++] = magData[1] & 0xFF; // mag y
			buf[len++] = magData[1] >> 8;
			buf[len++] = magData[2] & 0xFF; // mag z
			buf[len++] = magData[2] >> 8;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
		} break;
		case MspFn::GET_MOTOR: {
			u16 motors[8];
			for (int i = 0; i < 4; i++) {
				motors[i] = throttles[i] / 2 + 1000;
			}
			for (int i = 4; i < 8; i++) {
				motors[i] = 0;
			}
			memcpy(buf, motors, 16);
			len += 16;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
		} break;
		case MspFn::RC: {
			u16 channels[16];
			for (int i = 0; i < 16; i++)
				channels[i] = ELRS->channels[i];
			memcpy(buf, channels, 32);
			len += 32;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
		} break;
		case MspFn::MSP_ATTITUDE: {
			// not used by Kolibri configurator, that uses GET_ROTATION
			i16 rollInt = (-roll * (FIX_RAD_TO_DEG * 10)).geti32(); // decidegrees
			i16 pitchInt = (pitch * (FIX_RAD_TO_DEG * 10)).geti32(); // decidegrees
			i16 yawInt = (combinedHeading * FIX_RAD_TO_DEG).geti32(); // degrees
			buf[len++] = rollInt & 0xFF;
			buf[len++] = rollInt >> 8;
			buf[len++] = pitchInt & 0xFF;
			buf[len++] = pitchInt >> 8;
			buf[len++] = yawInt & 0xFF;
			buf[len++] = yawInt >> 8;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
		} break;
		case MspFn::MSP_ALTITUDE: {
			i32 altitudeInt = combinedAltitude.raw / 656; // fix32 raw (m) to cm
			buf[len++] = altitudeInt & 0xFF;
			buf[len++] = altitudeInt >> 8;
			buf[len++] = altitudeInt >> 16;
			buf[len++] = altitudeInt >> 24;
			buf[len++] = 0; // vario
			buf[len++] = 0; // vario
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
		} break;
		case MspFn::BOXIDS:
			// only exists for compatibility with BLHeliSuite32
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
			break;
		case MspFn::GET_MOTOR_3D_CONFIG:
			buf[len++] = 1450 & 0xFF; // deadband low
			buf[len++] = 1450 >> 8;
			buf[len++] = 1550 & 0xFF; // deadband high
			buf[len++] = 1550 >> 8;
			buf[len++] = 1500 & 0xFF; // neutral
			buf[len++] = 1500 >> 8;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
			break;
		case MspFn::MSP_BATTERY_STATE:
			buf[len++] = batCells;
			buf[len++] = 0; // battery capacity
			buf[len++] = 0;
			buf[len++] = (adcVoltage + 5) / 10; // voltage in 0.1V steps :/
			buf[len++] = 0; // mAh drawn
			buf[len++] = 0;
			buf[len++] = 0; // amps in 0.01A steps
			buf[len++] = 0;
			buf[len++] = batState == 0 ? 4 : 0; // 4 = init, 0 = ok
			buf[len++] = adcVoltage;
			buf[len++] = adcVoltage >> 8;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
		case MspFn::GET_MOTOR_CONFIG:
			buf[len++] = (1000 + idlePermille) & 0xFF; // min throttle
			buf[len++] = (1000 + idlePermille) >> 8;
			buf[len++] = 2000 & 0xFF; // max throttle
			buf[len++] = 2000 >> 8;
			buf[len++] = 1000 & 0xFF; // min command
			buf[len++] = 1000 >> 8;
			buf[len++] = 4; // motor count
			buf[len++] = MOTOR_POLES;
			buf[len++] = 1; // use dshot telemetry
			buf[len++] = 0; // esc sensor
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
			break;
		case MspFn::UID: {
			const char *chipId = rp2040.getChipID();
			memcpy(buf, chipId, 12);
			len = 12;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
		} break;
		case MspFn::ACC_CALIBRATION:
			accelCalibrationCycles = QUIET_SAMPLES + CALIBRATION_SAMPLES;
			armingDisableFlags |= 0x40;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, 1);
			break;
		case MspFn::MAG_CALIBRATION:
			magStateAfterRead = MagState::CALIBRATE;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, 1);
			snprintf(buf, 32, "Offsets: %d %d %d", magOffset[0], magOffset[1], magOffset[2]);
			sendMsp(serialNum, MspMsgType::REQUEST, MspFn::IND_MESSAGE, version, buf, strlen(buf));
			break;
		case MspFn::SET_MOTOR:
			if (!armed) {
				throttles[(u8)MOTOR::RR] = ((u16)reqPayload[0] + ((u16)reqPayload[1] << 8)) * 2 - 2000;
				throttles[(u8)MOTOR::FR] = ((u16)reqPayload[2] + ((u16)reqPayload[3] << 8)) * 2 - 2000;
				throttles[(u8)MOTOR::RL] = ((u16)reqPayload[4] + ((u16)reqPayload[5] << 8)) * 2 - 2000;
				throttles[(u8)MOTOR::FL] = ((u16)reqPayload[6] + ((u16)reqPayload[7] << 8)) * 2 - 2000;
			}
			mspOverrideMotors = 0;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
			break;
		case MspFn::ENABLE_4WAY_IF:
			begin4Way(serialNum);
			buf[0] = 4; // ESC count
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, 1);
			break;
		case MspFn::SET_RTC: {
			if (reqLen < 6) {
				sendMsp(serialNum, MspMsgType::ERROR, fn, version);
				break;
			}
			const struct timespec t = {
				.tv_sec = DECODE_U4((u8 *)reqPayload),
				.tv_nsec = DECODE_U2((u8 *)&reqPayload[4]) * 1000000, // convert millis to nanoseconds
			};
			rtcSetTime(&t, TIME_QUALITY_MSP);
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
		} break;
		case MspFn::GET_RTC: {
			struct tm tm;
			struct timespec ts;
			rtcGetTime(&ts, true);
			rtcConvertToTm(&ts, &tm);
			buf[0] = tm.tm_year & 0xFF;
			buf[1] = tm.tm_year >> 8;
			buf[2] = tm.tm_mon;
			buf[3] = tm.tm_mday;
			buf[4] = tm.tm_hour;
			buf[5] = tm.tm_min;
			buf[6] = tm.tm_sec;
			u16 millis = ts.tv_nsec / 1000000;
			buf[7] = millis & 0xFF; // millis
			buf[8] = millis >> 8; // millis
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, 9);
		} break;
		case MspFn::STATUS: {
			u16 voltage = adcVoltage;
			buf[len++] = voltage & 0xFF;
			buf[len++] = voltage >> 8;
			buf[len++] = armed;
			buf[len++] = (u8)flightMode;
			buf[len++] = (u8)(armingDisableFlags & 0xFF);
			buf[len++] = (u8)(armingDisableFlags >> 8);
			buf[len++] = (u8)(armingDisableFlags >> 16);
			buf[len++] = (u8)(armingDisableFlags >> 24);
			buf[len++] = (u8)(configuratorConnected & 0xFF);
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
		} break;
		case MspFn::CONFIGURATOR_PING:
			configuratorConnected = true;
			lastConfigPingRx = 0;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
			break;
			break;
		case MspFn::SERIAL_PASSTHROUGH: {
			if (reqPayload[0] >= SERIAL_COUNT)
				return sendMsp(serialNum, MspMsgType::ERROR, fn, version);

			u8 fromNum = serialNum;
			if (reqLen > 5) fromNum = reqPayload[5];
			if (fromNum >= SERIAL_COUNT)
				return sendMsp(serialNum, MspMsgType::ERROR, fn, version);

			BufferedWriter *from = serials[fromNum].stream;
			BufferedWriter *to = serials[reqPayload[0]].stream;
			u32 baud = DECODE_U4((u8 *)&reqPayload[1]);
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, (char *)reqPayload, 5);
			serials[serialNum].stream->flush();

			u8 plusCount = 0;
			elapsedMillis breakoutCounter = 0;
			to->end();
			to->begin(baud);
			while (true) {
				if (breakoutCounter > 1000 && plusCount >= 3) break;
				if (from->available()) {
					breakoutCounter = 0;

					char c = from->read();
					if (c == '+')
						plusCount++;
					else
						plusCount = 0;
					to->write(c);
				}
				if (to->available()) {
					from->write(to->read());
				}
				from->loop();
				to->loop();
				rp2040.wdt_reset();
			}
		} break;
		case MspFn::CLI_INIT: {
			// send start info
			// const char *startInfo = "\n" FIRMWARE_VERSION_STRING "\n" targetIdentifier "" ">> ";
			char startInfo[256] = {0};
			snprintf(startInfo, 256, "\n" FIRMWARE_NAME " v" FIRMWARE_VERSION_STRING "\n%s => %s\nType 'help' to get a list of commands\n>> ", targetIdentifier, targetFullName);
			openSettingsFile();
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, startInfo, strlen(startInfo));
		} break;
		case MspFn::CLI_COMMAND: {
			string response = string(reqPayload, reqLen);
			response += "\n";
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, response.c_str(), response.length());
			response = processCliCommand(reqPayload, reqLen);
			response += "\n>> ";
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, response.c_str(), response.length());
		} break;
		case MspFn::CLI_GET_SUGGESTION:
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
			break;
		case MspFn::SAVE_SETTINGS:
			closeSettingsFile();
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
			break;
		case MspFn::WRITE_OSD_FONT_CHARACTER:
			if (reqLen < 55) {
				sendMsp(serialNum, MspMsgType::ERROR, fn, version);
				break;
			}
			updateCharacter(reqPayload[0], (u8 *)&reqPayload[1]);
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, reqPayload, 1);
			break;
		case MspFn::GET_BB_SETTINGS: {
			u8 bbSettings[9];
			bbSettings[0] = bbFreqDivider;
			memcpy(&bbSettings[1], &bbFlags, 8);
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, (char *)bbSettings, sizeof(bbSettings));
		} break;
		case MspFn::SET_BB_SETTINGS: {
			u8 bbSettings[9];
			memcpy(bbSettings, reqPayload, sizeof(bbSettings));
			bbFreqDivider = bbSettings[0];
			memcpy(&bbFlags, &bbSettings[1], 8);
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
			openSettingsFile();
			getSetting(SETTING_BB_DIV)->updateSettingInFile();
			getSetting(SETTING_BB_FLAGS)->updateSettingInFile();
		} break;
		case MspFn::BB_FILE_LIST: {
			int i = 0;
			u16 b[500];
#if BLACKBOX_STORAGE == SD_BB
			FsFile dir = sdCard.open("/blackbox");
			FsFile file;
			while (file.openNext(&dir)) {
				if (file.isFile()) {
					char path[32];
					file.getName(path, 32);
					String name = path;
					if (!name.startsWith("KOLI") || !name.endsWith(".kbb")) {
						continue;
					}
					if (name.length() != 12) {
						continue;
					}
					char num[5];
					name.substring(4, 8).toCharArray(num, 5);
					u32 index = 0;
					for (int j = 0; j < 4; j++) {
						if (num[j] < '0' || num[j] > '9') {
							index = 99999;
						}
						index = index * 10 + (num[j] - '0');
					}
					if (index < 10000) {
						b[i++] = index;
					}
				}
				if (i >= 500) {
					break;
				}
			}
#endif
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, (const char *)b, i * 2);
		} break;
		case MspFn::BB_FILE_INFO: {
			/* data of command
			 * 0...len-1: file numbers (LE 2 bytes each)
			 *
			 * data of response
			 * 0-1: file number
			 * 2-5: file size in bytes
			 * 6-8: version of bb file format
			 * 9-12: time of recording start
			 * 13-16: duration in ms
			 */
			u8 len = reqLen / 2;
			len = len > 15 ? 15 : len;
			u16 fileNums[len];
			memcpy(fileNums, reqPayload, len * 2);
			u8 buffer[17 * len];
			u8 index = 0;
			for (int i = 0; i < len; i++) {
				rp2040.wdt_reset();
				char path[32];
				u16 fileNum = fileNums[i];
#if BLACKBOX_STORAGE == SD_BB
				snprintf(path, 32, "/blackbox/KOLI%04d.kbb", fileNum);
				FsFile logFile = sdCard.open(path);
#endif
				if (!logFile)
					continue;
				buffer[index++] = fileNum;
				buffer[index++] = fileNum >> 8;
				buffer[index++] = logFile.size() & 0xFF;
				buffer[index++] = (logFile.size() >> 8) & 0xFF;
				buffer[index++] = (logFile.size() >> 16) & 0xFF;
				buffer[index++] = (logFile.size() >> 24) & 0xFF;
				logFile.seek(LOG_HEAD_BB_VERSION);
				// version, timestamp, pid and divider can directly be read from the file
				for (int i = 0; i < 7; i++)
					buffer[index++] = logFile.read();
				logFile.seek(LOG_HEAD_DURATION);
				for (int i = 0; i < 4; i++)
					buffer[index++] = logFile.read();
				logFile.close();
			}
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, (char *)buffer, index);
		} break;
		case MspFn::BB_FILE_DOWNLOAD: {
			u16 fileNum = DECODE_U2((u8 *)&reqPayload[0]);
			i32 chunkNum = -1;
			if (reqLen >= 6) {
				chunkNum = DECODE_I4((u8 *)&reqPayload[2]);
			}
			printLogBin(serialNum, version, fileNum, chunkNum);
		} break;
		case MspFn::BB_FILE_DELETE: {
			// data just includes one byte of file number
			u8 fileNum = reqPayload[0];
			char path[32];
#if BLACKBOX_STORAGE == SD_BB
			snprintf(path, 32, "/blackbox/KOLI%04d.kbb", fileNum);
			if (sdCard.remove(path))
#endif
				sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, (char *)&fileNum, 1);
			else
				sendMsp(serialNum, MspMsgType::ERROR, fn, version, (char *)&fileNum, 1);
		} break;
		case MspFn::BB_FORMAT:
			if (clearBlackbox())
				sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
			else
				sendMsp(serialNum, MspMsgType::ERROR, fn, version);
			break;
		case MspFn::BB_FILE_INIT: {
			if (reqLen < 2) {
				sendMsp(serialNum, MspMsgType::ERROR, fn, version);
				break;
			}
			u16 fileNum = DECODE_U2((u8 *)&reqPayload[0]);
			printFileInit(serialNum, version, fileNum);
		} break;
		case MspFn::GET_GPS_STATUS:
			buf[0] = gpsStatus.gpsInited;
			buf[1] = gpsStatus.initStep;
			buf[2] = gpsStatus.fixType;
			buf[3] = gpsStatus.timeValidityFlags;
			buf[4] = gpsStatus.flags;
			buf[5] = gpsStatus.flags2;
			buf[6] = gpsStatus.flags3 & 0xFF;
			buf[7] = gpsStatus.flags3 >> 8;
			buf[8] = gpsStatus.satCount;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, 9);
			break;
		case MspFn::GET_GPS_ACCURACY:
			// through padding and compiler optimization, it is not possible to just memcpy the struct
			memcpy(buf, &gpsAcc.tAcc, 4);
			memcpy(&buf[4], &gpsAcc.hAcc, 4);
			memcpy(&buf[8], &gpsAcc.vAcc, 4);
			memcpy(&buf[12], &gpsAcc.sAcc, 4);
			memcpy(&buf[16], &gpsAcc.headAcc, 4);
			memcpy(&buf[20], &gpsAcc.pDop, 4);
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, 24);
			break;
		case MspFn::GET_GPS_TIME:
			buf[0] = gpsTime.tm_year & 0xFF;
			buf[1] = gpsTime.tm_year >> 8;
			buf[2] = gpsTime.tm_mon;
			buf[3] = gpsTime.tm_mday;
			buf[4] = gpsTime.tm_hour;
			buf[5] = gpsTime.tm_min;
			buf[6] = gpsTime.tm_sec;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, 7);
			break;
		case MspFn::GET_GPS_MOTION: {
			memcpy(buf, &gpsMotion.lat, 4);
			memcpy(&buf[4], &gpsMotion.lon, 4);
			memcpy(&buf[8], &gpsMotion.alt, 4);
			memcpy(&buf[12], &gpsMotion.velN, 4);
			memcpy(&buf[16], &gpsMotion.velE, 4);
			memcpy(&buf[20], &gpsMotion.velD, 4);
			memcpy(&buf[24], &gpsMotion.gSpeed, 4);
			memcpy(&buf[28], &gpsMotion.headMot, 4);
			memcpy(&buf[32], &combinedAltitude.raw, 4);
			memcpy(&buf[36], &vVel.raw, 4);
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, 40);
		} break;
		case MspFn::GET_MAG_DATA: {
			i16 raw[6] = {(i16)magData[0], (i16)magData[1], (i16)magData[2], (i16)magRight.geti32(), (i16)magFront.geti32(), (i16)(magHeading * FIX_RAD_TO_DEG).geti32()};
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, (char *)raw, sizeof(raw));
		} break;
		case MspFn::GET_BARO_DATA: {
			i32 raw[4] = {
				(i32)(baroASL.raw / 66),
				(i32)(baroPres * 1000),
				(i32)(baroTemp * 100),
				pressureRaw};
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, (char *)raw, sizeof(raw));
		} break;
		case MspFn::GET_ROTATION: {
			// https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
			int rotationPitch = (pitch * 8192).geti32();
			int rotationRoll = (roll * 8192).geti32();
			int rotationYaw = (yaw * 8192).geti32();
			int heading = combinedHeading.raw >> 3;
			buf[0] = rotationPitch & 0xFF;
			buf[1] = rotationPitch >> 8;
			buf[2] = rotationRoll & 0xFF;
			buf[3] = rotationRoll >> 8;
			buf[4] = rotationYaw & 0xFF;
			buf[5] = rotationYaw >> 8;
			buf[6] = heading & 0xFF;
			buf[7] = heading >> 8;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, 8);
		} break;
		case MspFn::TASK_STATUS: {
			u32 buf[TASK_LENGTH * 7];
			for (int i = 0; i < TASK_LENGTH; i++) {
				buf[i * 7 + 0] = tasks[i].debugInfo;
				buf[i * 7 + 1] = tasks[i].minMaxDuration;
				buf[i * 7 + 2] = tasks[i].frequency;
				buf[i * 7 + 3] = tasks[i].lastTotalDuration;
				buf[i * 7 + 4] = tasks[i].errorCount;
				buf[i * 7 + 5] = tasks[i].lastError;
				buf[i * 7 + 6] = tasks[i].maxGap;
			}
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, (char *)buf, sizeof(buf));
			for (int i = 0; i < TASK_LENGTH; i++) {
				tasks[i].minMaxDuration = 0x7FFF0000;
				tasks[i].maxGap = 0;
			}
		} break;
		case MspFn::GET_RX_STATUS: {
			buf[0] = ELRS->isReceiverUp;
			buf[1] = ELRS->isLinkUp;
			buf[2] = ELRS->uplinkRssi[0];
			buf[3] = ELRS->uplinkRssi[1];
			buf[4] = ELRS->uplinkLinkQuality;
			buf[5] = ELRS->uplinkSNR;
			buf[6] = ELRS->antennaSelection;
			buf[7] = ELRS->packetRateIdx;
			memcpy(&buf[8], &ELRS->txPower, 2);
			memcpy(&buf[10], &ELRS->targetPacketRate, 2);
			memcpy(&buf[12], &ELRS->actualPacketRate, 2);
			memcpy(&buf[14], &ELRS->rcMsgCount, 4);
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, 18);
		} break;
		case MspFn::GET_RX_MODES: {
			mspGetRxModes(serialNum, version);
		} break;
		case MspFn::SET_RX_MODES: {
			mspSetRxModes(serialNum, version, reqPayload, reqLen);
		} break;
		case MspFn::GET_BATTERY_SETTINGS: {
			buf[len++] = cellCountSetting;
			buf[len++] = emptyVoltageSetting;
			buf[len++] = emptyVoltageSetting >> 8;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
		} break;
		case MspFn::SET_BATTERY_SETTINGS: {
			cellCountSetting = reqPayload[0];
			emptyVoltageSetting = reqPayload[1] | ((i16)reqPayload[2] << 8);
			openSettingsFile();
			getSetting(SETTING_CELL_COUNT)->updateSettingInFile();
			getSetting(SETTING_EMPTY_VOLTAGE)->updateSettingInFile();
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
		} break;
		case MspFn::GET_MOTOR_LAYOUT: {
			getMotorPins((u8 *)buf);
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, 4);
		} break;
		case MspFn::SET_MOTOR_LAYOUT: {
			buf[0] = updateMotorPins((const u8 *)reqPayload);
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, 1);
		} break;
		case MspFn::GET_TZ_OFFSET: {
			buf[0] = rtcTimezoneOffset;
			buf[1] = rtcTimezoneOffset >> 8;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, 2);
		} break;
		case MspFn::SET_TZ_OFFSET: {
			i16 offset = DECODE_I2((u8 *)reqPayload);
			if (offset > 14 * 60 || offset < -12 * 60) {
				sendMsp(serialNum, MspMsgType::ERROR, fn, version);
				break;
			}
			rtcTimezoneOffset = offset;
			openSettingsFile();
			getSetting(SETTING_TIMEZONE_OFFSET)->updateSettingInFile();
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
		} break;
		case MspFn::GET_PIDS: {
			// copying just for future proofing, in case more things are added
			u16 pids[3][5];
			for (int i = 0; i < 3; i++) {
				pids[i][0] = pidGainsNice[i][0];
				pids[i][1] = pidGainsNice[i][1];
				pids[i][2] = pidGainsNice[i][2];
				pids[i][3] = pidGainsNice[i][3];
				pids[i][4] = pidGainsNice[i][4];
			}
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, (char *)pids, sizeof(pids));
		} break;
		case MspFn::SET_PIDS: {
			u16 pids[3][5];
			memcpy(pids, reqPayload, sizeof(pids));
			for (int i = 0; i < 3; i++) {
				pidGainsNice[i][0] = pids[i][0];
				pidGainsNice[i][1] = pids[i][1];
				pidGainsNice[i][2] = pids[i][2];
				pidGainsNice[i][3] = pids[i][3];
				pidGainsNice[i][4] = pids[i][4];
			}
			convertPidsFromNice();
			openSettingsFile();
			getSetting(SETTING_PID_GAINS)->updateSettingInFile();
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
		} break;
		case MspFn::GET_RATES: {
			i16 rates[3][3];
			for (int ax = 0; ax < 3; ax++) {
				rates[ax][ACTUAL_CENTER_SENSITIVITY] = rateCoeffs[ax][ACTUAL_CENTER_SENSITIVITY].geti32();
				rates[ax][ACTUAL_MAX_RATE] = rateCoeffs[ax][ACTUAL_MAX_RATE].geti32();
				rates[ax][ACTUAL_EXPO] = rateCoeffs[ax][ACTUAL_EXPO].raw >> 3; // expo, 3.13 fixed point
			}
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, (char *)rates, sizeof(rates));
		} break;
		case MspFn::SET_RATES: {
			i16 rates[3][3];
			memcpy(rates, reqPayload, sizeof(rates));
			for (int ax = 0; ax < 3; ax++) {
				if (rates[ax][ACTUAL_CENTER_SENSITIVITY] > rates[ax][ACTUAL_MAX_RATE]) {
					rates[ax][ACTUAL_MAX_RATE] = rates[ax][ACTUAL_CENTER_SENSITIVITY];
				}
				rateCoeffs[ax][ACTUAL_CENTER_SENSITIVITY] = rates[ax][ACTUAL_CENTER_SENSITIVITY];
				rateCoeffs[ax][ACTUAL_MAX_RATE] = rates[ax][ACTUAL_MAX_RATE];
				rateCoeffs[ax][ACTUAL_EXPO].raw = (i32)rates[ax][ACTUAL_EXPO] << 3; // 3.13 fixed point for expo (normally [0,1], but technically [-4,4) are allowed here)
			}
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
			openSettingsFile();
			getSetting(SETTING_RATE_COEFFS)->updateSettingInFile();
		} break;
		case MspFn::GET_EXT_PID: {
			u16 ifall = iFalloff.geti32();
			buf[len++] = ifall & 0xFF;
			buf[len++] = ifall >> 8;
			buf[len++] = useDynamicIdle;
			buf[len++] = idlePermille;
			buf[len++] = dynamicIdleRpm;
			buf[len++] = dynamicIdleRpm >> 8;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
		} break;
		case MspFn::SET_EXT_PID: {
			iFalloff = DECODE_U2(reqPayload);
			useDynamicIdle = reqPayload[2];
			idlePermille = reqPayload[3];
			dynamicIdleRpm = DECODE_U2(&reqPayload[4]);
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
			openSettingsFile();
			getSetting(SETTING_IFALLOFF)->updateSettingInFile();
			getSetting(SETTING_DYNAMIC_IDLE_EN)->updateSettingInFile();
			getSetting(SETTING_IDLE_PERMILLE)->updateSettingInFile();
			getSetting(SETTING_DYNAMIC_IDLE_RPM)->updateSettingInFile();
		} break;
		case MspFn::GET_FILTER_CONFIG: {
			buf[len++] = gyroFilterCutoff & 0xFF;
			buf[len++] = gyroFilterCutoff >> 8;
			u16 data = accelFilterCutoff.geti32();
			buf[len++] = data & 0xFF;
			buf[len++] = data >> 8;
			buf[len++] = dFilterCutoff & 0xFF;
			buf[len++] = dFilterCutoff >> 8;
			data = (iRelaxCutoff * 10 + 0.5f).geti32();
			buf[len++] = data & 0xFF;
			buf[len++] = data >> 8;
			data = (ffCutoff * 10 + 0.5f).geti32();
			buf[len++] = data & 0xFF;
			buf[len++] = data >> 8;
			data = (magFilterCutoff * 100 + 0.5f).geti32();
			buf[len++] = data & 0xFF;
			buf[len++] = data >> 8;
			data = (vvelFFFilterCutoff * 100 + 0.5f).geti32();
			buf[len++] = data & 0xFF;
			buf[len++] = data >> 8;
			data = (vvelDFilterCutoff * 10 + 0.5f).geti32();
			buf[len++] = data & 0xFF;
			buf[len++] = data >> 8;
			data = (hvelFfFilterCutoff * 100 + 0.5f).geti32();
			buf[len++] = data & 0xFF;
			buf[len++] = data >> 8;
			data = (hvelIRelaxFilterCutoff * 100 + 0.5f).geti32();
			buf[len++] = data & 0xFF;
			buf[len++] = data >> 8;
			data = (hvelPushFilterCutoff * 10 + 0.5f).geti32();
			buf[len++] = data & 0xFF;
			buf[len++] = data >> 8;
			data = (gpsVelocityFilterCutoff * 100 + 0.5f).geti32();
			buf[len++] = data & 0xFF;
			buf[len++] = data >> 8;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
		} break;
		case MspFn::SET_FILTER_CONFIG: {
			if (reqLen < 22) {
				sendMsp(serialNum, MspMsgType::ERROR, fn, version);
				break;
			}
			openSettingsFile();

			gyroFilterCutoff = DECODE_U2((u8 *)&reqPayload[0]);
			getSetting(SETTING_GYRO_FILTER_CUTOFF)->updateSettingInFile();

			accelFilterCutoff = DECODE_U2((u8 *)&reqPayload[2]);
			getSetting(SETTING_ACC_FILTER_CUTOFF)->updateSettingInFile();

			dFilterCutoff = DECODE_U2((u8 *)&reqPayload[4]);
			getSetting(SETTING_DFILTER_CUTOFF)->updateSettingInFile();

			iRelaxCutoff = DECODE_U2((u8 *)&reqPayload[6]) / 10.0f;
			getSetting(SETTING_IRELAX_CUTOFF)->updateSettingInFile();

			ffCutoff = DECODE_U2((u8 *)&reqPayload[8]) / 10.0f;
			getSetting(SETTING_FF_CUTOFF)->updateSettingInFile();

			magFilterCutoff = DECODE_U2((u8 *)&reqPayload[10]) / 100.0f;
			getSetting(SETTING_MAG_FILTER_CUTOFF)->updateSettingInFile();

			vvelFFFilterCutoff = DECODE_U2((u8 *)&reqPayload[12]) / 100.0f;
			getSetting(SETTING_VVEL_FF_FILTER_CUTOFF)->updateSettingInFile();

			vvelDFilterCutoff = DECODE_U2((u8 *)&reqPayload[14]) / 10.0f;
			getSetting(SETTING_VVEL_D_FILTER_CUTOFF)->updateSettingInFile();

			hvelFfFilterCutoff = DECODE_U2((u8 *)&reqPayload[16]) / 100.0f;
			getSetting(SETTING_HVEL_FF_FILTER_CUTOFF)->updateSettingInFile();

			hvelIRelaxFilterCutoff = DECODE_U2((u8 *)&reqPayload[18]) / 100.0f;
			getSetting(SETTING_HVEL_I_RELAX_FILTER_CUTOFF)->updateSettingInFile();

			hvelPushFilterCutoff = DECODE_U2((u8 *)&reqPayload[20]) / 10.0f;
			getSetting(SETTING_HVEL_PUSH_FILTER_CUTOFF)->updateSettingInFile();

			gpsVelocityFilterCutoff = DECODE_U2((u8 *)&reqPayload[22]) / 100.0f;
			getSetting(SETTING_GPS_VEL_FILTER_CUTOFF)->updateSettingInFile();

			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
		} break;
		case MspFn::GET_CRASH_DUMP:
			for (int i = 0; i < 256; i++) {
				rp2040.wdt_reset();
				buf[i] = EEPROM.read(4096 - 256 + i);
			}
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, 256);
			break;
		case MspFn::CLEAR_CRASH_DUMP:
			for (int i = 0; i < 256; i++) {
				rp2040.wdt_reset();
				EEPROM.write(4096 - 256 + i, 0);
			}
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
			break;
		case MspFn::SET_DEBUG_LED:
			p.neoPixelSetValue(1, reqPayload[0] * 255, reqPayload[0] * 255, reqPayload[0] * 255, true);
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version);
			break;
		case MspFn::PLAY_SOUND: {
			const u16 startFreq = random(1000, 5000);
			const u16 endFreq = random(1000, 5000);
			const u16 sweepDuration = random(400, 1000);
			u16 pauseDuration = random(100, 1000);
			const u16 pauseEn = random(0, 2);
			pauseDuration *= pauseEn;
			const u16 repeat = random(1, 11);
			makeSweepSound(startFreq, endFreq, ((sweepDuration + pauseDuration) * repeat) - 1, sweepDuration, pauseDuration);
			u8 len = 0;
			buf[len++] = startFreq & 0xFF;
			buf[len++] = startFreq >> 8;
			buf[len++] = endFreq & 0xFF;
			buf[len++] = endFreq >> 8;
			buf[len++] = sweepDuration & 0xFF;
			buf[len++] = sweepDuration >> 8;
			buf[len++] = pauseDuration & 0xFF;
			buf[len++] = pauseDuration >> 8;
			buf[len++] = pauseEn;
			buf[len++] = repeat;
			buf[len++] = (((sweepDuration + pauseDuration) * repeat) - 1) & 0xFF;
			buf[len++] = (((sweepDuration + pauseDuration) * repeat) - 1) >> 8;
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, len);
		} break;
		case MspFn::DEBUG_SENSORS: {
			memcpy(buf, mspDebugSensors, sizeof(mspDebugSensors));
			sendMsp(serialNum, MspMsgType::RESPONSE, fn, version, buf, sizeof(mspDebugSensors));
		} break;
		default:
			sendMsp(serialNum, MspMsgType::ERROR, fn, version, "Unknown command", strlen("Unknown command"));
			break;
		}
	}
}

void mspHandleByte(u8 c, u8 serialNum) {
	TASK_START(TASK_CONFIGURATOR);
	static char payloadBuf[2052] = {0}; // worst case: 2048 bytes payload + 3 bytes checksum (v2 over v1 jumbo) + 1 byte start. After the start byte, the index is reset to 0
	static u16 payloadBufIndex = 0;
	static u16 payloadLen = 0;
	static MspFn fn = MspFn::API_VERSION;
	static MspMsgType msgType = MspMsgType::ERROR;
	static u8 msgFlag = 0;
	static u32 crcV1 = 0;
	static u32 crcV2 = 0;
	static MspVersion msgMspVer = MspVersion::V2;
	static MspState mspState = MspState::IDLE;

	switch (mspState) {
	case MspState::IDLE:
		if (c == '$')
			mspState = MspState::PACKET_START;
		break;
	case MspState::PACKET_START:
		payloadBufIndex = 0;
		switch (c) {
		case 'M':
			mspState = MspState::TYPE_V1;
			msgMspVer = MspVersion::V1;
			break;
		case 'X':
			mspState = MspState::TYPE_V2;
			msgMspVer = MspVersion::V2;
			break;
		default:
			mspState = MspState::IDLE;
			break;
		}
		break;
	case MspState::TYPE_V1:
		crcV1 = 0;
		mspState = MspState::LEN_V1;
		switch (c) {
		case '<':
			msgType = MspMsgType::REQUEST;
			break;
		case '>':
			msgType = MspMsgType::RESPONSE;
			break;
		case '!':
			msgType = MspMsgType::ERROR;
			break;
		default:
			mspState = MspState::IDLE;
			break;
		}
		break;
	case MspState::LEN_V1:
		crcV1 ^= c;
		payloadLen = c;
		mspState = MspState::CMD_V1;
		break;
	case MspState::CMD_V1:
		crcV1 ^= c;
		fn = (MspFn)c;
		if (c == (u8)MspFn::MSP_V2_FRAME) {
			msgMspVer = MspVersion::V2_OVER_V1;
			crcV2 = 0;
			mspState = MspState::FLAG_V2_OVER_V1;
		} else if (payloadLen == 255) {
			msgMspVer = MspVersion::V1_JUMBO;
			mspState = MspState::JUMBO_LEN_LO_V1;
		} else {
			mspState = payloadLen ? MspState::PAYLOAD_V1 : MspState::CHECKSUM_V1;
		}
		break;
	case MspState::JUMBO_LEN_LO_V1:
		payloadLen = c;
		crcV1 ^= c;
		mspState = MspState::JUMBO_LEN_HI_V1;
		break;
	case MspState::JUMBO_LEN_HI_V1:
		payloadLen |= ((u16)c << 8);
		if (payloadLen > 2048) {
			mspState = MspState::IDLE;
			break;
		}
		crcV1 ^= c;
		mspState = payloadLen ? MspState::PAYLOAD_V1 : MspState::CHECKSUM_V1;
		break;
	case MspState::PAYLOAD_V1:
		crcV1 ^= c;
		payloadBuf[payloadBufIndex++] = c;
		if (payloadBufIndex == payloadLen)
			mspState = MspState::CHECKSUM_V1;
		break;
	case MspState::FLAG_V2_OVER_V1:
		msgFlag = c;
		crcV1 ^= c;
		CRC_LUT_D5_APPLY(crcV2, c);
		mspState = MspState::CMD_LO_V2_OVER_V1;
		break;
	case MspState::CMD_LO_V2_OVER_V1:
		fn = (MspFn)c;
		crcV1 ^= c;
		CRC_LUT_D5_APPLY(crcV2, c);
		mspState = MspState::CMD_HI_V2_OVER_V1;
		break;
	case MspState::CMD_HI_V2_OVER_V1:
		fn = (MspFn)((u32)fn | (u32)c << 8);
		crcV1 ^= c;
		CRC_LUT_D5_APPLY(crcV2, c);
		mspState = MspState::LEN_LO_V2_OVER_V1;
		break;
	case MspState::LEN_LO_V2_OVER_V1:
		payloadLen = c;
		crcV1 ^= c;
		CRC_LUT_D5_APPLY(crcV2, c);
		mspState = MspState::LEN_HI_V2_OVER_V1;
		break;
	case MspState::LEN_HI_V2_OVER_V1:
		payloadLen |= ((u16)c << 8);
		if (payloadLen > 2048) {
			mspState = MspState::IDLE;
			break;
		}
		crcV1 ^= c;
		CRC_LUT_D5_APPLY(crcV2, c);
		mspState = payloadLen ? MspState::PAYLOAD_V2_OVER_V1 : MspState::CHECKSUM_V2_OVER_V1;
		payloadBufIndex = 0;
		break;
	case MspState::PAYLOAD_V2_OVER_V1:
		crcV1 ^= c;
		CRC_LUT_D5_APPLY(crcV2, c);
		payloadBuf[payloadBufIndex++] = c;
		if (payloadBufIndex == payloadLen)
			mspState = MspState::CHECKSUM_V2_OVER_V1;
		break;
	case MspState::CHECKSUM_V2_OVER_V1:
		if (c != crcV2) {
			mspState = MspState::IDLE;
			sendMsp(serialNum, MspMsgType::ERROR, fn, msgMspVer, "CRCv2", 5);
			break;
		}
		crcV1 ^= c;
		mspState = MspState::CHECKSUM_V1;
		break;
	case MspState::CHECKSUM_V1:
		if (c == crcV1)
			processMspCmd(serialNum, msgType, fn, msgMspVer, payloadBuf, payloadLen);
		else
			sendMsp(serialNum, MspMsgType::ERROR, fn, msgMspVer, "CRCv1", 5);
		mspState = MspState::IDLE;
		break;
	case MspState::TYPE_V2:
		mspState = MspState::FLAG_V2;
		crcV2 = 0;
		switch (c) {
		case '<':
			msgType = MspMsgType::REQUEST;
			break;
		case '>':
			msgType = MspMsgType::RESPONSE;
			break;
		case '!':
			msgType = MspMsgType::ERROR;
			break;
		default:
			mspState = MspState::IDLE;
			break;
		}
		break;
	case MspState::FLAG_V2:
		msgFlag = c;
		CRC_LUT_D5_APPLY(crcV2, c);
		mspState = MspState::CMD_LO_V2;
		break;
	case MspState::CMD_LO_V2:
		fn = (MspFn)c;
		CRC_LUT_D5_APPLY(crcV2, c);
		mspState = MspState::CMD_HI_V2;
		break;
	case MspState::CMD_HI_V2:
		fn = (MspFn)((u32)fn | (u32)c << 8);
		CRC_LUT_D5_APPLY(crcV2, c);
		mspState = MspState::LEN_LO_V2;
		break;
	case MspState::LEN_LO_V2:
		payloadLen = c;
		CRC_LUT_D5_APPLY(crcV2, c);
		mspState = MspState::LEN_HI_V2;
		break;
	case MspState::LEN_HI_V2:
		payloadLen |= ((u16)c << 8);
		if (payloadLen > 2048) {
			mspState = MspState::IDLE;
			break;
		}
		CRC_LUT_D5_APPLY(crcV2, c);
		mspState = payloadLen ? MspState::PAYLOAD_V2 : MspState::CHECKSUM_V2;
		break;
	case MspState::PAYLOAD_V2:
		CRC_LUT_D5_APPLY(crcV2, c);
		payloadBuf[payloadBufIndex++] = c;
		if (payloadBufIndex == payloadLen)
			mspState = MspState::CHECKSUM_V2;
		break;
	case MspState::CHECKSUM_V2:
		if (c == crcV2)
			processMspCmd(serialNum, msgType, fn, msgMspVer, payloadBuf, payloadLen);
		else
			sendMsp(serialNum, MspMsgType::ERROR, fn, msgMspVer, "CRCv2", 5);
		mspState = MspState::IDLE;
		break;
	}
	TASK_END(TASK_CONFIGURATOR);
}

void printIndMessage(String msg) {
	if (msg.length() > 256) {
		msg = msg.substring(0, 256);
	}
	sendMsp(0, MspMsgType::REQUEST, MspFn::IND_MESSAGE, MspVersion::V2, msg.c_str(), msg.length());
}

void printfIndMessage(const char *format, ...) {
	va_list args;
	char msg[256];
	va_start(args, format);
	vsnprintf(msg, sizeof(msg), format, args);
	va_end(args);
	printIndMessage(msg);
}
