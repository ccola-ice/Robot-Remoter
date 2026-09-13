#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fonts.h"
#include "gui.h"
#include "gui_analog_filter.h"

#define WHITE 0xffffU
#define GREY 0xf7deU
#define BLACK 0U
#define BLUE 0x001fU
#define BLUE2 0x051fU
#define GREEN 0x07e0U
#define RED 0xf800U
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
static struct { uint16_t chLower[7], chMiddle[7], chUpper[7]; uint8_t chReverse[7]; } param;
static char displayBuffer[100];
static void GTP_IRQ_Disable(void) {}
static void gui_boot_menu_badge(void) {}

#include "lcd_page_gui.inc"

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
    bus_writes = 0U;
    LCD_BeginPage(WHITE);
    LCD_EndPage();
    assert(bus_writes == 0U);
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

int main(void)
{
    test_transitions();
    test_edges_and_fallback();
    test_ascii_fast_path();
    puts("LCD page tests passed: repeated menu/channel/robot transitions match direct rendering; no LCD writes while composing; one complete transfer; edges, portrait, fallback and live updates.");
    return 0;
}
