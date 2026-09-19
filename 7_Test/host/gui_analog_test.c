#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "gui.h"
#include "gui_analog_filter.h"
#include "gui_robot_filter.h"

enum { WHITE, GREY, BLACK, BLUE, BLUE2, GREEN, RED };
enum { LCD_X_LENGTH = 800, LCD_Y_LENGTH = 480 };
static int Font16x32, Font8x16;
static uint16_t ADC1_Value[7], ADC3_Value[3];
static struct { uint16_t chLower[7], chMiddle[7], chUpper[7]; uint8_t chReverse[7]; int PWMadjustValue[7]; } param;
static const char *fake_status = "DISARMED";
static uint32_t fake_ms;
static const char *control_link_status(void) { return fake_status; }
static int get_tick_count(unsigned long *count) { *count = fake_ms; return 0; }
static char displayBuffer[100];
static uint8_t display_flag;
static unsigned circles, lines, adc_texts, stick_texts, telemetry_texts;
static unsigned blits, fills, numeric_telemetry_texts, status_texts;
static unsigned font_width = 16U, font_height = 32U;
static uint8_t checking_robot;
static uint16_t text_raw_x[2], text_raw_y[2];
static uint16_t displayed_adc[10];

static void LCD_SetFont(void *font)
{ font_width = font == &Font8x16 ? 8U : 16U; font_height = font_width * 2U; }
static void LCD_SetBackColor(uint16_t color) { (void)color; }
static void LCD_SetTextColor(uint16_t color) { (void)color; }
static void GTP_IRQ_Disable(void) {}
static void ILI9806G_Fill(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2, uint16_t color)
{ (void)x; (void)y; (void)x2; (void)y2; (void)color; fills++; }
static void ILI9806G_DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t fill)
{ (void)x; (void)y; (void)w; (void)h; (void)fill; }
static void ILI9806G_DrawCircle(uint16_t x, uint16_t y, uint16_t r, uint8_t fill)
{ (void)x; (void)y; (void)r; (void)fill; circles++; }
static void ILI9806G_DrawLine(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2)
{ (void)x; (void)y; (void)x2; (void)y2; lines++; }
static void LCD_BlitRGB565(uint16_t x, uint16_t y, uint16_t width,
                           uint16_t height, const uint16_t *pixels)
{
    assert(width == 17U && height == 17U && pixels != NULL);
    assert(x + width <= LCD_X_LENGTH && y + height <= LCD_Y_LENGTH);
    blits++;
}
static void ILI9806G_DispString_EN(uint16_t x, uint16_t y, char *text)
{
    if(checking_robot) {
        unsigned right = x + (unsigned)strlen(text) * font_width;
        assert(right <= LCD_X_LENGTH && y + font_height <= LCD_Y_LENGTH);
        if(y >= 112U && y < 272U)
            assert(right <= (x < 264U ? 264U : x < 532U ? 532U : 796U));
        if(y >= 272U)
            assert(right <= (x < 264U ? 264U : x < 528U ? 528U : 796U));
        if(y == 136U || y == 174U || y == 212U || y == 240U ||
           y == 296U || y == 332U || y == 368U || y == 404U) {
            if(strncmp(text, "NO TELEMETRY", 12U)) numeric_telemetry_texts++;
        }
        if(y == 40U) status_texts++;
        if(y == 452U) {
            unsigned raw_x, raw_y, side = x < 400U ? 0U : 1U;
            assert(sscanf(text, "ADC X:%u Y:%u", &raw_x, &raw_y) == 2);
            text_raw_x[side] = (uint16_t)raw_x; text_raw_y[side] = (uint16_t)raw_y;
        }
    }
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
    uint16_t value;
    assert(gui_analog_filter_update(&filter, 2048U, 1U) == 2048U);
    for(i = 0U; i < 1000U; i++)
        assert(gui_analog_filter_update(&filter, (uint16_t)(2048 + (int)(i % 13U) - 6), 0U) == 2048U);
    assert(gui_analog_filter_update(&filter, 4095U, 0U) == 4095U);
    assert(gui_analog_filter_update(&filter, 0U, 0U) == 0U);
    assert(gui_analog_filter_update(&filter, 1200U, 0U) == 1200U);
    assert(gui_analog_filter_update(&filter, 1232U, 0U) == 1232U);
    assert(gui_analog_filter_update(&filter, 1200U, 0U) == 1200U);
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
    channel_monitor_page();
    assert(adc_texts == before + 1U && displayed_adc[0] == 4095U);
    ADC1_Value[0] = 0U;
    channel_monitor_page();
    assert(adc_texts == before + 2U && displayed_adc[0] == 0U);
    ADC1_Value[0] = 1234U;
    display_flag = 1U;
    channel_monitor_page();
    assert(displayed_adc[0] == 1234U); /* Re-entry seeds from current input. */
}

