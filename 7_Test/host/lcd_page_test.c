#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include "fonts.h"
#include "gui.h"
#include "gui_analog_filter.h"
#include "gui_robot_filter.h"
#include "diagnostics.h"
#include "hardware_tests.h"
#include "menu.h"

#define WHITE 0xffffU
#define GREY 0xf7deU
#define BLACK 0U
#define BLUE 0x001fU
#define BLUE2 0x051fU
#define GREEN 0x07e0U
#define RED 0xf800U
#define YELLOW 0xffe0U
#define CMD_SetCoordinateX 0x2aU
#define CMD_SetCoordinateY 0x2bU
#define CMD_SetPixel 0x2cU
#define ILI9806G_DispWindow_X_Star 0U
#define ILI9806G_DispWindow_Y_Star 0U

static uint16_t sram[800U * 480U + 16U];
#define SRAM_BASE_ADDR ((uintptr_t)sram)
static uint16_t panel[800U * 480U], expected[800U * 480U], old_frame[800U * 480U];
uint16_t LCD_X_LENGTH = 800U, LCD_Y_LENGTH = 480U;
static sFONT *LCD_Currentfonts = &Font16x32;
static uint16_t CurrentTextColor = BLACK, CurrentBackColor = WHITE;
static struct { uint32_t CYCCNT; } test_cycle_counter;
#define DWT (&test_cycle_counter)
static uint32_t SystemCoreClock = 168000000UL;
static uint32_t bus_writes, pixel_writes, memory_commands;
static uint16_t command, x0, x1, y0, y1, cursor_x, cursor_y, coord;
static unsigned parameter;
static uint8_t watch_overlap;
static uint16_t watch_old_x, watch_old_y, watch_new_x, watch_new_y;
static unsigned overlap_writes;
static uint32_t fake_ms;
static int get_tick_count(unsigned long *count) { *count = fake_ms; return 0; }

static void ILI9806G_Write_Cmd(uint16_t cmd)
{
    command = cmd;
    parameter = 0U;
    bus_writes++;
    if(cmd == CMD_SetPixel) { cursor_x = x0; cursor_y = y0; memory_commands++; }
}
static void ILI9806G_Write_Data(uint16_t data)
{
    bus_writes++;
    if(command == CMD_SetPixel)
    {
        if(watch_overlap) {
            int32_t old_dx = (int32_t)cursor_x - watch_old_x;
            int32_t old_dy = (int32_t)cursor_y - watch_old_y;
            int32_t new_dx = (int32_t)cursor_x - watch_new_x;
            int32_t new_dy = (int32_t)cursor_y - watch_new_y;
            if(old_dx * old_dx + old_dy * old_dy <= 64L &&
               new_dx * new_dx + new_dy * new_dy <= 64L) {
                assert(data == BLUE2); /* No transient clearing of shared dot pixels. */
                overlap_writes++;
            }
        }
        if(cursor_x < LCD_X_LENGTH && cursor_y < LCD_Y_LENGTH)
            panel[(uint32_t)cursor_y * LCD_X_LENGTH + cursor_x] = data;
        pixel_writes++;
        if(++cursor_x > x1) { cursor_x = x0; if(++cursor_y > y1) cursor_y = y0; }
    }
    else
    {
        assert(command == CMD_SetCoordinateX || command == CMD_SetCoordinateY);
        coord = parameter % 2U == 0U ? (uint16_t)(data << 8) : (uint16_t)(coord | data);
        if(parameter == 1U) { if(command == CMD_SetCoordinateX) x0 = coord; else y0 = coord; }
        if(parameter == 3U) { if(command == CMD_SetCoordinateX) x1 = coord; else y1 = coord; }
        parameter++;
    }
}
static uint16_t ILI9806G_Read_PixelData(void) { return 0U; }

#include "lcd_page_driver.inc"

static uint8_t display_flag, clock_force_redraw;
static uint16_t ADC1_Value[7] = {2047, 2047, 2047, 2047, 2047, 2047, 2047};
static uint16_t ADC3_Value[3] = {1024, 2048, 3072};
static struct { uint16_t chLower[7], chMiddle[7], chUpper[7]; uint8_t chReverse[7]; int PWMadjustValue[7]; } param;
static const char *control_link_status(void) { return "DISARMED"; }
static char displayBuffer[100];
static void GTP_IRQ_Disable(void) {}
static void gui_boot_menu_badge(void) {}

