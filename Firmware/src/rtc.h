#include "Arduino.h"
#include "hardware/rtc.h"
#include "typedefs.h"

enum TimeQuality : u8 {
	TIME_QUALITY_NONE = 0,
	TIME_QUALITY_FULLY_RESOLVED = 1,
	TIME_QUALITY_VALID = 2,
	TIME_QUALITY_MSP = 3,
	TIME_QUALITY_CONFIRMED = 4,
};
extern u8 rtcTimeQuality; // TIME_QUALITY_...

void rtcInit();
bool rtcSetDatetime(datetime_t *t, u8 quality, bool hasDotw = true);
void rtcConvertToDatetime(u32 timestamp, datetime_t *t);
time_t rtcGetUnixTimestamp();
u32 rtcGetBlackboxTimestamp();
void setDotwInDatetime(datetime_t *t);
inline bool rtcGetDatetime(datetime_t *t) {
	return rtc_get_datetime(t);
}