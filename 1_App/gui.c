#include "gui.h"
#include "menu_catalog.h"
#include "gui_analog_filter.h"
#include "gui_robot_filter.h"
#include "nmea_decode_test.h"
#include "control_link.h"
#include "bsp_fsmc_lcd.h"
#include "bsp_adc1_independent_dual.h"
#include "bsp_adc3_independent_dual.h"
#include "bsp_usart_debug.h"
#include "bsp_Systick.h"
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h" 
#include "bsp_mpu6050.h"
#include "bsp_rtc.h"
#include "nmea/nmea.h"
#include "gt9xx.h"
#include "ff.h"
#include "param.h"
#include <string.h>
#include <float.h>
#include "gui_theme.h"

extern volatile uint16_t ADC1_Value[NUM_OF_ADC1CHANNEL];
extern volatile uint16_t ADC3_Value[NUM_OF_ADC3CHANNEL];

extern nmeaINFO info;          		//GPS解码后得到的信息
extern nmeaTIME beiJingTime; 		//北京时间
extern double deg_lat;				//转换成[degree].[degree]格式的纬度
extern double deg_lon;				//转换成[degree].[degree]格式的经度

extern float pitch,roll,yaw; 		//欧拉角
extern short aacx,aacy,aacz;		//加速度传感器原始数据
extern short gyrox,gyroy,gyroz;		//陀螺仪原始数据
extern short temp;					//温度
extern uint8_t imu_data_valid;

/* ARM scatter-loading symbols: their addresses are the linker-calculated sizes. */
extern uint8_t Image$$ER_IROM1$$Length;
extern uint8_t Image$$RW_IRAM1$$Length;
extern uint8_t Image$$RW_IRAM1$$ZI$$Length;

#define GUI_MCU_FLASH_BYTES (1024UL * 1024UL)
#define GUI_MCU_RAM_BYTES   (128UL * 1024UL)

char displayBuffer[100];

static uint8_t display_flag = 0;	//界面切换标志，仅首次绘制页面静态内容
static uint8_t clock_force_redraw = 1U;
static uint8_t clock_last_seconds = 0xffU;
static uint16_t boot_progress_width;

void gui_prepare_page(void)
{
	/* Compose the full page off-screen; menu_process presents it after the clock. */
	LCD_BeginPage(WHITE);
	display_flag = 1;
	clock_force_redraw = 1U;
}

/* Restore layout boundaries when page content is drawn, including home pagination. */
static const char *gui_control_status_text(const char *status)
{
    if(strcmp(status, "BOOT LOCK") == 0) return "\327\324\274\354\316\264\267\305\320\320";
    if(strcmp(status, "RADIO OFF") == 0) return "\316\336\317\337\267\242\311\344\316\264\277\252\306\364";
    if(strcmp(status, "INPUT STALE") == 0) return "\312\344\310\353\312\375\276\335\322\321\271\375\306\332";
    if(strcmp(status, "LOW TX BATTERY") == 0) return "\322\243\277\330\306\367\265\347\263\330\265\347\321\271\265\315";
    if(strcmp(status, "STOP / OPEN CONTROL PAGE") == 0) return "\267\307\277\330\326\306\322\263\303\346: \275\366\267\242\313\315\260\262\310\253\326\241";
    if(strcmp(status, "NO RADIO ACK") == 0) return "\265\310\264\375\316\336\317\337\323\262\274\376\273\330\326\264";
    if(strcmp(status, "ENABLED / HOLD DCH1") == 0) return "\324\313\266\257\322\321\324\312\320\355 / \260\264\327\241 DCH1";
    if(strcmp(status, "READY / PRESS DCH1") == 0) return "\322\321\276\315\320\367 / \260\264\327\241 DCH1 \312\271\304\334";
    if(strcmp(status, "RELEASE DCH1 / CENTER STICKS") == 0) return "\313\311\277\252 DCH1 \262\242\261\243\263\326\322\241\270\313\273\330\326\320";
    if(strcmp(status, "DISARMED") == 0) return "\261\276\273\372\316\264\312\271\304\334\324\313\266\257";
    return status;
}

void gui_clock_overlay(void)
{
    static char last_date[32], last_time[32];
    static const char *weekdays[7] = {"\322\273","\266\376","\310\375","\313\304","\316\345","\301\371","\310\325"};
    RtcCalendar now;
    ControlLinkSnapshot link;
    uint32_t mv;
    uint8_t readable;
    char text[24], date_text[32], time_text[32];
    memset(&now,0,sizeof(now));
    readable = RTC_ReadCalendar(&now) == 0U;
    if(readable) {
        snprintf(date_text,sizeof(date_text),"%04u-%02u-%02u \326\334%s",now.year,now.month,now.day,
            now.weekday >= 1U && now.weekday <= 7U ? weekdays[now.weekday-1U] : "?");
        snprintf(time_text,sizeof(time_text),"%02u:%02u:%02u%s",now.hour,now.minute,now.second,
            RTC_TimeIsValid() ? "" : " \316\264\320\243\312\261");
    } else {
        strcpy(date_text,"\310\325\306\332\266\301\310\241\312\247\260\334");
        strcpy(time_text,"--:--:--");
    }
    if(!clock_force_redraw && clock_last_seconds == now.second &&
       strcmp(last_date,date_text) == 0 && strcmp(last_time,time_text) == 0) return;
    clock_force_redraw = 0U;
    clock_last_seconds = now.second;
    strcpy(last_date,date_text); strcpy(last_time,time_text);
    memset(&link,0,sizeof(link));
    control_link_get_snapshot(&link);
    if(link.sampled && link.input_fresh && link.sample_age_ms <= 100UL && ADC1_Value[6] <= 4095U) {
        mv = (uint32_t)ADC1_Value[6] * 66UL * param.batVoltAdjust / 40950UL;
        snprintf(text,sizeof(text),"TX %lu.%02luV",(unsigned long)(mv/1000UL),
            (unsigned long)((mv%1000UL)/10UL));
    } else strcpy(text,"TX --.--V");
    ui_fill(480U,0U,320U,32U,UI_INK);
    ui_text(480U,8U,10U,text,WHITE,UI_INK,0U);
    ui_text(568U,8U,8U,!param.NRF_Mode ? "RF OFF" :
        link.ack_seen && link.ack_age_ms < 150UL ? "RF ACK" : "RF WAIT",WHITE,UI_INK,0U);
    ui_text(640U,0U,17U,date_text,WHITE,UI_INK,0U);
    ui_text(640U,16U,17U,time_text,readable && RTC_TimeIsValid() ? WHITE : UI_AMBER,UI_INK,0U);
    LCD_SetFont(&Font16x32); LCD_SetBackColor(WHITE); LCD_SetTextColor(BLACK);
}

static const BootReport *boot_last_report;

static uint16_t gui_boot_color(BootState state)
{
    if(state == BOOT_PASS) return GREEN;
    if(state == BOOT_FAIL) return RED;
    if(state == BOOT_NOT_TESTED) return YELLOW;
    if(state == BOOT_RUNNING) return BLUE2;
    return GREY;
}

/* In-ROM 16px glyphs: not tested (U+672A U+6D4B U+8BD5).
 * Diagnostics remain readable if the external Chinese-font Flash fails. */
static void gui_boot_not_tested_label(uint16_t x, uint16_t y)
{
    static const uint16_t glyphs[3][16] = {
    {0x0100U, 0x0100U, 0x0100U, 0x3ff8U, 0x0100U, 0x0100U, 0x0100U, 0xfffeU, 0x0380U, 0x0540U, 0x0920U, 0x1110U, 0x2108U, 0xc106U, 0x0100U, 0x0100U},
    {0x0004U, 0x27c4U, 0x1444U, 0x1454U, 0x8554U, 0x4554U, 0x4554U, 0x1554U, 0x1554U, 0x2554U, 0xe554U, 0x2104U, 0x2284U, 0x2244U, 0x2414U, 0x0808U},
    {0x0028U, 0x2024U, 0x1024U, 0x1020U, 0x07feU, 0x0020U, 0xf020U, 0x17e0U, 0x1120U, 0x1110U, 0x1110U, 0x1510U, 0x19caU, 0x170aU, 0x0206U, 0x0002U}
    };
    uint8_t glyph, row, column;
    LCD_SetTextColor(YELLOW);
    for(glyph = 0U; glyph < 3U; glyph++)
        for(row = 0U; row < 16U; row++)
            for(column = 0U; column < 16U; column++)
                if(glyphs[glyph][row] & (0x8000U >> column))
                    ILI9806G_SetPointPixel(x + glyph * 16U + column, y + row);
}

static void gui_boot_row(const BootReport *report, uint8_t item)
{
    uint16_t rows = (BOOT_ITEM_COUNT + 1U) / 2U;
    uint16_t x = item < rows ? 20U : 410U;
    uint16_t y = 62U + (item % rows) * 21U;
    BootState state = report->items[item].state;
    char text[48];
    LCD_SetFont(&Font8x16);
    LCD_SetBackColor(BLACK);
    LCD_SetTextColor(gui_boot_color(state));
    sprintf(text, "%-23.23s %-10.10s", boot_item_names[item], boot_state_name(state));
    ILI9806G_DispString_EN(x, y, text);
    if(state == BOOT_NOT_TESTED) gui_boot_not_tested_label(x + 280U, y);
}

void gui_boot_begin(void)
{
    uint8_t frame, dot;
    boot_progress_width = 0U;
    boot_last_report = 0;
    LCD_SetBackColor(BLACK);
    LCD_SetTextColor(BLACK);
    ILI9806G_Clear(0U, 0U, LCD_X_LENGTH, LCD_Y_LENGTH);
    LCD_SetFont(&Font16x32);
    LCD_SetTextColor(WHITE);
    ILI9806G_DispString_EN(344U, 104U, "REMOTER");
    LCD_SetFont(&Font8x16);
    LCD_SetTextColor(BLUE2);
    ILI9806G_DispString_EN(292U, 151U, "ROBOT REMOTE CONTROL SYSTEM");
    ILI9806G_DrawRectangle(280U, 92U, 240U, 88U, 0U);
    LCD_SetTextColor(WHITE);
    ILI9806G_DispString_EN(284U, 284U, "Starting hardware checks... 0%");
    /* Decorative intro only. The progress value stays zero until checks finish. */
    for(frame = 0U; frame < 12U; frame++) {
        for(dot = 0U; dot < 6U; dot++) {
            LCD_SetTextColor(dot == frame % 6U ? BLUE2 : GREY);
            ILI9806G_DrawRectangle(334U + dot * 24U, 220U, 12U, 12U, 1U);
        }
        Delay_ms(25U);
    }
    LCD_SetTextColor(BLUE2);
    ILI9806G_DrawRectangle(20U, 362U, 760U, 20U, 0U);
}

void gui_boot_update(const BootReport *report, uint8_t item)
{
    uint16_t next_width;
    char text[96];
    uint8_t percent = boot_report_percent(report);
    LCD_SetFont(&Font16x32);
    LCD_SetBackColor(BLACK);
    LCD_SetTextColor(gui_boot_color(report->items[item].state));
    sprintf(text, "%-42.42s", boot_item_names[item]);
    ILI9806G_DispString_EN(64U, 278U, text);
    next_width = (uint16_t)(756UL * percent / 100UL);
    LCD_SetTextColor(report->failed ? RED : BLUE2);
    if(next_width != 0U && (report->failed || next_width > boot_progress_width)) {
        ILI9806G_DrawRectangle(22U, 364U, next_width, 16U, 1U);
        boot_progress_width = next_width;
    }
    LCD_SetFont(&Font8x16);
    LCD_SetBackColor(BLACK);
    LCD_SetTextColor(WHITE);
    sprintf(text, "Completed %2u/%u  %3u%% | PASS %2u  FAIL %2u  NOT TESTED %2u | %lu ms   ",
            report->completed, (unsigned)BOOT_ITEM_COUNT, percent,
            report->passed, report->failed, report->not_tested,
            (unsigned long)report->elapsed_ms);
    ILI9806G_DispString_EN(20U, 388U, text);
    LCD_SetTextColor(gui_boot_color(report->items[item].state));
    sprintf(text, "%-90.90s", report->items[item].state == BOOT_RUNNING ?
            boot_item_names[item] : report->items[item].detail);
    ILI9806G_DispString_EN(20U, 411U, text);
    /* Hardware completion alone drives the progress bar. */
}

void gui_boot_finish(const BootReport *report)
{
    BootOutcome outcome = boot_report_outcome(report);
    const char *summary;
    uint8_t item;
    LCD_SetBackColor(BLACK);
    LCD_SetTextColor(BLACK);
    ILI9806G_Clear(0U, 0U, LCD_X_LENGTH, LCD_Y_LENGTH);
    LCD_SetFont(&Font16x32);
    LCD_SetTextColor(WHITE);
    ILI9806G_DispString_EN(20U, 6U, "REMOTER / TEST RESULTS");
    for(item = 0U; item < BOOT_ITEM_COUNT; item++) gui_boot_row(report, item);
    LCD_SetFont(&Font8x16);
    LCD_SetTextColor(WHITE);
    snprintf(displayBuffer, sizeof(displayBuffer), "Completed %u/%u (%u%%)  PASS %u  FAIL %u  NOT TESTED %u",
            report->completed, (unsigned)BOOT_ITEM_COUNT, boot_report_percent(report),
            report->passed, report->failed, report->not_tested);
    ILI9806G_DispString_EN(20U, 370U, displayBuffer);
    ILI9806G_DispString_EN(20U, 398U, "Open Hardware Tests for operator / external-fixture tests.");
    boot_last_report = report;
    LCD_SetFont(&Font8x16);
    LCD_SetBackColor(BLACK);
    if(outcome == BOOT_FAILED || outcome == BOOT_INCOMPLETE) {
        LCD_SetTextColor(RED);
        summary = "CHECKS FAILED / INCOMPLETE - review results; press and release OK to continue";
    } else if(outcome == BOOT_PARTIAL) {
        LCD_SetTextColor(YELLOW);
        summary = "CHECKS COMPLETE - untested items remain; this is NOT an all-hardware pass";
    } else {
        LCD_SetTextColor(GREEN);
        summary = "ALL LISTED CHECKS PASSED";
    }
    ILI9806G_DispString_EN(20U, 446U, (char *)summary);
}

