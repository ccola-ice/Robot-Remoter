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

static void gui_clear_page_band(uint16_t y, uint16_t height)
{
	ILI9806G_Fill(0U, y, LCD_X_LENGTH, y + height, WHITE);
}

/* Restore layout boundaries when page content is drawn, including home pagination. */
static void gui_clear_page_content(void)
{
	ILI9806G_Fill(0U, 64U, LCD_X_LENGTH, 72U, WHITE);
	ILI9806G_Fill(0U, 72U, 4U, LCD_Y_LENGTH, WHITE);
	ILI9806G_Fill(796U, 72U, LCD_X_LENGTH, LCD_Y_LENGTH, WHITE);
}

void gui_clock_overlay(void)
{
	RTC_TimeTypeDef rtc_time;
	RTC_DateTypeDef rtc_date;
	char clock_text[9];

	RTC_GetTime(RTC_Format_BIN, &rtc_time);
	/* Reading the date unlocks the STM32 RTC shadow registers. */
	RTC_GetDate(RTC_Format_BIN, &rtc_date);
	(void)rtc_date;

	if((clock_force_redraw == 0U) &&
	   (clock_last_seconds == rtc_time.RTC_Seconds))
	{
		return;
	}

	clock_force_redraw = 0U;
	clock_last_seconds = rtc_time.RTC_Seconds;
	sprintf(clock_text, "%02u:%02u:%02u", rtc_time.RTC_Hours,
			rtc_time.RTC_Minutes, rtc_time.RTC_Seconds);

	/* A self-contained badge works on both blue and white page headers. */
	LCD_SetTextColor(BLACK);
	ILI9806G_DrawRectangle(700U, 4U, 92U, 24U, 1U);
	LCD_SetTextColor(BLUE2);
	ILI9806G_DrawRectangle(700U, 4U, 92U, 24U, 0U);
	LCD_SetFont(&Font8x16);
	LCD_SetBackColor(BLACK);
	LCD_SetTextColor(WHITE);
	ILI9806G_DispString_EN(714U, 8U, clock_text);

	LCD_SetFont(&Font16x32);
	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLACK);
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

static void gui_boot_menu_badge(void)
{
    char text[96];
    if(boot_last_report == 0) return;
    LCD_SetFont(&Font8x16);
    LCD_SetBackColor(WHITE);
    LCD_SetTextColor(boot_last_report->failed ? RED : BLUE);
    sprintf(text, "Boot: PASS %u / FAIL %u / NOT TESTED %u - details in startup serial log",
            boot_last_report->passed, boot_last_report->failed,
            boot_last_report->not_tested);
    ILI9806G_DispString_EN(8U, 456U, text);
}

static void gui_draw_progress_bar(uint16_t x, uint16_t y, uint16_t width,
							  uint16_t height, uint32_t percent_x10,
							  uint16_t fill_color)
{
	uint16_t inner_width;
	uint16_t filled_width;

	if(percent_x10 > 1000UL)
	{
		percent_x10 = 1000UL;
	}

	inner_width = (width > 4U) ? (width - 4U) : 0U;
	filled_width = (uint16_t)((inner_width * percent_x10 + 500UL) / 1000UL);

	LCD_SetTextColor(WHITE);
	ILI9806G_DrawRectangle(x, y, width, height, 1U);
	if(filled_width > 0U)
	{
		LCD_SetTextColor(fill_color);
		ILI9806G_DrawRectangle(x + 2U, y + 2U, filled_width, height - 4U, 1U);
	}
	LCD_SetTextColor(BLACK);
	ILI9806G_DrawRectangle(x, y, width, height, 0U);
}

static void gui_update_progress_bar(uint16_t x, uint16_t y, uint16_t width,
								uint16_t height, uint32_t percent_x10,
								uint16_t fill_color, uint16_t *previous_width,
								uint8_t draw_frame)
{
	uint16_t inner_width;
	uint16_t filled_width;
	uint16_t old_width;
	uint16_t changed_width;

	if(percent_x10 > 1000UL)
	{
		percent_x10 = 1000UL;
	}

	inner_width = (width > 4U) ? (width - 4U) : 0U;
	filled_width = (uint16_t)((inner_width * percent_x10 + 500UL) / 1000UL);
	old_width = (*previous_width <= inner_width) ? *previous_width : 0U;

	if(draw_frame != 0U)
	{
		LCD_SetTextColor(WHITE);
		ILI9806G_DrawRectangle(x, y, width, height, 1U);
		LCD_SetTextColor(BLACK);
		ILI9806G_DrawRectangle(x, y, width, height, 0U);
		old_width = 0U;
	}
	else
	{
		changed_width = (filled_width > old_width) ?
						(filled_width - old_width) : (old_width - filled_width);
		/* Ignore one-pixel ADC/float jitter to avoid continuous LCD writes. */
		if(changed_width < 2U)
		{
			return;
		}
	}

	if(filled_width > old_width)
	{
		LCD_SetTextColor(fill_color);
		ILI9806G_DrawRectangle(x + 2U + old_width, y + 2U,
							  filled_width - old_width, height - 4U, 1U);
	}
	else if(old_width > filled_width)
	{
		LCD_SetTextColor(WHITE);
		ILI9806G_DrawRectangle(x + 2U + filled_width, y + 2U,
							  old_width - filled_width, height - 4U, 1U);
	}

	*previous_width = filled_width;
}

static void gui_draw_channel_card(uint16_t x, uint16_t y, uint8_t channel,
							  uint16_t value, uint16_t bar_color,
							  uint16_t *previous_width, uint16_t *previous_value,
							  uint8_t draw_text, uint8_t draw_frame)
{
	uint16_t shown_value = (value > 4095U) ? 4095U : value;
	uint16_t bar_x = x + 170U;
	uint16_t bar_width = 206U;

	if(draw_frame != 0U)
	{
		LCD_SetTextColor(GREY);
		ILI9806G_DrawRectangle(x, y, 392U, 46U, 1U);
		LCD_SetBackColor(GREY);
		LCD_SetTextColor(BLACK);
		if(channel == 6U) strcpy(displayBuffer, "BAT");
        else snprintf(displayBuffer, sizeof(displayBuffer), "A%02u",
                      (unsigned)(channel < 6U ? channel + 1U : channel));
		ILI9806G_DispString_EN(x + 8U, y + 7U, displayBuffer);
	}

	if(draw_frame != 0U || (draw_text != 0U && value != *previous_value))
	{
		LCD_SetBackColor(GREY);
		LCD_SetTextColor(BLACK);
		snprintf(displayBuffer, sizeof(displayBuffer), "%4u", value);
		ILI9806G_DispString_EN(x + 88U, y + 7U, displayBuffer);
		*previous_value = value;
	}

	gui_update_progress_bar(bar_x, y + 13U, bar_width, 20U,
							((uint32_t)shown_value * 1000UL) / 4095UL,
							bar_color, previous_width, draw_frame);
	LCD_SetTextColor(RED);
	ILI9806G_DrawLine(bar_x + bar_width / 2U, y + 11U,
					  bar_x + bar_width / 2U, y + 35U);
}

static uint32_t gui_imu_axis_percent(float value, float limit)
{
	if(value < -limit)
	{
		value = -limit;
	}
	else if(value > limit)
	{
		value = limit;
	}

	return (uint32_t)(((value + limit) * 1000.0f) / (2.0f * limit) + 0.5f);
}

static void gui_draw_imu_axis(uint16_t y, const char *name, float value,
						  float limit, uint16_t color,
						  uint16_t *previous_width, uint8_t draw_frame)
{
	if(draw_frame != 0U)
	{
		LCD_SetTextColor(GREY);
		ILI9806G_DrawRectangle(4U, y, 792U, 80U, 1U);
		LCD_SetBackColor(GREY);
		LCD_SetTextColor(BLACK);
		ILI9806G_DispString_EN(20U, y + 8U, (char *)name);
		snprintf(displayBuffer, sizeof(displayBuffer), "Range: +/-%3.0f deg", limit);
		ILI9806G_DispString_EN(476U, y + 8U, displayBuffer);
	}

	LCD_SetBackColor(GREY);
	LCD_SetTextColor(color);
	snprintf(displayBuffer, sizeof(displayBuffer), "%+7.1f deg", value);
	ILI9806G_DispString_EN(156U, y + 8U, displayBuffer);
	gui_update_progress_bar(20U, y + 48U, 760U, 22U,
							gui_imu_axis_percent(value, limit), color,
							previous_width, draw_frame);
	LCD_SetTextColor(RED);
	ILI9806G_DrawLine(400U, y + 45U, 400U, y + 73U);
}

