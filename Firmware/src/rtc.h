#include "Arduino.h"
#include "typedefs.h"

typedef struct {
	int16_t year; ///< 0..4095
	int8_t month; ///< 1..12, 1 is January
	int8_t day; ///< 1..28,29,30,31 depending on month
	int8_t dotw; ///< 0..6, 0 is Sunday
	int8_t hour; ///< 0..23
	int8_t min; ///< 0..59
	int8_t sec; ///< 0..59
} datetime_t;

enum TimeQuality : u8 {
	TIME_QUALITY_NONE = 0,
	TIME_QUALITY_FULLY_RESOLVED = 1,
	TIME_QUALITY_VALID = 2,
	TIME_QUALITY_MSP = 3,
	TIME_QUALITY_CONFIRMED = 4,
};
extern u8 rtcTimeQuality; // TIME_QUALITY_...
extern i16 rtcTimezoneOffset; // RTC has UTC time, this is the offset to local time, e.g. +60 for GMT+1

void rtcInit();
bool rtcSetDatetime(datetime_t *t, u8 quality, bool hasDotw = true);
void rtcConvertToDatetime(u32 timestamp, datetime_t *t);
time_t rtcGetUnixTimestamp();
void setDotwInDatetime(datetime_t *t);
bool rtcGetDatetime(datetime_t *t);