#include <assert.h>
#include <float.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"
#include "gui_robot_filter.h"
#include "param.h"

#define __IO volatile
static struct { uint32_t DEMCR; } debug_registers;
static struct { uint32_t CYCCNT, CTRL; } dwt_registers;
static unsigned advance_cycles, dwt_reads;
static void *fake_dwt(void)
{
    if(advance_cycles) dwt_registers.CYCCNT += 168U;
    dwt_reads++;
    return &dwt_registers;
}
#define DWT ((typeof(dwt_registers) *)fake_dwt())
#define CoreDebug (&debug_registers)
#define CoreDebug_DEMCR_TRCENA_Msk 1UL
#define DWT_CTRL_CYCCNTENA_Msk 1UL
static uint32_t SystemCoreClock = 168000000UL, systick_period;
static uint32_t SysTick_Config(uint32_t ticks) { systick_period = ticks; return 0UL; }

static uint32_t interrupt_mask;
static volatile uint8_t *inject_pending;
static uint32_t __get_PRIMASK(void) { return interrupt_mask; }
static void __disable_irq(void) { interrupt_mask = 1U; }
static void __set_PRIMASK(uint32_t mask)
{
    assert(interrupt_mask == 1U);
    if(inject_pending != 0) {
        assert(*inject_pending == 0U);
        if(mask == 0U) *inject_pending = 1U;
    }
    interrupt_mask = mask;
}

#define GENERAL_TIM5 5U
#define TIM_IT_Update 1U
#define RESET 0U
static unsigned timer_pending, timer_clears;
static uint8_t TIM_GetITStatus(unsigned timer, unsigned bit)
{ assert(timer == GENERAL_TIM5 && bit == TIM_IT_Update); return (uint8_t)timer_pending; }
static void TIM_ClearITPendingBit(unsigned timer, unsigned bit)
{ assert(timer == GENERAL_TIM5 && bit == TIM_IT_Update); timer_pending = 0U; timer_clears++; }
static volatile uint8_t finish_100hz, finish_50hz, finish_20hz, finish_10hz;
static volatile uint8_t finish_5hz, finish_2hz, finish_1hz;
static uint8_t refresh_due;
static param_Config param_edit;
static const uint8_t nrf_power_register[4] = {0x09U, 0x0bU, 0x0dU, 0x0fU};

#define WHITE 0xffffU
#define GREY 0xccccU
#define BLACK 0U
#define BLUE2 0x1234U
#define GREEN 0x07e0U
#define RED 0xf800U
#define YELLOW 0xffe0U
static unsigned Font16x32, Font8x16, Font24x48;
static uint8_t display_flag;
static uint16_t ADC1_Value[8];
static struct { uint32_t before; char text[100]; uint32_t after; } text_guard;
#define displayBuffer text_guard.text
static unsigned rendered_strings;
static char last_link[96];
static char gps_position_text[100], gps_time_text[100];
static struct {
    int sig, fix, mode;
    struct { int inuse, inview; } satinfo, BDsatinfo;
    double elv, speed, direction, HDOP, PDOP;
} info;
static struct { int year, mon, day, hour, min, sec; } beiJingTime;
static double deg_lat, deg_lon;
static uint8_t gps_position_fresh, gps_clock_fresh;
static uint8_t gps_data_is_fresh(void) { return gps_position_fresh; }
static uint8_t gps_time_is_fresh(void) { return gps_clock_fresh; }
static const char *control_link_status(void) { return "DISARMED"; }
static void ignore_draw(unsigned first, ...) { (void)first; }
static void GTP_IRQ_Disable(void) {}
#define LCD_SetTextColor ignore_draw
#define LCD_SetBackColor ignore_draw
#define LCD_SetFont(font) ((void)(font))
#define ILI9806G_DrawRectangle ignore_draw
#define ILI9806G_DrawLine ignore_draw
#define ILI9806G_DrawCircle ignore_draw
#define ILI9806G_Fill ignore_draw
#define gui_robot_card ignore_draw
#define gui_gps_draw_card ignore_draw
#define gui_clear_page_content() ((void)0)
#define gui_clear_page_band ignore_draw
#define gui_update_progress_bar ignore_draw
#define gui_draw_progress_bar ignore_draw
#define gui_robot_draw_stick ignore_draw
static int16_t gui_robot_stick_value(uint16_t raw, uint8_t channel)
{ (void)channel; return (int16_t)(raw / 4U) - 500; }
static void capture_text(unsigned x, unsigned y, const char *text)
{
    (void)x; (void)y;
    assert(text_guard.before == 0x12345678UL && text_guard.after == 0x87654321UL);
    assert(strlen(text) < sizeof(displayBuffer));
    if(x == 40U && y == 364U) snprintf(last_link, sizeof(last_link), "%s", text);
    if(x == 40U && y == 132U) snprintf(gps_position_text, sizeof(gps_position_text), "%s", text);
    if(x == 552U && y == 274U) snprintf(gps_time_text, sizeof(gps_time_text), "%s", text);
    rendered_strings++;
}
static void ILI9806G_DispString_EN(unsigned x, unsigned y, const char *text)
{ (void)x; (void)y; assert(strlen(text) < sizeof(displayBuffer)); }
static void LCD_DispString_EN_Bold(uint16_t x, uint16_t y, const char *text)
{ ILI9806G_DispString_EN(x, y, text); }
static int GetGBKCode(uint8_t *bitmap, uint16_t code)
{ (void)code; memset(bitmap,0x55,128U); return 0; }
uint8_t FLASH_GetIoError(void) { return 0U; }
static void LCD_BlitRGB565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels)
{ assert(pixels && x+w<=800U && y+h<=480U); }
#define ui_text ui_real_text
#include "gui_theme.h"
#undef ui_text
static void ui_text(uint16_t x, uint16_t y, uint8_t columns,
    const char *text, uint16_t fg, uint16_t bg, uint8_t large)
{
    char captured[101];
    ui_real_text(x,y,columns,text,fg,bg,large);
    snprintf(captured,sizeof(captured),"%-*.*s",columns,columns,text ? text : "");
    capture_text(x,y,captured);
}
#include "runtime_safety_impl.inc"

