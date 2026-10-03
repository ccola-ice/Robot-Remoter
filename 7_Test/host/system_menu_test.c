#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"
#include "param.h"
#include "menu.h"

volatile param_Config param;
static param_Config saved;
static unsigned save_calls, brightness_calls, beep_calls, timestamp_ticks, beep_ticks;
static unsigned rtc_writes, redraws;
static uint8_t save_error, applied_brightness;
static uint8_t shown_selected, shown_editing, shown_dirty;
static GuiParamRow shown_rows[5];
static char shown_status[96];
static struct { int year, mon, day, hour, min, sec; } beiJingTime;

uint8_t write_param(void)
{
    save_calls++;
    if(save_error != 0U) return 1U;
    saved = param;
    return 0U;
}
static void LCD_SetBrightness(uint8_t value)
{
    assert(value >= 10U && value <= 100U);
    applied_brightness = value;
    brightness_calls++;
}
static uint8_t RTC_ReadCalendar(RtcCalendar *value)
{
    memset(value, 0, sizeof(*value));
    value->year = 2026U; value->month = 10U; value->day = 3U;
    value->weekday = 6U;
    return 0U;
}
static uint8_t RTC_TimeIsValid(void) { return 1U; }
static RtcTimeSource RTC_TimeSource(void) { return RTC_TIME_MANUAL; }
static uint8_t RTC_SetCalendar(const RtcCalendar *value, RtcTimeSource source)
{
    assert(RTC_CalendarValidate(value) && source == RTC_TIME_MANUAL);
    rtc_writes++;
    return 0U;
}
static uint8_t gps_time_is_fresh(void) { return 0U; }
static void system_key_beep(void) { beep_calls++; }
static void TimeStamp_Increment(void) { timestamp_ticks++; }
static void system_settings_tick_1ms(void) { beep_ticks++; }

void system_settings_page(const GuiParamRow *rows, uint8_t selected, uint8_t editing,
                          uint8_t dirty, const char *status)
{
    assert(selected < 5U && editing <= 1U && dirty <= 1U);
    assert(strlen(status) < sizeof(shown_status));
    memcpy(shown_rows, rows, sizeof(shown_rows));
    strcpy(shown_status, status);
    shown_selected = selected; shown_editing = editing; shown_dirty = dirty;
    redraws++;
}

#include "system_menu_impl.inc"

static void reset_menu(void)
{
    param_load_defaults(&param);
    param.screenBrightness = 80U;
    param.keySound = 0U;
    param.chLower[0] = 175U;
    param.chMiddle[2] = 1800U;
    param.chUpper[6] = 3890U;
    param.PWMadjustValue[7] = -46;
    param.NRF_Channel = 95U;
    param.NRF_Power = 0x0bU;
    param.NRF_DataRate = 1U;
    save_error = 0U;
    save_calls = brightness_calls = beep_calls = rtc_writes = redraws = 0U;
    applied_brightness = param.screenBrightness;
    current_page = MENU_PAGE_SYSTEM_SETTINGS;
    page_dirty = page_changed = 0U;
    repeat_pending = event_read_index = event_write_index = 0U;
    menu_system_load();
}

static void select_row(uint8_t target)
{
    unsigned steps;
    assert(!system_editing && current_page == MENU_PAGE_SYSTEM_SETTINGS);
    for(steps = 0U; steps < 5U && system_selected != target; steps++)
        menu_handle_system_key(MENU_KEY_RIGHT);
    assert(system_selected == target);
}

static void check_runtime(const param_Config *expected)
{
    assert(memcmp((const void *)&param, expected, sizeof(*expected)) == 0);
}

