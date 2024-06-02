#include "global.h"

#define EPOCH_2000 946684800
#define DAYS_IN_4_YEARS 1461
const u16 days[4][12] =
	{
		{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335},
		{366, 397, 425, 456, 486, 517, 547, 578, 609, 639, 670, 700},
		{731, 762, 790, 821, 851, 882, 912, 943, 974, 1004, 1035, 1065},
		{1096, 1127, 1155, 1186, 1216, 1247, 1277, 1308, 1339, 1369, 1400, 1430},
};

void rtcInit() {
	u32 rtcFreq = clock_get_hz(clk_rtc);
	assert(rtcFreq != 0);

	reset_block(RESETS_RESET_RTC_BITS);
	unreset_block_wait(RESETS_RESET_RTC_BITS);

	rtcFreq -= 1;
	assert(rtcFreq <= RTC_CLKDIV_M1_BITS);
	rtc_hw->clkdiv_m1 = rtcFreq;

	datetime_t t;
	rtcConvertToDatetime(EPOCH_2000, &t);
	rtcSetDatetime(&t);
}

void rtcSetDatetime(datetime_t *t) {
	rtc_hw->setup_0 = (((u32)t->year) << RTC_SETUP_0_YEAR_LSB) | (((u32)t->month) << RTC_SETUP_0_MONTH_LSB) | (((u32)t->day) << RTC_SETUP_0_DAY_LSB);
	rtc_hw->setup_1 = (((u32)t->dotw) << RTC_SETUP_1_DOTW_LSB) | (((u32)t->hour) << RTC_SETUP_1_HOUR_LSB) | (((u32)t->min) << RTC_SETUP_1_MIN_LSB) | (((u32)t->sec) << RTC_SETUP_1_SEC_LSB);
	rtc_hw->ctrl    = RTC_CTRL_LOAD_BITS;
}

void rtcConvertToDatetime(u32 timestamp, datetime_t *t) {
	if (timestamp < EPOCH_2000) {
		t->year  = 2000;
		t->month = 1;
		t->day   = 1;
		t->dotw  = 5;
		t->hour  = 0;
		t->min   = 0;
		t->sec   = 0;
		return;
	}
	timestamp -= EPOCH_2000;
	t->sec = timestamp % 60;
	timestamp /= 60;
	t->min = timestamp % 60;
	timestamp /= 60;
	t->hour = timestamp % 24;
	timestamp /= 24;
	t->dotw  = (timestamp + 5) % 7; // 2000-01-01 was a Saturday (5)
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
	t->year = year;
	for (u8 i = 11; i >= 0; i--) {
		if (timestamp >= daysOfThisYear[i]) {
			t->month = i + 1;
			t->day   = timestamp - daysOfThisYear[i] + 1;
			break;
		}
	}
}

void rtcGetDatetime(datetime_t *t) {
	u32 setup0 = rtc_hw->setup_0;
	u32 setup1 = rtc_hw->setup_1;
	t->year    = (setup0 & RTC_SETUP_0_YEAR_BITS) >> RTC_SETUP_0_YEAR_LSB;
	t->month   = (setup0 & RTC_SETUP_0_MONTH_BITS) >> RTC_SETUP_0_MONTH_LSB;
	t->day     = (setup0 & RTC_SETUP_0_DAY_BITS) >> RTC_SETUP_0_DAY_LSB;
	t->dotw    = (setup1 & RTC_SETUP_1_DOTW_BITS) >> RTC_SETUP_1_DOTW_LSB;
	t->hour    = (setup1 & RTC_SETUP_1_HOUR_BITS) >> RTC_SETUP_1_HOUR_LSB;
	t->min     = (setup1 & RTC_SETUP_1_MIN_BITS) >> RTC_SETUP_1_MIN_LSB;
	t->sec     = (setup1 & RTC_SETUP_1_SEC_BITS) >> RTC_SETUP_1_SEC_LSB;
}

u32 rtcGetBlackboxTimestamp() {
	u32 s0 = rtc_hw->setup_0;
	u32 s1 = rtc_hw->setup_1;
	return (s1 & RTC_SETUP_1_SEC_BITS) | (s1 & RTC_SETUP_1_MIN_BITS) >> 2 | (s1 & RTC_SETUP_1_HOUR_BITS) >> 4 | (s0 & RTC_SETUP_0_DAY_BITS) << 17 | (s0 & RTC_SETUP_0_MONTH_BITS) << 14 | ((s0 & RTC_SETUP_0_YEAR_BITS) - (2000 << RTC_SETUP_0_YEAR_LSB)) << 14;
}

void setDotwInDatetime(datetime_t *t) {
	u32 daysSince2000 = 5;
	u16 years         = t->year - 2000;
	daysSince2000 += (years / 4) * 1461;
	daysSince2000 += days[years % 4][t->month - 1] + t->day - 1;
	t->dotw = daysSince2000 % 7;
}