#include <Arduino.h>
enum class EEPROM_POS : uint16_t
{
	EEPROM_INIT = 0,
	EEPROM_VERSION = 1,
	UAV_NAME = 2,
	PID_GAINS = 22,
	RATE_FACTORS = 106,
	MOTOR_ORDER = 166,			   // 2 bits per motor: 7,6: fl, 5,4: rl, 3,2: fr, 1,0: rr
	PROP_DIRECTION = 167,		   // 1 = PROPS_OUT, -1 = PROPS_IN
	BATTERY_EMPTY_THRESHOLD = 168, // 2 bytes, centivolts
	UAV_TYPE = 169,

};

void readEEPROM();