#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui.h"
#include "gui_analog_filter.h"

enum { WHITE, GREY, BLACK, BLUE, BLUE2, GREEN, RED };
enum { LCD_X_LENGTH = 800, LCD_Y_LENGTH = 480 };
static int Font16x32, Font8x16;
static uint16_t ADC1_Value[7], ADC3_Value[3];
static struct { uint16_t chLower[7], chMiddle[7], chUpper[7]; uint8_t chReverse[7]; } param;
static char displayBuffer[100];
static uint8_t display_flag;
static unsigned circles, lines, adc_texts, stick_texts, telemetry_texts;
static uint16_t displayed_adc[10];

static void LCD_SetFont(void *font) { (void)font; }
static void LCD_SetBackColor(uint16_t color) { (void)color; }
static void LCD_SetTextColor(uint16_t color) { (void)color; }
static void GTP_IRQ_Disable(void) {}
static void ILI9806G_Fill(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2, uint16_t color)
{ (void)x; (void)y; (void)x2; (void)y2; (void)color; }
static void ILI9806G_DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t fill)
{ (void)x; (void)y; (void)w; (void)h; (void)fill; }
static void ILI9806G_DrawCircle(uint16_t x, uint16_t y, uint16_t r, uint8_t fill)
{ (void)x; (void)y; (void)r; (void)fill; circles++; }
static void ILI9806G_DrawLine(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2)
{ (void)x; (void)y; (void)x2; (void)y2; lines++; }
static void ILI9806G_DispString_EN(uint16_t x, uint16_t y, char *text)
{
    if((x == 92U || x == 492U) && y >= 87U && y <= 343U && (y - 87U) % 64U == 0U)
    {
        displayed_adc[(y - 87U) / 64U + (x == 492U ? 5U : 0U)] = (uint16_t)atoi(text);
        adc_texts++;
    }
    if(y == 416U || y == 452U) stick_texts++;
    if(x == 16U && y == 80U) telemetry_texts++;
}

/* Runner extracts these complete functions from the production gui.c. */
#include "gui_analog_functions.inc"

static void test_filter(void)
{
    GuiAnalogFilter filter;
    unsigned i;
    uint16_t value, previous;
    assert(gui_analog_filter_update(&filter, 2048U, 1U) == 2048U);
    for(i = 0U; i < 1000U; i++)
        assert(gui_analog_filter_update(&filter, (uint16_t)(2048 + (int)(i % 13U) - 6), 0U) == 2048U);
    previous = 2048U;
    for(i = 0U; i < 4U; i++)
    {
        value = gui_analog_filter_update(&filter, 4095U, 0U);
        assert(value >= previous && value <= 4095U);
        previous = value;
    }
    assert(value > 3900U); /* At least 90% of the movement in 200 ms. */
    for(i = 0U; i < 60U; i++) value = gui_analog_filter_update(&filter, 4095U, 0U);
    assert(value == 4095U);
    for(i = 0U; i < 60U; i++) value = gui_analog_filter_update(&filter, 0U, 0U);
    assert(value == 0U);
    assert(gui_analog_filter_update(&filter, 3000U, 1U) == 3000U);
    assert(gui_analog_filter_update(&filter, 65535U, 1U) == 4095U);
    gui_analog_filter_update(&filter, 2000U, 1U);
    for(i = 0U; i < 100U; i++) value = gui_analog_filter_update(&filter, 2016U, 0U);
    assert(value >= 2008U && value <= 2016U); /* Small sustained changes still register. */
}

static void test_monitor(void)
{
    unsigned i, frame, before;
    for(i = 0U; i < 7U; i++) ADC1_Value[i] = (uint16_t)(1000U + i * 100U);
    for(i = 0U; i < 3U; i++) ADC3_Value[i] = (uint16_t)(2000U + i * 100U);
    display_flag = 1U;
    adc_texts = 0U;
    channel_monitor_page();
    assert(adc_texts == 10U && displayed_adc[0] == 1000U && displayed_adc[9] == 2200U);
    for(frame = 0U; frame < 80U; frame++)
    {
        ADC1_Value[0] = (uint16_t)(1000 + (int)(frame % 11U) - 5);
        channel_monitor_page();
    }
    assert(adc_texts == 10U);
    ADC1_Value[0] = 4095U;
    before = adc_texts;
    for(frame = 0U; frame < 20U; frame++) channel_monitor_page();
    assert(adc_texts - before <= 5U && displayed_adc[0] > 4000U);
    ADC1_Value[0] = 1234U;
    display_flag = 1U;
    channel_monitor_page();
    assert(displayed_adc[0] == 1234U); /* Re-entry seeds from current input. */
}

static void test_robot(void)
{
    GuiRobotTelemetry telemetry = {0};
    unsigned i, frame, before;
    for(i = 0U; i < 7U; i++) ADC1_Value[i] = 2047U;
    display_flag = 1U;
    robot_control_page(&telemetry);
    circles = lines = stick_texts = 0U;
    for(frame = 0U; frame < 80U; frame++)
    {
        for(i = 0U; i < 7U; i++) ADC1_Value[i] = (uint16_t)(2047 + (int)(frame % 11U) - 5);
        robot_control_page(&telemetry);
    }
    assert(circles == 0U && lines == 0U && stick_texts == 0U);
    ADC1_Value[2] = 4095U;
    for(frame = 0U; frame < 4U; frame++) robot_control_page(&telemetry);
    assert(circles > 0U && stick_texts == 2U);
    before = telemetry_texts;
    for(frame = 0U; frame < 20U; frame++)
    {
        telemetry.packet_count++;
        robot_control_page(&telemetry);
    }
    assert(telemetry_texts - before == 5U);
    before = telemetry_texts;
    telemetry.link_online = 1U;
    robot_control_page(&telemetry);
    assert(telemetry_texts == before + 1U);
    circles = 0U;
    display_flag = 1U;
    robot_control_page(&telemetry);
    assert(circles == 4U); /* Both widgets redraw on re-entry. */
}

static void test_dot_pixel_gate(void)
{
    uint16_t x = 0U, y = 0U, raw_x = 0U, raw_y = 0U;
    gui_robot_draw_stick(134U, 356U, 2047U, 2047U, 0, 0, BLUE,
                         &x, &y, &raw_x, &raw_y, 1U, 1U);
    circles = lines = stick_texts = 0U;
    gui_robot_draw_stick(134U, 356U, 2057U, 2047U, 5, 0, BLUE,
                         &x, &y, &raw_x, &raw_y, 1U, 0U);
    assert(circles == 0U && lines == 0U && stick_texts == 2U);
    gui_robot_draw_stick(134U, 356U, 2100U, 2047U, 25, 0, BLUE,
                         &x, &y, &raw_x, &raw_y, 0U, 0U);
    assert(circles == 0U && x == 134U);
    gui_robot_draw_stick(134U, 356U, 2200U, 2047U, 75, 0, BLUE,
                         &x, &y, &raw_x, &raw_y, 0U, 0U);
    assert(circles == 3U && lines == 2U && x == 137U);
}

int main(void)
{
    test_filter();
    test_monitor();
    test_robot();
    test_dot_pixel_gate();
    puts("GUI analog tests passed: filtering, response, endpoints, cadence, page reset, dot redraw, link status.");
    return 0;
}
