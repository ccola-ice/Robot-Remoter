#ifndef CALENDAR_UI_H
#define CALENDAR_UI_H
#include "rtc_calendar.h"

enum { CALENDAR_BROWSE, CALENDAR_ACTIONS, CALENDAR_EDIT };
enum { CALENDAR_TODAY, CALENDAR_SET_TIME, CALENDAR_GPS_SYNC, CALENDAR_ACTION_COUNT };
enum { CALENDAR_STATUS_NONE, CALENDAR_STATUS_MANUAL_SAVED, CALENDAR_STATUS_GPS_SAVED,
       CALENDAR_STATUS_GPS_WAIT, CALENDAR_STATUS_WRITE_FAILED, CALENDAR_STATUS_CANCELED };

typedef struct {
    RtcCalendar now;
    RtcCalendar draft;
    uint16_t year;
    uint8_t month;
    uint8_t mode;
    uint8_t action;
    uint8_t field; /* year, month, day, hour, minute, second, save */
    uint8_t status;
    uint8_t readable;
    uint8_t time_valid;
    uint8_t source;
    uint8_t gps_available;
} GuiCalendarState;

/* Pure draft editing: touching a field never writes the RTC. */
static __inline void calendar_edit_step(GuiCalendarState *state, int8_t direction)
{
    RtcCalendar *date = &state->draft;
    uint16_t value, minimum, maximum;
    if(!direction || state->field >= 6U) return;
    switch(state->field) {
    case 0U: value=date->year; minimum=2000U; maximum=2099U; break;
    case 1U: value=date->month; minimum=1U; maximum=12U; break;
    case 2U: value=date->day; minimum=1U; maximum=RTC_CalendarDaysInMonth(date->year,date->month); break;
    case 3U: value=date->hour; minimum=0U; maximum=23U; break;
    case 4U: value=date->minute; minimum=0U; maximum=59U; break;
    default: value=date->second; minimum=0U; maximum=59U; break;
    }
    if(direction > 0) value = value >= maximum ? minimum : (uint16_t)(value+1U);
    else value = value <= minimum ? maximum : (uint16_t)(value-1U);
    switch(state->field) {
    case 0U: date->year=value; break;
    case 1U: date->month=(uint8_t)value; break;
    case 2U: date->day=(uint8_t)value; break;
    case 3U: date->hour=(uint8_t)value; break;
    case 4U: date->minute=(uint8_t)value; break;
    default: date->second=(uint8_t)value; break;
    }
    maximum=RTC_CalendarDaysInMonth(date->year,date->month);
    if(date->day > maximum) date->day=(uint8_t)maximum;
    date->weekday=RTC_CalendarWeekday(date->year,date->month,date->day);
}

static __inline void calendar_month_step(GuiCalendarState *state, int8_t direction)
{
    if(direction > 0) {
        if(state->month < 12U) state->month++;
        else if(state->year < 2099U) { state->year++; state->month=1U; }
    } else if(direction < 0) {
        if(state->month > 1U) state->month--;
        else if(state->year > 2000U) { state->year--; state->month=12U; }
    }
}
#endif