static void gui_settings_row(uint8_t row, uint16_t number,
                             const char *label, const char *value,
                             uint8_t selected, uint8_t editing)
{
    uint16_t y = 104U + (uint16_t)row * 48U;
    uint16_t background = selected ? (editing ? UI_AMBER : UI_TINT) : UI_SURFACE;
    uint16_t foreground = selected ? (editing ? UI_SURFACE : UI_ACCENT) : UI_INK;
    char index_text[5];
    ui_round_rect(192U, y, 580U, 44U, 10U, background);
    snprintf(index_text, sizeof(index_text), "%02u", number);
    ui_text(204U, y + 6U, 2U, index_text,
            selected ? foreground : UI_MUTED, background, 3U);
    ui_text(244U, y + 6U, 14U, label, foreground, background, 3U);
    ui_text(480U, y + 6U, 16U, value, foreground, background, 3U);
    ui_text(748U, y + 6U, 1U, selected && editing ? "*" : ">",
            selected ? foreground : UI_MUTED, background, 3U);
}

static void gui_settings_scroll(uint8_t first_visible, uint8_t visible_count,
                                uint8_t total_items)
{
    uint16_t height, offset;
    ui_round_rect(782U, 108U, 4U, 276U, 2U, UI_TRACK);
    if(!total_items || !visible_count) return;
    height = (uint16_t)(276UL * visible_count / total_items);
    if(height < 24U) height = 24U;
    if(height > 276U) height = 276U;
    offset = total_items > visible_count ?
        (uint16_t)((276UL - height) * first_visible / (total_items - visible_count)) : 0U;
    if(offset > 276U - height) offset = 276U - height;
    ui_round_rect(782U, 108U + offset, 4U, height, 2U, UI_ACCENT);
}

static void gui_file_row_icon(uint16_t x, uint16_t y, uint8_t directory, uint16_t color)
{
    LCD_SetTextColor(color);
    if(directory) {
        ILI9806G_DrawLine(x, y + 4U, x + 10U, y + 4U);
        ILI9806G_DrawLine(x + 10U, y + 4U, x + 14U, y + 8U);
        ILI9806G_DrawLine(x + 14U, y + 8U, x + 26U, y + 8U);
        ILI9806G_DrawLine(x + 26U, y + 8U, x + 26U, y + 24U);
        ILI9806G_DrawLine(x + 26U, y + 24U, x, y + 24U);
        ILI9806G_DrawLine(x, y + 24U, x, y + 4U);
        ILI9806G_DrawLine(x, y + 12U, x + 26U, y + 12U);
    } else {
        ILI9806G_DrawRectangle(x + 4U, y + 2U, 18U, 24U, 0U);
        ILI9806G_DrawLine(x + 8U, y + 10U, x + 18U, y + 10U);
        ILI9806G_DrawLine(x + 8U, y + 16U, x + 18U, y + 16U);
    }
}

static void gui_monitor_tabs(uint8_t output)
{
    ui_round_rect(536U,44U,240U,36U,10U,UI_SURFACE);
    ui_round_rect(output ? 656U : 540U,48U,116U,28U,8U,UI_ACCENT);
    ui_text(564U,54U,8U,"\324\255\312\274 ADC",output ? UI_MUTED : UI_SURFACE,
        output ? UI_SURFACE : UI_ACCENT,0U);
    ui_text(684U,54U,8U,"\320\243\327\274\312\344\263\366",output ? UI_SURFACE : UI_MUTED,
        output ? UI_ACCENT : UI_SURFACE,0U);
}

static void gui_tools_capacity(uint16_t x, uint16_t y, const char *label,
                               uint32_t used, uint32_t capacity, uint16_t color)
{
    uint32_t percent = (used * 1000UL + capacity / 2UL) / capacity;
    uint16_t width = (uint16_t)(328UL * (used > capacity ? capacity : used) / capacity);
    char text[48];
    ui_round_rect(x, y, 368U, 128U, 14U, UI_SURFACE);
    ui_text(x + 20U, y + 12U, 26U, label, UI_MUTED, UI_SURFACE, 0U);
    snprintf(text, sizeof(text), "%lu.%01lu / %lu KB", (unsigned long)(used / 1024UL),
             (unsigned long)((used % 1024UL) * 10UL / 1024UL), (unsigned long)(capacity / 1024UL));
    ui_text(x + 20U, y + 36U, 20U, text, UI_INK, UI_SURFACE, 1U);
    ui_round_rect(x + 20U, y + 80U, 328U, 8U, 4U, UI_TRACK);
    if(width) ui_round_rect(x + 20U, y + 80U, width, 8U, 4U, color);
    snprintf(text, sizeof(text), "%lu.%01lu%%", (unsigned long)(percent / 10UL),
             (unsigned long)(percent % 10UL));
    ui_text(x + 280U, y + 12U, 9U, text, color, UI_SURFACE, 0U);
}

static void gui_robot_format_value(char *field, size_t size, uint8_t columns,
    const char *format, double value)
{
    int length;
    if(!(value >= -DBL_MAX && value <= DBL_MAX)) {
        snprintf(field,size,"--");
        return;
    }
    length = snprintf(field,size,format,value);
    if(length < 0 || (unsigned)length > columns || (size_t)length >= size)
        snprintf(field,size,"OVR");
}

void system_basic_information(void)
{
    static uint8_t last_boot[5];
    uint8_t boot[5], first_draw = display_flag != 0U;
    uint32_t code_ro_bytes, rw_bytes, zi_bytes, ram_bytes, flash_bytes, remaining;
    uint16_t state_color;
    const char *outcome_text;
    char text[80];

    if(first_draw) {
        display_flag = 0U;
        GTP_IRQ_Disable();
        code_ro_bytes = (uint32_t)(uintptr_t)&Image$$ER_IROM1$$Length;
        rw_bytes = (uint32_t)(uintptr_t)&Image$$RW_IRAM1$$Length;
        zi_bytes = (uint32_t)(uintptr_t)&Image$$RW_IRAM1$$ZI$$Length;
        ram_bytes = rw_bytes + zi_bytes;
        flash_bytes = code_ro_bytes + rw_bytes;
        ui_shell("\317\265\315\263\320\305\317\242", "\311\350\261\270 / \304\332\264\346 / \271\314\274\376", "\271\244\276\337");
        ui_round_rect(24U, 104U, 752U, 88U, 14U, UI_SURFACE);
        ui_round_rect(40U, 116U, 64U, 64U, 12U, UI_TINT);
        ui_icon(48U, 124U, 48U, UI_ICON_CHIP, UI_ACCENT, UI_TINT);
        ui_text(124U, 116U, 26U, "STM32F407ZGT6", UI_INK, UI_SURFACE, 1U);
        ui_text(124U, 158U, 48U, "ARM Cortex-M4  /  168 MHz", UI_MUTED, UI_SURFACE, 0U);
        ui_text(568U, 124U, 23U, "\273\372\306\367\310\313\322\243\277\330\306\367", UI_ACCENT, UI_SURFACE, 0U);
        ui_text(568U, 154U, 23U, "800 x 480 \317\324\312\276\306\301", UI_MUTED, UI_SURFACE, 0U);
        gui_tools_capacity(24U, 204U, "Flash \271\314\274\376\325\274\323\303", flash_bytes, GUI_MCU_FLASH_BYTES, UI_ACCENT);
        gui_tools_capacity(408U, 204U, "RAM \276\262\314\254\325\274\323\303", ram_bytes, GUI_MCU_RAM_BYTES, UI_GREEN);
        snprintf(text, sizeof(text), "CODE+RO %lu KB   RW %lu B", (unsigned long)(code_ro_bytes / 1024UL),
                 (unsigned long)rw_bytes);
        ui_text(44U, 308U, 41U, text, UI_MUTED, UI_SURFACE, 0U);
        remaining = ram_bytes < GUI_MCU_RAM_BYTES ? GUI_MCU_RAM_BYTES - ram_bytes : 0UL;
        snprintf(text, sizeof(text), "ZI/BSS %lu KB  AVAILABLE %lu KB", (unsigned long)(zi_bytes / 1024UL),
                 (unsigned long)(remaining / 1024UL));
        ui_text(428U, 308U, 41U, text, UI_MUTED, UI_SURFACE, 0U);
        ui_round_rect(24U, 344U, 296U, 88U, 14U, UI_SURFACE);
        ui_text(44U, 356U, 31U, "\271\314\274\376\260\346\261\276", UI_MUTED, UI_SURFACE, 0U);
        snprintf(text, sizeof(text), "%s  /  %s", FM_VERSION, FM_TIME);
        ui_text(44U, 386U, 31U, text, UI_INK, UI_SURFACE, 0U);
        ui_round_rect(336U, 344U, 440U, 88U, 14U, UI_SURFACE);
        ui_footer("BACK \267\265\273\330", "\261\340\322\353\304\332\264\346\325\274\323\303");
    }
    memset(boot, 0, sizeof(boot));
    if(boot_last_report) {
        boot[0] = 1U;
        boot[1] = boot_last_report->passed;
        boot[2] = boot_last_report->failed;
        boot[3] = boot_last_report->not_tested;
        boot[4] = (uint8_t)boot_report_outcome(boot_last_report);
    }
    if(first_draw || memcmp(boot, last_boot, sizeof(boot)) != 0) {
        state_color = UI_MUTED;
        outcome_text = "\316\336\327\324\274\354\274\307\302\274";
        if(boot[0]) {
            if(boot[4] == BOOT_PASSED) { outcome_text = "\311\317\265\347\327\324\274\354\315\250\271\375"; state_color = UI_GREEN; }
            else if(boot[4] == BOOT_FAILED) { outcome_text = "\311\317\265\347\327\324\274\354\312\247\260\334"; state_color = UI_RED; }
            else if(boot[4] == BOOT_PARTIAL) { outcome_text = "\327\324\274\354\262\277\267\326\315\352\263\311"; state_color = UI_AMBER; }
            else { outcome_text = "\311\317\265\347\327\324\274\354\316\264\315\352\263\311"; state_color = UI_AMBER; }
        }
        ui_text(356U, 356U, 49U, outcome_text, state_color, UI_SURFACE, 0U);
        if(boot[0]) snprintf(text, sizeof(text), "\315\250\271\375 %u   \312\247\260\334 %u   \316\264\262\342\312\324 %u", boot[1], boot[2], boot[3]);
        else strcpy(text, "\311\320\316\336\277\311\323\303\265\304\311\317\265\347\327\324\274\354\274\307\302\274");
        ui_text(356U, 386U, 49U, text, UI_MUTED, UI_SURFACE, 0U);
        memcpy(last_boot, boot, sizeof(last_boot));
    }
}

/* Read-only dashboard: RF ACK is explicitly separate from robot telemetry. */
static void gui_dashboard_status(uint8_t first)
{
    static uint32_t last_ms;
    static uint16_t last_mv;
    static uint8_t last_radio = 255U;
    static uint8_t last_valid = 255U;
    ControlLinkSnapshot snapshot;
    unsigned long now;
    uint32_t mv;
    uint8_t radio, valid;
    char text[40];
    get_tick_count(&now);
    if(!first && (uint32_t)(now - last_ms) < 250U) return;
    last_ms = (uint32_t)now;
    control_link_get_snapshot(&snapshot);
    mv = ADC1_Value[6] <= 4095U ? (uint32_t)ADC1_Value[6] * 66UL * param.batVoltAdjust / 40950UL : 0UL;
    radio = !param.NRF_Mode ? 0U : snapshot.ack_seen && snapshot.ack_age_ms < 150U ? 2U : 1U;
    valid = snapshot.sampled && snapshot.input_fresh && snapshot.sample_age_ms <= 100U && ADC1_Value[6] <= 4095U;
    if(first) {
        ui_round_rect(24U,96U,752U,72U,12U,UI_SURFACE);
        ui_fill(260U,108U,1U,48U,UI_LINE);
        ui_fill(518U,108U,1U,48U,UI_LINE);
        ui_text(44U,104U,22U,"\322\243\277\330\306\367\265\347\263\330",UI_MUTED,UI_SURFACE,0U);
        ui_text(280U,104U,25U,"\316\336\317\337\323\262\274\376\273\330\326\264",UI_MUTED,UI_SURFACE,0U);
        ui_text(540U,104U,26U,"\324\313\266\257\277\330\326\306",UI_MUTED,UI_SURFACE,0U);
        ui_text(540U,126U,13U,"\322\321\275\373\326\271",UI_MUTED,UI_SURFACE,1U);
    }
    if(first || mv != last_mv || valid != last_valid) {
        if(valid)
            snprintf(text,sizeof(text),"%lu.%02lu V",(unsigned long)(mv/1000U),(unsigned long)((mv%1000U)/10U));
        else strcpy(text,"--.-- V");
        ui_text(44U,126U,12U,text,UI_INK,UI_SURFACE,1U);
        last_mv = (uint16_t)mv;
        last_valid = valid;
    }
    if(first || radio != last_radio) {
        ui_text(280U,126U,13U,radio==0U ? "\271\330\261\325" : radio==1U ? "\265\310\264\375\326\320" : "\322\321\312\325\265\275\273\330\326\264",
                radio==2U ? UI_GREEN : UI_MUTED,UI_SURFACE,1U);
        last_radio = radio;
    }
}