void system_basic_information(void)
{
	uint32_t code_ro_bytes;
	uint32_t flash_bytes;
	uint32_t ram_bytes;
	uint32_t zi_bytes;
	uint32_t rw_bytes;
	uint32_t flash_percent_x10;
	uint32_t ram_percent_x10;
	uint8_t first_draw = 0U;

	if(display_flag == 1)
	{
		display_flag = 0;
		GTP_IRQ_Disable();
		first_draw = 1U;
	}
	
	code_ro_bytes = (uint32_t)(uintptr_t)&Image$$ER_IROM1$$Length;
	rw_bytes = (uint32_t)(uintptr_t)&Image$$RW_IRAM1$$Length;
	zi_bytes = (uint32_t)(uintptr_t)&Image$$RW_IRAM1$$ZI$$Length;
	ram_bytes = rw_bytes + zi_bytes;
	flash_bytes = code_ro_bytes + rw_bytes;
	flash_percent_x10 = (flash_bytes * 1000UL + GUI_MCU_FLASH_BYTES / 2UL) /
						GUI_MCU_FLASH_BYTES;
	ram_percent_x10 = (ram_bytes * 1000UL + GUI_MCU_RAM_BYTES / 2UL) /
					GUI_MCU_RAM_BYTES;

	LCD_SetFont(&Font16x32);
	LCD_SetBackColor(WHITE);
	if(first_draw != 0U) gui_clear_page_band(LINE(0), 32U);
	LCD_SetTextColor(BLUE);
	ILI9806G_DispString_EN(4U, LINE(0), "System / Memory Information");
	if(first_draw != 0U) gui_clear_page_band(LINE(1), 32U);
	LCD_SetTextColor(BLACK);
	ILI9806G_DispString_EN(4U, LINE(1), "MCU: STM32F407ZGT6 @ 168 MHz");
	if(first_draw != 0U)
	{
		LCD_SetBackColor(WHITE);
		LCD_SetTextColor(BLACK);
	}

	if(first_draw != 0U) gui_clear_page_band(LINE(2), 32U);
	snprintf(displayBuffer, sizeof(displayBuffer), "Flash linked: %lu.%01lu / 1024.0 KB   %lu.%01lu%% ",
			flash_bytes / 1024UL, ((flash_bytes % 1024UL) * 10UL) / 1024UL,
			flash_percent_x10 / 10UL, flash_percent_x10 % 10UL);
	ILI9806G_DispString_EN(4U, LINE(2), displayBuffer);
	if(first_draw != 0U) gui_clear_page_band(LINE(3), 32U);
	gui_draw_progress_bar(20U, LINE(3) + 5U, 760U, 22U, flash_percent_x10, BLUE);

	if(first_draw != 0U) gui_clear_page_band(LINE(4), 32U);
	snprintf(displayBuffer, sizeof(displayBuffer), "RAM linked: %lu.%01lu / 128.0 KB      %lu.%01lu%% ",
			ram_bytes / 1024UL, ((ram_bytes % 1024UL) * 10UL) / 1024UL,
		ram_percent_x10 / 10UL, ram_percent_x10 % 10UL);
	ILI9806G_DispString_EN(4U, LINE(4), displayBuffer);
	if(first_draw != 0U) gui_clear_page_band(LINE(5), 32U);
	gui_draw_progress_bar(20U, LINE(5) + 5U, 760U, 22U, ram_percent_x10, GREEN);

	if(first_draw != 0U)
	{
		gui_clear_page_band(LINE(6), 32U);
		gui_clear_page_band(LINE(7), 32U);
	}
	snprintf(displayBuffer, sizeof(displayBuffer), "ROM Code + RO: %lu.%01lu KB                  ",
			code_ro_bytes / 1024UL, ((code_ro_bytes % 1024UL) * 10UL) / 1024UL);
	ILI9806G_DispString_EN(4U, LINE(7), displayBuffer);
	if(first_draw != 0U) gui_clear_page_band(LINE(8), 32U);
	snprintf(displayBuffer, sizeof(displayBuffer), "RW initialized: %lu bytes                    ", rw_bytes);
	ILI9806G_DispString_EN(4U, LINE(8), displayBuffer);
	if(first_draw != 0U) gui_clear_page_band(LINE(9), 32U);
	snprintf(displayBuffer, sizeof(displayBuffer), "ZI/BSS + Heap/Stack: %lu.%01lu KB            ",
			zi_bytes / 1024UL, ((zi_bytes % 1024UL) * 10UL) / 1024UL);
	ILI9806G_DispString_EN(4U, LINE(9), displayBuffer);

	if(first_draw != 0U) gui_clear_page_band(LINE(10), 32U);
	snprintf(displayBuffer, sizeof(displayBuffer), "RAM remaining: %lu.%01lu KB                  ",
			(GUI_MCU_RAM_BYTES - ram_bytes) / 1024UL,
			(((GUI_MCU_RAM_BYTES - ram_bytes) % 1024UL) * 10UL) / 1024UL);
	ILI9806G_DispString_EN(4U, LINE(10), displayBuffer);
	if(first_draw != 0U) gui_clear_page_band(LINE(11), 32U);
	snprintf(displayBuffer, sizeof(displayBuffer), "Flash remaining: %lu.%01lu KB                ",
			(GUI_MCU_FLASH_BYTES - flash_bytes) / 1024UL,
			(((GUI_MCU_FLASH_BYTES - flash_bytes) % 1024UL) * 10UL) / 1024UL);
	ILI9806G_DispString_EN(4U, LINE(11), displayBuffer);

	if(first_draw != 0U)
	{
		gui_clear_page_band(LINE(12), 64U);
		gui_clear_page_band(LINE(14), 32U);
	}
	LCD_SetTextColor(BLUE);
	ILI9806G_DispString_EN(4U, LINE(14), "Live values from current linker image");
}

void menu_group_page(uint8_t selected_group)
{
    static const char * const titles[3] = {"CONTROL", "SETUP", "TOOLS"};
    static const char * const hints[3] = {
        "Robot control / channel monitor / digital inputs",
        "Parameters / calibration values / wireless settings",
        "System / IMU / GPS / files / hardware tests / EEPROM"
    };
    static uint8_t previous = 0xffU;
    uint8_t i, first = display_flag != 0U;
    uint16_t y;
    if(selected_group >= MENU_GROUP_COUNT) selected_group = 0U;
    if(first) {
        display_flag = 0U;
        GTP_IRQ_Disable();
        LCD_SetTextColor(BLUE);
        ILI9806G_DrawRectangle(4U, 0U, 792U, 64U, 1U);
        LCD_SetFont(&Font16x32);
        LCD_SetBackColor(BLUE);
        LCD_SetTextColor(WHITE);
        ILI9806G_DispString_EN(20U, 0U, "ROBOT REMOTE");
        LCD_SetFont(&Font8x16);
        ILI9806G_DispString_EN(20U, 40U, "Select a category to open its functions");
        gui_clear_page_content();
    }
    for(i = 0U; i < MENU_GROUP_COUNT; i++) {
        y = 80U + (uint16_t)i * 104U;
        if(first) {
            LCD_SetTextColor(GREY);
            ILI9806G_DrawRectangle(4U, y, 792U, 92U, 1U);
            LCD_SetFont(&Font16x32);
            LCD_SetBackColor(GREY);
            LCD_SetTextColor(BLACK);
            snprintf(displayBuffer, sizeof(displayBuffer), "%02u  %s", i + 1U, titles[i]);
            ILI9806G_DispString_EN(28U, y + 8U, displayBuffer);
            LCD_SetFont(&Font8x16);
            ILI9806G_DispString_EN(92U, y + 54U, (char *)hints[i]);
            snprintf(displayBuffer, sizeof(displayBuffer), "%u functions", menu_group_count(i));
            ILI9806G_DispString_EN(676U, y + 14U, displayBuffer);
        }
        if(first || i == selected_group || i == previous) {
            LCD_SetTextColor(i == selected_group ? BLUE : GREY);
            ILI9806G_DrawRectangle(8U, y + 4U, 8U, 84U, 1U);
            LCD_SetTextColor(i == selected_group ? BLUE : BLACK);
            ILI9806G_DrawRectangle(4U, y, 792U, 92U, 0U);
        }
    }
    if(first) {
        LCD_SetFont(&Font16x32);
        LCD_SetBackColor(WHITE);
        LCD_SetTextColor(BLUE);
        ILI9806G_DispString_EN(12U, 400U, "LEFT/RIGHT: Select    OK: Open");
        gui_boot_menu_badge();
    }
    LCD_SetBackColor(WHITE);
    LCD_SetTextColor(BLACK);
    previous = selected_group;
}

void main_menu(uint8_t selected_item)
{
#define MENU_LABEL(page, label, hint) label,
    static const char * const labels[] = { MENU_ENTRY_LIST(MENU_LABEL) };
#undef MENU_LABEL
#define MENU_HINT(page, label, hint) hint,
    static const char * const hints[] = { MENU_ENTRY_LIST(MENU_HINT) };
#undef MENU_HINT
    static const char * const groups[3] = {"CONTROL", "SETUP", "TOOLS"};
    static uint8_t previous = 0xffU;
    uint8_t group, first_entry, count, i, entry, first;
    uint16_t x, y;
    if(selected_item >= MENU_ENTRY_COUNT) selected_item = 0U;
    group = menu_entry_group(selected_item);
    first_entry = menu_group_first(group);
    count = menu_group_count(group);
    first = display_flag != 0U || previous == 0xffU || menu_entry_group(previous) != group;
    if(first) {
        display_flag = 0U;
        GTP_IRQ_Disable();
        LCD_SetTextColor(BLUE);
        ILI9806G_DrawRectangle(4U, 0U, 792U, 64U, 1U);
        LCD_SetBackColor(BLUE);
        LCD_SetFont(&Font16x32);
        LCD_SetTextColor(WHITE);
        snprintf(displayBuffer, sizeof(displayBuffer), "REMOTER / %s", groups[group]);
        ILI9806G_DispString_EN(20U, 0U, displayBuffer);
        LCD_SetFont(&Font8x16);
        ILI9806G_DispString_EN(20U, 40U, "LEFT/RIGHT: Select   OK: Enter   BACK: Categories");
        gui_clear_page_band(64U, 416U);
    }
    for(i = 0U; i < count; i++) {
        entry = first_entry + i;
        x = (i & 1U) ? 404U : 4U;
        y = 80U + (uint16_t)(i / 2U) * 104U;
        if(first) {
            LCD_SetTextColor(GREY);
            ILI9806G_DrawRectangle(x, y, 392U, 92U, 1U);
            LCD_SetBackColor(GREY);
            LCD_SetTextColor(BLACK);
            LCD_SetFont(&Font16x32);
            ILI9806G_DispString_EN(x + 24U, y + 8U, (char *)labels[entry]);
            LCD_SetFont(&Font8x16);
            snprintf(displayBuffer, sizeof(displayBuffer), "%-45.45s", hints[entry]);
            ILI9806G_DispString_EN(x + 24U, y + 56U, displayBuffer);
        }
        if(first || entry == selected_item || entry == previous) {
            LCD_SetTextColor(entry == selected_item ? BLUE : GREY);
            ILI9806G_DrawRectangle(x + 4U, y + 4U, 8U, 84U, 1U);
            LCD_SetTextColor(entry == selected_item ? BLUE : BLACK);
            ILI9806G_DrawRectangle(x, y, 392U, 92U, 0U);
        }
    }
    LCD_SetFont(&Font16x32);
    LCD_SetBackColor(WHITE);
    LCD_SetTextColor(BLUE);
    snprintf(displayBuffer, sizeof(displayBuffer), "Selected: %u / %u    BACK: Categories", selected_item - first_entry + 1U, count);
    ILI9806G_DispString_EN(12U, 400U, displayBuffer);
    if(first) gui_boot_menu_badge();
    previous = selected_item;
}

