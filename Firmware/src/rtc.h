#include "typedefs.h"
#include <pico/aon_timer.h>

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
/// @brief Set the RTC time if quality is at least as high as the current quality.
bool rtcSetTime(const struct timespec *t, u8 quality);
/** @brief Convert a timespec to a tm structure
 *
 * Requires tm_year, tm_mon, tm_mday, tm_hour, tm_min and tm_sec to be set. tm_yday, tm_wday and tm_isdst are ignored
 */
void rtcConvertToTm(const struct timespec *ts, struct tm *tm);
/// @brief Convert a tm structure to a timespec,
void rtcConvertToTimespec(const struct tm *tm, struct timespec *ts);
/// @brief Get the current RTC time as a unix timestamp.
/// @return The number of seconds since the Unix epoch (1970-01-01 00:00:00 UTC). Without the timezone offset.
time_t rtcGetUnixTimestamp();
/// @brief Get the current RTC time as a unix timestamp with the timezone offset applied.
/// @return The number of seconds since the Unix epoch (1970-01-01 00:00:00 UTC) with the timezone offset applied.
time_t rtcGetUnixTimestampWithOffset();
/// @brief Get the current RTC time as a timespec.
bool rtcGetTime(struct timespec *t, bool withOffset = false);
/// @brief Get the current RTC time as a filesystem time (date, time and milliseconds).
void getFsTime(u16 *date, u16 *time, u8 *ms10);
