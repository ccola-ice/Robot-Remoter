#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "calendar_ui.h"

typedef enum { MENU_KEY_LEFT=0,MENU_KEY_RIGHT,MENU_KEY_OK,MENU_KEY_BACK } MenuKey;
enum { MENU_PAGE_CATEGORY=1,MENU_PAGE_CALENDAR=2 };
static GuiCalendarState calendar_state;
static RtcCalendar rtc_now,last_written;
static RtcTimeSource rtc_source,last_source;
static uint8_t read_error,write_error,rtc_valid,gps_fresh,gps_expires_during_action;
static uint8_t current_page,page_changed,page_dirty;
static unsigned rtc_writes,fresh_checks;
static struct { int year,mon,day,hour,min,sec; } beiJingTime;

static uint8_t RTC_ReadCalendar(RtcCalendar *value)
{
    assert(value);
    if(read_error) return read_error;
    *value=rtc_now;
    return 0U;
}
static uint8_t RTC_TimeIsValid(void) { return rtc_valid && !read_error; }
static RtcTimeSource RTC_TimeSource(void) { return read_error ? RTC_TIME_UNSET:rtc_source; }
static uint8_t RTC_SetCalendar(const RtcCalendar *value,RtcTimeSource source)
{
    assert(RTC_CalendarValidate(value));
    assert(source==RTC_TIME_MANUAL || source==RTC_TIME_GPS);
    rtc_writes++; last_written=*value; last_source=source;
    if(write_error) return write_error;
    rtc_now=*value; rtc_now.weekday=RTC_CalendarWeekday(value->year,value->month,value->day);
    rtc_source=source; rtc_valid=1U;
    return 0U;
}
static uint8_t gps_time_is_fresh(void)
{
    fresh_checks++;
    return gps_fresh && (!gps_expires_during_action || fresh_checks==1U);
}
#include "calendar_menu_impl.inc"