void digital_channel_monitor_page(const uint8_t *raw_values,
								  const uint8_t *stable_values)
{
	static const char *channel_pin[6] =
	{
		"PD6", "PD3", "PA8", "PD7", "PE3", "PE2"
	};
	static const uint8_t channel_is_button[6] = {1U, 0U, 1U, 1U, 0U, 0U};
	static uint8_t last_raw[6] = {0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU};
	static uint8_t last_stable[6] = {0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU};
	uint8_t channel;
	uint8_t first_draw = 0U;
	uint16_t card_x;
	uint16_t card_y;
	uint16_t state_color;
	const char *state_text;

	if(display_flag == 1U)
	{
		display_flag = 0U;
		GTP_IRQ_Disable();
		first_draw = 1U;
		for(channel = 0U; channel < 6U; channel++)
		{
			last_raw[channel] = 0xffU;
			last_stable[channel] = 0xffU;
		}
	}

	if(first_draw != 0U)
	{
		LCD_SetTextColor(BLUE2);
		ILI9806G_DrawRectangle(4U, 0U, 792U, 64U, 1U);
		LCD_SetBackColor(BLUE2);
		LCD_SetTextColor(WHITE);
		LCD_SetFont(&Font16x32);
		ILI9806G_DispString_EN(20U, 0U, "DIGITAL CHANNEL MONITOR");
		LCD_SetFont(&Font8x16);
		ILI9806G_DispString_EN(20U, 36U,
			"DCH1..DCH6 / RAW AND 30 ms DEBOUNCED VALUES");
		gui_clear_page_content();

		for(channel = 0U; channel < 6U; channel++)
		{
			card_x = ((channel & 1U) == 0U) ? 4U : 404U;
			card_y = 72U + (uint16_t)(channel / 2U) * 112U;
			LCD_SetTextColor(GREY);
			ILI9806G_DrawRectangle(card_x, card_y, 392U, 104U, 1U);
			LCD_SetTextColor(BLUE2);
			ILI9806G_DrawRectangle(card_x, card_y, 392U, 104U, 0U);

			LCD_SetBackColor(GREY);
			LCD_SetTextColor(BLACK);
			LCD_SetFont(&Font16x32);
			snprintf(displayBuffer, sizeof(displayBuffer), "DCH%u", (uint16_t)(channel + 1U));
			ILI9806G_DispString_EN(card_x + 16U, card_y + 4U, displayBuffer);
			LCD_SetFont(&Font8x16);
			snprintf(displayBuffer, sizeof(displayBuffer), "%s / %s",
					(channel_is_button[channel] != 0U) ? "BUTTON" : "TOGGLE",
					channel_pin[channel]);
			ILI9806G_DispString_EN(card_x + 136U, card_y + 12U, displayBuffer);
		}

		LCD_SetTextColor(WHITE);
		ILI9806G_DrawRectangle(396U, 72U, 8U, 328U, 1U);
		ILI9806G_DrawRectangle(4U, 176U, 792U, 8U, 1U);
		ILI9806G_DrawRectangle(4U, 288U, 792U, 8U, 1U);
		ILI9806G_DrawRectangle(4U, 400U, 792U, 80U, 1U);
		LCD_SetBackColor(WHITE);
		LCD_SetTextColor(BLUE2);
		LCD_SetFont(&Font8x16);
		ILI9806G_DispString_EN(12U, 416U,
			"10 ms SAMPLE / 30 ms DEBOUNCE     BUTTONS ARE ACTIVE LOW");
		ILI9806G_DispString_EN(12U, 456U, "LEFT: BACK");
	}

	for(channel = 0U; channel < 6U; channel++)
	{
		card_x = ((channel & 1U) == 0U) ? 4U : 404U;
		card_y = 72U + (uint16_t)(channel / 2U) * 112U;

		if((first_draw != 0U) || (raw_values[channel] != last_raw[channel]))
		{
			LCD_SetBackColor(GREY);
			LCD_SetTextColor(BLACK);
			LCD_SetFont(&Font8x16);
			snprintf(displayBuffer, sizeof(displayBuffer), "RAW LEVEL: %u   ",
					(uint16_t)raw_values[channel]);
			ILI9806G_DispString_EN(card_x + 16U, card_y + 44U, displayBuffer);
			last_raw[channel] = raw_values[channel];
		}

		if((first_draw != 0U) ||
		   (stable_values[channel] != last_stable[channel]))
		{
			if(channel_is_button[channel] != 0U)
			{
				state_text = (stable_values[channel] == 0U) ?
							 "PRESSED" : "RELEASED";
				state_color = (stable_values[channel] == 0U) ? GREEN : BLACK;
			}
			else
			{
				state_text = (stable_values[channel] == 0U) ?
							 "POSITION A" : "POSITION B";
				state_color = (stable_values[channel] == 0U) ? RED : BLUE2;
			}

			LCD_SetBackColor(GREY);
			LCD_SetTextColor(state_color);
			LCD_SetFont(&Font16x32);
			snprintf(displayBuffer, sizeof(displayBuffer), "VALUE %u  %-10s",
					(uint16_t)stable_values[channel], state_text);
			ILI9806G_DispString_EN(card_x + 16U, card_y + 68U, displayBuffer);
			last_stable[channel] = stable_values[channel];
		}
	}

	LCD_SetFont(&Font16x32);
	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLACK);
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
		sprintf(text, "%lu B", size);
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
	static uint8_t last_selected_item = 0xffU;
	static uint8_t last_first_visible = 0xffU;
	char safe_text[96];
	char size_text[16];
	uint8_t row;
	uint8_t item_index;
	uint16_t row_y;
	uint8_t first_draw = 0U;
	uint8_t content_changed;

	if(display_flag == 1)
	{
		display_flag = 0;
		GTP_IRQ_Disable();
		first_draw = 1U;
		last_revision = 0xffffU;
		last_selected_item = 0xffU;
		last_first_visible = 0xffU;
	}

	content_changed = ((first_draw != 0U) || (revision != last_revision) ||
					   (first_visible != last_first_visible)) ? 1U : 0U;
	LCD_SetFont(&Font16x32);
	if(first_draw != 0U)
	{
		LCD_SetTextColor(BLUE);
		ILI9806G_DrawRectangle(4U, 0U, 792U, 64U, 1U);
		LCD_SetBackColor(BLUE);
		LCD_SetTextColor(WHITE);
		ILI9806G_DispString_EN(20U, 0U, "FILE BROWSER");
		ILI9806G_DispString_EN(20U, 32U, "SD Card  Read-only mode");
		gui_clear_page_content();
	}

	if(content_changed != 0U)
	{
		LCD_SetTextColor(GREY);
		ILI9806G_DrawRectangle(4U, 72U, 792U, 32U, 1U);
		LCD_SetBackColor(GREY);
		LCD_SetTextColor(BLACK);
		gui_file_display_text(safe_text, sizeof(safe_text), path, 672U);
		strcpy(displayBuffer, "Path: ");
		strcat(displayBuffer, safe_text);
		ILI9806G_DispString_EN_CH(12U, 72U, displayBuffer);

		for(row = 0U; row < 6U; row++)
		{
			item_index = (uint8_t)(first_visible + row);
			row_y = 112U + (uint16_t)row * 48U;
			if(item_index >= item_count)
			{
				LCD_SetTextColor(WHITE);
				ILI9806G_DrawRectangle(4U, row_y, 792U, 40U, 1U);
				ILI9806G_DrawRectangle(4U, row_y + 40U, 792U, 8U, 1U);
				continue;
			}

			LCD_SetTextColor(GREY);
			ILI9806G_DrawRectangle(4U, row_y, 792U, 40U, 1U);
			LCD_SetBackColor(GREY);
			LCD_SetTextColor(BLACK);
			gui_file_display_text(safe_text, sizeof(safe_text),
							  entries[item_index].name,
							  (entries[item_index].is_directory != 0U) ? 672U : 496U);
			if(entries[item_index].is_directory != 0U)
			{
				strcpy(displayBuffer, "DIR  ");
				strcat(displayBuffer, safe_text);
			}
			else
			{
				gui_file_size_text(entries[item_index].size, size_text);
				strcpy(displayBuffer, "FILE ");
				strcat(displayBuffer, safe_text);
			}
			ILI9806G_DispString_EN_CH(20U, row_y + 4U, displayBuffer);
			if(entries[item_index].is_directory == 0U)
			{
				ILI9806G_DispString_EN(620U, row_y + 4U, size_text);
			}
			LCD_SetTextColor(WHITE);
			ILI9806G_DrawRectangle(4U, row_y + 40U, 792U, 8U, 1U);
		}
		if(item_count == 0U)
		{
			LCD_SetBackColor(WHITE);
			LCD_SetTextColor(GREY);
			ILI9806G_DispString_EN(260U, 224U, "<EMPTY DIRECTORY>");
		}
	}

	for(row = 0U; row < 6U; row++)
	{
		item_index = (uint8_t)(first_visible + row);
		if(item_index >= item_count)
		{
			break;
		}
		if((content_changed == 0U) && (item_index != selected_item) &&
		   (item_index != last_selected_item))
		{
			continue;
		}

		row_y = 112U + (uint16_t)row * 48U;
		LCD_SetTextColor((item_index == selected_item) ? BLUE : GREY);
		ILI9806G_DrawRectangle(8U, row_y + 4U, 8U, 32U, 1U);
		LCD_SetTextColor((item_index == selected_item) ? BLUE : BLACK);
		ILI9806G_DrawRectangle(4U, row_y, 792U, 40U, 0U);
	}

	if(first_draw != 0U)
	{
		gui_clear_page_band(104U, 8U);
		gui_clear_page_band(400U, 8U);
		gui_clear_page_band(424U, 8U);
		gui_clear_page_band(464U, 16U);
	}
	LCD_SetFont(&Font8x16);
	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLUE);
	ILI9806G_DispString_EN(12U, 408U,
						"LEFT/RIGHT Select   OK Open   BACK Parent/Exit");
	LCD_SetFont(&Font16x32);
	LCD_SetTextColor(WHITE);
	ILI9806G_DrawRectangle(4U, 432U, 792U, 32U, 1U);
	LCD_SetTextColor(BLACK);
	gui_file_display_text(safe_text, sizeof(safe_text), status_text, 640U);
	strcpy(displayBuffer, "Status: ");
	strcat(displayBuffer, safe_text);
	ILI9806G_DispString_EN_CH(12U, 432U, displayBuffer);

	last_revision = revision;
	last_selected_item = selected_item;
	last_first_visible = first_visible;
}