static void test_cycle_and_brightness(void)
{
    unsigned step;
    param_Config original;
    reset_menu(); original = param;
    assert(!system_editing && system_selected == 0U);
    menu_draw_system();
    assert(!shown_dirty && !shown_editing && shown_selected == 0U);
    assert(strcmp(shown_rows[0].value, "80 %") == 0);
    menu_handle_system_key(MENU_KEY_LEFT); assert(system_selected == 4U);
    menu_handle_system_key(MENU_KEY_RIGHT); assert(system_selected == 0U);
    for(step = 0U; step < 30U; step++) {
        menu_handle_system_key(MENU_KEY_RIGHT);
        assert(system_selected == (step + 1U) % 5U);
    }
    menu_handle_system_key(MENU_KEY_OK); assert(system_editing);
    menu_handle_system_key(MENU_KEY_LEFT);
    assert(system_brightness == 75U && applied_brightness == 75U && system_selected == 0U);
    menu_draw_system(); assert(shown_dirty && shown_editing);
    check_runtime(&original);
    menu_handle_system_key(MENU_KEY_BACK);
    assert(!system_editing && system_brightness == 80U && applied_brightness == 80U);
    assert(current_page == MENU_PAGE_SYSTEM_SETTINGS);
    menu_handle_system_key(MENU_KEY_OK);
    for(step = 1U; step <= 18U; step++) {
        menu_handle_system_key(MENU_KEY_LEFT);
        assert(system_brightness == (step > 14U ? 10U : 80U - step * 5U));
        assert(applied_brightness == system_brightness);
    }
    for(step = 1U; step <= 22U; step++) {
        menu_handle_system_key(MENU_KEY_RIGHT);
        assert(system_brightness == (step > 18U ? 100U : 10U + step * 5U));
        assert(applied_brightness == system_brightness);
    }
    menu_handle_system_key(MENU_KEY_OK); assert(!system_editing);
    menu_draw_system(); assert(shown_dirty);
    check_runtime(&original);
    menu_handle_system_key(MENU_KEY_BACK);
    assert(current_page == MENU_PAGE_CATEGORY && page_changed && applied_brightness == 80U);
    check_runtime(&original);
    menu_system_load();
    assert(system_brightness == 80U && system_sound == 0U && !system_editing);
    assert(save_calls == 0U && rtc_writes == 0U);
}

static void test_sound_and_save(void)
{
    param_Config original, expected;
    char failure_status[96];
    reset_menu(); original = param;
    select_row(0U); menu_handle_system_key(MENU_KEY_OK);
    menu_handle_system_key(MENU_KEY_LEFT); menu_handle_system_key(MENU_KEY_OK);
    select_row(1U); menu_handle_system_key(MENU_KEY_OK);
    menu_handle_system_key(MENU_KEY_RIGHT);
    assert(system_sound == 1U && param.keySound == 0U);
    menu_handle_system_key(MENU_KEY_BACK);
    assert(system_sound == 0U && system_brightness == 75U && applied_brightness == 75U);
    menu_handle_system_key(MENU_KEY_OK); menu_handle_system_key(MENU_KEY_LEFT);
    menu_handle_system_key(MENU_KEY_OK);
    assert(system_sound == 1U && !system_editing);
    select_row(3U);
    save_error = 1U; menu_handle_system_key(MENU_KEY_OK);
    check_runtime(&original);
    assert(save_calls == 1U && applied_brightness == 75U);
    assert(system_brightness == 75U && system_sound == 1U);
    menu_draw_system(); assert(shown_dirty && shown_selected == 3U);
    strcpy(failure_status, shown_status);
    save_error = 0U; menu_handle_system_key(MENU_KEY_OK);
    expected = original; expected.screenBrightness = 75U; expected.keySound = 1U;
    check_runtime(&expected);
    assert(memcmp(&saved, &expected, sizeof(saved)) == 0 && save_calls == 2U);
    menu_draw_system();
    assert(!shown_dirty && strcmp(failure_status, shown_status) != 0);
    menu_handle_system_key(MENU_KEY_BACK);
    assert(current_page == MENU_PAGE_CATEGORY && applied_brightness == 75U);
    menu_system_load();
    assert(system_brightness == 75U && system_sound == 1U);
}