static void test_ticks_and_atomic_flags(void)
{
    unsigned i, second, hz100, hz50, hz20, hz10, hz5, hz2, hz1;
    unsigned long now;
    volatile uint8_t pending;
    SysTick_Init();
    assert(systick_period == 168000UL && dwt_registers.CYCCNT == 0UL);
    assert(get_tick_count(0) == -1);
    for(i = 0U; i < 1234U; i++) SysTick_Handler();
    assert(get_tick_count(&now) == 0 && now == 1234UL);
    now = 0UL;
    mget_ms(&now);
    assert(now == 1234UL); /* MPU timestamps use the same millisecond clock. */
    mget_ms(0); /* The adapter retains the null-safe clock behavior. */
    for(second = 0U; second < 5U; second++) {
        hz100 = hz50 = hz20 = hz10 = hz5 = hz2 = hz1 = 0U;
        for(i = 0U; i < 1000U; i++) {
            timer_pending = 1U;
            GENERAL_TIM5_IRQHandler();
            hz100 += take_tick(&finish_100hz); hz50 += take_tick(&finish_50hz);
            hz20 += take_tick(&finish_20hz); hz10 += take_tick(&finish_10hz);
            hz5 += take_tick(&finish_5hz); hz2 += take_tick(&finish_2hz);
            hz1 += take_tick(&finish_1hz);
        }
        assert(hz100 == 100U && hz50 == 50U && hz20 == 20U && hz10 == 10U);
        assert(hz5 == 5U && hz2 == 2U && hz1 == 1U);
    }
    assert(timer_clears == 5000U);
    GENERAL_TIM5_IRQHandler(); /* A spurious IRQ must not advance the schedule. */
    assert(timer_clears == 5000U);
    assert(get_tick_count(&now) == 0 && now == 1234UL); /* No duplicate timebase. */
    pending = 1U;
    inject_pending = &pending;
    assert(take_tick(&pending) == 1U && pending == 1U && interrupt_mask == 0UL);
    inject_pending = 0;
    assert(take_tick(&pending) == 1U && pending == 0U);
    interrupt_mask = 1UL;
    pending = 1U;
    assert(take_tick(&pending) == 1U && interrupt_mask == 1UL);
    interrupt_mask = 0UL;
    g_ul_ms_ticks = 0xffffffffUL;
    SysTick_Handler();
    assert(get_tick_count(&now) == 0 && now == 0UL);
    advance_cycles = 1U;
    dwt_registers.CYCCNT = 0xfffffff0UL;
    dwt_reads = 0U;
    Delay_us(25U);
    assert(dwt_reads >= 26U && dwt_reads < 30U); /* Delay also survives counter wrap. */
    dwt_reads = 0U;
    Delay_us(1000005U);
    assert(dwt_reads >= 1000007U && dwt_reads < 1000010U);
    advance_cycles = 0U;
}