void menu_group_page(uint8_t selected_group)
{
    static const char * const titles[] = {"\322\243\277\330", "\311\350\326\303", "\271\244\276\337"};
    static const char * const hints[] = {"\273\372\306\367\310\313\277\330\326\306\323\353\274\340\312\323", "\322\243\277\330\306\367\262\316\312\375\305\344\326\303", "\264\253\270\320\306\367\323\353\323\262\274\376\325\357\266\317"};
    static const uint8_t icons[] = {UI_ICON_CONTROL, UI_ICON_SETTINGS, UI_ICON_TOOLS};
    static const uint16_t colors[] = {UI_ACCENT, UI_GREEN, UI_AMBER};
    static uint8_t previous = 255U;
    uint8_t first = display_flag != 0U, i;
    uint16_t x, bg;
    char count[24];
    if(selected_group >= MENU_GROUP_COUNT) selected_group = 0U;
    if(first) {
        display_flag = 0U; GTP_IRQ_Disable();
        ui_shell("\322\243\277\330\306\367\326\367\322\263", "\307\353\321\241\324\361\271\246\304\334\267\326\300\340", "\326\367\322\263");
        ui_footer("\311\317/\317\302 \321\241\324\361\267\326\300\340", "OK \275\370\310\353");
    }
    gui_dashboard_status(first);
    for(i = 0U; i < MENU_GROUP_COUNT; i++) {
        if(!first && selected_group == previous) continue;
        if(!first && i != selected_group && i != previous) continue;
        x = 24U + (uint16_t)i * 256U;
        bg = UI_SURFACE;
        if(first) ui_round_rect(x,184U,240U,216U,16U,bg);
        ui_round_outline(x,184U,240U,216U,16U,2U,i == selected_group ? UI_ACCENT : UI_SURFACE);
        if(!first) continue;
        ui_round_rect(x+76U,206U,88U,88U,20U,colors[i]);
        ui_icon(x+96U,226U,48U,icons[i],UI_SURFACE,colors[i]);
        ui_text(x + (uint16_t)((240U - strlen(titles[i])*16U)/2U),306U,
                (uint8_t)strlen(titles[i]),titles[i],UI_INK,bg,1U);
        ui_text(x+(uint16_t)((240U-strlen(hints[i])*8U)/2U),348U,
                (uint8_t)strlen(hints[i]),hints[i],UI_MUTED,bg,0U);
        snprintf(count,sizeof(count),"%u \317\356\271\246\304\334",menu_group_count(i));
        ui_text(x+76U,374U,11U,count,UI_MUTED,bg,0U);
    }
    if(first || previous != selected_group) {
        for(i=0U;i<MENU_GROUP_COUNT;i++) {
            LCD_SetTextColor(i==selected_group?UI_ACCENT:UI_LINE);
            ILI9806G_DrawCircle(382U+(uint16_t)i*18U,423U,4U,1U);
        }
    }
    previous=selected_group;
}

void main_menu(uint8_t selected_item)
{
#define MENU_LABEL(page,label,hint) label,
    static const char * const names[] = {MENU_ENTRY_LIST(MENU_LABEL)};
#undef MENU_LABEL
    static const char * const subtitles[] = {
        "\312\326\266\257\277\330\326\306", "\312\344\310\353\323\353\267\242\313\315\312\375\276\335", "\260\264\274\374\323\353\262\246\270\313", "\317\265\315\263\323\353\315\250\265\300", "\316\336\317\337\301\264\302\267",
        "\310\325\300\372\323\353\320\243\312\261",
        "\271\314\274\376\323\353\304\332\264\346", "\261\276\273\372\327\313\314\254", "\266\250\316\273\323\353\316\300\320\307", "SD / Flash \316\304\274\376",
        "\323\262\274\376\274\354\262\342", "\264\346\264\242\325\357\266\317"
    };
    static const char * const groups[] = {"\322\243\277\330","\311\350\326\303","\271\244\276\337"};
    static const uint8_t icons[] = {UI_ICON_CONTROL,UI_ICON_CHANNEL,UI_ICON_SWITCH,
        UI_ICON_SETTINGS,UI_ICON_RADIO,UI_ICON_CALENDAR,UI_ICON_CHIP,UI_ICON_IMU,UI_ICON_GPS,
        UI_ICON_FOLDER,UI_ICON_TOOLS,UI_ICON_MEMORY};
    static uint8_t previous=255U;
    uint8_t first, group, start, count, i, entry;
    uint16_t x,y,bg;
    char text[24];
    if(selected_item>=MENU_ENTRY_COUNT) selected_item=0U;
    group=menu_entry_group(selected_item); start=menu_group_first(group); count=menu_group_count(group);
    first=display_flag!=0U || previous==255U || menu_entry_group(previous)!=group;
    if(first) {
        display_flag=0U; GTP_IRQ_Disable();
        ui_shell(groups[group],"\307\353\321\241\324\361\271\246\304\334", "\271\246\304\334\262\313\265\245");
        ui_round_rect(24U,96U,160U,336U,12U,UI_SURFACE);
        ui_text(40U,112U,16U,"\271\246\304\334\267\326\300\340",UI_MUTED,UI_SURFACE,0U);
        for(i=0U;i<MENU_GROUP_COUNT;i++) {
            bg=i==group?UI_TINT:UI_SURFACE;
            ui_round_rect(32U,144U+(uint16_t)i*56U,144U,44U,8U,bg);
            ui_text(44U,158U+(uint16_t)i*56U,15U,groups[i],i==group?UI_ACCENT:UI_MUTED,bg,0U);
        }
        ui_text(40U,360U,15U,"BACK",UI_MUTED,UI_SURFACE,0U);
        ui_text(40U,382U,15U,"\267\265\273\330\267\326\300\340",UI_MUTED,UI_SURFACE,0U);
        ui_footer("\311\317/\317\302 \321\241\324\361\271\246\304\334", "OK \275\370\310\353  BACK \267\265\273\330");
    }
    for(i=0U;i<count;i++) {
        entry=start+i;
        if(!first && entry!=selected_item && entry!=previous) continue;
        x=200U+(uint16_t)(i%3U)*196U; y=96U+(uint16_t)(i/3U)*172U;
        bg=UI_SURFACE;
        if(first) ui_round_rect(x,y,184U,160U,12U,bg);
        ui_round_outline(x,y,184U,160U,12U,2U,entry==selected_item?UI_ACCENT:UI_SURFACE);
        if(!first) continue;
        ui_round_rect(x+58U,y+14U,68U,68U,16U,UI_SURFACE);
        ui_icon(x+68U,y+24U,48U,icons[entry],UI_ACCENT,UI_SURFACE);
        ui_text(x+12U,y+98U,20U,names[entry],UI_INK,bg,0U);
        ui_text(x+12U,y+124U,20U,subtitles[entry],UI_MUTED,bg,0U);
    }
    if(count<=3U && first) {
        ui_text(216U,304U,65U,group==0U ? "\275\366\324\332\273\372\306\367\310\313\277\330\326\306\322\263\324\312\320\355\312\271\304\334\324\313\266\257\241\243" : "\262\316\312\375\320\336\270\304\272\363\320\350\326\264\320\320\261\243\264\346\262\305\311\372\320\247\241\243",UI_MUTED,UI_BG,0U);
        ui_text(216U,332U,65U,group==0U ? "\313\311\277\252 DCH1 \273\362\315\313\263\366\277\330\326\306\322\263\243\254\261\276\273\372\263\267\317\372\324\313\266\257\312\271\304\334\241\243" : "\261\340\274\255\312\261\260\264 BACK \310\241\317\373\243\254\315\313\263\366\307\260\274\354\262\351\261\243\264\346\327\264\314\254\241\243",UI_MUTED,UI_BG,0U);
    }
    snprintf(text,sizeof(text),"%u / %u",selected_item-start+1U,count);
    ui_text(720U,60U,7U,text,UI_ACCENT,UI_BG,0U);
    previous=selected_item;
}


void digital_channel_monitor_page(const uint8_t *raw_values, const uint8_t *stable_values)
{
    static const char *pins[6] = {"PD6", "PD3", "PA8", "PD7", "PE3", "PE2"};
    static const uint8_t is_button[6] = {1U, 0U, 1U, 1U, 0U, 0U};
    static uint8_t last_raw[6], last_stable[6];
    uint8_t first_draw = display_flag != 0U, channel, raw, stable, changed;
    uint16_t x, y, background, color;
    const char *state;
    char text[32];

    if(first_draw) {
        display_flag = 0U;
        GTP_IRQ_Disable();
        ui_shell("\312\375\327\326\312\344\310\353", "6 \302\267\312\344\310\353 / 30 ms \310\245\266\266", "\322\243\277\330 / \274\340\312\323");
        ui_footer("BACK \267\265\273\330  \260\264\274\374\265\315\265\347\306\275\323\320\320\247", "\324\255\312\274\323\353\316\310\266\250\265\347\306\275");
    }
    for(channel = 0U; channel < 6U; channel++) {
        raw = raw_values ? raw_values[channel] : 0xffU;
        stable = stable_values ? stable_values[channel] : 0xffU;
        changed = first_draw || stable != last_stable[channel];
        x = 24U + (uint16_t)(channel % 3U) * 256U;
        y = 104U + (uint16_t)(channel / 3U) * 168U;
        background = stable == 0U ? UI_TINT : UI_SURFACE;
        color = stable > 1U ? UI_MUTED : is_button[channel] ?
                (stable == 0U ? UI_GREEN : UI_MUTED) : UI_ACCENT;
        if(changed) {
            ui_round_rect(x, y, 240U, 152U, 14U, background);
            snprintf(text, sizeof(text), "DCH%u", channel + 1U);
            ui_text(x + 16U, y + 12U, 5U, text, UI_INK, background, 1U);
            ui_text(x + 128U, y + 20U, 12U, channel == 0U ? "\324\313\266\257\312\271\304\334" :
                    is_button[channel] ? "\260\264\274\374" : "\262\246\270\313", color, background, 0U);
            snprintf(text, sizeof(text), "%s / %s", pins[channel], is_button[channel] ? "\327\324\270\264\316\273" : "\301\275\265\265");
            ui_text(x + 16U, y + 46U, 26U, text, UI_MUTED, background, 0U);
            if(stable > 1U) state = "--";
            else if(is_button[channel]) state = stable == 0U ? "\322\321\260\264\317\302" : "\322\321\313\311\277\252";
            else state = stable == 0U ? "\316\273\326\303 A" : "\316\273\326\303 B";
            ui_text(x + 16U, y + 68U, 8U, state, color, background, 2U);
        }
        if(changed || raw != last_raw[channel]) {
            snprintf(text, sizeof(text), "\324\255\312\274 %c    \316\310\266\250 %c", raw <= 1U ? '0' + raw : '-',
                     stable <= 1U ? '0' + stable : '-');
            ui_text(x + 16U, y + 128U, 26U, text, UI_MUTED, background, 0U);
        }
        last_raw[channel] = raw;
        last_stable[channel] = stable;
    }
}

static uint8_t gui_file_decode_utf8(const uint8_t *source,
									uint32_t *codepoint,
									uint8_t *source_advance)
{
	uint8_t byte0 = source[0];
	uint8_t byte1 = source[1];
	uint8_t byte2;
	uint8_t byte3;

	if((byte0 >= 0xc2U) && (byte0 <= 0xdfU) &&
	   ((byte1 & 0xc0U) == 0x80U))
	{
		*codepoint = ((uint32_t)(byte0 & 0x1fU) << 6) |
					 (uint32_t)(byte1 & 0x3fU);
		*source_advance = 2U;
		return 1U;
	}

	if((byte0 >= 0xe0U) && (byte0 <= 0xefU) &&
	   ((byte1 & 0xc0U) == 0x80U))
	{
		byte2 = source[2];
		if(((byte2 & 0xc0U) == 0x80U) &&
		   !((byte0 == 0xe0U) && (byte1 < 0xa0U)) &&
		   !((byte0 == 0xedU) && (byte1 >= 0xa0U)))
		{
			*codepoint = ((uint32_t)(byte0 & 0x0fU) << 12) |
						 ((uint32_t)(byte1 & 0x3fU) << 6) |
						 (uint32_t)(byte2 & 0x3fU);
			*source_advance = 3U;
			return 1U;
		}
	}

	if((byte0 >= 0xf0U) && (byte0 <= 0xf4U) &&
	   ((byte1 & 0xc0U) == 0x80U))
	{
		byte2 = source[2];
		if((byte2 & 0xc0U) == 0x80U)
		{
			byte3 = source[3];
			if(((byte3 & 0xc0U) == 0x80U) &&
			   !((byte0 == 0xf0U) && (byte1 < 0x90U)) &&
			   !((byte0 == 0xf4U) && (byte1 > 0x8fU)))
			{
				*codepoint = ((uint32_t)(byte0 & 0x07U) << 18) |
							 ((uint32_t)(byte1 & 0x3fU) << 12) |
							 ((uint32_t)(byte2 & 0x3fU) << 6) |
							 (uint32_t)(byte3 & 0x3fU);
				*source_advance = 4U;
				return 1U;
			}
		}
	}

	return 0U;
}

static uint8_t gui_file_source_is_utf8(const char *source)
{
	uint16_t index = 0U;
	uint8_t source_advance;
	uint8_t has_multibyte = 0U;
	uint32_t codepoint;

	while(source[index] != '\0')
	{
		if((uint8_t)source[index] < 0x80U)
		{
			index++;
			continue;
		}
		if(gui_file_decode_utf8((const uint8_t *)&source[index],
								&codepoint, &source_advance) == 0U)
		{
			return 0U;
		}
		has_multibyte = 1U;
		index += source_advance;
	}

	return has_multibyte;
}

uint8_t gui_file_name_can_render(const char *name)
{
	uint16_t index = 0U;
	uint16_t oem_code;
	uint32_t codepoint;
	uint8_t first_byte;
	uint8_t second_byte;
	uint8_t source_advance;
	uint8_t utf8_source = gui_file_source_is_utf8(name);

	while(name[index] != '\0')
	{
		first_byte = (uint8_t)name[index];
		if((first_byte >= 32U) && (first_byte <= 126U))
		{
			index++;
			continue;
		}

		if(utf8_source != 0U)
		{
			if((gui_file_decode_utf8((const uint8_t *)&name[index],
								  &codepoint, &source_advance) == 0U) ||
			   (codepoint > 0xffffUL))
			{
				return 0U;
			}
			oem_code = ff_convert((WCHAR)codepoint, 0U);
			first_byte = (uint8_t)(oem_code >> 8);
			second_byte = (uint8_t)oem_code;
			index += source_advance;
		}
		else
		{
			second_byte = (uint8_t)name[index + 1U];
			index += 2U;
		}

		if((first_byte < 0xa1U) || (first_byte > 0xf7U) ||
		   (second_byte < 0xa1U) || (second_byte > 0xfeU))
		{
			return 0U;
		}
	}

	return 1U;
}