#include "lcd_page_gui.inc"

static uint8_t key_levels[4];
static unsigned delay_calls, replay_entry;
static uint8_t read_button_left_gpio(uint8_t id) { (void)id; return !key_levels[0]; }
static uint8_t read_button_right_gpio(uint8_t id) { (void)id; return !key_levels[1]; }
static uint8_t read_button_ok_gpio(uint8_t id) { (void)id; return !key_levels[2]; }
static uint8_t read_button_back_gpio(uint8_t id) { (void)id; return !key_levels[3]; }
static void Delay_ms(unsigned ms)
{
    assert(ms == 10U);
    assert(++delay_calls < 100U);
    if(replay_entry) {
        /* The complete screen must be visible even with entry OK still held. */
        assert(lcd_page_active == 0U);
        assert(memcmp(panel, expected, sizeof(panel)) == 0);
        if(delay_calls == 4U) key_levels[MENU_KEY_OK] = 0U;
        if(delay_calls == 10U) key_levels[MENU_KEY_BACK] = 1U;
    }
}
/* Glyph shapes are mocked here; production layout/fills and LCD bus are real. */
static void ILI9806G_DisplayStringEx(uint16_t x, uint16_t y, uint16_t w,
                                    uint16_t h, uint8_t *text, uint16_t mode)
{
    (void)mode;
    while(*text) {
        uint16_t width = *text > 0x80U && text[1] ? w : w / 2U;
        ILI9806G_Fill(x, y, x + width - 1U, y + h - 1U, CurrentBackColor);
        ILI9806G_Fill(x + 1U, y + 1U, x + width - 2U, y + h - 2U, CurrentTextColor);
        text += width == w ? 2U : 1U;
        x += width;
    }
}
static uint8_t confirm(const char *text) { (void)text; assert(0); return 0U; }
static HwResult lcd_test(void) { assert(0); return HW_FAIL; }
static HwResult keys_test(void) { assert(0); return HW_FAIL; }
static HwResult analog_test(void) { assert(0); return HW_FAIL; }
static HwResult battery_test(void) { assert(0); return HW_FAIL; }
static HwResult outputs_test(void) { assert(0); return HW_FAIL; }
HwResult hardware_uart_loopback_test(void) { assert(0); return HW_FAIL; }
HwResult hardware_radio_test(uint8_t receive) { (void)receive; assert(0); return HW_FAIL; }
HwResult hardware_memory_test(void) { assert(0); return HW_FAIL; }
const char *hardware_result_name(HwResult result) { (void)result; return "mock"; }
#include "lcd_page_diag.inc"

static void test_diagnostics(void)
{
    /* Generate the reference through the actual menu, including its own labels. */
    unsigned i;
    LCD_PageBuffer_Enable(0U);
    for(i = 0U; i < 800U * 480U; i++) panel[i] = WHITE;
    key_levels[MENU_KEY_BACK] = 0U;
    key_levels[MENU_KEY_OK] = 0U;
    /* The reference uses the same full draw function and exact menu labels. */
    {
#include "lcd_diag_names.inc"
        uint8_t states[TEST_COUNT] = {0};
        diagnostics_menu_draw(names, states, 0U, 0U);
        diag_present();
    }
    memcpy(expected, panel, sizeof(panel));
    for(i = 0U; i < 800U * 480U; i++) panel[i] = RED;
    LCD_PageBuffer_Enable(1U);
    pixel_writes = memory_commands = delay_calls = 0U;
    key_levels[MENU_KEY_OK] = 1U;
    replay_entry = 1U;
    diagnostics_menu();
    replay_entry = 0U;
    assert(key_levels[MENU_KEY_BACK] == 1U); /* Exit on press, before release. */
    assert(memory_commands == 1U && pixel_writes == 800U * 480U);
    assert(panel[479U * 800U + 799U] == WHITE); /* No old-screen margins. */
    /* Return home immediately, with BACK still down. */
    gui_prepare_page();
    main_menu(0U);
    LCD_EndPage();
    assert(lcd_page_active == 0U && diag_transition_pending == 0U);
    key_levels[MENU_KEY_BACK] = 0U;
    delay_calls = 0U;
    diag_release();
    key_levels[MENU_KEY_OK] = 1U;
    assert(diag_key() == -1);
    key_levels[MENU_KEY_OK] = 0U; /* Bounce must not become a press. */
    assert(diag_key() == -1);
    key_levels[MENU_KEY_OK] = 1U;
    assert(diag_key() == -1 && diag_key() == -1);
    assert(diag_key() == MENU_KEY_OK);
    for(i = 0U; i < 50U; i++) assert(diag_key() == -1);
    assert(delay_calls == 3U); /* diag_key never blocks for release. */
    key_levels[MENU_KEY_OK] = 0U;
}

