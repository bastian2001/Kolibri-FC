#include "global.h"
void readEEPROM() {
	initPID();
	if (EEPROM.read((u16)EEPROM_POS::EEPROM_INIT) != 0x42) {
		// EEPROM is not initialized
		EEPROM.write((u16)EEPROM_POS::EEPROM_INIT, 0x42);
		EEPROM.write((u16)EEPROM_POS::EEPROM_VERSION, 0x01);
		i32 pg[3][7];
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 7; j++)
				pg[i][j] = pidGains[i][j].getRaw();
		EEPROM.put((u16)EEPROM_POS::PID_GAINS, pg);
		i32 rf[5][3];
		for (int i = 0; i < 5; i++)
			for (int j = 0; j < 3; j++)
				rf[i][j] = rateFactors[i][j].getRaw();
		EEPROM.put((u16)EEPROM_POS::RATE_FACTORS, rf);
		EEPROM.put((u16)EEPROM_POS::BB_FLAGS, (u64)0);
		EEPROM.put((u16)EEPROM_POS::BB_FREQ_DIVIDER, (u8)2);
		EEPROM.put((u16)EEPROM_POS::ACCEL_CALIBRATION, (u16)0);
		EEPROM.put((u16)EEPROM_POS::ACCEL_CALIBRATION + 2, (u16)0);
		EEPROM.put((u16)EEPROM_POS::ACCEL_CALIBRATION + 4, (u16)0);
		rp2040.wdt_reset();
		EEPROM.commit();
	}
	i32 pg[3][7];
	EEPROM.get((u16)EEPROM_POS::PID_GAINS, pg);
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 7; j++)
			pidGains[i][j].setRaw(pg[i][j]);
	i32 rf[5][3];
	EEPROM.get((u16)EEPROM_POS::RATE_FACTORS, rf);
	for (int i = 0; i < 5; i++)
		for (int j = 0; j < 3; j++)
			rateFactors[i][j].setRaw(rf[i][j]);
	EEPROM.get((u16)EEPROM_POS::BB_FLAGS, bbFlags);
	EEPROM.get((u16)EEPROM_POS::BB_FREQ_DIVIDER, bbFreqDivider);
	i16 data;
	EEPROM.get((u16)EEPROM_POS::ACCEL_CALIBRATION, data);
	accelCalibrationOffset[0] = data;
	EEPROM.get((u16)EEPROM_POS::ACCEL_CALIBRATION + 2, data);
	accelCalibrationOffset[1] = data;
	EEPROM.get((u16)EEPROM_POS::ACCEL_CALIBRATION + 4, data);
	accelCalibrationOffset[2] = data;
}