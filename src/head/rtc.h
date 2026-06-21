#ifndef RTC_H
#define RTC_H

#include "harlin_API.h"

struct rtc_time {
    u8 second;
    u8 minute;
    u8 hour;
    u8 day;
    u8 month;
    u16 year;
};

void rtc_init(void);
void rtc_read(struct rtc_time* out);
u64 rtc_boot_seconds(void);

#endif