void parameter_settings_page(const GuiParamRow *rows, uint8_t visible_count,
							 uint8_t selected_row, uint8_t first_visible,
							 uint8_t total_items, uint8_t editing,
							 uint8_t dirty, uint16_t revision,
							 const char *status_text)
{
	static uint8_t last_selected_item = 0xffU;
	static uint8_t last_first_visible = 0xffU;
	static uint8_t last_visible_count;
	static uint8_t last_editing = 0xffU;
	static uint8_t last_dirty = 0xffU;
	static GuiParamRow last_rows[GUI_PARAM_VISIBLE_ROWS];
	static char last_status_text[80];
	uint8_t row;
	uint8_t item_index;
	uint8_t selected_item;
	uint8_t first_draw = 0U;
	uint8_t window_changed;
	uint8_t state_changed;
	uint8_t row_text_changed;
	uint8_t indicator_changed;
	uint16_t row_y;
	uint16_t scroll_height;
	uint16_t scroll_y;
	uint16_t selection_color;
	const char *state_text;

	if(display_flag == 1)
	{
		display_flag = 0;
		GTP_IRQ_Disable();
		first_draw = 1U;
		last_selected_item = 0xffU;
		last_first_visible = 0xffU;
		last_visible_count = 0U;
		last_editing = 0xffU;
		last_dirty = 0xffU;
		memset(last_rows, 0, sizeof(last_rows));
		last_status_text[0] = '\0';
	}
	(void)revision;

	if(visible_count == 0U)
	{
		return;
	}
	if(selected_row >= visible_count)
	{
		selected_row = 0U;
	}
	selected_item = (uint8_t)(first_visible + selected_row);
	window_changed = ((first_draw != 0U) ||
					  (first_visible != last_first_visible) ||
					  (visible_count != last_visible_count)) ? 1U : 0U;
	state_changed = ((first_draw != 0U) ||
					 (editing != last_editing) || (dirty != last_dirty) ||
					 (first_visible != last_first_visible) ||
					 (strcmp(status_text, last_status_text) != 0)) ? 1U : 0U;

	if(first_draw != 0U)
	{
		LCD_SetTextColor(BLUE);
		ILI9806G_DrawRectangle(4U, 0U, 792U, 64U, 1U);
		LCD_SetBackColor(BLUE);
		LCD_SetTextColor(WHITE);
		LCD_SetFont(&Font16x32);
		ILI9806G_DispString_EN(20U, 0U, "PARAMETER SETTINGS");
		ILI9806G_DispString_EN(20U, 32U, "Scrollable configuration / explicit Flash save");
		gui_clear_page_content();

		LCD_SetTextColor(GREY);
		ILI9806G_DrawRectangle(4U, 72U, 792U, 32U, 1U);

		gui_clear_page_band(104U, 8U);
		gui_clear_page_band(400U, 32U);
		LCD_SetBackColor(WHITE);
		LCD_SetTextColor(BLUE);
		LCD_SetFont(&Font8x16);
		ILI9806G_DispString_EN(12U, 408U,
			"LEFT/RIGHT Select/Change   OK Edit/Confirm   BACK Cancel/Exit");
		LCD_SetTextColor(WHITE);
		ILI9806G_DrawRectangle(4U, 432U, 792U, 32U, 1U);
		gui_clear_page_band(464U, 16U);
	}

	if(state_changed != 0U)
	{
		state_text = (editing != 0U) ? "EDITING" :
					 ((dirty != 0U) ? "UNSAVED" : "SAVED");
		LCD_SetBackColor(GREY);
		LCD_SetTextColor((editing != 0U) ? RED :
					 ((dirty != 0U) ? RED : BLACK));
		LCD_SetFont(&Font8x16);
		snprintf(displayBuffer, sizeof(displayBuffer),
				"ITEMS %u-%u / %u     STATE: %-8s                              ",
				(uint16_t)(first_visible + 1U),
				(uint16_t)(first_visible + visible_count),
				(uint16_t)total_items, state_text);
		ILI9806G_DispString_EN(20U, 80U, displayBuffer);
	}

	for(row = 0U; row < visible_count; row++)
	{
		item_index = (uint8_t)(first_visible + row);
		row_y = 112U + (uint16_t)row * 48U;
		row_text_changed = ((window_changed != 0U) ||
			(strcmp(rows[row].label, last_rows[row].label) != 0) ||
			(strcmp(rows[row].value, last_rows[row].value) != 0)) ? 1U : 0U;
		indicator_changed = ((window_changed != 0U) ||
			(item_index == selected_item) ||
			(item_index == last_selected_item)) ? 1U : 0U;

		if((first_draw != 0U) || (row >= last_visible_count))
		{
			LCD_SetTextColor(GREY);
			ILI9806G_DrawRectangle(4U, row_y, 768U, 40U, 1U);
			LCD_SetTextColor(BLACK);
			ILI9806G_DrawRectangle(4U, row_y, 768U, 40U, 0U);
		}

		if(row_text_changed != 0U)
		{
			LCD_SetBackColor(GREY);
			LCD_SetTextColor(BLACK);
			LCD_SetFont(&Font16x32);
			snprintf(displayBuffer, sizeof(displayBuffer), "%-23.23s %-20.20s",
					rows[row].label, rows[row].value);
			ILI9806G_DispString_EN(20U, row_y + 4U, displayBuffer);
		}

		if(indicator_changed != 0U)
		{
			/* Narrow marker and outline avoid tearing from full-width fills. */
			LCD_SetTextColor(GREY);
			ILI9806G_DrawRectangle(8U, row_y + 4U, 8U, 32U, 1U);
			LCD_SetTextColor(BLACK);
			ILI9806G_DrawRectangle(4U, row_y, 768U, 40U, 0U);
			if(item_index == selected_item)
			{
				selection_color = (editing != 0U) ? RED : BLUE;
				LCD_SetTextColor(selection_color);
				ILI9806G_DrawRectangle(8U, row_y + 4U, 8U, 32U, 1U);
				ILI9806G_DrawRectangle(4U, row_y, 768U, 40U, 0U);
			}
		}

		strcpy(last_rows[row].label, rows[row].label);
		strcpy(last_rows[row].value, rows[row].value);
	}
	if(first_draw != 0U)
	{
		LCD_SetTextColor(WHITE);
		for(row = 0U; row < GUI_PARAM_VISIBLE_ROWS; row++)
		{
			row_y = 112U + (uint16_t)row * 48U;
			if(row >= visible_count)
			{
				ILI9806G_DrawRectangle(4U, row_y, 792U, 40U, 1U);
			}
			ILI9806G_DrawRectangle(4U, row_y + 40U, 792U, 8U, 1U);
		}
	}

	if(window_changed != 0U)
	{
		/* Scrollbar is redrawn only when the six-row window actually moves. */
		LCD_SetTextColor(GREY);
		ILI9806G_DrawRectangle(780U, 116U, 8U, 280U, 1U);
		scroll_height = (uint16_t)(280UL * visible_count / total_items);
		if(scroll_height < 24U)
		{
			scroll_height = 24U;
		}
		if(total_items > visible_count)
		{
			scroll_y = (uint16_t)(116U +
				(256UL * first_visible / (total_items - visible_count)));
		}
		else
		{
			scroll_y = 116U;
		}
		LCD_SetTextColor(BLUE);
		ILI9806G_DrawRectangle(780U, scroll_y, 8U, scroll_height, 1U);
	}

	if(state_changed != 0U)
	{
		LCD_SetBackColor(WHITE);
		LCD_SetTextColor(((editing != 0U) || (dirty != 0U)) ? RED : BLACK);
		LCD_SetFont(&Font16x32);
		snprintf(displayBuffer, sizeof(displayBuffer), "Status: %-39.39s", status_text);
		ILI9806G_DispString_EN(12U, 432U, displayBuffer);
	}

	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLACK);
	last_selected_item = selected_item;
	last_first_visible = first_visible;
	last_visible_count = visible_count;
	last_editing = editing;
	last_dirty = dirty;
	strncpy(last_status_text, status_text, sizeof(last_status_text) - 1U);
	last_status_text[sizeof(last_status_text) - 1U] = '\0';
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
	uint8_t i;
	uint8_t first_draw = 0U;
	char marker;

	if(display_flag == 1)
	{
		display_flag = 0;
		GTP_IRQ_Disable();
		first_draw = 1U;
	}

	if(power_index > 3U)
	{
		power_index = 0U;
	}
	if(data_rate > 2U)
	{
		data_rate = 2U;
	}
	if(runtime_power_index > 3U)
	{
		runtime_power_index = 0U;
	}
	if(runtime_data_rate > 2U)
	{
		runtime_data_rate = 2U;
	}

	LCD_SetFont(&Font16x32);
	LCD_SetBackColor(WHITE);
	if(first_draw != 0U) gui_clear_page_band(LINE(0), 32U);
	LCD_SetTextColor(BLUE);
	ILI9806G_DispString_EN(4U, LINE(0), "NRF Wireless Settings");
	if(first_draw != 0U) gui_clear_page_band(LINE(1), 32U);
	ILI9806G_DispString_EN(4U, LINE(1), "LEFT/RIGHT: Select   OK: Edit/Run   BACK: Exit");
	if(first_draw != 0U)
	{
		gui_clear_page_band(LINE(2), 32U);
		LCD_SetFont(&Font16x32);
		LCD_SetBackColor(WHITE);
	}

	for(i = 0; i < 6U; i++)
	{
		marker = (i == selected_item) ? ((editing != 0U) ? '*' : '>') : ' ';
		if(i == selected_item)
		{
			LCD_SetBackColor((editing != 0U) ? RED : BLUE);
			LCD_SetTextColor(WHITE);
		}
		else
		{
			LCD_SetBackColor(WHITE);
			LCD_SetTextColor(BLACK);
		}

		switch(i)
		{
			case 0U:
				snprintf(displayBuffer, sizeof(displayBuffer), "%c Wireless:   %-3s                         ", marker,
						enabled ? "ON" : "OFF");
				break;
			case 1U:
				snprintf(displayBuffer, sizeof(displayBuffer), "%c RF Channel: %3u  (%4u MHz)              ", marker,
						channel, 2400U + channel);
				break;
			case 2U:
				snprintf(displayBuffer, sizeof(displayBuffer), "%c TX Power:   %3d dBm                     ", marker,
						power_dbm[power_index]);
				break;
			case 3U:
				snprintf(displayBuffer, sizeof(displayBuffer), "%c Air Rate:   %-8s                    ", marker,
						rate_text[data_rate]);
				break;
			case 4U:
				snprintf(displayBuffer, sizeof(displayBuffer), "%c Apply & Save                              ", marker);
				break;
			default:
				snprintf(displayBuffer, sizeof(displayBuffer), "%c Check Module Connection                   ", marker);
				break;
		}
		if(first_draw != 0U) gui_clear_page_band(LINE(i + 3U), 32U);
		ILI9806G_DispString_EN(4U, LINE(i + 3U), displayBuffer);
	}

	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLUE);
	if(first_draw != 0U) gui_clear_page_band(LINE(9), 32U);
	snprintf(displayBuffer, sizeof(displayBuffer), "Connection: %-35s", runtime_valid ? "OK" : "FAILED");
	ILI9806G_DispString_EN(4U, LINE(9), displayBuffer);
	if(runtime_valid != 0U)
	{
		snprintf(displayBuffer, sizeof(displayBuffer), "Actual: %-3s CH%3u %3d dBm %-8s          ",
				runtime_enabled ? "ON" : "OFF", runtime_channel,
				power_dbm[runtime_power_index], rate_text[runtime_data_rate]);
	}
	else
	{
		snprintf(displayBuffer, sizeof(displayBuffer), "Actual: register readback unavailable          ");
	}
	if(first_draw != 0U) gui_clear_page_band(LINE(10), 32U);
	ILI9806G_DispString_EN(4U, LINE(10), displayBuffer);
	if(first_draw != 0U) gui_clear_page_band(LINE(11), 32U);
	snprintf(displayBuffer, sizeof(displayBuffer), "Result: %-38s", status_text);
	ILI9806G_DispString_EN(4U, LINE(11), displayBuffer);
	if(first_draw != 0U)
	{
		gui_clear_page_band(LINE(12), 32U);
		gui_clear_page_band(LINE(13), 32U);
	}
	LCD_SetTextColor(BLACK);
	ILI9806G_DispString_EN(4U, LINE(13), "Blue=selected  Red=editing  OK=confirm");
	if(first_draw != 0U) gui_clear_page_band(LINE(14), 32U);
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

