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

static int rtc_read_safe(u8* sec, u8* min, u8* hour, u8* day, u8* mon, u8* year)
{
    int retry = 5;
    u8 s1, m1, h1, d1, mo1, y1;
    u8 s2, m2, h2, d2, mo2, y2;
    while (retry--) {
        outb(CMOS_ADDR, RTC_STATUS_A);
        if (inb(CMOS_DATA) & 0x80)
            continue;
        s1 = cmos_read(RTC_SECOND); m1 = cmos_read(RTC_MINUTE);
        h1 = cmos_read(RTC_HOUR);   d1 = cmos_read(RTC_DAY);
        mo1 = cmos_read(RTC_MONTH); y1 = cmos_read(RTC_YEAR);
        outb(CMOS_ADDR, RTC_STATUS_A);
        if (inb(CMOS_DATA) & 0x80)
            continue;
        s2 = cmos_read(RTC_SECOND); m2 = cmos_read(RTC_MINUTE);
        h2 = cmos_read(RTC_HOUR);   d2 = cmos_read(RTC_DAY);
        mo2 = cmos_read(RTC_MONTH); y2 = cmos_read(RTC_YEAR);
        if (s1 == s2 && m1 == m2 && h1 == h2 && d1 == d2 && mo1 == mo2 && y1 == y2) {
            *sec = s1; *min = m1; *hour = h1;
            *day = d1; *mon = mo1; *year = y1;
            return 0;
        }
    }
    return -1;
}

void rtc_read(struct rtc_time* out)
{
    u8 status_b;
    u8 s, mi, h, d, mo, y;
    if (!out)
        return;
    if (rtc_read_safe(&s, &mi, &h, &d, &mo, &y) != 0)
        return;
    outb(CMOS_ADDR, RTC_STATUS_B);
    status_b = inb(CMOS_DATA);
    out->second = s;
    out->minute = mi;
    out->hour   = h;
    out->day    = d;
    out->month  = mo;
    out->year   = y;
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