static void gui_file_display_text(char *destination, uint16_t destination_size,
								  const char *source, uint16_t maximum_width)
{
	uint16_t source_index = 0U;
	uint16_t destination_index = 0U;
	uint16_t used_width = 0U;
	uint16_t oem_code;
	uint32_t codepoint;
	uint16_t character_width;
	uint8_t first_byte;
	uint8_t second_byte;
	uint8_t source_advance;
	uint8_t output_bytes;
	uint8_t utf8_source = gui_file_source_is_utf8(source);

	while(source[source_index] != '\0')
	{
		first_byte = (uint8_t)source[source_index];
		second_byte = (uint8_t)source[source_index + 1U];
		source_advance = 1U;
		output_bytes = 1U;
		character_width = 16U;

		if((first_byte >= 32U) && (first_byte <= 126U))
		{
			destination[destination_index] = (char)first_byte;
		}
		else if(utf8_source != 0U)
		{
			if(gui_file_decode_utf8((const uint8_t *)&source[source_index],
								&codepoint, &source_advance) != 0U)
			{
				if(codepoint <= 0xffffUL)
				{
					oem_code = ff_convert((WCHAR)codepoint, 0U);
					first_byte = (uint8_t)(oem_code >> 8);
					second_byte = (uint8_t)oem_code;
				}
				if((codepoint <= 0xffffUL) &&
				   (first_byte >= 0xa1U) && (first_byte <= 0xf7U) &&
				   (second_byte >= 0xa1U) && (second_byte <= 0xfeU))
				{
					destination[destination_index] = (char)first_byte;
					destination[destination_index + 1U] = (char)second_byte;
					output_bytes = 2U;
					character_width = 32U;
				}
				else
				{
					destination[destination_index] = '?';
				}
			}
			else
			{
				destination[destination_index] = '?';
				source_advance = 1U;
			}
		}
		else if((first_byte >= 0xa1U) && (first_byte <= 0xf7U) &&
				(second_byte >= 0xa1U) && (second_byte <= 0xfeU))
		{
			/* The external 32x32 font contains the GB2312 subset of CP936. */
			destination[destination_index] = (char)first_byte;
			destination[destination_index + 1U] = (char)second_byte;
			source_advance = 2U;
			output_bytes = 2U;
			character_width = 32U;
		}
		else
		{
			/* Skip a complete unsupported GBK pair and show one replacement. */
			destination[destination_index] = '?';
			if((first_byte >= 0x81U) && (first_byte <= 0xfeU) &&
			   (second_byte >= 0x40U) && (second_byte <= 0xfeU) &&
			   (second_byte != 0x7fU))
			{
				source_advance = 2U;
			}
		}

		if((used_width + character_width) > maximum_width)
		{
			break;
		}
		if((destination_index + output_bytes + 1U) > destination_size)
		{
			break;
		}
		destination_index += output_bytes;
		source_index += source_advance;
		used_width += character_width;
	}
	destination[destination_index] = '\0';
}

static void gui_file_size_text(uint32_t size, char *text)
{
	if(size < 1024UL)
	{
		sprintf(text, "%lu B", (unsigned long)size);
	}
	else if(size < (1024UL * 1024UL))
	{
		sprintf(text, "%lu KB", size / 1024UL);
	}
	else
	{
		sprintf(text, "%lu.%01lu MB", size / (1024UL * 1024UL),
				(size % (1024UL * 1024UL)) * 10UL / (1024UL * 1024UL));
	}
}

void file_browser_page(const char *path, const GuiFileEntry *entries,
                       uint8_t item_count, uint8_t selected_item,
                       uint8_t first_visible, uint16_t revision,
                       const char *status_text)
{
    static uint16_t last_revision = 0xffffU;
    static uint8_t last_selected = 0xffU, last_first_visible = 0xffU, last_count;
    static char last_path[96], last_status[96];
    uint8_t row, first_draw = 0U, content_changed, selection_changed;
    uint16_t index, y, background, foreground;
    char safe_path[96], safe_status[96], safe_name[96], size_text[16], count_text[32];

    if(display_flag == 1U) {
        display_flag = 0U;
        GTP_IRQ_Disable();
        first_draw = 1U;
    }
    if(!entries) item_count = 0U;
    if(!item_count) { selected_item = 0U; first_visible = 0U; }
    else {
        if(selected_item >= item_count) selected_item = item_count - 1U;
        if(first_visible >= item_count) first_visible = item_count - 1U;
    }
    gui_file_display_text(safe_path, sizeof(safe_path), path ? path : "", 656U);
    gui_file_display_text(safe_status, sizeof(safe_status), status_text ? status_text : "", 752U);
    content_changed = first_draw || revision != last_revision || first_visible != last_first_visible ||
                      item_count != last_count || strcmp(safe_path, last_path) != 0;
    selection_changed = selected_item != last_selected;
    if(first_draw) {
        ui_shell("\316\304\274\376\344\257\300\300", "SD \277\250 / \316\304\274\376\344\257\300\300", "\271\244\276\337");
        ui_round_rect(16U, 96U, 768U, 40U, 10U, UI_TINT);
        gui_file_row_icon(32U, 102U, 1U, UI_ACCENT);
    }
    if(first_draw || strcmp(safe_path, last_path) != 0) {
        ui_fill(80U, 100U, 672U, 32U, UI_TINT);
        LCD_SetFont(&Font16x32);
        LCD_SetTextColor(UI_ACCENT);
        LCD_SetBackColor(UI_TINT);
        ILI9806G_DispString_EN_CH(80U, 100U, safe_path);
    }
    if(content_changed) ui_fill(16U, 144U, 768U, 260U, UI_BG);
    for(row = 0U; row < 6U; row++) {
        index = (uint16_t)first_visible + row;
        if(index >= item_count) break;
        if(!content_changed && !(selection_changed && (index == selected_item || index == last_selected)))
            continue;
        y = 144U + (uint16_t)row * 44U;
        background = index == selected_item ? UI_TINT : UI_SURFACE;
        foreground = index == selected_item ? UI_ACCENT : UI_INK;
        ui_round_rect(16U, y, 768U, 40U, 9U, background);
        gui_file_row_icon(32U, y + 6U, entries[index].is_directory, foreground);
        gui_file_display_text(safe_name, sizeof(safe_name), entries[index].name, 512U);
        LCD_SetFont(&Font16x32);
        LCD_SetTextColor(foreground);
        LCD_SetBackColor(background);
        ILI9806G_DispString_EN_CH(80U, y + 4U, safe_name);
        if(entries[index].is_directory) strcpy(size_text, "\316\304\274\376\274\320");
        else gui_file_size_text(entries[index].size, size_text);
        ui_text(620U, y + 12U, 16U, size_text, UI_MUTED, background, 0U);
        ui_text(756U, y + 12U, 1U, ">", foreground, background, 0U);
    }
    if(content_changed && !item_count) {
        ui_round_rect(16U, 144U, 768U, 260U, 14U, UI_SURFACE);
        ui_icon(376U, 204U, 48U, UI_ICON_FOLDER, UI_MUTED, UI_SURFACE);
        ui_text(312U, 276U, 24U, "This directory is empty", UI_MUTED, UI_SURFACE, 0U);
    }
    if(first_draw || strcmp(safe_status, last_status) != 0) {
        ui_fill(24U, 408U, 752U, 32U, UI_BG);
        LCD_SetFont(&Font16x32);
        LCD_SetTextColor(UI_MUTED);
        LCD_SetBackColor(UI_BG);
        ILI9806G_DispString_EN_CH(24U, 408U, safe_status);
    }
    if(content_changed || selection_changed) {
        snprintf(count_text, sizeof(count_text), "%u / %u FILES", item_count ? selected_item + 1U : 0U, item_count);
        ui_footer("\311\317/\317\302 \321\241\324\361  OK \264\362\277\252  BACK \311\317\274\266/\315\313\263\366", count_text);
    }
    memcpy(last_path, safe_path, sizeof(last_path));
    memcpy(last_status, safe_status, sizeof(last_status));
    last_selected = selected_item;
    last_first_visible = first_visible;
    last_count = item_count;
    last_revision = revision;
}

void parameter_settings_page(const GuiParamRow *rows, uint8_t visible_count,
                             uint8_t selected_row, uint8_t first_visible,
                             uint8_t total_items, uint8_t editing,
                             uint8_t dirty, uint16_t revision,
                             const char *status_text)
{
    static uint8_t last_selected_row = 0xffU;
    static uint8_t last_first_visible = 0xffU;
    static uint8_t last_visible_count, last_total_items;
    static uint8_t last_editing = 0xffU, last_dirty = 0xffU;
    static GuiParamRow last_rows[GUI_PARAM_VISIBLE_ROWS];
    static char last_status[80];
    uint8_t row, first_draw = 0U, window_changed, selection_changed;
    uint8_t current_selected, previous_selected;
    uint16_t state_color;
    GuiParamRow current;
    char status[80], text[32];
    const char *state_text;

    if(display_flag == 1U) {
        display_flag = 0U;
        GTP_IRQ_Disable();
        first_draw = 1U;
    }
    (void)revision;
    if(visible_count > GUI_PARAM_VISIBLE_ROWS) visible_count = GUI_PARAM_VISIBLE_ROWS;
    if(!rows || !total_items || first_visible >= total_items) visible_count = 0U;
    else if(visible_count > (uint8_t)(total_items - first_visible))
        visible_count = (uint8_t)(total_items - first_visible);
    if(!visible_count || selected_row >= visible_count) selected_row = 0U;
    if(!total_items) first_visible = 0U;
    snprintf(status, sizeof(status), "%.79s", status_text ? status_text : "");
    window_changed = first_draw || first_visible != last_first_visible ||
                     visible_count != last_visible_count || total_items != last_total_items;
    selection_changed = first_draw || selected_row != last_selected_row ||
                        editing != last_editing;

    if(first_draw) {
        ui_shell("\262\316\312\375\311\350\326\303", "\317\265\315\263 / \315\250\265\300\305\344\326\303", "\311\350\326\303");
        ui_round_rect(16U, 104U, 160U, 320U, 14U, UI_SURFACE);
        ui_round_rect(64U, 124U, 64U, 64U, 14U, UI_TINT);
        ui_icon(72U, 132U, 48U, UI_ICON_SETTINGS, UI_ACCENT, UI_TINT);
        ui_text(32U, 196U, 16U, "\262\316\312\375\311\350\326\303", UI_MUTED, UI_SURFACE, 0U);
        ui_text(32U, 284U, 16U, "\265\261\307\260\317\356\304\277", UI_MUTED, UI_SURFACE, 0U);
        ui_text(32U, 360U, 16U, "\320\336\270\304\272\363\307\353\326\264\320\320", UI_MUTED, UI_SURFACE, 0U);
        ui_text(32U, 382U, 16U, "\301\320\261\355\304\251\316\262\265\304\261\243\264\346", UI_MUTED, UI_SURFACE, 0U);
    }

    if(first_draw || editing != last_editing || dirty != last_dirty) {
        state_text = editing ? "\261\340\274\255\326\320" : dirty ? "\316\264\261\243\264\346" : "\322\321\261\243\264\346";
        state_color = editing || dirty ? UI_AMBER : UI_GREEN;
        ui_round_rect(32U, 224U, 128U, 40U, 9U, state_color);
        ui_text(40U, 236U, 14U, state_text, UI_SURFACE, state_color, 0U);
        ui_footer(editing ? "\311\317/\317\302 \265\367\325\373  OK \310\267\310\317  BACK \310\241\317\373" :
                  "\311\317/\317\302 \321\241\324\361  OK \261\340\274\255/\326\264\320\320  BACK \267\265\273\330",
                  dirty ? "\320\336\270\304\311\320\316\264\261\243\264\346" : "\262\316\312\375\311\350\326\303");
    }
    if(window_changed || selection_changed) {
        snprintf(text, sizeof(text), "%u/%u",
                 visible_count ? (uint16_t)first_visible + selected_row + 1U : 0U,
                 (uint16_t)total_items);
        ui_text(32U, 308U, 8U, text, UI_INK, UI_SURFACE, 1U);
    }

    if(window_changed) ui_fill(192U, 104U, 580U, 284U, UI_BG);
    for(row = 0U; row < GUI_PARAM_VISIBLE_ROWS; row++) {
        if(row >= visible_count) {
            if(window_changed) ui_fill(192U, 104U + (uint16_t)row * 48U, 580U, 44U, UI_BG);
            continue;
        }
        memcpy(&current, &rows[row], sizeof(current));
        current.label[GUI_PARAM_LABEL_LENGTH - 1U] = '\0';
        current.value[GUI_PARAM_VALUE_LENGTH - 1U] = '\0';
        current_selected = row == selected_row;
        previous_selected = row == last_selected_row;
        if(window_changed || strcmp(current.label, last_rows[row].label) != 0 ||
           strcmp(current.value, last_rows[row].value) != 0 ||
           (selection_changed && (current_selected || previous_selected))) {
            gui_settings_row(row, (uint16_t)first_visible + row + 1U,
                             current.label, current.value, current_selected, editing);
        }
        memcpy(&last_rows[row], &current, sizeof(current));
    }
    if(window_changed) {
        gui_settings_scroll(first_visible, visible_count, total_items);
        if(!visible_count) {
            ui_round_rect(192U, 104U, 580U, 284U, 14U, UI_SURFACE);
            ui_text(344U, 220U, 23U, "\324\335\316\336\277\311\323\303\262\316\312\375", UI_MUTED, UI_SURFACE, 0U);
        }
    }
    if(first_draw || strcmp(status, last_status) != 0 || editing != last_editing || dirty != last_dirty) {
        ui_text(200U, 408U, 72U, status,
                editing || dirty ? UI_AMBER : UI_MUTED, UI_BG, 0U);
    }
    last_selected_row = selected_row;
    last_first_visible = first_visible;
    last_visible_count = visible_count;
    last_total_items = total_items;
    last_editing = editing;
    last_dirty = dirty;
    memcpy(last_status, status, sizeof(last_status));
}

