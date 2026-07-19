#include "time.h"
#include "io.h"
#include "../include/timer/pit.h"

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static void rtc_wait(void) {
    while (cmos_read(0x0A) & 0x80);
}

static uint8_t bcd2bin(uint8_t v) {
    return ((v >> 4) * 10) + (v & 0x0F);
}

datetime_t time_get_datetime(void) {
    datetime_t a, b;
    uint8_t regb;

    rtc_wait();
    a.second = cmos_read(0x00);
    a.minute = cmos_read(0x02);
    a.hour   = cmos_read(0x04);
    a.day    = cmos_read(0x07);
    a.month  = cmos_read(0x08);
    a.year   = cmos_read(0x09);

    do {
        b = a;
        rtc_wait();
        a.second = cmos_read(0x00);
        a.minute = cmos_read(0x02);
        a.hour   = cmos_read(0x04);
        a.day    = cmos_read(0x07);
        a.month  = cmos_read(0x08);
        a.year   = cmos_read(0x09);
    } while (a.second != b.second || a.minute != b.minute ||
             a.hour   != b.hour   || a.day    != b.day);

    regb = cmos_read(0x0B);

    if (!(regb & 0x04)) {
        a.second = bcd2bin(a.second);
        a.minute = bcd2bin(a.minute);
        a.hour   = bcd2bin(a.hour & 0x7F) | (a.hour & 0x80);
        a.day    = bcd2bin(a.day);
        a.month  = bcd2bin(a.month);
        a.year   = bcd2bin(a.year);
    }

    if (!(regb & 0x02) && (a.hour & 0x80)) {
        a.hour = ((a.hour & 0x7F) + 12) % 24;
    } else {
        a.hour &= 0x7F;
    }

    a.year = (uint16_t)(2000 + a.year);
    return a;
}

static int is_leap(uint16_t y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

uint32_t time_datetime_to_unix(datetime_t dt) {
    uint32_t days = 0;
    uint16_t y;
    uint8_t m;
    uint8_t dim[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};

    for (y = 1970; y < dt.year; y++)
        days += is_leap(y) ? 366 : 365;

    if (is_leap(dt.year)) dim[2] = 29;
    for (m = 1; m < dt.month; m++)
        days += dim[m];

    days += dt.day - 1;
    return days * 86400 + dt.hour * 3600 + dt.minute * 60 + dt.second;
}

datetime_t time_unix_to_datetime(uint32_t ts) {
    datetime_t dt = {0};
    uint8_t dim[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    uint32_t days;

    dt.second = ts % 60;
    dt.minute = (ts / 60) % 60;
    dt.hour   = (ts / 3600) % 24;
    days      = ts / 86400;
    dt.year   = 1970;

    while (1) {
        uint16_t dy = is_leap(dt.year) ? 366 : 365;
        if (days < dy) break;
        days -= dy;
        dt.year++;
    }

    if (is_leap(dt.year)) dim[2] = 29;
    dt.month = 1;
    while (days >= dim[dt.month]) {
        days -= dim[dt.month];
        dt.month++;
    }
    dt.day = days + 1;
    return dt;
}

uint32_t time_get_unix_timestamp(void) {
    return time_datetime_to_unix(time_get_datetime());
}

uint32_t get_uptime(void) {
    return (uint32_t)pit_get_ticks();
}
