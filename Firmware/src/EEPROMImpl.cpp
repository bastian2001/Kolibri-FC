#include "global.h"
void readEEPROM()
{
	if (EEPROM.read((uint16_t)EEPROM_POS::EEPROM_INIT) != 0x42)
	{
		// EEPROM is not initialized
		initPID();
		EEPROM.write((uint16_t)EEPROM_POS::EEPROM_INIT, 0x42);
		EEPROM.write((uint16_t)EEPROM_POS::EEPROM_VERSION, 0x01);
		EEPROM.put((uint16_t)EEPROM_POS::PID_GAINS, pidGains);
		EEPROM.put((uint16_t)EEPROM_POS::RATE_FACTORS, rateFactors);
		rp2040.wdt_reset();
		EEPROM.commit();
	}
	EEPROM.get((uint16_t)EEPROM_POS::PID_GAINS, pidGains);
	EEPROM.get((uint16_t)EEPROM_POS::RATE_FACTORS, rateFactors);
}