static void draw_page(unsigned page)
{
    GuiRobotTelemetry telemetry = {0};
    if(page == 1U) channel_monitor_page();
    else if(page == 3U) robot_control_page(&telemetry);
    else main_menu(page == 2U ? 1U : 0U);
    /* A late overlay must be included in the presented image as well. */
    LCD_SetTextColor(BLACK);
    ILI9806G_DrawRectangle(700U, 4U, 92U, 24U, 1U);
    LCD_SetFont(&Font8x16);
    LCD_SetTextColor(WHITE);
    LCD_SetBackColor(BLACK);
    ILI9806G_DispString_EN(714U, 8U, "12:34:56");
}

static void test_transitions(void)
{
    unsigned page, repeat, i;
    for(i = 800U * 480U; i < sizeof(sram) / sizeof(sram[0]); i++) sram[i] = 0x1234U;
    for(repeat = 0U; repeat < 4U; repeat++)
        for(page = 0U; page < 4U; page++)
        {
            /* Independent reference: existing direct rendering onto a clean panel. */
            LCD_PageBuffer_Enable(0U);
            for(i = 0U; i < 800U * 480U; i++) panel[i] = WHITE;
            display_flag = 1U;
            draw_page(page);
            memcpy(expected, panel, sizeof(panel));
            memcpy(panel, old_frame, sizeof(panel));
            LCD_PageBuffer_Enable(1U);
            bus_writes = pixel_writes = memory_commands = 0U;
            gui_prepare_page();
            draw_page(page);
            assert(bus_writes == 0U); /* No visible clearing or partial widgets. */
            assert(memcmp(panel, old_frame, sizeof(panel)) == 0);
            LCD_EndPage();
            if(memory_commands != 1U || pixel_writes != 800U * 480U)
                fprintf(stderr, "page %u: commands=%lu pixels=%lu size=%ux%u active=%u\n", page,
                        (unsigned long)memory_commands, (unsigned long)pixel_writes,
                        lcd_page_width, lcd_page_height, lcd_page_active);
            assert(memory_commands == 1U && pixel_writes == 800U * 480U);
            assert(memcmp(panel, expected, sizeof(panel)) == 0);
            /* A missing SRAM buffer must also clear every old-page pixel. */
            LCD_PageBuffer_Enable(0U);
            memcpy(panel, old_frame, sizeof(panel));
            gui_prepare_page();
            draw_page(page);
            LCD_EndPage();
            assert(memcmp(panel, expected, sizeof(panel)) == 0);
            LCD_PageBuffer_Enable(1U);
            memcpy(old_frame, panel, sizeof(panel));
            for(i = 800U * 480U; i < sizeof(sram) / sizeof(sram[0]); i++) assert(sram[i] == 0x1234U);
            bus_writes = 0U;
            LCD_EndPage();
            assert(bus_writes == 0U);
            ILI9806G_DrawPoint(3U, 3U, RED);
            assert(panel[3U * 800U + 3U] == RED); /* Live writes resume after presentation. */
        }
}