void nrf_settings_page(uint8_t selected_item, uint8_t editing,
                       uint8_t enabled, uint8_t channel,
                       uint8_t power_index, uint8_t data_rate,
                       const char *status_text, uint8_t runtime_valid,
                       uint8_t runtime_enabled, uint8_t runtime_channel,
                       uint8_t runtime_power_index, uint8_t runtime_data_rate)
{
    static const int8_t power_dbm[4] = {-18, -12, -6, 0};
    static const char *rate_text[3] = {"250 Kbps", "1 Mbps", "2 Mbps"};
    static const char *labels[6] = {"\316\336\317\337\267\242\311\344", "\316\336\317\337\306\265\265\300", "\267\242\311\344\271\246\302\312",
                                   "\277\325\326\320\313\331\302\312", "\323\246\323\303\262\242\261\243\264\346", "\274\354\262\342\304\243\277\351"};
    static char last_values[6][24];
    static char last_status[80];
    static uint8_t last_runtime[5], last_selected = 0xffU, last_editing = 0xffU;
    uint8_t runtime[5], row, first_draw = 0U, selection_changed;
    uint8_t runtime_changed, actual_differs;
    char values[6][24], status[80], text[24];
    uint16_t state_color;

    if(display_flag == 1U) {
        display_flag = 0U;
        GTP_IRQ_Disable();
        first_draw = 1U;
    }
    if(selected_item >= 6U) selected_item = 0U;
    if(power_index > 3U) power_index = 0U;
    if(data_rate > 2U) data_rate = 2U;
    if(runtime_power_index > 3U) runtime_power_index = 0U;
    if(runtime_data_rate > 2U) runtime_data_rate = 2U;
    enabled = enabled ? 1U : 0U;
    runtime_enabled = runtime_enabled ? 1U : 0U;
    runtime_valid = runtime_valid ? 1U : 0U;
    runtime[0] = runtime_valid;
    runtime[1] = runtime_enabled;
    runtime[2] = runtime_channel;
    runtime[3] = runtime_power_index;
    runtime[4] = runtime_data_rate;
    runtime_changed = first_draw || memcmp(runtime, last_runtime, sizeof(runtime)) != 0;
    selection_changed = first_draw || selected_item != last_selected || editing != last_editing;
    actual_differs = runtime_valid && (enabled != runtime_enabled || channel != runtime_channel ||
                      power_index != runtime_power_index || data_rate != runtime_data_rate);
    snprintf(status, sizeof(status), "%.79s", status_text ? status_text : "");
    memset(values, 0, sizeof(values));
    snprintf(values[0], sizeof(values[0]), "%s", enabled ? "\277\252\306\364" : "\271\330\261\325");
    snprintf(values[1], sizeof(values[1]), "%u / %u MHz", channel, 2400U + channel);
    snprintf(values[2], sizeof(values[2]), "%d dBm", power_dbm[power_index]);
    snprintf(values[3], sizeof(values[3]), "%s", rate_text[data_rate]);
    snprintf(values[4], sizeof(values[4]), "%s", "\326\264\320\320");
    snprintf(values[5], sizeof(values[5]), "%s", "\326\264\320\320");

    if(first_draw) {
        ui_shell("NRF \316\336\317\337\311\350\326\303", "NRF24 / \316\336\317\337\267\242\311\344\305\344\326\303", "\311\350\326\303");
        ui_round_rect(16U, 104U, 160U, 320U, 14U, UI_SURFACE);
        ui_round_rect(64U, 124U, 64U, 64U, 14U, UI_TINT);
        ui_icon(72U, 132U, 48U, UI_ICON_RADIO, UI_ACCENT, UI_TINT);
        ui_text(32U, 196U, 16U, "\304\243\277\351\327\264\314\254\273\330\266\301", UI_MUTED, UI_SURFACE, 0U);
        ui_text(32U, 268U, 16U, "\265\261\307\260\311\372\320\247\311\350\326\303", UI_MUTED, UI_SURFACE, 0U);
    }
    if(first_draw) ui_text(200U,430U,72U,"\316\336\317\337\277\252\306\364\272\363\326\334\306\332\267\242\260\374\243\273\324\313\266\257\320\350\324\332\277\330\326\306\322\263\265\245\266\300\312\271\304\334\241\243",UI_MUTED,UI_BG,0U);
    if(runtime_changed) {
        state_color = runtime_valid ? UI_GREEN : UI_RED;
        ui_round_rect(32U, 224U, 128U, 32U, 8U, state_color);
        ui_text(40U, 232U, 14U, runtime_valid ? "\304\243\277\351\325\375\263\243" : "\266\301\310\241\312\247\260\334",
                UI_SURFACE, state_color, 0U);
        ui_text(32U, 292U, 8U, runtime_valid ? (runtime_enabled ? "\277\252\306\364" : "\271\330\261\325") : "--",
                runtime_valid ? UI_INK : UI_MUTED, UI_SURFACE, 1U);
        snprintf(text, sizeof(text), runtime_valid ? "CH%u  %uMHz" : "CH --",
                 runtime_channel, 2400U + runtime_channel);
        ui_text(32U, 336U, 16U, text, UI_MUTED, UI_SURFACE, 0U);
        snprintf(text, sizeof(text), runtime_valid ? "%d dBm" : "-- dBm", power_dbm[runtime_power_index]);
        ui_text(32U, 360U, 16U, text, UI_MUTED, UI_SURFACE, 0U);
        ui_text(32U, 384U, 16U, runtime_valid ? rate_text[runtime_data_rate] : "--",
                UI_MUTED, UI_SURFACE, 0U);
    }
    for(row = 0U; row < 6U; row++) {
        if(first_draw || strcmp(values[row], last_values[row]) != 0 ||
           (selection_changed && (row == selected_item || row == last_selected))) {
            gui_settings_row(row, row + 1U, labels[row], values[row], row == selected_item, editing);
        }
    }
    if(first_draw || selection_changed || runtime_changed ||
       memcmp(values, last_values, sizeof(values)) != 0 || strcmp(status, last_status) != 0) {
        ui_text(200U, 408U, 72U, status, UI_MUTED, UI_BG, 0U);
        ui_footer(editing ? "\311\317/\317\302 \265\367\325\373  OK/BACK \275\341\312\370\261\340\274\255" :
                  "\311\317/\317\302 \321\241\324\361  OK \261\340\274\255/\326\264\320\320  BACK \267\265\273\330",
                  editing ? "\261\340\274\255\326\320" : actual_differs ? "\320\336\270\304\311\320\316\264\323\246\323\303" : "\316\336\317\337\311\350\326\303");
    }
    memcpy(last_values, values, sizeof(last_values));
    memcpy(last_runtime, runtime, sizeof(last_runtime));
    memcpy(last_status, status, sizeof(last_status));
    last_selected = selected_item;
    last_editing = editing;
}

typedef struct
{
	int sig;
	int fix;
	int mode;
	int gps_inuse;
	int gps_inview;
	int bds_inuse;
	int bds_inview;
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	double latitude;
	double longitude;
	double altitude;
	double speed;
	double course;
	double hdop;
	double pdop;
} gui_gps_snapshot_t;

void system_data_read_and_set(void)
{
    static gui_gps_snapshot_t previous;
    static uint8_t snapshot_valid;
    gui_gps_snapshot_t current;
    uint8_t fresh = gps_data_is_fresh();
    uint8_t position_valid, time_valid, i;
    uint16_t color;
    const char *fix_text;
    char text[80];
    double latitude, longitude;

    memset(&current, 0, sizeof(current));
    current.sig = fresh ? info.sig : 0;
    current.fix = fresh ? info.fix : 1;
    current.mode = info.mode;
    current.gps_inuse = fresh ? info.satinfo.inuse : -1;
    current.gps_inview = fresh ? info.satinfo.inview : -1;
    current.bds_inuse = fresh ? info.BDsatinfo.inuse : -1;
    current.bds_inview = fresh ? info.BDsatinfo.inview : -1;
    current.year = gps_time_is_fresh() ? beiJingTime.year : 0;
    current.month = beiJingTime.mon; current.day = beiJingTime.day;
    current.hour = beiJingTime.hour; current.minute = beiJingTime.min;
    current.second = beiJingTime.sec;
    current.latitude = deg_lat; current.longitude = deg_lon;
    current.altitude = info.elv; current.speed = info.speed;
    current.course = info.direction; current.hdop = info.HDOP; current.pdop = info.PDOP;

    if(display_flag != 0U) {
        display_flag = 0U; GTP_IRQ_Disable(); snapshot_valid = 0U;
        ui_shell("\316\300\320\307\266\250\316\273", "\261\276\273\372 GNSS / \266\250\316\273\323\353\312\261\274\344", "\271\244\276\337");
        ui_round_rect(24U,96U,480U,140U,12U,UI_SURFACE);
        ui_round_rect(520U,96U,256U,140U,12U,UI_SURFACE);
        ui_text(40U,108U,36U,"\276\255\316\263\266\310 / WGS84",UI_MUTED,UI_SURFACE,0U);
        ui_text(536U,108U,28U,"\266\250\316\273\327\264\314\254",UI_MUTED,UI_SURFACE,0U);
        for(i=0U;i<3U;i++) {
            uint16_t x = 24U + 256U*i;
            ui_round_rect(x,248U,240U,96U,12U,UI_SURFACE);
            ui_round_rect(x,356U,240U,76U,12U,UI_SURFACE);
        }
        ui_text(40U,258U,26U,"GPS / \312\271\323\303 : \277\311\274\373",UI_MUTED,UI_SURFACE,0U);
        ui_text(296U,258U,26U,"\261\261\266\267 / \312\271\323\303 : \277\311\274\373",UI_MUTED,UI_SURFACE,0U);
        ui_text(552U,258U,26U,"\316\300\320\307\312\261\274\344 / UTC+8",UI_MUTED,UI_SURFACE,0U);
        ui_text(40U,366U,26U,"\272\243\260\316 / m",UI_MUTED,UI_SURFACE,0U);
        ui_text(296U,366U,26U,"\265\330\303\346\313\331\266\310 / km/h",UI_MUTED,UI_SURFACE,0U);
        ui_text(552U,366U,26U,"\325\346\272\275\317\362 / deg",UI_MUTED,UI_SURFACE,0U);
        ui_footer("BACK \267\265\273\330", "\261\276\273\372 GNSS");
    }
    if(snapshot_valid && memcmp(&current,&previous,sizeof(current)) == 0) return;
    previous = current; snapshot_valid = 1U;
    position_valid = current.sig > 0 && current.fix >= 2 &&
        current.latitude >= -90.0 && current.latitude <= 90.0 &&
        current.longitude >= -180.0 && current.longitude <= 180.0;
    time_valid = current.year >= 100 && current.month >= 1 && current.month <= 12 &&
        current.day >= 1 && current.day <= 31 && current.hour >= 0 && current.hour <= 23 &&
        current.minute >= 0 && current.minute <= 59 && current.second >= 0 && current.second <= 60;
    color = position_valid ? UI_GREEN : UI_MUTED;
    fix_text = !position_valid ? "\316\264\266\250\316\273" : current.sig == 2 ? "\262\356\267\326\266\250\316\273" :
        current.fix >= 3 ? "3D \266\250\316\273" : "2D \266\250\316\273";
    if(position_valid) {
        latitude = current.latitude < 0.0 ? -current.latitude : current.latitude;
        longitude = current.longitude < 0.0 ? -current.longitude : current.longitude;
        snprintf(text,sizeof(text),"LAT  %c %10.6f",current.latitude < 0.0 ? 'S' : 'N',latitude);
        ui_text(40U,132U,28U,text,UI_INK,UI_SURFACE,1U);
        snprintf(text,sizeof(text),"LON  %c %10.6f",current.longitude < 0.0 ? 'W' : 'E',longitude);
        ui_text(40U,180U,28U,text,UI_INK,UI_SURFACE,1U);
    } else {
        ui_text(40U,132U,28U,"\316\263\266\310 -- \265\310\264\375\323\320\320\247\266\250\316\273",UI_MUTED,UI_SURFACE,1U);
        ui_text(40U,180U,28U,"\276\255\266\310 -- \265\310\264\375\323\320\320\247\266\250\316\273",UI_MUTED,UI_SURFACE,1U);
    }
    ui_text(536U,132U,14U,fix_text,color,UI_SURFACE,1U);
    ui_text(536U,178U,28U,fresh ? "\322\321\312\325\265\275 NMEA \312\375\276\335" : "\265\310\264\375 NMEA \312\375\276\335",UI_MUTED,UI_SURFACE,0U);
    if(position_valid) snprintf(text,sizeof(text),"HDOP %.2f  PDOP %.2f",current.hdop,current.pdop);
    else strcpy(text,"HDOP --    PDOP --");
    ui_text(536U,204U,28U,text,UI_MUTED,UI_SURFACE,0U);
    for(i=0U;i<2U;i++) {
        uint16_t x = 40U + 256U*i;
        int used = i ? current.bds_inuse : current.gps_inuse;
        int view = i ? current.bds_inview : current.gps_inview;
        uint16_t width = used <= 0 ? 0U : used >= 12 ? 208U : (uint16_t)(used*208/12);
        if(fresh) snprintf(text,sizeof(text),"%02d : %02d",used,view);
        else strcpy(text,"-- : --");
        ui_text(x,278U,13U,text,fresh ? UI_INK : UI_MUTED,UI_SURFACE,1U);
        ui_fill(x,324U,208U,6U,UI_TRACK);
        ui_fill(x,324U,width,6U,i ? UI_GREEN : UI_ACCENT);
    }
    if(time_valid) snprintf(text,sizeof(text),"%02d:%02d:%02d",current.hour,current.minute,current.second);
    else strcpy(text,"\265\310\264\375\326\320");
    ui_text(552U,274U,13U,text,time_valid ? UI_INK : UI_MUTED,UI_SURFACE,1U);
    if(time_valid) snprintf(text,sizeof(text),"%04d-%02d-%02d",current.year+1900,current.month,current.day);
    else strcpy(text,"\324\335\316\336\323\320\320\247\316\300\320\307\312\261\274\344");
    ui_text(552U,320U,26U,text,UI_MUTED,UI_SURFACE,0U);
    for(i=0U;i<3U;i++) {
        double value = i == 0U ? current.altitude : i == 1U ? current.speed : current.course;
        if(position_valid && value > -1000000.0 && value < 1000000.0)
            snprintf(text,sizeof(text),"%.2f",value);
        else strcpy(text,"--");
        ui_text(40U+256U*i,390U,13U,text,position_valid ? UI_INK : UI_MUTED,UI_SURFACE,1U);
    }
}

