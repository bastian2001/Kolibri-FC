#include "global.h"
void readEEPROM()
{
	if (EEPROM.read((uint16_t)EEPROM_POS::EEPROM_INIT) != 0x42)
	{
		// EEPROM is not initialized
		initPID();
		EEPROM.write((uint16_t)EEPROM_POS::EEPROM_INIT, 0x42);
		EEPROM.write((uint16_t)EEPROM_POS::EEPROM_VERSION, 0x01);
		int32_t pg[3][7];
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 7; j++)
				pg[i][j] = pidGains[i][j].getRaw();
		EEPROM.put((uint16_t)EEPROM_POS::PID_GAINS, pg);
		int32_t rf[5][3];
		for (int i = 0; i < 5; i++)
			for (int j = 0; j < 3; j++)
				rf[i][j] = rateFactors[i][j].getRaw();
		EEPROM.put((uint16_t)EEPROM_POS::RATE_FACTORS, rf);
		rp2040.wdt_reset();
		EEPROM.commit();
	}
	int32_t pg[3][7];
	EEPROM.get((uint16_t)EEPROM_POS::PID_GAINS, pg);
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 7; j++)
			pidGains[i][j].setRaw(pg[i][j]);
	int32_t rf[5][3];
	EEPROM.get((uint16_t)EEPROM_POS::RATE_FACTORS, rf);
	for (int i = 0; i < 5; i++)
		for (int j = 0; j < 3; j++)
			rateFactors[i][j].setRaw(rf[i][j]);
}