static void test_edges_and_fallback(void)
{
    unsigned i;
    LCD_BeginPage(WHITE);
    ILI9806G_OpenWindow(799U, 479U, 2U, 2U);
    lcd_begin_pixels();
    for(i = 0U; i < 4U; i++) lcd_write_pixel(RED);
    assert(ILI9806G_GetPointPixel(799U, 479U) == RED);
    assert(ILI9806G_GetPointPixel(800U, 479U) == 0U);
    assert(sram[800U * 480U] == 0x1234U);
    ILI9806G_Fill(798U, 478U, 803U, 483U, GREEN);
    assert(ILI9806G_GetPointPixel(799U, 479U) == GREEN);
    assert(ILI9806G_GetPointPixel(797U, 479U) == WHITE);
    assert(sram[800U * 480U] == 0x1234U);
    ILI9806G_OpenWindow(100U, 100U, 5U, 2U);
    ILI9806G_FillColor(7U, RED); /* Partial-window fallback preserves stride. */
    assert(ILI9806G_GetPointPixel(104U, 100U) == RED);
    assert(ILI9806G_GetPointPixel(101U, 101U) == RED);
    assert(ILI9806G_GetPointPixel(102U, 101U) == WHITE);
    LCD_Draw_Rect(10U, 14U, 10U, 14U, BLUE);
    assert(ILI9806G_GetPointPixel(12U, 12U) == BLUE);
    LCD_EndPage();
    LCD_PageBuffer_Enable(0U);
    for(i = 0U; i < 800U * 480U; i++) panel[i] = RED;
    LCD_SetFont(&Font16x32); LCD_SetBackColor(RED); LCD_SetTextColor(BLACK);
    ILI9806G_DispString_EN(288U, 320U, "OLD PAGE TEXT");
    bus_writes = pixel_writes = memory_commands = 0U;
    LCD_BeginPage(WHITE);
    assert(lcd_page_active == 0U);
    LCD_EndPage();
    assert(memory_commands == 1U && pixel_writes == 800U * 480U);
    for(i = 0U; i < 800U * 480U; i++) assert(panel[i] == WHITE);
    assert(CurrentBackColor == RED && CurrentTextColor == BLACK);
    assert(LCD_Currentfonts == &Font16x32);
    ILI9806G_DrawPoint(2U, 2U, GREEN);
    assert(panel[2U * 800U + 2U] == GREEN);
    LCD_PageBuffer_Enable(1U);
    LCD_X_LENGTH = 480U; LCD_Y_LENGTH = 800U;
    LCD_BeginPage(WHITE);
    ILI9806G_DrawPoint(479U, 799U, BLUE);
    LCD_EndPage();
    assert(panel[800U * 480U - 1U] == BLUE);
    LCD_X_LENGTH = 800U; LCD_Y_LENGTH = 480U;
}

static void test_rgb565_blit(void)
{
    static const uint16_t pixels[] = {
        RED, GREEN, BLUE, BLACK,
        BLUE2, YELLOW, GREY, RED,
        BLACK, BLUE, GREEN, YELLOW
    };
    unsigned buffered, row, column, i;
    for(buffered = 0U; buffered < 2U; buffered++)
    {
        LCD_PageBuffer_Enable((uint8_t)buffered);
        for(i = 0U; i < 800U * 480U; i++) panel[i] = WHITE;
        if(buffered != 0U) LCD_BeginPage(WHITE);
        bus_writes = pixel_writes = memory_commands = 0U;
        LCD_BlitRGB565(798U, 478U, 4U, 3U, pixels);
        LCD_BlitRGB565(10U, 20U, 4U, 3U, pixels);
        if(buffered != 0U)
        {
            assert(bus_writes == 0U);
            assert(panel[478U * 800U + 798U] == WHITE);
            LCD_EndPage();
        }
        else
        {
            assert(memory_commands == 2U && pixel_writes == 16U);
        }
        assert(panel[478U * 800U + 798U] == RED);
        assert(panel[478U * 800U + 799U] == GREEN);
        assert(panel[479U * 800U + 798U] == BLUE2);
        assert(panel[479U * 800U + 799U] == YELLOW);
        assert(panel[478U * 800U + 797U] == WHITE);
        for(row = 0U; row < 3U; row++)
            for(column = 0U; column < 4U; column++)
                assert(panel[(20U + row) * 800U + 10U + column] == pixels[row * 4U + column]);
        for(i = 800U * 480U; i < sizeof(sram) / sizeof(sram[0]); i++) assert(sram[i] == 0x1234U);

        if(buffered != 0U) LCD_BeginPage(WHITE);
        memcpy(expected, panel, sizeof(panel));
        bus_writes = 0U;
        LCD_BlitRGB565(0U, 0U, 1U, 1U, 0);
        LCD_BlitRGB565(0U, 0U, 0U, 3U, pixels);
        LCD_BlitRGB565(0U, 0U, 4U, 0U, pixels);
        LCD_BlitRGB565(800U, 0U, 4U, 3U, pixels);
        LCD_BlitRGB565(0U, 480U, 4U, 3U, pixels);
        LCD_BlitRGB565(65535U, 65535U, 4U, 3U, pixels);
        assert(bus_writes == 0U && memcmp(panel, expected, sizeof(panel)) == 0);
        if(buffered != 0U)
        {
            for(i = 0U; i < 800U * 480U; i++) assert(sram[i] == WHITE);
            LCD_EndPage();
        }
    }
}

