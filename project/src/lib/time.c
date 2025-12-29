#include "time.h"
#include "io.h"

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return ((bcd / 16) * 10) + (bcd & 0xF);
}

datetime_t time_get_datetime(void) {
    datetime_t dt;
    
    dt.second = bcd_to_bin(cmos_read(0x00));
    dt.minute = bcd_to_bin(cmos_read(0x02));
    dt.hour   = bcd_to_bin(cmos_read(0x04));
    dt.day    = bcd_to_bin(cmos_read(0x07));
    dt.month  = bcd_to_bin(cmos_read(0x08));
    dt.year   = 2000 + bcd_to_bin(cmos_read(0x09));
    
    return dt;
}

static int is_leap_year(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

uint32_t time_datetime_to_unix(datetime_t dt) {
    uint32_t days = 0;
    
    for (uint16_t y = 1970; y < dt.year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }
    
    uint8_t days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (is_leap_year(dt.year))
        days_in_month[2] = 29;
    
    for (uint8_t m = 1; m < dt.month; m++)
        days += days_in_month[m];
    
    days += dt.day - 1;
    
    return days * 86400 + dt.hour * 3600 + dt.minute * 60 + dt.second;
}

datetime_t time_unix_to_datetime(uint32_t ts) {
    datetime_t dt = {0};
    
    dt.second = ts % 60;
    dt.minute = (ts / 60) % 60;
    dt.hour = (ts / 3600) % 24;
    
    uint32_t days = ts / 86400;
    
    dt.year = 1970;
    while (1) {
        uint16_t days_in_year = is_leap_year(dt.year) ? 366 : 365;
        if (days < days_in_year) break;
        days -= days_in_year;
        dt.year++;
    }
    
    uint8_t days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (is_leap_year(dt.year))
        days_in_month[2] = 29;
    
    dt.month = 1;
    while (days >= days_in_month[dt.month]) {
        days -= days_in_month[dt.month];
        dt.month++;
    }
    
    dt.day = days + 1;
    
    return dt;
}

uint32_t time_get_unix_timestamp(void) {
    return time_datetime_to_unix(time_get_datetime());
}