static void gui_gps_draw_card(uint16_t x, uint16_t y, uint16_t width,
							  uint16_t height, const char *title)
{
	LCD_SetTextColor(GREY);
	ILI9806G_DrawRectangle(x, y, width, height, 1U);
	LCD_SetTextColor(BLUE2);
	ILI9806G_DrawRectangle(x, y, width, height, 0U);
	LCD_SetFont(&Font8x16);
	LCD_SetBackColor(GREY);
	LCD_SetTextColor(BLACK);
	ILI9806G_DispString_EN(x + 12U, y + 8U, (char *)title);
}

void system_data_read_and_set(void)
{
	static gui_gps_snapshot_t previous;
	static uint8_t snapshot_valid;
	gui_gps_snapshot_t current;
	uint8_t first_draw = 0U;
	uint8_t position_valid;
	uint8_t time_valid;
	uint16_t status_color;
	uint32_t gps_level;
	uint32_t bds_level;
	int total_inuse;
	int total_inview;
	char latitude_hemisphere;
	char longitude_hemisphere;
	char mode;
	double latitude;
	double longitude;
	const char *fix_text;
	const char *signal_text;

	memset(&current, 0, sizeof(current));
	current.sig = gps_data_is_fresh() ? info.sig : 0;
	current.fix = gps_data_is_fresh() ? info.fix : 1;
	current.mode = info.mode;
	current.gps_inuse = info.satinfo.inuse;
	current.gps_inview = info.satinfo.inview;
	current.bds_inuse = info.BDsatinfo.inuse;
	current.bds_inview = info.BDsatinfo.inview;
	current.year = gps_time_is_fresh() ? beiJingTime.year : 0;
	current.month = beiJingTime.mon;
	current.day = beiJingTime.day;
	current.hour = beiJingTime.hour;
	current.minute = beiJingTime.min;
	current.second = beiJingTime.sec;
	current.latitude = deg_lat;
	current.longitude = deg_lon;
	current.altitude = info.elv;
	current.speed = info.speed;
	current.course = info.direction;
	current.hdop = info.HDOP;
	current.pdop = info.PDOP;

	if(display_flag == 1)
	{
		display_flag = 0;
		GTP_IRQ_Disable();
		first_draw = 1U;
		snapshot_valid = 0U;
	}

	if(first_draw != 0U)
	{
		LCD_SetTextColor(BLUE2);
		ILI9806G_DrawRectangle(4U, 0U, 792U, 64U, 1U);
		LCD_SetFont(&Font16x32);
		LCD_SetBackColor(BLUE2);
		LCD_SetTextColor(WHITE);
		ILI9806G_DispString_EN(20U, 0U, "GPS / BDS NAVIGATION");
		LCD_SetFont(&Font8x16);
		ILI9806G_DispString_EN(20U, 36U, "LIVE NMEA POSITION AND RECEIVER STATUS");
		gui_clear_page_content();

		LCD_SetTextColor(GREY);
		ILI9806G_DrawRectangle(4U, 72U, 792U, 32U, 1U);
		LCD_SetTextColor(BLUE2);
		ILI9806G_DrawRectangle(4U, 72U, 792U, 32U, 0U);

		gui_gps_draw_card(4U, 112U, 500U, 136U, "POSITION");
		gui_gps_draw_card(512U, 112U, 284U, 136U, "SOLUTION");
		gui_gps_draw_card(4U, 256U, 252U, 96U, "GPS SATELLITES");
		gui_gps_draw_card(264U, 256U, 252U, 96U, "BDS SATELLITES");
		gui_gps_draw_card(524U, 256U, 272U, 96U, "NMEA LOCAL TIME");
		gui_gps_draw_card(4U, 360U, 252U, 80U, "ALTITUDE");
		gui_gps_draw_card(264U, 360U, 252U, 80U, "GROUND SPEED");
		gui_gps_draw_card(524U, 360U, 272U, 80U, "TRUE COURSE");

		gui_clear_page_band(104U, 8U);
		gui_clear_page_band(248U, 8U);
		gui_clear_page_band(352U, 8U);
		gui_clear_page_band(440U, 40U);
		ILI9806G_Fill(504U, 112U, 512U, 248U, WHITE);
		ILI9806G_Fill(256U, 256U, 264U, 440U, WHITE);
		ILI9806G_Fill(516U, 256U, 524U, 440U, WHITE);
		LCD_SetFont(&Font8x16);
		LCD_SetBackColor(WHITE);
		LCD_SetTextColor(BLACK);
		ILI9806G_DispString_EN(12U, 456U, "LEFT: BACK");
		LCD_SetTextColor(BLUE2);
		ILI9806G_DispString_EN(596U, 456U, "GNSS DATA / LIVE");
	}

	if((snapshot_valid != 0U) &&
	   (memcmp(&current, &previous, sizeof(current)) == 0))
	{
		return;
	}

	previous = current;
	snapshot_valid = 1U;
	position_valid = ((current.sig > 0) && (current.fix >= 2)) ? 1U : 0U;
	time_valid = ((current.year >= 100) && (current.month >= 1) &&
				  (current.month <= 12) && (current.day >= 1) &&
				  (current.day <= 31)) ? 1U : 0U;
	mode = ((current.mode >= 32) && (current.mode <= 126)) ?
		   (char)current.mode : '-';

	if(position_valid == 0U)
	{
		fix_text = "NO FIX";
		status_color = RED;
	}
	else if(current.sig == 2)
	{
		fix_text = "DGNSS FIX";
		status_color = GREEN;
	}
	else if(current.fix >= 3)
	{
		fix_text = "3D FIX";
		status_color = GREEN;
	}
	else
	{
		fix_text = "2D FIX";
		status_color = YELLOW;
	}

	switch(current.sig)
	{
		case 1: signal_text = "STANDARD"; break;
		case 2: signal_text = "DIFFERENTIAL"; break;
		case 3: signal_text = "SENSITIVE"; break;
		default: signal_text = "INVALID"; break;
	}

	total_inuse = current.gps_inuse + current.bds_inuse;
	total_inview = current.gps_inview + current.bds_inview;
	LCD_SetFont(&Font8x16);
	LCD_SetBackColor(GREY);
	LCD_SetTextColor(status_color);
	snprintf(displayBuffer, sizeof(displayBuffer), "%-10s", fix_text);
	ILI9806G_DispString_EN(20U, 80U, displayBuffer);
	LCD_SetTextColor(BLACK);
	snprintf(displayBuffer, sizeof(displayBuffer),
			"MODE:%c  SIGNAL:%-12s  USED:%02d  VIEW:%02d  HDOP:%5.2f    ",
			mode, signal_text, total_inuse, total_inview, current.hdop);
	ILI9806G_DispString_EN(116U, 80U, displayBuffer);

	LCD_SetFont(&Font16x32);
	LCD_SetBackColor(GREY);
	LCD_SetTextColor(BLUE2);
	if(position_valid != 0U)
	{
		latitude = current.latitude;
		longitude = current.longitude;
		latitude_hemisphere = (latitude < 0.0) ? 'S' : 'N';
		longitude_hemisphere = (longitude < 0.0) ? 'W' : 'E';
		if(latitude < 0.0) latitude = -latitude;
		if(longitude < 0.0) longitude = -longitude;
		snprintf(displayBuffer, sizeof(displayBuffer), "LAT  %c %10.6f deg   ", latitude_hemisphere, latitude);
		ILI9806G_DispString_EN(20U, 144U, displayBuffer);
		snprintf(displayBuffer, sizeof(displayBuffer), "LON  %c %10.6f deg   ", longitude_hemisphere, longitude);
		ILI9806G_DispString_EN(20U, 192U, displayBuffer);
	}
	else
	{
		ILI9806G_DispString_EN(20U, 144U, "LAT  -- NO VALID POSITION ");
		ILI9806G_DispString_EN(20U, 192U, "LON  -- NO VALID POSITION ");
	}

	LCD_SetTextColor(status_color);
	snprintf(displayBuffer, sizeof(displayBuffer), "%-13s", fix_text);
	ILI9806G_DispString_EN(528U, 144U, displayBuffer);
	LCD_SetFont(&Font8x16);
	LCD_SetTextColor(BLACK);
	snprintf(displayBuffer, sizeof(displayBuffer), "QUALITY: %-14s  ", signal_text);
	ILI9806G_DispString_EN(528U, 184U, displayBuffer);
	snprintf(displayBuffer, sizeof(displayBuffer), "HDOP:%5.2f  PDOP:%5.2f      ", current.hdop, current.pdop);
	ILI9806G_DispString_EN(528U, 208U, displayBuffer);

	LCD_SetFont(&Font16x32);
	LCD_SetTextColor(BLUE2);
	snprintf(displayBuffer, sizeof(displayBuffer), "USED %02d / %02d ", current.gps_inuse, current.gps_inview);
	ILI9806G_DispString_EN(20U, 280U, displayBuffer);
	snprintf(displayBuffer, sizeof(displayBuffer), "USED %02d / %02d ", current.bds_inuse, current.bds_inview);
	ILI9806G_DispString_EN(280U, 280U, displayBuffer);

	gps_level = (current.gps_inuse <= 0) ? 0UL :
				((current.gps_inuse >= 12) ? 1000UL :
				 (uint32_t)current.gps_inuse * 1000UL / 12UL);
	bds_level = (current.bds_inuse <= 0) ? 0UL :
				((current.bds_inuse >= 12) ? 1000UL :
				 (uint32_t)current.bds_inuse * 1000UL / 12UL);
	gui_draw_progress_bar(20U, 328U, 220U, 12U, gps_level, BLUE2);
	gui_draw_progress_bar(280U, 328U, 220U, 12U, bds_level, GREEN);

	LCD_SetBackColor(GREY);
	if(time_valid != 0U)
	{
		LCD_SetFont(&Font16x32);
		LCD_SetTextColor(BLUE2);
		snprintf(displayBuffer, sizeof(displayBuffer), "%02d:%02d:%02d   ", current.hour,
				current.minute, current.second);
		ILI9806G_DispString_EN(540U, 280U, displayBuffer);
		LCD_SetFont(&Font8x16);
		LCD_SetTextColor(BLACK);
		snprintf(displayBuffer, sizeof(displayBuffer), "%04d/%02d/%02d           ", current.year + 1900,
				current.month, current.day);
		ILI9806G_DispString_EN(540U, 328U, displayBuffer);
	}
	else
	{
		LCD_SetFont(&Font16x32);
		LCD_SetTextColor(RED);
		ILI9806G_DispString_EN(540U, 280U, "WAITING...     ");
		LCD_SetFont(&Font8x16);
		LCD_SetTextColor(BLACK);
		ILI9806G_DispString_EN(540U, 328U, "NO VALID NMEA TIME        ");
	}

	LCD_SetFont(&Font16x32);
	LCD_SetBackColor(GREY);
	LCD_SetTextColor(BLUE2);
	if(position_valid != 0U)
	{
		snprintf(displayBuffer, sizeof(displayBuffer), "%7.1f m   ", current.altitude);
		ILI9806G_DispString_EN(20U, 392U, displayBuffer);
		snprintf(displayBuffer, sizeof(displayBuffer), "%7.1f km/h", current.speed);
		ILI9806G_DispString_EN(280U, 392U, displayBuffer);
		snprintf(displayBuffer, sizeof(displayBuffer), "%6.1f deg     ", current.course);
		ILI9806G_DispString_EN(540U, 392U, displayBuffer);
	}
	else
	{
		ILI9806G_DispString_EN(20U, 392U, "--           ");
		ILI9806G_DispString_EN(280U, 392U, "--           ");
		ILI9806G_DispString_EN(540U, 392U, "--             ");
	}

	LCD_SetFont(&Font16x32);
	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLACK);
}

