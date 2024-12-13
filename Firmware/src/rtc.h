#include "Arduino.h"
#include "hardware/rtc.h"
#include "typedefs.h"

void rtcInit();
void rtcSetDatetime(datetime_t *t);
void rtcConvertToDatetime(u32 timestamp, datetime_t *t);
void rtcGetDatetime(datetime_t *t);
time_t rtcGetUnixTimestamp();
u32 rtcGetBlackboxTimestamp();
void setDotwInDatetime(datetime_t *t);