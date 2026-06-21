#include "rtc.h"
#include "io.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

#define RTC_SECOND 0x00
#define RTC_MINUTE 0x02
#define RTC_HOUR   0x04
#define RTC_DAY    0x07
#define RTC_MONTH  0x08
#define RTC_YEAR   0x09
#define RTC_STATUS_A 0x0A
#define RTC_STATUS_B 0x0B

static u8 cmos_read(u8 reg)
{
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static u8 bcd_to_bin(u8 val)
{
    return ((val >> 4) * 10) + (val & 0x0F);
}

void rtc_init(void)
{
    cmos_read(RTC_STATUS_B);
}

void rtc_read(struct rtc_time* out)
{
    u8 status_b;
    if (!out)
        return;
    outb(CMOS_ADDR, RTC_STATUS_B);
    status_b = inb(CMOS_DATA);
    out->second = cmos_read(RTC_SECOND);
    out->minute = cmos_read(RTC_MINUTE);
    out->hour   = cmos_read(RTC_HOUR);
    out->day    = cmos_read(RTC_DAY);
    out->month  = cmos_read(RTC_MONTH);
    out->year   = cmos_read(RTC_YEAR);
    if (!(status_b & 0x04)) {
        out->second = bcd_to_bin(out->second);
        out->minute = bcd_to_bin(out->minute);
        out->hour   = bcd_to_bin(out->hour);
        out->day    = bcd_to_bin(out->day);
        out->month  = bcd_to_bin(out->month);
        out->year   = bcd_to_bin((u8)out->year);
    }
    if (!(status_b & 0x02)) {
        if (out->hour & 0x80)
            out->hour = ((out->hour & 0x7F) + 12) % 24;
    }
    out->year += 2000;
}

u64 rtc_boot_seconds(void)
{
    struct rtc_time t;
    rtc_read(&t);
    return (u64)t.second + (u64)t.minute * 60 + (u64)t.hour * 3600;
}
