#include <Arduino.h>
enum class EEPROM_POS : uint16_t
{
	UAV_NAME = 0,
	UAV_TYPE = 20,
	PROP_DIRECTION, // 1 = PROPS_OUT, -1 = PROPS_IN
	PID_GAINS,
	RATE_FACTORS = 106,
	MOTOR_ORDER = 166,			   // 2 bits per motor: 7,6: fl, 5,4: rl, 3,2: fr, 1,0: rr
	BATTERY_EMPTY_THRESHOLD = 167, // 2 bytes, centivolts

};