void channel_monitor_page(void)
{
	static const uint16_t card_y[5] = {80U, 144U, 208U, 272U, 336U};
	static uint16_t previous_bar_width[10], previous_value[10];
	static GuiAnalogFilter filters[10];
	/* ADC values are considered on every live frame. */
	uint8_t draw_text;
	uint16_t channel_value[10];
	uint8_t i;
	uint8_t first_draw = 0U;

	if(display_flag == 1)
	{
		display_flag = 0;
		GTP_IRQ_Disable();
		first_draw = 1U;
	}

	for(i = 0U; i < 7U; i++)
	{
		channel_value[i] = gui_analog_filter_update(&filters[i], ADC1_Value[i], first_draw);
	}
	for(i = 0U; i < 3U; i++)
	{
		channel_value[i + 7U] = gui_analog_filter_update(&filters[i + 7U], ADC3_Value[i], first_draw);
	}
	draw_text = 1U;

	LCD_SetFont(&Font16x32);
	if(first_draw != 0U)
	{
		LCD_SetTextColor(BLUE);
		ILI9806G_DrawRectangle(4U, 0U, 792U, 64U, 1U);
		LCD_SetBackColor(BLUE);
		LCD_SetTextColor(WHITE);
		ILI9806G_DispString_EN(20U, 0U, "CHANNEL MONITOR");
		ILI9806G_DispString_EN(20U, 32U, "RAW: A01..A09 + BAT / A09 is a button");
		gui_clear_page_content();
	}

	for(i = 0U; i < 5U; i++)
	{
		gui_draw_channel_card(4U, card_y[i], i, channel_value[i], BLUE,
						  &previous_bar_width[i], &previous_value[i], draw_text, first_draw);
		gui_draw_channel_card(404U, card_y[i], i + 5U,
						  channel_value[i + 5U], GREEN,
						  &previous_bar_width[i + 5U], &previous_value[i + 5U],
						  draw_text, first_draw);
	}
	if(first_draw != 0U)
	{
		LCD_SetTextColor(WHITE);
		ILI9806G_DrawRectangle(396U, 72U, 8U, 328U, 1U);
		for(i = 0U; i < 4U; i++)
		{
			ILI9806G_DrawRectangle(4U, 126U + (uint16_t)i * 64U,
								  792U, 18U, 1U);
		}
		gui_clear_page_band(382U, 42U);
		gui_clear_page_band(456U, 24U);
		LCD_SetBackColor(WHITE);
		LCD_SetTextColor(BLUE);
		LCD_SetFont(&Font16x32);
		ILI9806G_DispString_EN(4U, 424U,
			"LEFT/RIGHT/OK: Output view  BACK: Up");
	}

	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLACK);
}

/* Input values come from the same sample and normalization as the control task.
 * TX fields show the last packet queued to the radio, never a GUI reconstruction. */
void channel_output_monitor_page(const ControlLinkSnapshot *snapshot)
{
    static const char * const names[6] = {"A1", "A2 Y", "A3 X", "A4", "A5", "A6 YAW"};
    static ControlLinkSnapshot previous;
    static uint8_t previous_reverse[6];
    static uint16_t bar_width[6];
    static uint32_t text_ms;
    unsigned long now;
    uint8_t first = display_flag != 0U, i, update_text;
    uint16_t y;
    int16_t value;
    if(!snapshot) return;
    get_tick_count(&now);
    update_text = first || (uint32_t)(now - text_ms) >= 100U;
    if(first) {
        display_flag = 0U;
        GTP_IRQ_Disable();
        LCD_SetTextColor(BLUE);
        ILI9806G_DrawRectangle(4U, 0U, 792U, 64U, 1U);
        LCD_SetFont(&Font16x32);
        LCD_SetBackColor(BLUE);
        LCD_SetTextColor(WHITE);
        ILI9806G_DispString_EN(20U, 0U, "CHANNEL MONITOR / OUTPUT");
        LCD_SetFont(&Font8x16);
        ILI9806G_DispString_EN(20U, 40U, "LEFT/RIGHT/OK: Raw view   BACK: Up");
        gui_clear_page_content();
        LCD_SetTextColor(GREY);
        ILI9806G_DrawRectangle(4U, 80U, 448U, 300U, 1U);
        ILI9806G_DrawRectangle(468U, 80U, 328U, 300U, 1U);
        LCD_SetBackColor(GREY);
        LCD_SetTextColor(BLUE);
        ILI9806G_DispString_EN(16U, 86U, "INPUT      RAW     CALIBRATED   REV");
        ILI9806G_DispString_EN(480U, 86U, "LAST QUEUED RC v1 FRAME");
        LCD_SetBackColor(WHITE);
        LCD_SetTextColor(BLACK);
        ILI9806G_DispString_EN(12U, 420U, "Input: endpoints -> trim -> reverse -> clamp -> 5% deadband");
        ILI9806G_DispString_EN(12U, 444U, "TX: X=-A3  Y=A2  Heading=-A6(%) x 1.8 deg    Radio ACK is not execution feedback");
        ILI9806G_DispString_EN(12U, 464U, "A7/A8/A9 and battery are in RAW view. This monitor cannot arm the robot.");
    }
    LCD_SetFont(&Font8x16);
    for(i = 0U; i < 6U; i++) {
        y = 112U + (uint16_t)i * 44U;
        value = snapshot->calibrated[i];
        if(value > 1000) value = 1000;
        if(value < -1000) value = -1000;
        LCD_SetBackColor(GREY);
        LCD_SetTextColor(BLACK);
        if(first) ILI9806G_DispString_EN(16U, y, (char *)names[i]);
        if(update_text && (first || snapshot->raw[i] != previous.raw[i] ||
           value != previous.calibrated[i] || param.chReverse[i] != previous_reverse[i])) {
            snprintf(displayBuffer, sizeof(displayBuffer), "%4u    %c%3u.%u%%      %s",
                snapshot->raw[i], value < 0 ? '-' : '+',
                (unsigned)(value < 0 ? -value : value) / 10U,
                (unsigned)(value < 0 ? -value : value) % 10U,
                param.chReverse[i] ? "ON " : "OFF");
            ILI9806G_DispString_EN(104U, y, displayBuffer);
            previous.raw[i] = snapshot->raw[i];
            previous.calibrated[i] = value;
            previous_reverse[i] = param.chReverse[i];
        }
        gui_update_progress_bar(104U, y + 20U, 328U, 12U,
            (uint32_t)(value + 1000) / 2U, BLUE2, &bar_width[i], first);
        LCD_SetTextColor(RED);
        ILI9806G_DrawLine(268U, y + 18U, 268U, y + 33U);
    }
    if(update_text) {
        LCD_SetBackColor(GREY);
        LCD_SetTextColor(snapshot->sent && snapshot->tx_age_ms <= 100U ? BLUE : RED);
        snprintf(displayBuffer, sizeof(displayBuffer), "%-36s", !snapshot->sent ? "NOT SENT" :
            snapshot->tx_age_ms > 100U ? "STALE / LAST PACKET ONLY" :
            snapshot->transmitted.armed ? "ARMED IN LAST PACKET" : "DISARMED / ZERO MOTION");
        ILI9806G_DispString_EN(480U, 112U, displayBuffer);
        LCD_SetTextColor(BLACK);
        snprintf(displayBuffer, sizeof(displayBuffer), "Seq:%5u     age:%5u ms   ",
            snapshot->transmitted.sequence, snapshot->tx_age_ms);
        ILI9806G_DispString_EN(480U, 144U, displayBuffer);
        snprintf(displayBuffer, sizeof(displayBuffer), "X:%+5d   Y:%+5d             ",
            snapshot->transmitted.x, snapshot->transmitted.y);
        ILI9806G_DispString_EN(480U, 176U, displayBuffer);
        snprintf(displayBuffer, sizeof(displayBuffer), "Heading:%+5d x0.1 deg       ", snapshot->transmitted.heading);
        ILI9806G_DispString_EN(480U, 208U, displayBuffer);
        snprintf(displayBuffer, sizeof(displayBuffer), "Limit:%4u  DCH:0x%02X       ",
            snapshot->transmitted.limit, snapshot->transmitted.digital);
        ILI9806G_DispString_EN(480U, 240U, displayBuffer);
        if(snapshot->ack_seen)
            snprintf(displayBuffer, sizeof(displayBuffer), "Radio ACK age: %5u ms       ", snapshot->ack_age_ms);
        else snprintf(displayBuffer, sizeof(displayBuffer), "%-36s", "Radio ACK: NONE");
        ILI9806G_DispString_EN(480U, 272U, displayBuffer);
        snprintf(displayBuffer, sizeof(displayBuffer), "Sent:%10lu ACK:%10lu",
            (unsigned long)snapshot->tx_started, (unsigned long)snapshot->tx_acked);
        ILI9806G_DispString_EN(480U, 304U, displayBuffer);
        snprintf(displayBuffer, sizeof(displayBuffer), "Failed:%10lu                 ", (unsigned long)snapshot->tx_failed);
        ILI9806G_DispString_EN(480U, 336U, displayBuffer);
        LCD_SetBackColor(WHITE);
        LCD_SetTextColor(snapshot->sampled && snapshot->input_fresh && snapshot->sample_age_ms <= 100U ? BLUE : RED);
        snprintf(displayBuffer, sizeof(displayBuffer), "Input: %-10s age:%5u ms   %-40.40s",
            !snapshot->sampled ? "NO SAMPLE" : !snapshot->input_fresh || snapshot->sample_age_ms > 100U ? "STALE" : "LIVE",
            snapshot->sample_age_ms, control_link_status());
        ILI9806G_DispString_EN(12U, 394U, displayBuffer);
        text_ms = (uint32_t)now;
    }
    LCD_SetBackColor(WHITE);
    LCD_SetTextColor(BLACK);
}

