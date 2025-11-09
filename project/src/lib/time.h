#ifndef TIME_H
#define TIME_H
#include <stdint.h>

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} datetime_t;


datetime_t time_get_datetime(void);
uint32_t time_get_unix_timestamp(void);
uint32_t time_datetime_to_unix(datetime_t dt);
datetime_t time_unix_to_datetime(uint32_t ts);

#endif