void channel_monitor_page(void)
{
    /* Battery and the joystick button are intentionally outside the axis table. */
    static const uint8_t order[8] = {0U,1U,2U,3U,4U,5U,7U,8U};
    static const char * const labels[8] = {
        "A01 \320\375\305\245", "A02 \327\363\322\241\270\313 Y", "A03 \327\363\322\241\270\313 X", "A04 \320\375\305\245",
        "A05 \323\322\322\241\270\313 Y", "A06 \323\322\322\241\270\313 X", "A07 \273\254\314\365", "A08 \270\250\326\372\312\344\310\353"
    };
    static GuiAnalogFilter filters[10];
    static uint16_t previous_value[10];
    static int16_t previous_bar[8];
    uint16_t values[10], x, y;
    uint8_t i, channel, first = display_flag != 0U;
    int16_t normalized;
    if(first) {
        display_flag = 0U;
        GTP_IRQ_Disable();
        ui_shell("\315\250\265\300\274\340\312\323", "", "\322\243\277\330 / \324\255\312\274\312\344\310\353");
        gui_monitor_tabs(0U);
        ui_round_rect(24U,104U,368U,266U,12U,UI_SURFACE);
        ui_round_rect(408U,104U,368U,266U,12U,UI_SURFACE);
        ui_text(40U,116U,24U,"\304\243\304\342\312\344\310\353 / A01 - A04",UI_MUTED,UI_SURFACE,0U);
        ui_text(424U,116U,24U,"\304\243\304\342\312\344\310\353 / A05 - A08",UI_MUTED,UI_SURFACE,0U);
        ui_text(320U,116U,6U,"12 \316\273",UI_MUTED,UI_SURFACE,0U);
        ui_text(704U,116U,6U,"12 \316\273",UI_MUTED,UI_SURFACE,0U);
        ui_round_rect(24U,382U,368U,50U,10U,UI_SURFACE);
        ui_round_rect(408U,382U,368U,50U,10U,UI_SURFACE);
        ui_text(40U,390U,26U,"A09 \322\241\270\313\260\264\321\271",UI_INK,UI_SURFACE,0U);
        ui_text(40U,410U,26U,"ADC \262\311\321\371 / \311\317\300\255\312\344\310\353",UI_MUTED,UI_SURFACE,0U);
        ui_text(424U,390U,26U,"BAT \261\276\273\372\265\347\263\330",UI_INK,UI_SURFACE,0U);
        ui_text(424U,410U,26U,"ADC \262\311\321\371 / \261\276\273\372\265\347\263\330",UI_MUTED,UI_SURFACE,0U);
        ui_footer("\311\317/\317\302 / OK \307\320\273\273\312\323\315\274", "BACK \267\265\273\330\322\243\277\330\262\313\265\245");
    }
    /* Keep the established display-only filter and live frame cadence. */
    for(i = 0U; i < 7U; i++)
        values[i] = gui_analog_filter_update(&filters[i],ADC1_Value[i],first);
    for(i = 0U; i < 3U; i++)
        values[i + 7U] = gui_analog_filter_update(&filters[i + 7U],ADC3_Value[i],first);
    for(i = 0U; i < 8U; i++) {
        channel = order[i];
        x = i < 4U ? 40U : 424U;
        y = 146U + (uint16_t)(i % 4U) * 54U;
        if(first) ui_text(x,y + 6U,18U,labels[i],UI_INK,UI_SURFACE,0U);
        if(first || values[channel] != previous_value[channel]) {
            snprintf(displayBuffer,sizeof(displayBuffer),"%4u",values[channel]);
            ui_text(x + 264U,y,4U,displayBuffer,UI_ACCENT,UI_SURFACE,1U);
            previous_value[channel] = values[channel];
        }
        /* The raw center means ADC midpoint, not a calibrated control value. */
        normalized = (int16_t)((int32_t)values[channel] * 2000L / 4095L - 1000L);
        ui_bipolar_bar(x,y + 36U,328U,8U,normalized,&previous_bar[i],first);
    }
    if(first || values[9] != previous_value[9]) {
        snprintf(displayBuffer,sizeof(displayBuffer),"%4u",values[9]);
        ui_text(304U,392U,4U,displayBuffer,UI_ACCENT,UI_SURFACE,1U);
        previous_value[9] = values[9];
    }
    if(first || values[6] != previous_value[6]) {
        snprintf(displayBuffer,sizeof(displayBuffer),"%4u",values[6]);
        ui_text(688U,392U,4U,displayBuffer,UI_ACCENT,UI_SURFACE,1U);
        previous_value[6] = values[6];
    }
    LCD_SetFont(&Font16x32); LCD_SetBackColor(UI_BG); LCD_SetTextColor(UI_INK);
}

/* Input values come from the same sample and normalization as the control task.
 * TX fields show the last packet queued to the radio, never a GUI reconstruction. */
void channel_output_monitor_page(const ControlLinkSnapshot *snapshot)
{
    static const char * const names[6] = {"A01", "A02 Y", "A03 X", "A04", "A05", "A06 YAW"};
    static ControlLinkSnapshot previous;
    static uint8_t previous_reverse[6];
    static int16_t bar_position[6];
    static uint32_t text_ms;
    unsigned long ticks;
    uint8_t i, first = display_flag != 0U, update_text, live;
    uint16_t y, state_color;
    int16_t value;
    const char *state;
    if(!snapshot) return;
    get_tick_count(&ticks);
    update_text = first || (uint32_t)((uint32_t)ticks - text_ms) >= 100U;
    if(first) {
        display_flag = 0U;
        GTP_IRQ_Disable();
        ui_shell("\315\250\265\300\274\340\312\323", "", "\322\243\277\330 / \312\265\274\312\267\242\313\315");
        gui_monitor_tabs(1U);
        ui_round_rect(24U,104U,424U,284U,12U,UI_SURFACE);
        ui_round_rect(464U,104U,312U,284U,12U,UI_SURFACE);
        ui_text(40U,118U,8U,"\312\344\310\353",UI_MUTED,UI_SURFACE,0U);
        ui_text(128U,118U,6U,"\324\255\312\274\326\265",UI_MUTED,UI_SURFACE,0U);
        ui_text(224U,118U,12U,"\320\243\327\274\326\265",UI_MUTED,UI_SURFACE,0U);
        ui_text(400U,118U,4U,"\267\264\317\362",UI_MUTED,UI_SURFACE,0U);
        ui_text(480U,118U,35U,"\327\356\275\374\314\341\275\273\265\304\267\242\313\315\326\241 / RC v1",UI_MUTED,UI_SURFACE,0U);
        ui_fill(40U,142U,392U,1U,UI_LINE);
        ui_fill(480U,180U,280U,1U,UI_LINE);
        ui_text(480U,192U,7U,"X / Y",UI_MUTED,UI_SURFACE,0U);
        ui_text(480U,232U,9U,"\304\277\261\352\272\275\317\362",UI_MUTED,UI_SURFACE,0U);
        ui_text(480U,258U,9U,"\313\331\266\310\311\317\317\336",UI_MUTED,UI_SURFACE,0U);
        ui_fill(480U,284U,280U,1U,UI_LINE);
        ui_text(480U,368U,35U,"\323\262\274\376\273\330\326\264\262\273\264\372\261\355\273\372\306\367\310\313\326\264\320\320\275\341\271\373",UI_MUTED,UI_SURFACE,0U);
        ui_text(24U,418U,64U,"\315\250\265\300\323\263\311\344: X=-A03  Y=A02  \272\275\317\362=-A06",UI_MUTED,UI_BG,0U);
        ui_text(632U,418U,18U,"\326\273\266\301\274\340\312\323",UI_MUTED,UI_BG,0U);
        ui_footer("\311\317/\317\302 / OK \307\320\273\273\312\323\315\274", "BACK \267\265\273\330\322\243\277\330\262\313\265\245");
    }
    for(i = 0U; i < 6U; i++) {
        y = 150U + (uint16_t)i * 38U;
        value = snapshot->calibrated[i];
        if(value > 1000) value = 1000;
        if(value < -1000) value = -1000;
        if(first) ui_text(40U,y,9U,names[i],UI_INK,UI_SURFACE,0U);
        if(update_text && (first || snapshot->raw[i] != previous.raw[i] ||
            value != previous.calibrated[i] || param.chReverse[i] != previous_reverse[i])) {
            snprintf(displayBuffer,sizeof(displayBuffer),"%4u",snapshot->raw[i]);
            ui_text(128U,y,5U,displayBuffer,UI_MUTED,UI_SURFACE,0U);
            snprintf(displayBuffer,sizeof(displayBuffer),"%c%3u.%u%%",value < 0 ? '-' : '+',
                (unsigned)(value < 0 ? -value : value) / 10U,
                (unsigned)(value < 0 ? -value : value) % 10U);
            ui_text(224U,y,9U,displayBuffer,UI_ACCENT,UI_SURFACE,0U);
            ui_text(400U,y,4U,param.chReverse[i] ? "\277\252\306\364" : "--",
                param.chReverse[i] ? UI_ACCENT : UI_MUTED,UI_SURFACE,0U);
            previous.raw[i] = snapshot->raw[i];
            previous.calibrated[i] = value;
            previous_reverse[i] = param.chReverse[i];
        }
        ui_bipolar_bar(128U,y + 22U,296U,7U,value,&bar_position[i],first);
    }
    if(update_text) {
        live = snapshot->sent && snapshot->tx_age_ms <= 100U;
        state = !snapshot->sent ? "\311\320\316\264\267\242\313\315" : !live ? "\267\242\313\315\326\241\322\321\271\375\306\332" :
            snapshot->transmitted.armed ? "\324\312\320\355\324\313\266\257" : "\324\313\266\257\322\321\275\373\326\271";
        state_color = !live ? UI_AMBER : snapshot->transmitted.armed ? UI_GREEN : UI_MUTED;
        ui_text(480U,148U,17U,state,state_color,UI_SURFACE,1U);
        if(snapshot->sent)
            snprintf(displayBuffer,sizeof(displayBuffer),"%+5d %+5d",snapshot->transmitted.x,snapshot->transmitted.y);
        else snprintf(displayBuffer,sizeof(displayBuffer),"  --     --");
        ui_text(576U,184U,11U,displayBuffer,UI_INK,UI_SURFACE,1U);
        if(snapshot->sent)
            snprintf(displayBuffer,sizeof(displayBuffer),"%+5d x0.1 deg",snapshot->transmitted.heading);
        else snprintf(displayBuffer,sizeof(displayBuffer),"--");
        ui_text(584U,232U,22U,displayBuffer,UI_INK,UI_SURFACE,0U);
        if(snapshot->sent)
            snprintf(displayBuffer,sizeof(displayBuffer),"%4u  DCH:0x%02X",snapshot->transmitted.limit,snapshot->transmitted.digital);
        else snprintf(displayBuffer,sizeof(displayBuffer),"--   DCH:--");
        ui_text(584U,258U,22U,displayBuffer,UI_INK,UI_SURFACE,0U);
        if(snapshot->sent)
            snprintf(displayBuffer,sizeof(displayBuffer),"SEQ %5u    FRAME AGE %5u ms",snapshot->transmitted.sequence,snapshot->tx_age_ms);
        else snprintf(displayBuffer,sizeof(displayBuffer),"SEQ --       FRAME AGE --");
        ui_text(480U,290U,35U,displayBuffer,UI_MUTED,UI_SURFACE,0U);
        if(snapshot->ack_seen)
            snprintf(displayBuffer,sizeof(displayBuffer),"RADIO ACK   %5u ms ago",snapshot->ack_age_ms);
        else snprintf(displayBuffer,sizeof(displayBuffer),"RADIO ACK   NONE");
        ui_text(480U,310U,35U,displayBuffer,snapshot->ack_seen ? UI_GREEN : UI_AMBER,UI_SURFACE,0U);
        snprintf(displayBuffer,sizeof(displayBuffer),"TX %10lu   ACK %10lu",(unsigned long)snapshot->tx_started,(unsigned long)snapshot->tx_acked);
        ui_text(480U,330U,35U,displayBuffer,UI_MUTED,UI_SURFACE,0U);
        snprintf(displayBuffer,sizeof(displayBuffer),"FAILED %10lu",(unsigned long)snapshot->tx_failed);
        ui_text(480U,348U,35U,displayBuffer,UI_MUTED,UI_SURFACE,0U);
        live = snapshot->sampled && snapshot->input_fresh && snapshot->sample_age_ms <= 100U;
        if(snapshot->sampled)
            snprintf(displayBuffer,sizeof(displayBuffer),"INPUT %-5s  AGE %5u ms",live ? "LIVE" : "STALE",snapshot->sample_age_ms);
        else snprintf(displayBuffer,sizeof(displayBuffer),"INPUT NO SAMPLE");
        ui_text(24U,396U,34U,displayBuffer,live ? UI_GREEN : UI_AMBER,UI_BG,0U);
        ui_text(328U,396U,56U,gui_control_status_text(control_link_status()),UI_MUTED,UI_BG,0U);
        text_ms = (uint32_t)ticks;
    }
    LCD_SetFont(&Font16x32); LCD_SetBackColor(UI_BG); LCD_SetTextColor(UI_INK);
}