static void robot_enter(GuiRobotTelemetry *telemetry, uint32_t now)
{
    unsigned i;
    for(i = 0U; i < 7U; i++) ADC1_Value[i] = 2047U;
    checking_robot = 1U; fake_ms = now; fake_status = "DISARMED";
    display_flag = 1U;
    circles = lines = blits = fills = stick_texts = telemetry_texts = 0U;
    numeric_telemetry_texts = status_texts = 0U;
    robot_control_page(telemetry);
    assert(circles == 2U && lines == 4U && blits == 2U);
}

static void test_robot_noise_and_response(void)
{
    GuiRobotTelemetry telemetry = {0};
    unsigned i, frame, before;
    robot_enter(&telemetry, 0U);
    assert(numeric_telemetry_texts == 0U && status_texts == 1U);
    circles = lines = blits = 0U;
    before = fills;
    for(frame = 0U; frame < 160U; frame++) {
        for(i = 0U; i < 7U; i++)
            ADC1_Value[i] = (uint16_t)(2047 + (int)((frame * 73U) % 161U) - 80);
        telemetry.packet_count++; telemetry.packet_age_ms++;
        fake_ms += 20U; robot_control_page(&telemetry);
    }
    assert(blits == 0U && circles == 0U && lines == 0U);
    assert(fills == before && telemetry_texts == 1U && numeric_telemetry_texts == 0U);
    assert(status_texts == 1U);
    robot_enter(&telemetry, 0U);
    blits = 0U;
    ADC1_Value[2] = 4095U; fake_ms += 20U; robot_control_page(&telemetry);
    ADC1_Value[2] = 2047U; fake_ms += 20U; robot_control_page(&telemetry);
    fake_ms += 20U; robot_control_page(&telemetry);
    assert(blits == 0U); /* One ADC spike must not move the marker. */
    ADC1_Value[2] = 4095U;
    fake_ms += 20U; robot_control_page(&telemetry);
    fake_ms += 20U; robot_control_page(&telemetry);
    assert(blits == 2U && circles == 2U && lines == 4U);
    ADC1_Value[2] = 0U;
    fake_ms += 20U; robot_control_page(&telemetry);
    fake_ms += 20U; robot_control_page(&telemetry);
    assert(blits == 4U); /* A deliberate reversal follows within two frames. */
    fake_status = "HOLD DCH1";
    before = status_texts; robot_control_page(&telemetry);
    assert(status_texts == before + 1U);
    robot_control_page(&telemetry); assert(status_texts == before + 1U);
}

static void test_robot_clock_cadence(void)
{
    GuiRobotTelemetry telemetry = {0};
    unsigned call, interval, before;
    robot_enter(&telemetry, 0U);
    stick_texts = 0U;
    for(interval = 1U; interval <= 4U; interval++) {
        ADC1_Value[2] = interval & 1U ? 4095U : 0U;
        for(call = 0U; call < 1000U; call++) robot_control_page(&telemetry);
        assert(stick_texts == 2U * (interval - 1U));
        fake_ms = interval * 250U - 1U; robot_control_page(&telemetry);
        assert(stick_texts == 2U * (interval - 1U));
        fake_ms++; robot_control_page(&telemetry);
        assert(stick_texts == 2U * interval);
        assert(text_raw_x[0] == ADC1_Value[2]);
    }
    /* Delayed menu calls produce one latest redraw, not a catch-up burst. */
    ADC1_Value[2] = 4095U;
    robot_control_page(&telemetry); robot_control_page(&telemetry);
    before = stick_texts; fake_ms += 10000U; robot_control_page(&telemetry);
    assert(stick_texts == before + 2U);
    robot_control_page(&telemetry); assert(stick_texts == before + 2U);
    robot_enter(&telemetry, UINT32_MAX - 100U);
    ADC1_Value[2] = 4095U;
    robot_control_page(&telemetry); robot_control_page(&telemetry);
    before = stick_texts;
    fake_ms = 148U; robot_control_page(&telemetry); assert(stick_texts == before);
    fake_ms = 149U; robot_control_page(&telemetry); assert(stick_texts == before + 2U);
    display_flag = 1U; robot_control_page(&telemetry);
    assert(text_raw_x[0] == 4095U); /* Page re-entry seeds current input. */
}