static void test_ascii_fast_path(void)
{
    unsigned font, ch, i;
    sFONT *fonts[] = { &Font8x16, &Font16x32, &Font24x48 };
    for(font = 0U; font < 3U; font++)
    {
        LCD_PageBuffer_Enable(0U);
        for(i = 0U; i < 800U * 480U; i++) panel[i] = WHITE;
        LCD_SetFont(fonts[font]); LCD_SetTextColor(BLUE); LCD_SetBackColor(GREY);
        for(ch = 32U; ch < 127U; ch++)
            ILI9806G_DispChar_EN((uint16_t)((ch - 32U) % 24U * 30U),
                                (uint16_t)((ch - 32U) / 24U * 50U), (char)ch);
        ILI9806G_DispChar_EN(797U, 478U, 'A'); /* Clipped glyph fallback. */
        memcpy(expected, panel, sizeof(panel));
        LCD_PageBuffer_Enable(1U); LCD_BeginPage(WHITE);
        for(ch = 32U; ch < 127U; ch++)
            ILI9806G_DispChar_EN((uint16_t)((ch - 32U) % 24U * 30U),
                                (uint16_t)((ch - 32U) / 24U * 50U), (char)ch);
        ILI9806G_DispChar_EN(797U, 478U, 'A');
        LCD_EndPage();
        assert(memcmp(panel, expected, sizeof(panel)) == 0);
    }
}

static void check_stick_pixels(uint16_t dot_x, uint16_t dot_y)
{
    uint16_t x, y, wanted;
    for(y = 300U; y <= 412U; y++)
        for(x = 78U; x <= 190U; x++) {
            int32_t dx = (int32_t)x - dot_x, dy = (int32_t)y - dot_y;
            wanted = dx * dx + dy * dy <= 64L ? BLUE2 : expected[y * 800U + x];
            if(panel[y * 800U + x] != wanted)
                fprintf(stderr, "Marker trail/outline mismatch at %u,%u (dot %u,%u): %04x != %04x\n",
                        x,y,dot_x,dot_y,panel[y * 800U + x],wanted);
            assert(panel[y * 800U + x] == wanted);
        }
}

static void test_robot_marker_pixels(void)
{
    uint16_t dot_x = 0U, dot_y = 0U, raw_x = 0U, raw_y = 0U, x, y;
    int control_x, control_y;
    unsigned i;
    LCD_PageBuffer_Enable(0U);
    for(i = 0U; i < 800U * 480U; i++) panel[i] = GREY;
    gui_robot_draw_stick(134U,356U,2047U,2047U,0,0,BLUE2,
                         &dot_x,&dot_y,&raw_x,&raw_y,1U,1U);
    memcpy(expected,panel,sizeof(panel));
    /* Independent reference keeps the actual static outline and axes only. */
    for(y = 348U; y <= 364U; y++)
        for(x = 126U; x <= 142U; x++)
            expected[y * 800U + x] = (x == 134U || y == 356U) ? WHITE : GREY;
    watch_old_x = dot_x; watch_old_y = dot_y;
    watch_new_x = 145U; watch_new_y = 356U;
    watch_overlap = 1U; overlap_writes = 0U;
    memory_commands = pixel_writes = 0U;
    gui_robot_draw_stick(134U,356U,2500U,2047U,250,0,BLUE2,
                         &dot_x,&dot_y,&raw_x,&raw_y,0U,0U);
    watch_overlap = 0U;
    assert(dot_x == 145U && dot_y == 356U && overlap_writes > 0U);
    assert(memory_commands == 2U && pixel_writes == 2U * 17U * 17U);
    check_stick_pixels(dot_x,dot_y);
    /* Repeated diagonal/full travel must preserve every static outline pixel. */
    for(control_x = -1000; control_x <= 1000; control_x += 250)
        for(control_y = -1000; control_y <= 1000; control_y += 250) {
            gui_robot_draw_stick(134U,356U,2500U,2500U,
                (int16_t)control_x,(int16_t)control_y,BLUE2,
                &dot_x,&dot_y,&raw_x,&raw_y,0U,0U);
            check_stick_pixels(dot_x,dot_y);
        }
    gui_robot_draw_stick(134U,356U,2047U,2047U,0,0,BLUE2,
                         &dot_x,&dot_y,&raw_x,&raw_y,0U,0U);
    assert(dot_x == 134U && dot_y == 356U);
    check_stick_pixels(dot_x,dot_y);
}