void imu6050_information(void)
{
    static const char *names[3] = {"\270\251\321\366 PITCH", "\272\341\271\366 ROLL", "\272\275\317\362 YAW"};
    static const float limits[3] = {90.0f, 180.0f, 180.0f};
    static const uint16_t colors[3] = {UI_ACCENT, UI_GREEN, UI_AMBER};
    static int16_t old_bar[3];
    static char last_angles[3][16], last_temp[16], last_acc[64], last_gyro[64];
    static uint8_t last_valid = 0xffU;
    static uint32_t last_numeric_ms;
    unsigned long ticks;
    uint32_t now;
    uint8_t first_draw = display_flag != 0U, valid, state_changed, i;
    uint16_t x;
    int16_t meter;
    float angles[3], limited;
    char text[64], angle[16];

    get_tick_count(&ticks);
    now = (uint32_t)ticks;
    angles[0] = pitch; angles[1] = roll; angles[2] = yaw;
    valid = imu_data_valid ? 1U : 0U;
    for(i = 0U; i < 3U; i++)
        if(!(angles[i] >= -3600.0f && angles[i] <= 3600.0f)) valid = 0U;
    state_changed = first_draw || valid != last_valid;
    if(!state_changed && (uint32_t)(now - last_numeric_ms) < 100U) return;
    last_numeric_ms = now;
    if(first_draw) {
        display_flag = 0U;
        GTP_IRQ_Disable();
        ui_shell("\261\276\273\372\327\313\314\254", "MPU6050 / \261\276\273\372\264\253\270\320\306\367", "\271\244\276\337 / \264\253\270\320\306\367");
        for(i = 0U; i < 3U; i++) {
            x = 24U + (uint16_t)i * 256U;
            ui_round_rect(x, 104U, 240U, 204U, 14U, UI_SURFACE);
            ui_text(x + 16U, 120U, 26U, names[i], colors[i], UI_SURFACE, 0U);
            ui_text(x + 16U, 212U, 26U, "\275\307\266\310 / deg", UI_MUTED, UI_SURFACE, 0U);
            snprintf(text, sizeof(text), "-%3.0f       0       +%3.0f", limits[i], limits[i]);
            ui_text(x + 16U, 278U, 26U, text, UI_MUTED, UI_SURFACE, 0U);
        }
        ui_round_rect(24U, 320U, 752U, 112U, 14U, UI_SURFACE);
        ui_text(40U, 332U, 14U, "\264\253\270\320\306\367\327\264\314\254", UI_MUTED, UI_SURFACE, 0U);
        ui_text(432U, 332U, 20U, "\316\302\266\310 / C", UI_MUTED, UI_SURFACE, 0U);
        ui_footer("BACK \267\265\273\330", "\261\276\273\372\264\253\270\320\306\367 / 100 ms");
    }
    for(i = 0U; i < 3U; i++) {
        x = 24U + (uint16_t)i * 256U;
        if(valid) snprintf(angle, sizeof(angle), "%+6.1f", angles[i]);
        else strcpy(angle, "--");
        if(state_changed || strcmp(angle, last_angles[i]) != 0) {
            ui_text(x + 16U, 152U, 8U, angle, valid ? UI_INK : UI_MUTED, UI_SURFACE, 2U);
            strcpy(last_angles[i], angle);
        }
        limited = angles[i];
        if(limited < -limits[i]) limited = -limits[i];
        if(limited > limits[i]) limited = limits[i];
        meter = valid ? (int16_t)(limited * 1000.0f / limits[i]) : 0;
        ui_bipolar_bar(x + 16U, 248U, 208U, 10U, meter, &old_bar[i], state_changed);
    }
    if(state_changed) {
        ui_text(168U, 332U, 25U, valid ? "\312\375\276\335\323\320\320\247 / DMP \276\315\320\367" : "\265\310\264\375\323\320\320\247\262\311\321\371",
                valid ? UI_GREEN : UI_AMBER, UI_SURFACE, 0U);
    }
    if(valid) snprintf(angle, sizeof(angle), "%5.1f", (float)temp / 100.0f);
    else strcpy(angle, "--");
    if(state_changed || strcmp(angle, last_temp) != 0) {
        ui_text(620U, 326U, 8U, angle, valid ? UI_INK : UI_MUTED, UI_SURFACE, 1U);
        strcpy(last_temp, angle);
    }
    if(valid) snprintf(text, sizeof(text), "ACC RAW   X %+6d     Y %+6d     Z %+6d", aacx, aacy, aacz);
    else strcpy(text, "ACC RAW   X --         Y --         Z --");
    if(state_changed || strcmp(text, last_acc) != 0) {
        ui_text(40U, 372U, 90U, text, UI_MUTED, UI_SURFACE, 0U);
        strcpy(last_acc, text);
    }
    if(valid) snprintf(text, sizeof(text), "GYRO RAW  X %+6d     Y %+6d     Z %+6d", gyrox, gyroy, gyroz);
    else strcpy(text, "GYRO RAW  X --         Y --         Z --");
    if(state_changed || strcmp(text, last_gyro) != 0) {
        ui_text(40U, 404U, 90U, text, UI_MUTED, UI_SURFACE, 0U);
        strcpy(last_gyro, text);
    }
    last_valid = valid;
}

/* CHANNEL MONITOR mapping (zero-based ADC1_Value indices):
 * left X=CH03, left Y=CH02; right X=CH06, right Y=CH05.
 * Both horizontal axes require display/control direction correction. */
#define ROBOT_LEFT_X_ADC_INDEX  2U
#define ROBOT_LEFT_Y_ADC_INDEX  1U
#define ROBOT_RIGHT_X_ADC_INDEX 5U
#define ROBOT_RIGHT_Y_ADC_INDEX 4U

static int16_t gui_robot_stick_value(uint16_t raw, uint8_t channel)
{
    uint16_t lower = param.chLower[channel];
    uint16_t middle = param.chMiddle[channel];
    uint16_t upper = param.chUpper[channel];
    uint16_t range;
    int32_t value;
    if((lower >= middle) || (middle >= upper)) {
        lower = 0U; middle = 2047U; upper = 4095U;
    }
    if(raw >= middle) {
        range = upper - middle;
        value = ((int32_t)raw - middle) * 1000L / range;
    } else {
        range = middle - lower;
        value = -((int32_t)middle - raw) * 1000L / range;
    }
    value += param.PWMadjustValue[channel];
    if(value > 1000L) value = 1000L;
    if(value < -1000L) value = -1000L;
    if(param.chReverse[channel] != 0U) value = -value;
    if(value >= -50L && value <= 50L) value = 0L;
    return (int16_t)value;
}

/* Padded, single-line text never crosses its card or wraps into another row. */
/* Commit final pixels directly: old/new overlapping marker pixels never go
 * through a blank intermediate frame. The static circle is outside this patch. */
static void gui_robot_dot_patch(uint16_t center_x, uint16_t center_y,
    uint16_t patch_x, uint16_t patch_y, uint16_t dot_x, uint16_t dot_y,
    uint16_t color)
{
    static uint16_t pixels[17U * 17U];
    uint16_t row, column, x, y, pixel;
    int32_t dx, dy;
    for(row = 0U; row < 17U; row++) {
        y = patch_y + row;
        for(column = 0U; column < 17U; column++) {
            x = patch_x + column;
            pixel = (x == center_x || y == center_y) ? UI_LINE : UI_SURFACE;
            dx = (int32_t)x - dot_x; dy = (int32_t)y - dot_y;
            if(dx * dx + dy * dy <= 64L) pixel = color;
            pixels[row * 17U + column] = pixel;
        }
    }
    LCD_BlitRGB565(patch_x,patch_y,17U,17U,pixels);
}

static void gui_robot_draw_stick(uint16_t center_x, uint16_t center_y,
    uint16_t raw_x, uint16_t raw_y, int16_t control_x, int16_t control_y,
    uint16_t color, uint16_t *old_x, uint16_t *old_y,
    uint16_t *old_raw_x, uint16_t *old_raw_y,
    uint8_t draw_text, uint8_t first_draw)
{
    int32_t dx = (int32_t)control_x * 44L / 1000L;
    int32_t dy = -(int32_t)control_y * 44L / 1000L;
    int32_t radius2 = dx * dx + dy * dy;
    uint16_t norm = 45U, dot_x, dot_y, text_x = center_x - 88U;
    int32_t move_x, move_y;
    uint8_t centered;
    if(radius2 > 44L * 44L) {
        while((int32_t)norm * norm < radius2) norm++;
        dx = dx * 44L / norm; dy = dy * 44L / norm;
    }
    dot_x = (uint16_t)((int32_t)center_x + dx);
    dot_y = (uint16_t)((int32_t)center_y + dy);
    move_x = (int32_t)dot_x - *old_x; move_y = (int32_t)dot_y - *old_y;
    centered = control_x == 0 && control_y == 0;
    if(first_draw) {
        LCD_SetTextColor(UI_LINE);
        ILI9806G_DrawLine(center_x - 56U,center_y,center_x + 56U,center_y);
        ILI9806G_DrawLine(center_x,center_y - 56U,center_x,center_y + 56U);
        ILI9806G_DrawCircle(center_x,center_y,56U,0U);
        /* The corner ticks give both sticks the same instrument scale. */
        LCD_SetTextColor(color);
        ILI9806G_DrawLine(center_x - 62U,center_y,center_x - 57U,center_y);
        ILI9806G_DrawLine(center_x + 57U,center_y,center_x + 62U,center_y);
        ILI9806G_DrawLine(center_x,center_y - 62U,center_x,center_y - 57U);
        ILI9806G_DrawLine(center_x,center_y + 57U,center_x,center_y + 62U);
    }
    if(first_draw || move_x >= 2L || move_x <= -2L || move_y >= 2L || move_y <= -2L ||
        (centered && (move_x || move_y))) {
        gui_robot_dot_patch(center_x,center_y,dot_x - 8U,dot_y - 8U,dot_x,dot_y,color);
        if(!first_draw)
            gui_robot_dot_patch(center_x,center_y,*old_x - 8U,*old_y - 8U,dot_x,dot_y,color);
        *old_x = dot_x; *old_y = dot_y;
    }
    if(!first_draw && (!draw_text || (raw_x == *old_raw_x && raw_y == *old_raw_y))) return;
    snprintf(displayBuffer,sizeof(displayBuffer),"X%+5d Y%+5d",control_x,control_y);
    ui_text(text_x,306U,22U,displayBuffer,color,UI_SURFACE,0U);
    snprintf(displayBuffer,sizeof(displayBuffer),"ADC X:%4u Y:%4u",raw_x,raw_y);
    ui_text(text_x,326U,22U,displayBuffer,UI_MUTED,UI_SURFACE,0U);
    *old_raw_x = raw_x; *old_raw_y = raw_y;
}