static void load(unsigned year,unsigned month,unsigned day)
{
    memset(&rtc_now,0,sizeof(rtc_now));
    rtc_now.year=(uint16_t)year; rtc_now.month=(uint8_t)month; rtc_now.day=(uint8_t)day;
    rtc_now.hour=23U; rtc_now.minute=59U; rtc_now.second=58U;
    rtc_now.weekday=RTC_CalendarWeekday(year,month,day);
    rtc_valid=1U; rtc_source=RTC_TIME_MANUAL;
    read_error=write_error=gps_fresh=gps_expires_during_action=0U;
    current_page=MENU_PAGE_CALENDAR; page_changed=page_dirty=0U; rtc_writes=fresh_checks=0U;
    memset(&beiJingTime,0,sizeof(beiJingTime)); menu_calendar_load();
}
static void action(unsigned selected)
{
    unsigned i;
    assert(calendar_state.mode==CALENDAR_BROWSE);
    menu_handle_calendar_key(MENU_KEY_OK);
    assert(calendar_state.mode==CALENDAR_ACTIONS);
    for(i=0U;i<CALENDAR_ACTION_COUNT && calendar_state.action!=selected;i++)
        menu_handle_calendar_key(MENU_KEY_RIGHT);
    assert(calendar_state.action==selected);
    menu_handle_calendar_key(MENU_KEY_OK);
}
static void draft_math(void)
{
    RtcCalendar before;
    unsigned field;
    load(2024U,1U,31U);
    calendar_state.field=1U; calendar_edit_step(&calendar_state,1);
    assert(calendar_state.draft.month==2U && calendar_state.draft.day==29U);
    calendar_state.field=0U; calendar_edit_step(&calendar_state,1);
    assert(calendar_state.draft.year==2025U && calendar_state.draft.day==28U);
    calendar_state.draft.year=2099U; calendar_edit_step(&calendar_state,1);
    assert(calendar_state.draft.year==2000U); calendar_edit_step(&calendar_state,-1);
    assert(calendar_state.draft.year==2099U);
    calendar_state.draft.month=12U; calendar_state.field=1U;
    calendar_edit_step(&calendar_state,1); assert(calendar_state.draft.month==1U);
    calendar_edit_step(&calendar_state,-1); assert(calendar_state.draft.month==12U);
    for(field=2U;field<6U;field++) {
        calendar_state.field=(uint8_t)field;
        if(field==2U) calendar_state.draft.day=31U;
        if(field==3U) calendar_state.draft.hour=23U;
        if(field==4U) calendar_state.draft.minute=59U;
        if(field==5U) calendar_state.draft.second=59U;
        calendar_edit_step(&calendar_state,1);
        assert(RTC_CalendarValidate(&calendar_state.draft));
        calendar_edit_step(&calendar_state,-1);
        assert(RTC_CalendarValidate(&calendar_state.draft));
    }
    before=calendar_state.draft; calendar_state.field=6U; calendar_edit_step(&calendar_state,1);
    assert(!memcmp(&before,&calendar_state.draft,sizeof(before)) && !rtc_writes);
    calendar_state.field=0U; calendar_edit_step(&calendar_state,0);
    assert(!memcmp(&before,&calendar_state.draft,sizeof(before)));
    puts("calendar draft: leap-day/month-end clamp, 2000..2099 field wrap, clock ranges, derived weekday passed");
}
static void browsing_and_cancel(void)
{
    RtcCalendar original,draft;
    unsigned i;
    load(2000U,1U,1U); original=rtc_now;
    for(i=0U;i<1300U;i++) menu_handle_calendar_key(MENU_KEY_RIGHT);
    assert(calendar_state.year==2099U && calendar_state.month==12U && !rtc_writes);
    for(i=0U;i<1300U;i++) menu_handle_calendar_key(MENU_KEY_LEFT);
    assert(calendar_state.year==2000U && calendar_state.month==1U && !rtc_writes);
    action(CALENDAR_SET_TIME); assert(calendar_state.mode==CALENDAR_EDIT && !rtc_writes);
    menu_handle_calendar_key(MENU_KEY_RIGHT); assert(calendar_state.draft.year==2001U);
    draft=calendar_state.draft; rtc_now.second=59U; menu_calendar_refresh();
    assert(calendar_state.now.second==59U && !memcmp(&draft,&calendar_state.draft,sizeof(draft)));
    menu_handle_calendar_key(MENU_KEY_BACK);
    assert(calendar_state.mode==CALENDAR_BROWSE && calendar_state.status==CALENDAR_STATUS_CANCELED);
    assert(!rtc_writes && rtc_now.year==original.year);
    action(CALENDAR_SET_TIME); assert(calendar_state.draft.year==2000U && calendar_state.draft.second==59U);
    menu_handle_calendar_key(MENU_KEY_BACK); menu_handle_calendar_key(MENU_KEY_BACK);
    assert(current_page==MENU_PAGE_CATEGORY && page_changed && !rtc_writes);
    load(2026U,9U,21U); rtc_valid=0U; rtc_source=RTC_TIME_UNSET; menu_calendar_load();
    assert(calendar_state.readable && !calendar_state.time_valid && calendar_state.year==2026U);
    read_error=2U; menu_calendar_load();
    assert(!calendar_state.readable && calendar_state.year==2000U && calendar_state.month==1U);
    assert(RTC_CalendarValidate(&calendar_state.draft));
    puts("calendar browsing: month bounds and Today/cancel/navigation are read-only; live refresh leaves drafts intact");
}
static void manual_save(void)
{
    RtcCalendar original,draft;
    unsigned field;
    load(2024U,1U,31U); original=rtc_now; action(CALENDAR_SET_TIME);
    for(field=0U;field<6U;field++) {
        assert(calendar_state.field==field && calendar_state.mode==CALENDAR_EDIT);
        menu_handle_calendar_key(MENU_KEY_RIGHT);
        assert(!rtc_writes && !memcmp(&rtc_now,&original,sizeof(original)));
        menu_handle_calendar_key(MENU_KEY_OK);
        assert(!rtc_writes);
    }
    assert(calendar_state.field==6U && calendar_state.mode==CALENDAR_EDIT);
    draft=calendar_state.draft;
    assert(draft.year==2025U && draft.month==2U && draft.day==1U);
    assert(draft.hour==0U && draft.minute==0U && draft.second==59U);
    write_error=4U; menu_handle_calendar_key(MENU_KEY_OK);
    assert(rtc_writes==1U && calendar_state.mode==CALENDAR_EDIT && calendar_state.field==6U);
    assert(calendar_state.status==CALENDAR_STATUS_WRITE_FAILED && !memcmp(&draft,&calendar_state.draft,sizeof(draft)));
    assert(!memcmp(&rtc_now,&original,sizeof(original)));
    write_error=0U; menu_handle_calendar_key(MENU_KEY_OK);
    assert(rtc_writes==2U && last_source==RTC_TIME_MANUAL && calendar_state.mode==CALENDAR_BROWSE);
    assert(calendar_state.status==CALENDAR_STATUS_MANUAL_SAVED && calendar_state.year==2025U && calendar_state.month==2U);
    assert(!memcmp(&last_written,&draft,sizeof(draft)));
    menu_calendar_refresh(); assert(calendar_state.time_valid && calendar_state.now.year==2025U);
    puts("calendar manual set: six field confirmations never write; explicit Save only; failure keeps draft and retry succeeds");
}
static void gps_set(void)
{
    static const int invalid[][6]={
        {99,1,1,0,0,0},{200,1,1,0,0,0},{126,0,1,0,0,0},{126,13,1,0,0,0},
        {126,2,29,0,0,0},{126,4,31,0,0,0},{126,1,0,0,0,0},{126,1,1,-1,0,0},
        {126,1,1,24,0,0},{126,1,1,0,60,0},{126,1,1,0,0,60}
    };
    unsigned i;
    load(2026U,9U,21U); beiJingTime.year=127; beiJingTime.mon=1; beiJingTime.day=1;
    beiJingTime.hour=0; beiJingTime.min=5; beiJingTime.sec=30;
    action(CALENDAR_GPS_SYNC);
    assert(!rtc_writes && calendar_state.mode==CALENDAR_ACTIONS && calendar_state.status==CALENDAR_STATUS_GPS_WAIT);
    gps_fresh=1U; gps_expires_during_action=1U; fresh_checks=0U;
    menu_handle_calendar_key(MENU_KEY_OK); assert(!rtc_writes && calendar_state.status==CALENDAR_STATUS_GPS_WAIT);
    gps_expires_during_action=0U;
    for(i=0U;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        beiJingTime.year=invalid[i][0]; beiJingTime.mon=invalid[i][1]; beiJingTime.day=invalid[i][2];
        beiJingTime.hour=invalid[i][3]; beiJingTime.min=invalid[i][4]; beiJingTime.sec=invalid[i][5];
        menu_handle_calendar_key(MENU_KEY_OK);
        assert(!rtc_writes && calendar_state.status==CALENDAR_STATUS_GPS_WAIT);
    }
    beiJingTime.year=127; beiJingTime.mon=1; beiJingTime.day=1;
    beiJingTime.hour=0; beiJingTime.min=5; beiJingTime.sec=30;
    write_error=4U; menu_handle_calendar_key(MENU_KEY_OK);
    assert(rtc_writes==1U && calendar_state.mode==CALENDAR_ACTIONS && calendar_state.status==CALENDAR_STATUS_WRITE_FAILED);
    write_error=0U; menu_handle_calendar_key(MENU_KEY_OK);
    assert(rtc_writes==2U && last_source==RTC_TIME_GPS && calendar_state.status==CALENDAR_STATUS_GPS_SAVED);
    assert(last_written.year==2027U && last_written.month==1U && last_written.day==1U);
    assert(last_written.hour==0U && last_written.minute==5U && last_written.second==30U); /* Already Beijing time: no second UTC+8 shift. */
    assert(calendar_state.mode==CALENDAR_BROWSE && calendar_state.year==2027U && calendar_state.month==1U);
    for(i=0U;i<100U;i++) { menu_calendar_refresh(); menu_handle_calendar_key(MENU_KEY_RIGHT); }
    assert(rtc_writes==2U); action(CALENDAR_TODAY);
    assert(calendar_state.year==2027U && calendar_state.month==1U && rtc_writes==2U);
    puts("calendar GPS: stale/racing/invalid samples rejected, Beijing time used exactly once, explicit action only, write failure retained");
}
int main(void)
{
    draft_math(); browsing_and_cancel(); manual_save(); gps_set();
    puts("calendar menu regression passed (actual menu functions + calendar_ui.h)");
    return 0;
}