static void render_robot_fresh(const GuiRobotTelemetry *telemetry)
{
    unsigned i;
    for(i = 0U; i < 7U; i++) ADC1_Value[i] = 2047U;
    LCD_PageBuffer_Enable(1U);
    gui_prepare_page(); robot_control_page(telemetry); LCD_EndPage();
}

static void test_robot_telemetry_pixels(void)
{
    GuiRobotTelemetry telemetry = {0};
    unsigned i;
    telemetry.link_online = 1U;
    telemetry.speed_mps = FLT_MAX; telemetry.position_x_m = -FLT_MAX;
    telemetry.position_y_m = FLT_MAX; telemetry.position_z_m = -FLT_MAX;
    telemetry.acceleration_x_mps2 = FLT_MAX; telemetry.acceleration_y_mps2 = -FLT_MAX;
    telemetry.acceleration_z_mps2 = FLT_MAX;
    telemetry.roll_deg = FLT_MAX; telemetry.pitch_deg = -FLT_MAX; telemetry.yaw_deg = FLT_MAX;
    telemetry.latitude_deg = DBL_MAX; telemetry.longitude_deg = -DBL_MAX;
    telemetry.voltage_v = FLT_MAX; telemetry.gps_altitude_m = -FLT_MAX;
    telemetry.packet_count = UINT32_MAX; telemetry.packet_age_ms = UINT16_MAX;
    telemetry.battery_percent = telemetry.satellites = telemetry.gps_fix = UINT8_MAX;
    fake_ms = 0U;
    render_robot_fresh(&telemetry); memcpy(expected,panel,sizeof(panel));
    telemetry.link_online = 0U;
    render_robot_fresh(&telemetry); memcpy(old_frame,panel,sizeof(panel));
    telemetry.link_online = 1U;
    render_robot_fresh(&telemetry);
    assert(memcmp(panel,expected,sizeof(panel)) == 0);
    for(i = 0U; i < 4U; i++) {
        fake_ms++;
        telemetry.link_online = 0U; robot_control_page(&telemetry);
        assert(memcmp(panel,old_frame,sizeof(panel)) == 0);
        fake_ms++;
        telemetry.link_online = 1U; robot_control_page(&telemetry);
        assert(memcmp(panel,expected,sizeof(panel)) == 0);
    }
    /* A different page must not affect subsequent robot content or margins. */
    gui_prepare_page(); main_menu(0U); LCD_EndPage();
    render_robot_fresh(&telemetry);
    assert(memcmp(panel,expected,sizeof(panel)) == 0);
}

int main(void)
{
    test_transitions();
    test_edges_and_fallback();
    test_rgb565_blit();
    test_ascii_fast_path();
    test_diagnostics();
    test_diagnostics();
    test_robot_marker_pixels();
    test_robot_telemetry_pixels();
    puts("LCD page tests passed: menu/channel/robot transitions, Hardware Test entry while OK held, exit while BACK held, debounce, complete transfers, edges, portrait, fallback, RGB565 clipping, final marker pixels, intact outline and telemetry transitions.");
    return 0;
}