void imu6050_information(void)
{
	static uint16_t previous_bar_width[3];
	uint8_t first_draw = 0U;

	if(display_flag == 1)
	{
		display_flag = 0;
		GTP_IRQ_Disable();
		first_draw = 1U;
	}

	LCD_SetFont(&Font16x32);
	if(first_draw != 0U)
	{
		LCD_SetTextColor(BLUE);
		ILI9806G_DrawRectangle(4U, 0U, 792U, 64U, 1U);
		LCD_SetBackColor(BLUE);
		LCD_SetTextColor(WHITE);
		ILI9806G_DispString_EN(20U, 0U, "IMU / MPU6050");
		ILI9806G_DispString_EN(20U, 32U, "DMP attitude / 100 Hz sensor / 20 FPS UI");
		gui_clear_page_content();

		LCD_SetTextColor(GREY);
		ILI9806G_DrawRectangle(4U, 396U, 792U, 80U, 1U);
	}

	gui_draw_imu_axis(76U, "PITCH", pitch, 90.0f, BLUE,
					   &previous_bar_width[0], first_draw);
	gui_draw_imu_axis(172U, "ROLL", roll, 180.0f, GREEN,
					   &previous_bar_width[1], first_draw);
	gui_draw_imu_axis(268U, "YAW", yaw, 180.0f, RED,
					   &previous_bar_width[2], first_draw);
	if(first_draw != 0U)
	{
		gui_clear_page_band(72U, 4U);
		gui_clear_page_band(156U, 16U);
		gui_clear_page_band(252U, 16U);
		gui_clear_page_band(348U, 48U);
		gui_clear_page_band(476U, 4U);
	}

	LCD_SetBackColor(GREY);
	LCD_SetTextColor(imu_data_valid ? GREEN : RED);
	snprintf(displayBuffer, sizeof(displayBuffer), "%-7s TEMP:%5.1f C  ACC:%6d %6d %6d ",
			imu_data_valid ? "ONLINE" : "WAITING", (float)temp / 100.0f,
			aacx, aacy, aacz);
	ILI9806G_DispString_EN(12U, 400U, displayBuffer);
	LCD_SetTextColor(BLACK);
	snprintf(displayBuffer, sizeof(displayBuffer), "GYRO:%6d %6d %6d             BACK=Exit ",
			gyrox, gyroy, gyroz);
	ILI9806G_DispString_EN(12U, 436U, displayBuffer);

	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLACK);
}

static void gui_robot_card(uint16_t x, uint16_t y, uint16_t width,
						   uint16_t height, const char *title)
{
	LCD_SetTextColor(GREY);
	ILI9806G_DrawRectangle(x, y, width, height, 1U);
	LCD_SetTextColor(BLUE2);
	ILI9806G_DrawRectangle(x, y, width, height, 0U);
	LCD_SetFont(&Font8x16);
	LCD_SetBackColor(GREY);
	LCD_SetTextColor(BLUE2);
	ILI9806G_DispString_EN(x + 12U, y + 4U, (char *)title);
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

	if((lower >= middle) || (middle >= upper))
	{
		lower = 0U;
		middle = 2047U;
		upper = 4095U;
	}
	if(raw >= middle)
	{
		range = upper - middle;
		value = ((int32_t)raw - middle) * 1000L / range;
	}
	else
	{
		range = middle - lower;
		value = -((int32_t)middle - raw) * 1000L / range;
	}
	value += param.PWMadjustValue[channel];
	if(value > 1000L) value = 1000L;
	if(value < -1000L) value = -1000L;
	if(param.chReverse[channel] != 0U) value = -value;
	/* Match the control path neutral band without changing its input. */
	if(value >= -50L && value <= 50L) value = 0L;
	return (int16_t)value;
}

/* Padded, single-line text never crosses its card or wraps into another row. */
static void gui_robot_text(uint16_t x, uint16_t y, uint8_t columns, const char *text)
{
    char line[96];
    uint8_t i;
    if(columns >= sizeof(line)) columns = sizeof(line) - 1U;
    for(i = 0U; i < columns; i++) line[i] = *text ? *text++ : ' ';
    line[columns] = '\0';
    ILI9806G_DispString_EN(x, y, line);
}

/* Commit final pixels directly: old/new overlapping marker pixels never go
 * through a blank intermediate frame. The static circle is outside this patch. */
static void gui_robot_dot_patch(uint16_t center_x, uint16_t center_y,
                                uint16_t patch_x, uint16_t patch_y,
                                uint16_t dot_x, uint16_t dot_y, uint16_t color)
{
    static uint16_t pixels[17U * 17U];
    uint16_t row, column, x, y, pixel;
    int32_t dx, dy;
    for(row = 0U; row < 17U; row++) {
        y = patch_y + row;
        for(column = 0U; column < 17U; column++) {
            x = patch_x + column;
            pixel = (x == center_x || y == center_y) ? WHITE : GREY;
            dx = (int32_t)x - dot_x; dy = (int32_t)y - dot_y;
            if(dx * dx + dy * dy <= 64L) pixel = color;
            pixels[row * 17U + column] = pixel;
        }
    }
    LCD_BlitRGB565(patch_x, patch_y, 17U, 17U, pixels);
}

static void gui_robot_draw_stick(uint16_t center_x, uint16_t center_y,
                                 uint16_t raw_x, uint16_t raw_y,
                                 int16_t control_x, int16_t control_y,
                                 uint16_t color, uint16_t *old_x,
                                 uint16_t *old_y, uint16_t *old_raw_x,
                                 uint16_t *old_raw_y, uint8_t draw_text,
                                 uint8_t first_draw)
{
    int32_t dx = (int32_t)control_x * 44L / 1000L;
    int32_t dy = -(int32_t)control_y * 44L / 1000L;
    int32_t radius2 = dx * dx + dy * dy;
    uint16_t norm = 45U, dot_x, dot_y, text_x = center_x - 104U;
    int32_t move_x, move_y;
    uint8_t centered;
    /* Keep both 17x17 dirty rectangles inside the static outline. */
    if(radius2 > 44L * 44L) {
        while((int32_t)norm * norm < radius2) norm++;
        dx = dx * 44L / norm; dy = dy * 44L / norm;
    }
    dot_x = (uint16_t)((int32_t)center_x + dx);
    dot_y = (uint16_t)((int32_t)center_y + dy);
    move_x = (int32_t)dot_x - *old_x; move_y = (int32_t)dot_y - *old_y;
    centered = control_x == 0 && control_y == 0;
    if(first_draw) {
        LCD_SetTextColor(WHITE);
        ILI9806G_DrawLine(center_x - 56U, center_y, center_x + 56U, center_y);
        ILI9806G_DrawLine(center_x, center_y - 56U, center_x, center_y + 56U);
        LCD_SetTextColor(color);
        ILI9806G_DrawCircle(center_x, center_y, 56U, 0U);
    }
    if(first_draw || move_x >= 2L || move_x <= -2L || move_y >= 2L || move_y <= -2L ||
       (centered && (move_x || move_y))) {
        gui_robot_dot_patch(center_x, center_y, dot_x - 8U, dot_y - 8U, dot_x, dot_y, color);
        if(!first_draw)
            gui_robot_dot_patch(center_x, center_y, *old_x - 8U, *old_y - 8U, dot_x, dot_y, color);
        *old_x = dot_x; *old_y = dot_y;
    }
    if(!first_draw && (!draw_text || (raw_x == *old_raw_x && raw_y == *old_raw_y))) return;
    LCD_SetFont(&Font16x32); LCD_SetBackColor(GREY); LCD_SetTextColor(BLACK);
    snprintf(displayBuffer, sizeof(displayBuffer), "X%+5d Y%+5d", control_x, control_y);
    gui_robot_text(text_x, 416U, 13U, displayBuffer);
    LCD_SetFont(&Font8x16);
    snprintf(displayBuffer, sizeof(displayBuffer), "ADC X:%4u Y:%4u", raw_x, raw_y);
    gui_robot_text(text_x, 452U, 26U, displayBuffer);
    *old_raw_x = raw_x; *old_raw_y = raw_y;
}

