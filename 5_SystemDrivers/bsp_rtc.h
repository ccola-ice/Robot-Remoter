#ifndef __BSP_RTC_H__
#define __BSP_RTC_H__
#include "stm32f4xx.h"
#include "rtc_calendar.h"

/* The board's X2 is a 32.768 kHz crystal on PC14/PC15. */
#define RTC_CLOCK_SOURCE_LSE
#define ASYNCHPREDIV 0x7fU
#define SYNCHPREDIV  0xffU
#define RTC_BKP_DRX  RTC_BKP_DR0
#define RTC_BKP_DATA 0x32f3U

enum {
    RTC_CALENDAR_OK = 0,
    RTC_CALENDAR_ARGUMENT_ERROR = 1,
    RTC_CALENDAR_CLOCK_ERROR = 2,
    RTC_CALENDAR_READ_ERROR = 3,
    RTC_CALENDAR_WRITE_ERROR = 4
};

/* 0 = success. A running but unset calendar is not a hardware boot failure.
 * Ordinary resets preserve calendar, prescaler, and explicit setting source. */
uint8_t RTC_Config(void);
uint8_t RTC_CLK_Config(void);

/* 0 = a coherent, representable calendar was read. Accuracy is separate:
 * RTC_TimeIsValid/RTC_TimeSource distinguish unset/legacy time from set time. */
uint8_t RTC_ReadCalendar(RtcCalendar *calendar);

/* 0 = both date and time written and verified. Source must be MANUAL or GPS.
 * The local civil time is used as supplied (GPS caller converts UTC to UTC+8).
 * weekday is computed. This layer never implements automatic GPS policy. */
uint8_t RTC_SetCalendar(const RtcCalendar *calendar, RtcTimeSource source);
uint8_t RTC_TimeIsValid(void);
RtcTimeSource RTC_TimeSource(void);

/* Compatibility: initializes an invalid calendar to an explicitly unset
 * 2000-01-01 baseline; never injects compiler date/time. */
void RTC_TimeAndDate_Set(void);
void RTC_TimeAndDate_Show(void);
uint8_t RTC_SynchronizeCalendar(uint16_t year, uint8_t month, uint8_t day,
                                uint8_t hour, uint8_t minute, uint8_t second);
#endif
