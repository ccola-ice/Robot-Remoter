#ifndef RTC_CALENDAR_H
#define RTC_CALENDAR_H
#include <stdint.h>

typedef struct RtcCalendar {
    uint16_t year;      /* 2000..2099, full year (never years since 1900). */
    uint8_t month;
    uint8_t day;
    uint8_t weekday;    /* Derived: Monday=1 .. Sunday=7. */
    uint8_t hour;       /* Local civil time, 0..23. */
    uint8_t minute;
    uint8_t second;
} RtcCalendar;

typedef enum {
    RTC_TIME_UNSET = 0,
    RTC_TIME_MANUAL = 1,
    RTC_TIME_GPS = 2
} RtcTimeSource;

static __inline uint8_t RTC_CalendarDaysInMonth(uint16_t year, uint8_t month)
{
    static const uint8_t days[12] = {31U,28U,31U,30U,31U,30U,31U,31U,30U,31U,30U,31U};
    if(year < 2000U || year > 2099U || month < 1U || month > 12U) return 0U;
    return month == 2U && year % 4U == 0U ? 29U : days[month - 1U];
}

/* weekday is output-only; it need not be filled before validation/writing. */
static __inline uint8_t RTC_CalendarValidate(const RtcCalendar *value)
{
    uint8_t days;
    if(!value) return 0U;
    days = RTC_CalendarDaysInMonth(value->year, value->month);
    return days && value->day >= 1U && value->day <= days &&
           value->hour < 24U && value->minute < 60U && value->second < 60U;
}

static __inline uint8_t RTC_CalendarWeekday(uint16_t year, uint8_t month, uint8_t day)
{
    static const uint8_t offset[12] = {0U,3U,2U,5U,0U,3U,5U,1U,4U,6U,2U,4U};
    uint16_t adjusted = year;
    uint8_t result, days = RTC_CalendarDaysInMonth(year, month);
    if(!days || !day || day > days) return 0U;
    if(month < 3U) adjusted--;
    result = (uint8_t)((adjusted + adjusted / 4U - adjusted / 100U + adjusted / 400U +
                       offset[month - 1U] + day) % 7U);
    return result ? result : 7U;
}
#endif