void robot_control_page(const GuiRobotTelemetry *telemetry)
{
    static GuiRobotTelemetry previous;
    static uint8_t snapshot_valid;
    static uint16_t battery_bar_width;
    static uint16_t old_dot_x[2], old_dot_y[2], old_raw_x[2], old_raw_y[2];
    static GuiRobotFilter filters[4];
    static uint32_t last_numeric_ms;
    static char previous_status[48];
    unsigned long ticks;
    uint32_t now;
    uint8_t draw_text, first_draw = 0U, telemetry_changed, battery, card;
    uint16_t raw[4];
    int16_t left_x, left_y, right_x, right_y;
    const char *fix_text, *status;
    if(!telemetry) return;
    get_tick_count(&ticks); now = (uint32_t)ticks;
    if(display_flag == 1U) {
        display_flag = 0U; GTP_IRQ_Disable(); first_draw = 1U;
        snapshot_valid = 0U; battery_bar_width = 0U;
        old_raw_x[0] = old_raw_x[1] = old_raw_y[0] = old_raw_y[1] = 0xffffU;
        previous_status[0] = '\0';
    }
    if(first_draw) {
        LCD_SetTextColor(BLUE2);
        ILI9806G_DrawRectangle(4U, 0U, 792U, 64U, 1U);
        LCD_SetBackColor(BLUE2); LCD_SetTextColor(WHITE); LCD_SetFont(&Font16x32);
        gui_robot_text(20U, 0U, 40U, "ROBOT CONTROL / TELEMETRY");
        gui_clear_page_content();
        LCD_SetTextColor(GREY);
        ILI9806G_DrawRectangle(4U, 72U, 792U, 32U, 1U);
        gui_robot_card(4U, 112U, 260U, 152U, "MOTION / POSITION");
        gui_robot_card(272U, 112U, 260U, 152U, "ATTITUDE");
        gui_robot_card(540U, 112U, 256U, 152U, "POWER / GPS STATUS");
        gui_robot_card(4U, 272U, 260U, 204U, "LEFT JOYSTICK / CH03-X CH02-Y");
        gui_robot_card(272U, 272U, 256U, 204U, "ACCELERATION / GPS");
        gui_robot_card(536U, 272U, 260U, 204U, "RIGHT JOYSTICK / CH06-X CH05-Y");
        gui_clear_page_band(104U, 8U); gui_clear_page_band(264U, 8U);
        gui_clear_page_band(476U, 4U);
        ILI9806G_Fill(264U, 112U, 272U, 476U, WHITE);
        ILI9806G_Fill(532U, 112U, 540U, 264U, WHITE);
        ILI9806G_Fill(528U, 272U, 536U, 476U, WHITE);
    }
    /* A wall-clock gate is stable even if menu or LCD processing is delayed. */
    draw_text = first_draw || (uint32_t)(now - last_numeric_ms) >= 250U;
    if(draw_text) last_numeric_ms = now;
    telemetry_changed = !snapshot_valid || previous.link_online != telemetry->link_online ||
        (telemetry->link_online && draw_text && memcmp(&previous, telemetry, sizeof(previous)) != 0);
    if(telemetry_changed) {
        previous = *telemetry; snapshot_valid = 1U;
        LCD_SetFont(&Font8x16); LCD_SetBackColor(GREY);
        LCD_SetTextColor(telemetry->link_online ? GREEN : RED);
        gui_robot_text(16U, 80U, 14U, telemetry->link_online ? "DATA:ONLINE" : "DATA:NONE");
        LCD_SetTextColor(BLACK);
        if(!telemetry->link_online) {
            static const uint16_t x[] = {20U,288U,556U,288U};
            static const uint16_t y[] = {136U,136U,136U,296U};
            gui_robot_text(144U, 80U, 80U, "NO ROBOT TELEMETRY                       BACK:RETURN");
            /* Do not draw numeric placeholders and then attempt to cover them. */
            ILI9806G_Fill(12U,132U,256U,256U,GREY);
            ILI9806G_Fill(280U,132U,524U,256U,GREY);
            ILI9806G_Fill(548U,132U,788U,256U,GREY);
            ILI9806G_Fill(280U,292U,520U,468U,GREY);
            LCD_SetFont(&Font16x32); LCD_SetTextColor(RED);
            for(card = 0U; card < 4U; card++) gui_robot_text(x[card],y[card],14U,"NO TELEMETRY");
            battery_bar_width = 0U;
        } else {
            battery = telemetry->battery_percent > 100U ? 100U : telemetry->battery_percent;
            fix_text = telemetry->gps_fix >= 3U ? "3D" : telemetry->gps_fix == 2U ? "2D" : "NO";
            snprintf(displayBuffer,sizeof(displayBuffer),"PKT:%08lu AGE:%5ums GPS:%s SAT:%02u BACK:RETURN",
                     (unsigned long)telemetry->packet_count,telemetry->packet_age_ms,fix_text,telemetry->satellites);
            gui_robot_text(144U,80U,80U,displayBuffer);
            LCD_SetFont(&Font16x32);
            snprintf(displayBuffer,sizeof(displayBuffer),"SPD %+6.2f",telemetry->speed_mps);
            gui_robot_text(20U,136U,14U,displayBuffer);
            snprintf(displayBuffer,sizeof(displayBuffer),"X%+5.1f Y%+5.1f",telemetry->position_x_m,telemetry->position_y_m);
            gui_robot_text(20U,174U,14U,displayBuffer);
            snprintf(displayBuffer,sizeof(displayBuffer),"Z %+6.2f m",telemetry->position_z_m);
            gui_robot_text(20U,212U,14U,displayBuffer);
            snprintf(displayBuffer,sizeof(displayBuffer),"ROLL %+6.1f",telemetry->roll_deg);
            gui_robot_text(288U,136U,14U,displayBuffer);
            snprintf(displayBuffer,sizeof(displayBuffer),"PITCH%+6.1f",telemetry->pitch_deg);
            gui_robot_text(288U,174U,14U,displayBuffer);
            snprintf(displayBuffer,sizeof(displayBuffer),"YAW %+7.2f",telemetry->yaw_deg);
            gui_robot_text(288U,212U,14U,displayBuffer);
            snprintf(displayBuffer,sizeof(displayBuffer),"VOLT %6.2f V",telemetry->voltage_v);
            gui_robot_text(556U,136U,14U,displayBuffer);
            snprintf(displayBuffer,sizeof(displayBuffer),"BAT %3u%%",battery);
            gui_robot_text(556U,174U,14U,displayBuffer);
            gui_update_progress_bar(556U,214U,220U,16U,(uint32_t)battery*10UL,
                battery>20U ? GREEN : RED,&battery_bar_width,first_draw || !battery_bar_width);
            LCD_SetFont(&Font8x16);
            snprintf(displayBuffer,sizeof(displayBuffer),"GPS:%s SAT:%02u ALT:%+.1fm",fix_text,telemetry->satellites,telemetry->gps_altitude_m);
            gui_robot_text(556U,240U,28U,displayBuffer);
            LCD_SetFont(&Font16x32);
            snprintf(displayBuffer,sizeof(displayBuffer),"X%+5.2f Y%+5.2f",telemetry->acceleration_x_mps2,telemetry->acceleration_y_mps2);
            gui_robot_text(288U,296U,14U,displayBuffer);
            snprintf(displayBuffer,sizeof(displayBuffer),"AZ %+6.2f m/s2",telemetry->acceleration_z_mps2);
            gui_robot_text(288U,332U,14U,displayBuffer);
            snprintf(displayBuffer,sizeof(displayBuffer),"N %+10.5f",telemetry->latitude_deg);
            gui_robot_text(288U,368U,14U,displayBuffer);
            snprintf(displayBuffer,sizeof(displayBuffer),"E %+10.5f",telemetry->longitude_deg);
            gui_robot_text(288U,404U,14U,displayBuffer);
        }
    }
    status = control_link_status();
    if(first_draw || strcmp(previous_status,status)) {
        snprintf(previous_status,sizeof(previous_status),"%s",status);
        LCD_SetFont(&Font8x16); LCD_SetBackColor(BLUE2); LCD_SetTextColor(WHITE);
        snprintf(displayBuffer,sizeof(displayBuffer),"CONTROL: %s",previous_status);
        gui_robot_text(20U,40U,95U,displayBuffer);
    }
    raw[0] = gui_robot_filter_update(&filters[0],ADC1_Value[ROBOT_LEFT_X_ADC_INDEX],first_draw);
    raw[1] = gui_robot_filter_update(&filters[1],ADC1_Value[ROBOT_LEFT_Y_ADC_INDEX],first_draw);
    raw[2] = gui_robot_filter_update(&filters[2],ADC1_Value[ROBOT_RIGHT_X_ADC_INDEX],first_draw);
    raw[3] = gui_robot_filter_update(&filters[3],ADC1_Value[ROBOT_RIGHT_Y_ADC_INDEX],first_draw);
    left_x = -gui_robot_stick_value(raw[0],ROBOT_LEFT_X_ADC_INDEX);
    left_y = gui_robot_stick_value(raw[1],ROBOT_LEFT_Y_ADC_INDEX);
    right_x = -gui_robot_stick_value(raw[2],ROBOT_RIGHT_X_ADC_INDEX);
    right_y = gui_robot_stick_value(raw[3],ROBOT_RIGHT_Y_ADC_INDEX);
    gui_robot_draw_stick(134U,356U,raw[0],raw[1],left_x,left_y,BLUE2,
        &old_dot_x[0],&old_dot_y[0],&old_raw_x[0],&old_raw_y[0],draw_text,first_draw);
    gui_robot_draw_stick(666U,356U,raw[2],raw[3],right_x,right_y,GREEN,
        &old_dot_x[1],&old_dot_y[1],&old_raw_x[1],&old_raw_y[1],draw_text,first_draw);
    LCD_SetBackColor(WHITE); LCD_SetTextColor(BLACK);
}