static void test_defaults_and_calendar_return(void)
{
    param_Config original, expected;
    reset_menu(); original = param;
    select_row(4U); menu_handle_system_key(MENU_KEY_OK);
    assert(system_brightness == 100U && system_sound == 1U && applied_brightness == 100U);
    assert(save_calls == 0U); check_runtime(&original);
    select_row(2U); menu_handle_system_key(MENU_KEY_OK);
    assert(current_page == MENU_PAGE_CALENDAR && calendar_return_page == MENU_PAGE_SYSTEM_SETTINGS);
    assert(calendar_state.now.year == 2026U && calendar_state.mode == CALENDAR_BROWSE);
    menu_handle_calendar_key(MENU_KEY_BACK);
    assert(current_page == MENU_PAGE_SYSTEM_SETTINGS && system_selected == 2U);
    assert(system_brightness == 100U && system_sound == 1U && applied_brightness == 100U);
    menu_handle_system_key(MENU_KEY_OK);
    menu_handle_calendar_key(MENU_KEY_OK);
    menu_handle_calendar_key(MENU_KEY_RIGHT); /* 校时。 */
    menu_handle_calendar_key(MENU_KEY_OK);
    assert(calendar_state.mode == CALENDAR_EDIT);
    menu_handle_calendar_key(MENU_KEY_RIGHT);
    menu_handle_calendar_key(MENU_KEY_BACK);
    assert(calendar_state.mode == CALENDAR_BROWSE && current_page == MENU_PAGE_CALENDAR);
    menu_handle_calendar_key(MENU_KEY_BACK);
    assert(current_page == MENU_PAGE_SYSTEM_SETTINGS && !rtc_writes);
    check_runtime(&original);
    select_row(3U); menu_handle_system_key(MENU_KEY_OK);
    expected = original; expected.screenBrightness = 100U; expected.keySound = 1U;
    check_runtime(&expected);
    assert(memcmp(&saved, &expected, sizeof(saved)) == 0);
    assert(save_calls == 1U);
}

static void test_repeat_beep_and_tick_wiring(void)
{
    MenuKey key = MENU_KEY_BACK;
    unsigned i;
    reset_menu();
    menu_post_key(MENU_KEY_RIGHT);
    assert(beep_calls == 1U);
    menu_post_repeat(MENU_KEY_RIGHT); assert(!repeat_pending);
    assert(menu_get_key(&key) && key == MENU_KEY_RIGHT);
    menu_handle_system_key(key); assert(system_selected == 1U);
    menu_post_repeat(MENU_KEY_RIGHT);
    assert(repeat_pending && repeat_key == MENU_KEY_RIGHT && beep_calls == 1U);
    menu_handle_system_key(repeat_key);
    repeat_pending = 0U;
    assert(system_selected == 2U);
    menu_post_repeat(MENU_KEY_OK); menu_post_repeat(MENU_KEY_BACK);
    assert(!repeat_pending && beep_calls == 1U);
    for(i = 0U; i < MENU_EVENT_QUEUE_SIZE; i++) menu_post_key(MENU_KEY_LEFT);
    assert(beep_calls == MENU_EVENT_QUEUE_SIZE); /* 满队列不会发额外声音。 */
    assert(timestamp_ticks == 0U && beep_ticks == 0U);
    for(i = 0U; i < 1000U; i++) SysTick_Handler();
    assert(timestamp_ticks == 1000U && beep_ticks == 1000U);
}

int main(void)
{
    test_cycle_and_brightness();
    test_sound_and_save();
    test_defaults_and_calendar_return();
    test_repeat_beep_and_tick_wiring();
    puts("system menu: cyclic navigation, brightness preview/cancel, sound draft, save/rollback,");
    puts("system-only defaults, calibration preservation, calendar return and beep/tick wiring passed");
    return 0;
}