static void test_robot_telemetry_and_bounds(void)
{
    GuiRobotTelemetry telemetry = {0};
    unsigned i, before;
    robot_enter(&telemetry, 0U);
    telemetry.link_online = 1U;
    telemetry.speed_mps = telemetry.position_x_m = telemetry.position_y_m = FLT_MAX;
    telemetry.position_z_m = -FLT_MAX;
    telemetry.acceleration_x_mps2 = telemetry.acceleration_y_mps2 = FLT_MAX;
    telemetry.acceleration_z_mps2 = -FLT_MAX;
    telemetry.roll_deg = telemetry.pitch_deg = telemetry.yaw_deg = FLT_MAX;
    telemetry.voltage_v = telemetry.gps_altitude_m = FLT_MAX;
    telemetry.latitude_deg = DBL_MAX; telemetry.longitude_deg = -DBL_MAX;
    telemetry.packet_count = UINT32_MAX; telemetry.packet_age_ms = UINT16_MAX;
    telemetry.battery_percent = telemetry.satellites = telemetry.gps_fix = UINT8_MAX;
    before = telemetry_texts;
    robot_control_page(&telemetry);
    assert(telemetry_texts == before + 1U && numeric_telemetry_texts == 13U);
    before = telemetry_texts;
    for(i = 1U; i <= 1000U; i++) {
        fake_ms = i; telemetry.packet_count++; robot_control_page(&telemetry);
    }
    assert(telemetry_texts == before + 4U);
    telemetry.link_online = 0U;
    before = numeric_telemetry_texts;
    robot_control_page(&telemetry);
    assert(numeric_telemetry_texts == before);
    before = telemetry_texts;
    for(i = 0U; i < 20U; i++) {
        telemetry.packet_count++; fake_ms += 250U; robot_control_page(&telemetry);
    }
    assert(telemetry_texts == before); /* Offline payload changes are not visible data. */
}

static void test_dot_pixel_gate(void)
{
    uint16_t x = 0U, y = 0U, raw_x = 0U, raw_y = 0U;
    gui_robot_draw_stick(134U, 356U, 2047U, 2047U, 0, 0, BLUE,
                         &x, &y, &raw_x, &raw_y, 1U, 1U);
    circles = lines = blits = stick_texts = 0U;
    gui_robot_draw_stick(134U, 356U, 2057U, 2047U, 5, 0, BLUE,
                         &x, &y, &raw_x, &raw_y, 1U, 0U);
    assert(circles == 0U && lines == 0U && blits == 0U && stick_texts == 2U);
    gui_robot_draw_stick(134U, 356U, 2100U, 2047U, 25, 0, BLUE,
                         &x, &y, &raw_x, &raw_y, 0U, 0U);
    assert(blits == 0U && x == 134U);
    gui_robot_draw_stick(134U, 356U, 2200U, 2047U, 100, 0, BLUE,
                         &x, &y, &raw_x, &raw_y, 0U, 0U);
    assert(blits == 2U && circles == 0U && lines == 0U && x > 134U);
}

int main(void)
{
    test_filter(); test_monitor();
    test_robot_noise_and_response(); test_robot_clock_cadence();
    test_robot_telemetry_and_bounds(); test_dot_pixel_gate();
    puts("GUI analog tests passed: neutral noise/spikes, two-frame response, 250 ms cadence/wrap, offline/status redraw, card bounds, marker blits.");
    return 0;
}