static void test_telemetry_expiry(void)
{
    GuiRobotTelemetry received;
    memset(&received, 0, sizeof(received));
    menu_robot_telemetry_reset();
    g_ul_ms_ticks = 987654UL;
    menu_robot_telemetry_service();
    assert(robot_telemetry.link_online == 0U && robot_telemetry.packet_age_ms == 65535U);
    received.link_online = 1U;
    received.packet_count = 123UL;
    g_ul_ms_ticks = 0xfffffff0UL;
    menu_robot_telemetry_update(&received);
    menu_robot_telemetry_update(0);
    g_ul_ms_ticks += 999U;
    refresh_due = 0U;
    menu_robot_telemetry_service();
    assert(robot_telemetry.link_online == 1U && robot_telemetry.packet_age_ms == 999U);
    SysTick_Handler();
    menu_robot_telemetry_service();
    assert(robot_telemetry.link_online == 0U && robot_telemetry.packet_age_ms == 1000U);
    assert(refresh_due == 1U && robot_telemetry.packet_count == 123UL);
    g_ul_ms_ticks += 70000U;
    menu_robot_telemetry_service();
    assert(robot_telemetry.packet_age_ms == 65535U);
    menu_robot_telemetry_update(&received);
    menu_robot_telemetry_service();
    assert(robot_telemetry.link_online == 1U && robot_telemetry.packet_age_ms == 0U);
}

static void test_bounded_formatting(void)
{
    GuiRobotTelemetry extreme;
    struct { uint32_t before; GuiParamRow row; uint32_t after; } row_guard;
    unsigned i, offline_strings;
    uint32_t nan_bits = 0x7fc00000UL;
    text_guard.before = row_guard.before = 0x12345678UL;
    text_guard.after = row_guard.after = 0x87654321UL;
    memset(&extreme, 0, sizeof(extreme));
    extreme.speed_mps = extreme.position_x_m = extreme.acceleration_x_mps2 = FLT_MAX;
    extreme.position_y_m = extreme.acceleration_y_mps2 = -FLT_MAX;
    extreme.latitude_deg = DBL_MAX;
    extreme.longitude_deg = -DBL_MAX;
    extreme.packet_count = 0xffffffffUL;
    extreme.packet_age_ms = 65535U;
    extreme.battery_percent = extreme.satellites = extreme.gps_fix = 255U;
    display_flag = 1U;
    robot_control_page(&extreme);
    assert(strstr(last_link, "\265\310\264\375\275\323\312\325\266\313\273\330\264\253") != 0);
    offline_strings = rendered_strings;
    memcpy(&extreme.speed_mps, &nan_bits, sizeof(nan_bits));
    extreme.link_online = 1U;
    robot_control_page(&extreme);
    assert(strstr(last_link, "\273\372\306\367\310\313\322\243\262\342 / \323\320\320\247") != 0 && rendered_strings > offline_strings);
    memset(&param_edit, 0xff, sizeof(param_edit));
    param_edit.warnBatVolt = FLT_MAX;
    param_edit.RecWarnBatVolt = -FLT_MAX;
    for(i = 0U; i < PARAM_ITEM_COUNT; i++) {
        memset(&row_guard.row, 0xa5, sizeof(row_guard.row));
        menu_param_format_item((uint8_t)i, &row_guard.row);
        assert(row_guard.before == 0x12345678UL && row_guard.after == 0x87654321UL);
        assert(memchr(row_guard.row.label, 0, sizeof(row_guard.row.label)) != 0);
        assert(memchr(row_guard.row.value, 0, sizeof(row_guard.row.value)) != 0);
    }
    assert(PARAM_ITEM_COUNT == 40U && PARAM_CALIBRATION_CHANNELS == 6U);
    param_edit.batVoltAdjust = 1000U;
    menu_param_format_item(2U, &row_guard.row);
    assert(strcmp(row_guard.row.value, "1000") == 0);
}

static void test_gps_page_freshness(void)
{
    info.sig = 1; info.fix = 3; info.mode = 'A';
    beiJingTime.year = 126; beiJingTime.mon = 9; beiJingTime.day = 19;
    beiJingTime.hour = 16; beiJingTime.min = 30; beiJingTime.sec = 20;
    deg_lat = 31.2; deg_lon = 121.4;
    gps_position_fresh = gps_clock_fresh = 1U;
    display_flag = 1U;
    system_data_read_and_set();
    assert(strncmp(gps_position_text, "LAT  N", 6U) == 0);
    assert(strncmp(gps_time_text, "16:30:20", 8U) == 0);
    gps_position_fresh = 0U;
    system_data_read_and_set();
    assert(strstr(gps_position_text, "\265\310\264\375\323\320\320\247\266\250\316\273") != 0);
    assert(strncmp(gps_time_text, "16:30:20", 8U) == 0);
    gps_clock_fresh = 0U;
    system_data_read_and_set();
    assert(strncmp(gps_time_text, "\265\310\264\375\326\320", 6U) == 0);
}

int main(void)
{
    test_ticks_and_atomic_flags();
    test_telemetry_expiry();
    test_bounded_formatting();
    test_gps_page_freshness();
    puts("runtime: 1 ms clocks, timer rates, IRQ flags, wrap-safe expiry and bounded text passed");
    return 0;
}
