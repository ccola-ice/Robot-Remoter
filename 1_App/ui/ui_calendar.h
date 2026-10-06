#ifndef UI_CALENDAR_H
#define UI_CALENDAR_H
#include "rtc_calendar.h"

enum { CALENDAR_BROWSE, CALENDAR_ACTIONS, CALENDAR_EDIT };
enum { CALENDAR_TODAY, CALENDAR_SET_TIME, CALENDAR_GPS_SYNC, CALENDAR_ACTION_COUNT };
enum { CALENDAR_STATUS_NONE, CALENDAR_STATUS_MANUAL_SAVED, CALENDAR_STATUS_GPS_SAVED,
       CALENDAR_STATUS_GPS_WAIT, CALENDAR_STATUS_WRITE_FAILED, CALENDAR_STATUS_CANCELED };

/* now 是实时 RTC 快照，draft 是待保存时间，year/month 是独立的浏览月份。
 * 三者分离后，周期走时不会覆盖手动编辑，也不会强制跳回当前月份。 */
typedef struct {
    RtcCalendar now;
    RtcCalendar draft;
    uint16_t year;
    uint8_t month;
    /* mode 决定方向键行为；action 为操作菜单焦点，field 为时间编辑焦点。 */
    uint8_t mode;
    uint8_t action;
    uint8_t field; /* 年、月、日、时、分、秒、保存 */
    uint8_t status;
    /* readable 仅表示读取成功，time_valid 另表示已校时；source 记录 RTC 时间来源。 */
    uint8_t readable;
    uint8_t time_valid;
    uint8_t source;
    uint8_t gps_available;
} GuiCalendarState;

/* 仅修改草稿：调整任何字段都不会写入 RTC。 */
/* direction 的正负决定单步增减，字段在自身合法范围循环；field 指向保存项时不改日期。 */
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
    /* 年月变化可能缩短当月天数，先夹紧日期，再重新计算星期。 */
    maximum=RTC_CalendarDaysInMonth(date->year,date->month);
    if(date->day > maximum) date->day=(uint8_t)maximum;
    date->weekday=RTC_CalendarWeekday(date->year,date->month,date->day);
}

/* 浏览月份可跨年；在 2000 年 1 月至 2099 年 12 月范围内移动，到达范围边界后保持原值。 */
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
