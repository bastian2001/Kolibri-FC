#include "global.h"
#include <pico/aon_timer.h>

#define EPOCH_2000 946684800
#define DAYS_IN_4_YEARS 1461
#define SECONDS_PER_DAY 86400

u8 rtcTimeQuality = TIME_QUALITY_NONE;
i16 rtcTimezoneOffset = 0;
constexpr u16 days[4][12] =
	{
		{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335},
		{366, 397, 425, 456, 486, 517, 547, 578, 609, 639, 670, 700},
		{731, 762, 790, 821, 851, 882, 912, 943, 974, 1004, 1035, 1065},
		{1096, 1127, 1155, 1186, 1216, 1247, 1277, 1308, 1339, 1369, 1400, 1430},
};

void rtcInit() {
	addSetting(SETTING_TIMEZONE_OFFSET, &rtcTimezoneOffset, 0);

	const timespec time = {
		.tv_sec = EPOCH_2000,
		.tv_nsec = 0};
	aon_timer_start(&time);
	rtcSetTime(&time, TIME_QUALITY_NONE);
}

bool rtcSetTime(const struct timespec *t, u8 quality) {
	if (quality < rtcTimeQuality) return false;
	rtcTimeQuality = quality;
	return aon_timer_set_time(t);
}

void rtcConvertToTm(const struct timespec *ts, struct tm *tm) {
	u32 timestamp = ts->tv_sec;
	if (timestamp < EPOCH_2000) {
		tm->tm_year = 2000;
		tm->tm_mon = 1;
		tm->tm_mday = 1;
		tm->tm_wday = 5;
		tm->tm_yday = 0;
		tm->tm_hour = 0;
		tm->tm_min = 0;
		tm->tm_sec = 0;
		return;
	}
	timestamp -= EPOCH_2000;
	tm->tm_sec = timestamp % 60;
	timestamp /= 60;
	tm->tm_min = timestamp % 60;
	timestamp /= 60;
	tm->tm_hour = timestamp % 24;
	timestamp /= 24;
	tm->tm_wday = (timestamp + 5) % 7; // 2000-01-01 was a Saturday (5)
	u16 year = 2000;

	const u16 *daysOfThisYear = days[0];
	while (timestamp >= DAYS_IN_4_YEARS) {
		year += 4;
		timestamp -= DAYS_IN_4_YEARS;
	}
	for (u8 i = 3; i >= 0; i--) {
		if (timestamp >= days[i][0]) {
			year += i;
			daysOfThisYear = days[i];
			break;
		}
	}
	tm->tm_year = year;
	tm->tm_yday = timestamp;
	for (u8 i = 11; i >= 0; i--) {
		if (timestamp >= daysOfThisYear[i]) {
			tm->tm_mon = i + 1;
			tm->tm_mday = timestamp - daysOfThisYear[i] + 1;
			break;
		}
	}
}

void rtcConvertToTimespec(const struct tm *tm, struct timespec *ts) {
	u32 t = EPOCH_2000;
	t += (DAYS_IN_4_YEARS * SECONDS_PER_DAY) * ((tm->tm_year - 2000) / 4); // complete 4-year cycles
	t += SECONDS_PER_DAY * days[tm->tm_year % 4][tm->tm_mon - 1]; // until the beginning of the month
	t += SECONDS_PER_DAY * (tm->tm_mday - 1); // until the beginning of the day
	t += tm->tm_hour * 3600; // until the beginning of the hour
	t += tm->tm_min * 60; // until the beginning of the minute
	t += tm->tm_sec; // until the beginning of the second
	ts->tv_sec = t;
	ts->tv_nsec = 0; // No nanoseconds in this implementation
}

void getFsTime(u16 *date, u16 *time, u8 *ms10) {
	// While Linux expects a UTC timestamp here, officially FAT filesystems (and Windows) use local time
	struct timespec t;
	if (!rtcGetTime(&t, true)) {
		*date = 0;
		*time = 0;
		*ms10 = 0;
		return;
	}
	struct tm tm;
	rtcConvertToTm(&t, &tm);

	*date = FS_DATE(tm.tm_year, tm.tm_mon, tm.tm_mday);
	*time = FS_TIME(tm.tm_hour, tm.tm_min, tm.tm_sec);
	// 10ms chunks since the last even second
	*ms10 = (tm.tm_sec & 1) * 100 + t.tv_nsec / 10'000'000;
}

time_t rtcGetUnixTimestamp() {
	timespec t;
	rtcGetTime(&t);
	return t.tv_sec;
}
time_t rtcGetUnixTimestampWithOffset() {
	timespec t;
	rtcGetTime(&t, true);
	return t.tv_sec;
}

bool rtcGetTime(struct timespec *t, bool withOffset) {
	if (aon_timer_get_time(t)) {
		if (withOffset) t->tv_sec += rtcTimezoneOffset * 60; // convert minutes to seconds
		return true;
	}
	return false;
}