void robot_control_page(const GuiRobotTelemetry *telemetry)
{
    static GuiRobotTelemetry previous;
    static uint8_t snapshot_valid;
    static uint16_t old_dot_x[2], old_dot_y[2], old_raw_x[2], old_raw_y[2];
    static GuiRobotFilter filters[4];
    static uint32_t last_numeric_ms;
    static char previous_status[48];
    unsigned long ticks;
    uint32_t now;
    uint8_t draw_text, first_draw = display_flag != 0U, telemetry_changed, battery;
    uint16_t raw[4], state_color;
    int16_t left_x, left_y, right_x, right_y;
    const char *fix_text, *status, *state_label;
    char number[5][12];
    if(!telemetry) return;
    get_tick_count(&ticks); now = (uint32_t)ticks;
    if(first_draw) {
        display_flag = 0U; GTP_IRQ_Disable();
        snapshot_valid = 0U;
        old_raw_x[0] = old_raw_x[1] = old_raw_y[0] = old_raw_y[1] = 0xffffU;
        previous_status[0] = '\0';
        ui_shell("\273\372\306\367\310\313\277\330\326\306", "\322\241\270\313\312\344\310\353 / \273\372\306\367\310\313\322\243\262\342", "\322\243\277\330 / \277\330\326\306\314\250");
        ui_round_rect(24U,96U,752U,40U,10U,UI_SURFACE);
        ui_round_rect(24U,148U,208U,200U,12U,UI_SURFACE);
        ui_round_rect(568U,148U,208U,200U,12U,UI_SURFACE);
        ui_round_rect(248U,148U,304U,110U,12U,UI_SURFACE);
        ui_round_rect(248U,270U,144U,78U,12U,UI_SURFACE);
        ui_round_rect(408U,270U,144U,78U,12U,UI_SURFACE);
        ui_text(40U,158U,22U,"\327\363\322\241\270\313 / \306\275\322\306",UI_ACCENT,UI_SURFACE,0U);
        ui_text(584U,158U,22U,"\323\322\322\241\270\313 / \272\275\317\362",UI_GREEN,UI_SURFACE,0U);
        ui_text(268U,160U,32U,"\273\372\306\367\310\313\313\331\266\310",UI_MUTED,UI_SURFACE,0U);
        ui_text(496U,214U,4U,"m/s",UI_MUTED,UI_SURFACE,0U);
        ui_text(264U,282U,15U,"\273\372\306\367\310\313\265\347\321\271",UI_MUTED,UI_SURFACE,0U);
        ui_text(424U,282U,14U,"\272\275\317\362 / deg",UI_MUTED,UI_SURFACE,0U);
        ui_round_rect(24U,360U,752U,76U,10U,UI_SURFACE);
        ui_footer("\276\315\320\367\272\363\260\264\327\241 DCH1 \312\271\304\334\324\313\266\257", "BACK \263\267\317\372\312\271\304\334\262\242\267\265\273\330");
    }
    /* Preserve the existing wall-clock gate and display-only joystick filter. */
    draw_text = first_draw || (uint32_t)(now - last_numeric_ms) >= 250U;
    if(draw_text) last_numeric_ms = now;
    telemetry_changed = !snapshot_valid || previous.link_online != telemetry->link_online ||
        (telemetry->link_online && draw_text && memcmp(&previous,telemetry,sizeof(previous)) != 0);
    if(telemetry_changed) {
        previous = *telemetry; snapshot_valid = 1U;
        if(!telemetry->link_online) {
            ui_text(280U,184U,8U,"--",UI_MUTED,UI_SURFACE,2U);
            ui_text(268U,238U,32U,"\311\320\316\336\273\372\306\367\310\313\322\243\262\342",UI_AMBER,UI_SURFACE,0U);
            ui_text(264U,306U,7U,"--",UI_MUTED,UI_SURFACE,1U);
            ui_text(424U,306U,7U,"--",UI_MUTED,UI_SURFACE,1U);
            ui_text(40U,364U,90U,"\273\372\306\367\310\313\322\243\262\342 / \265\310\264\375\275\323\312\325\266\313\273\330\264\253",UI_AMBER,UI_SURFACE,0U);
            ui_text(40U,382U,90U,"POS X --   Y --   Z --       ROLL --   PITCH --",UI_MUTED,UI_SURFACE,0U);
            ui_text(40U,400U,90U,"ACC X --   Y --   Z --       BAT --   GPS --   SAT --",UI_MUTED,UI_SURFACE,0U);
            ui_text(40U,418U,90U,"LAT --          LON --          ALT --",UI_MUTED,UI_SURFACE,0U);
        } else {
            battery = telemetry->battery_percent > 100U ? 100U : telemetry->battery_percent;
            fix_text = telemetry->gps_fix >= 3U ? "3D" : telemetry->gps_fix == 2U ? "2D" : "NO";
            gui_robot_format_value(number[0],sizeof(number[0]),8U,"%6.2f",telemetry->speed_mps);
            ui_text(280U,184U,8U,number[0],UI_ACCENT,UI_SURFACE,2U);
            ui_text(268U,238U,32U,"\273\372\306\367\310\313\322\243\262\342 / \323\320\320\247",UI_GREEN,UI_SURFACE,0U);
            gui_robot_format_value(number[0],sizeof(number[0]),5U,"%5.2f",telemetry->voltage_v);
            snprintf(displayBuffer,sizeof(displayBuffer),"%s V",number[0]);
            ui_text(264U,306U,7U,displayBuffer,UI_INK,UI_SURFACE,1U);
            gui_robot_format_value(number[0],sizeof(number[0]),7U,"%6.1f",telemetry->yaw_deg);
            ui_text(424U,306U,7U,number[0],UI_INK,UI_SURFACE,1U);
            snprintf(displayBuffer,sizeof(displayBuffer),"\273\372\306\367\310\313\322\243\262\342 / \323\320\320\247  RX %10lu  \321\323\263\331 %5u ms",
                (unsigned long)telemetry->packet_count,telemetry->packet_age_ms);
            ui_text(40U,364U,90U,displayBuffer,UI_GREEN,UI_SURFACE,0U);
            gui_robot_format_value(number[0],sizeof(number[0]),11U,"%+7.2f",telemetry->position_x_m);
            gui_robot_format_value(number[1],sizeof(number[1]),11U,"%+7.2f",telemetry->position_y_m);
            gui_robot_format_value(number[2],sizeof(number[2]),11U,"%+7.2f",telemetry->position_z_m);
            gui_robot_format_value(number[3],sizeof(number[3]),7U,"%+6.1f",telemetry->roll_deg);
            gui_robot_format_value(number[4],sizeof(number[4]),7U,"%+6.1f",telemetry->pitch_deg);
            snprintf(displayBuffer,sizeof(displayBuffer),"POS X%7s Y%7s Z%7s m     ROLL%6s PITCH%6s deg",
                number[0],number[1],number[2],number[3],number[4]);
            ui_text(40U,382U,90U,displayBuffer,UI_MUTED,UI_SURFACE,0U);
            gui_robot_format_value(number[0],sizeof(number[0]),7U,"%+6.2f",telemetry->acceleration_x_mps2);
            gui_robot_format_value(number[1],sizeof(number[1]),7U,"%+6.2f",telemetry->acceleration_y_mps2);
            gui_robot_format_value(number[2],sizeof(number[2]),7U,"%+6.2f",telemetry->acceleration_z_mps2);
            snprintf(displayBuffer,sizeof(displayBuffer),"ACC X%6s Y%6s Z%6s m/s2    BAT %3u%%   GPS %s   SAT %02u",
                number[0],number[1],number[2],battery,fix_text,telemetry->satellites);
            ui_text(40U,400U,90U,displayBuffer,UI_MUTED,UI_SURFACE,0U);
            gui_robot_format_value(number[0],sizeof(number[0]),10U,"%+10.5f",telemetry->latitude_deg);
            gui_robot_format_value(number[1],sizeof(number[1]),11U,"%+11.5f",telemetry->longitude_deg);
            gui_robot_format_value(number[2],sizeof(number[2]),10U,"%+8.1f",telemetry->gps_altitude_m);
            snprintf(displayBuffer,sizeof(displayBuffer),"LAT %10s    LON %11s    ALT %8s m",
                number[0],number[1],number[2]);
            ui_text(40U,418U,90U,displayBuffer,UI_MUTED,UI_SURFACE,0U);
        }
    }
    status = control_link_status();
    if(first_draw || strcmp(previous_status,status)) {
        snprintf(previous_status,sizeof(previous_status),"%s",status);
        if(strncmp(status,"ENABLED",7U) == 0) {
            state_label = "\324\312\320\355\324\313\266\257"; state_color = UI_GREEN;
        } else if(strncmp(status,"READY",5U) == 0) {
            state_label = "\277\311\322\324\312\271\304\334"; state_color = UI_ACCENT;
        } else {
            state_label = "\324\313\266\257\322\321\275\373\326\271"; state_color = UI_AMBER;
        }
        ui_text(40U,108U,23U,state_label,state_color,UI_SURFACE,0U);
        ui_text(272U,108U,60U,gui_control_status_text(previous_status),UI_INK,UI_SURFACE,0U);
    }
    raw[0] = gui_robot_filter_update(&filters[0],ADC1_Value[ROBOT_LEFT_X_ADC_INDEX],first_draw);
    raw[1] = gui_robot_filter_update(&filters[1],ADC1_Value[ROBOT_LEFT_Y_ADC_INDEX],first_draw);
    raw[2] = gui_robot_filter_update(&filters[2],ADC1_Value[ROBOT_RIGHT_X_ADC_INDEX],first_draw);
    raw[3] = gui_robot_filter_update(&filters[3],ADC1_Value[ROBOT_RIGHT_Y_ADC_INDEX],first_draw);
    left_x = -gui_robot_stick_value(raw[0],ROBOT_LEFT_X_ADC_INDEX);
    left_y = gui_robot_stick_value(raw[1],ROBOT_LEFT_Y_ADC_INDEX);
    right_x = -gui_robot_stick_value(raw[2],ROBOT_RIGHT_X_ADC_INDEX);
    right_y = gui_robot_stick_value(raw[3],ROBOT_RIGHT_Y_ADC_INDEX);
    gui_robot_draw_stick(128U,238U,raw[0],raw[1],left_x,left_y,UI_ACCENT,
        &old_dot_x[0],&old_dot_y[0],&old_raw_x[0],&old_raw_y[0],draw_text,first_draw);
    gui_robot_draw_stick(672U,238U,raw[2],raw[3],right_x,right_y,UI_GREEN,
        &old_dot_x[1],&old_dot_y[1],&old_raw_x[1],&old_raw_y[1],draw_text,first_draw);
    LCD_SetFont(&Font16x32); LCD_SetBackColor(UI_BG); LCD_SetTextColor(UI_INK);
}

void calendar_page(const GuiCalendarState *state)
{
    static GuiCalendarState previous;
    static const char *weekdays[7] = {"\322\273","\266\376","\310\375","\313\304","\316\345","\301\371","\310\325"};
    static const char *actions[3] = {"\273\330\265\275\261\276\324\302","\311\350\326\303\310\325\306\332\312\261\274\344","GPS \320\243\312\261"};
    static const char *fields[6] = {"\304\352\267\335","\324\302\267\335","\310\325\306\332","\320\241\312\261","\267\326\326\323","\303\353\326\323"};
    static const char *messages[] = {"", "\310\325\306\332\312\261\274\344\322\321\261\243\264\346", "\322\321\260\264 GPS \261\261\276\251\312\261\274\344\320\243\327\274",
        "\324\335\316\336\323\320\320\247 GPS \312\261\274\344\243\254\307\353\265\310\264\375\273\362\312\326\266\257\311\350\326\303", "RTC \320\264\310\353\312\247\260\334\243\254\307\353\326\330\312\324", "\322\321\310\241\317\373\320\336\270\304"};
    uint8_t first = display_flag != 0U, month, old_month, grid_changed, sidebar_changed;
    uint8_t weekday, day, days, cell, row, column, today, editing_day, i;
    uint16_t year, old_year, x, y, background, color, value;
    char text[64];
    if(!state) return;
    year = state->mode == CALENDAR_EDIT ? state->draft.year : state->year;
    month = state->mode == CALENDAR_EDIT ? state->draft.month : state->month;
    old_year = previous.mode == CALENDAR_EDIT ? previous.draft.year : previous.year;
    old_month = previous.mode == CALENDAR_EDIT ? previous.draft.month : previous.month;
    days = RTC_CalendarDaysInMonth(year,month);
    if(!days) return;
    grid_changed = first || year != old_year || month != old_month ||
        state->mode != previous.mode || state->draft.day != previous.draft.day ||
        state->now.year != previous.now.year || state->now.month != previous.now.month ||
        state->now.day != previous.now.day || state->time_valid != previous.time_valid;
    sidebar_changed = first || state->mode != previous.mode || state->action != previous.action ||
        state->field != previous.field || memcmp(&state->draft,&previous.draft,sizeof(state->draft)) != 0 ||
        (state->mode != CALENDAR_EDIT && (memcmp(&state->now,&previous.now,sizeof(state->now)) != 0 ||
            state->readable != previous.readable || state->time_valid != previous.time_valid ||
            state->source != previous.source || state->gps_available != previous.gps_available));
    if(first) {
        display_flag=0U; GTP_IRQ_Disable();
        ui_shell("\310\325\300\372\323\353\312\261\326\323","\271\253\300\372 / \261\261\276\251\312\261\274\344 UTC+8","\311\350\326\303 / \310\325\300\372");
        ui_round_rect(24U,96U,488U,328U,12U,UI_SURFACE);
        ui_round_rect(536U,96U,240U,328U,12U,UI_SURFACE);
    }
    if(grid_changed) {
        ui_fill(40U,108U,456U,304U,UI_SURFACE);
        snprintf(text,sizeof(text),"%04u \304\352 %02u \324\302",year,month);
        ui_text(40U,108U,28U,text,UI_INK,UI_SURFACE,1U);
        for(i=0U;i<7U;i++)
            ui_text(60U+64U*i,156U,2U,weekdays[i],i>=5U?UI_AMBER:UI_MUTED,UI_SURFACE,0U);
        weekday=RTC_CalendarWeekday(year,month,1U);
        for(day=1U;day<=days;day++) {
            cell=(uint8_t)(weekday-1U+day-1U); row=cell/7U; column=cell%7U;
            x=40U+64U*column; y=178U+38U*row;
            today=state->time_valid && state->readable && state->now.year==year &&
                state->now.month==month && state->now.day==day;
            editing_day=state->mode==CALENDAR_EDIT && state->draft.day==day;
            background=editing_day?UI_AMBER:today?UI_TINT:UI_SURFACE;
            color=editing_day?UI_SURFACE:today?UI_ACCENT:column>=5U?UI_AMBER:UI_INK;
            if(today || editing_day) ui_round_rect(x,y,56U,36U,6U,background);
            snprintf(text,sizeof(text),"%2u",day);
            ui_text(x+12U,y+2U,2U,text,color,background,1U);
        }
    }
    if(sidebar_changed) {
        ui_round_rect(536U,96U,240U,328U,12U,UI_SURFACE);
        if(state->mode==CALENDAR_EDIT) {
            for(i=0U;i<6U;i++) {
                y=112U+42U*i;
                background=state->field==i?UI_TINT:UI_SURFACE;
                ui_round_rect(544U,y,224U,40U,6U,background);
                ui_text(552U,y+12U,5U,fields[i],UI_MUTED,background,0U);
                value=i==0U?state->draft.year:i==1U?state->draft.month:i==2U?state->draft.day:
                    i==3U?state->draft.hour:i==4U?state->draft.minute:state->draft.second;
                snprintf(text,sizeof(text),i==0U?"%04u":"%02u",value);
                ui_text(636U,y+4U,7U,text,state->field==i?UI_ACCENT:UI_INK,background,1U);
            }
            background=state->field==6U?UI_ACCENT:UI_TINT;
            ui_round_rect(544U,374U,224U,38U,7U,background);
            ui_text(568U,386U,22U,"OK \261\243\264\346\310\325\306\332\312\261\274\344",state->field==6U?UI_SURFACE:UI_INK,background,0U);
        } else {
            ui_text(552U,108U,26U,"\265\261\307\260\310\325\306\332\312\261\274\344",UI_MUTED,UI_SURFACE,0U);
            if(state->readable) snprintf(text,sizeof(text),"%04u-%02u-%02u",state->now.year,state->now.month,state->now.day);
            else strcpy(text,"--");
            ui_text(552U,134U,13U,text,UI_INK,UI_SURFACE,1U);
            if(state->readable) snprintf(text,sizeof(text),"%02u:%02u:%02u",state->now.hour,state->now.minute,state->now.second);
            else strcpy(text,"--:--:--");
            ui_text(552U,174U,13U,text,UI_INK,UI_SURFACE,1U);
            ui_text(552U,218U,26U,!state->time_valid?"\316\264\320\243\312\261\243\254\307\353\317\310\311\350\326\303":state->source==RTC_TIME_GPS?
                "\320\243\312\261\300\264\324\264: GPS":"\320\243\312\261\300\264\324\264: \312\326\266\257",state->time_valid?UI_GREEN:UI_AMBER,UI_SURFACE,0U);
            ui_text(552U,244U,26U,"OK \264\362\277\252\262\331\327\367\262\313\265\245",UI_MUTED,UI_SURFACE,0U);
            for(i=0U;i<CALENDAR_ACTION_COUNT;i++) {
                y=274U+46U*i;
                background=state->mode==CALENDAR_ACTIONS && state->action==i?UI_TINT:UI_SURFACE;
                ui_round_rect(544U,y,224U,40U,7U,background);
                ui_text(556U,y+12U,24U,actions[i],i==CALENDAR_GPS_SYNC&&!state->gps_available?
                    UI_MUTED:UI_INK,background,0U);
            }
        }
    }
    if(first || state->status!=previous.status)
        ui_text(40U,428U,90U,state->status < sizeof(messages)/sizeof(messages[0])?messages[state->status]:"",
            state->status==CALENDAR_STATUS_WRITE_FAILED?UI_RED:UI_MUTED,UI_BG,0U);
    if(first || state->mode!=previous.mode || state->field!=previous.field) {
        ui_footer(state->mode==CALENDAR_BROWSE?"\311\317/\317\302 \307\320\273\273\324\302\267\335  OK \262\331\327\367":state->mode==CALENDAR_ACTIONS?
            "\311\317/\317\302 \321\241\324\361  OK \326\264\320\320":"\311\317/\317\302 \265\367\325\373\312\375\326\265  OK \317\302\322\273\317\356/\261\243\264\346",
            state->mode==CALENDAR_BROWSE?"BACK \267\265\273\330\311\350\326\303":state->mode==CALENDAR_EDIT?"BACK \310\241\317\373\320\336\270\304":"BACK \267\265\273\330\344\257\300\300");
    }
    previous=*state;
}
