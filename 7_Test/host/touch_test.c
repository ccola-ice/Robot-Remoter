/* Host regression test: compile the production parser and palette against a
 * register bus and framebuffer. No STM32 peripherals or flash are accessed. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../5_ModuleDrivers/touch/gt9xx.c"
#include "../../5_ModuleDrivers/touch/palette.c"

/* Avoid the Windows CRT assertion dialog in unattended test runs. */
#undef assert
#define assert(condition) do { if(!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(1); } } while(0)

uint16_t LCD_X_LENGTH = 800U, LCD_Y_LENGTH = 480U;
uint8_t LCD_SCAN_MODE = 3U;
const int Font16x32 = 0;
static uint16_t pixels[800U * 800U];
static uint16_t fg, bg, reg_pointer;
static uint8_t registers[65536];
static uint8_t bus_addr = 0xBAU, fail_read, fail_ack, fail_points;
static unsigned reads, acknowledgements, draw_calls, command_calls;
static uint8_t canvas_only, require_ack_before_draw;

void I2C_Touch_Init(void) {}
void I2C_ResetChip(void) {}
void I2C_GTP_IRQDisable(void) {}
void I2C_GTP_IRQEnable(void) {}
void I2C_GTP_SetInterruptTrigger(uint8_t trigger) { assert(trigger < 4U); }
uint32_t I2C_WriteBytes(uint8_t addr, uint8_t *data, uint8_t size)
{
    if(addr != bus_addr) return 1U;
    assert(size >= 2U);
    reg_pointer = ((uint16_t)data[0] << 8) | data[1];
    if(size > 2U)
    {
        /* Normal operation must never overwrite the glass configuration. */
        assert(reg_pointer == 0x814EU && size == 3U && data[2] == 0U);
        if(fail_ack) return 1U;
        registers[reg_pointer] = 0U;
        acknowledgements++;
    }
    return 0U;
}
uint32_t I2C_ReadBytes(uint8_t addr, uint8_t *data, uint16_t size)
{
    reads++;
    if(addr != bus_addr || fail_read || (fail_points && reg_pointer == 0x814FU)) return 1U;
    assert((unsigned)reg_pointer + size <= sizeof(registers));
    memcpy(data, registers + reg_pointer, size);
    return 0U;
}
void LCD_SetFont(const void *font) { (void)font; }
void LCD_SetColors(uint16_t a, uint16_t b) { fg = a; bg = b; }
void LCD_SetTextColor(uint16_t c) { fg = c; }
void LCD_SetBackColor(uint16_t c) { bg = c; }
void ILI9806G_GramScan(uint8_t mode)
{
    LCD_SCAN_MODE = mode;
    LCD_X_LENGTH = mode & 1U ? 800U : 480U;
    LCD_Y_LENGTH = mode & 1U ? 480U : 800U;
}
void ILI9806G_SetPointPixel(uint16_t x, uint16_t y)
{
    assert(x < LCD_X_LENGTH && y < LCD_Y_LENGTH);
    if(canvas_only) assert(Palette_IsCanvasPoint(x, y));
    if(require_ack_before_draw) assert(registers[0x814EU] == 0U);
    pixels[y * 800U + x] = fg;
    draw_calls++;
}
void ILI9806G_DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t fill)
{
    unsigned row, col;
    assert(w > 0U && h > 0U && (unsigned)x + w <= LCD_X_LENGTH && (unsigned)y + h <= LCD_Y_LENGTH);
    for(row = y; row < (unsigned)y + h; row++)
        for(col = x; col < (unsigned)x + w; col++)
            if(fill || row == y || row == (unsigned)y + h - 1U || col == x || col == (unsigned)x + w - 1U)
                ILI9806G_SetPointPixel(col, row);
}
void ILI9806G_Clear(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t saved = fg;
    fg = bg;
    ILI9806G_DrawRectangle(x, y, w, h, 1U);
    fg = saved;
}
void ILI9806G_DrawLine(uint16_t x, uint16_t y, uint16_t xx, uint16_t yy)
{
    /* Used by toolbar previews only; canvas strokes use production Bresenham. */
    ILI9806G_SetPointPixel(x, y);
    ILI9806G_SetPointPixel(xx, yy);
}
void ILI9806G_DrawCircle(uint16_t x, uint16_t y, uint16_t r, uint8_t fill)
{ (void)r; (void)fill; ILI9806G_SetPointPixel(x, y); }
void ILI9806G_DispString_EN(uint16_t x, uint16_t y, const char *str)
{ (void)x; (void)y; (void)str; }
void ILI9806G_DispString_EN_CH(uint16_t x, uint16_t y, const char *str)
{ (void)x; (void)y; (void)str; }

static void point(unsigned slot, uint8_t id, uint16_t x, uint16_t y)
{
    uint8_t *p = registers + 0x814FU + slot * 8U;
    memset(p, 0, 8U);
    p[0] = id; p[1] = x; p[2] = x >> 8; p[3] = y; p[4] = y >> 8;
}
static void sized_point(unsigned slot, uint8_t id, uint16_t x, uint16_t y, uint16_t size)
{
    uint8_t *p = registers + 0x814FU + slot * 8U;
    point(slot, id, x, y);
    p[5] = size; p[6] = size >> 8;
}
static void report(uint8_t count)
{
    registers[0x814EU] = 0x80U | count;
    GTP_NotifyInterrupt();
    GTP_Service();
}
static void advance(unsigned ms)
{
    unsigned i;
    for(i = 0U; i < ms; i += 10U) GTP_Tick10ms();
    GTP_Service();
}
static void reset_board(void)
{
    canvas_only = 0U;
    require_ack_before_draw = 0U;
    fail_read = fail_ack = 0U;
    GTP_IRQ_Disable();
    ILI9806G_GramScan(3U);
    Palette_Init(3U);
    GTP_IRQ_Enable();
    require_ack_before_draw = 1U;
}
static void count_command(void *unused) { (void)unused; command_calls++; }

static void test_geometry(void)
{
    static const int32_t expected[8][2] = {
        {399,200}, {200,399}, {80,200}, {200,80},
        {399,599}, {599,399}, {80,599}, {599,80}
    };
    unsigned mode;
    int32_t x, y, previous;
    for(mode = 0U; mode < 8U; mode++)
    {
        ILI9806G_GramScan(mode);
        assert(GTP_MapCoordinates(200U, 80U, &x, &y));
        assert(x == expected[mode][0] && y == expected[mode][1]);
        assert(!GTP_MapCoordinates(800U, 0U, &x, &y));
        assert(!GTP_MapCoordinates(0U, 480U, &x, &y));
    }
    ILI9806G_GramScan(5U);
    previous = 800;
    for(mode = 0U; mode < 800U; mode++)
    {
        assert(GTP_MapCoordinates(mode, 0U, &x, &y));
        assert(x == previous - 1); /* No half-screen jump at the centre. */
        previous = x;
    }
    g_gtp_raw_width = 1600U; g_gtp_raw_height = 960U;
    assert(GTP_MapCoordinates(1599U, 959U, &x, &y) && x == 0 && y == 0);
    g_gtp_raw_width = 480U; g_gtp_raw_height = 800U;
    assert(GTP_MapCoordinates(479U, 799U, &x, &y) && x == 0 && y == 0);
    g_gtp_raw_width = 800U; g_gtp_raw_height = 480U;
}
static void test_frames(void)
{
    unsigned before, i;
    reset_board();
    point(0U, 0U, 300U, 200U);
    before = acknowledgements;
    registers[0x814EU] = 1U; /* INT arrives before buffer-ready. */
    GTP_NotifyInterrupt(); GTP_Service();
    assert(acknowledgements == before && g_gtp_active_mask == 0U);
    registers[0x814EU] = 0x81U;
    advance(10U); /* Ready later, no second IRQ. */
    assert(g_gtp_active_mask == 1U && pixels[200U * 800U + 300U] == CL_BLACK);
    report(0U);
    assert(g_gtp_active_mask == 0U && pre_x[0] == -1);

    point(0U, 3U, 350U, 100U); point(1U, 0U, 300U, 200U); report(2U);
    point(0U, 0U, 310U, 200U); point(1U, 3U, 360U, 100U); report(2U);
    assert(pre_x[0] == 310 && pre_x[3] == -1 && g_gtp_active_mask == 1U);
    point(0U, 3U, 370U, 100U); report(1U);
    assert(pre_x[0] == -1 && g_gtp_active_mask == 0U && g_gtp_single.blocked);
    assert(pixels[150U * 800U + 330U] == CL_WHITE); /* no cross-finger line */

    report(0U);
    for(i = 0U; i < GTP_MAX_TOUCH; i++) point(i, 4U - i, 400U + 20U * i, 250U);
    report(5U);
    assert(g_gtp_active_mask == 1U && pre_x[0] == 480 && pre_x[4] == -1);
    before = draw_calls;
    fail_points = 1U; report(5U); fail_points = 0U;
    assert(g_gtp_active_mask == 0U && draw_calls == before);

    before = draw_calls;
    point(0U, 1U, 300U, 200U); point(1U, 1U, 700U, 400U); report(2U);
    assert(g_gtp_active_mask == 0U && draw_calls == before);
    point(0U, 5U, 300U, 200U); report(1U);
    assert(g_gtp_active_mask == 0U && draw_calls == before);
    point(0U, 0U, 65535U, 200U); report(1U);
    assert(g_gtp_active_mask == 0U && draw_calls == before);
    report(6U);
    assert(g_gtp_active_mask == 0U && draw_calls == before);

    point(0U, 0U, 300U, 200U); report(1U);
    fail_read = 1U; advance(10U); fail_read = 0U;
    assert(g_gtp_active_mask == 0U && pre_x[0] == -1);
    point(0U, 0U, 700U, 400U); report(1U);
    assert(pixels[300U * 800U + 500U] == CL_WHITE);
    advance(200U);
    assert(g_gtp_active_mask == 0U);
    before = draw_calls;
    fail_ack = 1U; report(1U); fail_ack = 0U;
    assert(g_gtp_active_mask == 0U && draw_calls == before);
    GTP_IRQ_Disable();
    before = reads;
    advance(50U); GTP_NotifyInterrupt(); GTP_Service();
    assert(reads == before);

    reset_board();
    g_gtp_ticks_ms = 0xFFFFFFF0U;
    point(0U, 0U, 300U, 200U); report(1U);
    advance(210U);
    assert(g_gtp_active_mask == 0U); /* elapsed time also survives wraparound */
}
static void test_buttons(void)
{
    reset_board();
    button[0].btn_command = count_command;
    command_calls = 0U;
    point(0U, 1U, 10U, 10U); report(1U);
    point(1U, 0U, 300U, 200U); report(2U);
    point(0U, 1U, 10U, 10U); report(1U); /* drawing finger up */
    assert(command_calls == 0U && button[0].touch_flag == 1U);
    report(0U);
    assert(command_calls == 1U);
    Touch_Button_Up(10U, 10U);
    assert(command_calls == 1U); /* no down, no command */
    point(0U, 0U, 10U, 10U); report(1U);
    point(0U, 0U, 10U, 60U); report(1U); report(0U);
    assert(command_calls == 1U && button[0].touch_flag == 0U);
    point(0U, 0U, 0U, 0U); report(1U); report(0U);
    assert(command_calls == 2U); /* top/left border is part of the button */
    point(0U, 0U, 10U, 10U); report(1U); advance(200U);
    assert(command_calls == 2U && button[0].touch_flag == 0U);
    point(0U, 0U, 300U, 200U); report(1U);
    point(0U, 0U, 10U, 10U); report(1U); report(0U);
    assert(command_calls == 2U); /* canvas gesture cannot become a button tap */
}
static void test_brushes(void)
{
    unsigned shape;
    reset_board();
    canvas_only = 1U;
    for(shape = LINE_SINGLE_PIXCEL; shape <= RUBBER; shape++)
    {
        brush.shape = (SHAPE)shape;
        brush.color = CL_RED;
        Draw_Trail(-1, -1, PALETTE_START_X, 0, &brush);
        Draw_Trail(PALETTE_START_X, 0, 799, 0, &brush);
        Draw_Trail(799, 0, 799, 479, &brush);
        Draw_Trail(799, 479, PALETTE_START_X, 479, &brush);
        Draw_Trail(PALETTE_START_X, 479, PALETTE_START_X, 0, &brush);
        Draw_Trail(799, 479, 200, 10, &brush); /* progression from screen edge */
        assert(fg == CL_RED);
    }
    brush.shape = LINE_SINGLE_PIXCEL;
    Command_Clear_Palette(NULL);
    assert(fg == CL_RED && pixels[479U * 800U + 799U] == CL_WHITE);
    Draw_Trail(-1, -1, 799, 479, &brush);
    assert(pixels[479U * 800U + 799U] == CL_RED);
    Draw_Trail(-1, -1, 600, 300, &brush);
    brush.shape = RUBBER;
    Draw_Trail(400, 300, 700, 300, &brush);
    assert(pixels[300U * 800U + 600U] == CL_WHITE); /* continuous eraser */
    canvas_only = 0U;
}

static void calibration_tap(uint16_t x, uint16_t y)
{
    unsigned sample;
    for(sample = 0U; sample < 4U; sample++)
    {
        point(0U, 0U, x, y);
        report(1U);
        advance(10U);
    }
    report(0U);
}

static void calibration_begin(uint8_t scan)
{
    reset_board();
    Palette_Init(scan);
    GTP_CalibrationStart();
    assert(g_cal_active == 1U && !GTP_CalibrationIsReady());
    report(0U); report(0U);
    assert(g_cal_point == 0U); /* idle reports are not failed target taps */
}

static void test_calibration(void)
{
    unsigned mode, i, kind;
    int32_t x, y;
    /* No installation direction is inferred from the scan number. */
    for(mode = 0U; mode < 8U; mode++)
    {
        calibration_begin(mode);
        for(i = 0U; i < GTP_CAL_POINT_COUNT; i++)
            calibration_tap(g_cal_target_x[i], g_cal_target_y[i]);
        assert(GTP_CalibrationIsReady() && g_cal_active == 0U && g_cal_wrap_axis == 0U);
        assert(GTP_MapCoordinates(LCD_X_LENGTH - 1, LCD_Y_LENGTH - 1, &x, &y));
        assert(x == LCD_X_LENGTH - 1 && y == LCD_Y_LENGTH - 1);
        ILI9806G_GramScan((mode + 1U) & 7U);
        assert(!GTP_CalibrationIsReady());
    }
    for(kind = 0U; kind < 4U; kind++)
    {
        calibration_begin(5U);
        for(i = 0U; i < GTP_CAL_POINT_COUNT; i++)
        {
            uint16_t sx = g_cal_target_x[i], sy = g_cal_target_y[i];
            if(kind == 0U) calibration_tap(799U - sx, 479U - sy);
            if(kind == 1U) calibration_tap(sy, sx); /* Y exceeds nominal 480. */
            if(kind == 2U)
                calibration_tap((sx + 400U) % 800U, sy);
            if(kind == 3U) calibration_tap(sx, (sy + 240U) % 480U);
        }
        assert(GTP_CalibrationIsReady());
        assert(g_cal_wrap_axis == (kind < 2U ? 0U : kind - 1U));
        /* Verify all horizontal positions, including both sides of the centre. */
        for(i = 0U; i < 800U; i++)
        {
            uint16_t rx = i, ry = 240U;
            if(kind == 0U) { rx = 799U - i; ry = 239U; }
            if(kind == 1U) { rx = 240U; ry = i; }
            if(kind == 2U) rx = (i + 400U) % 800U;
            if(kind == 3U) ry = 0U;
            assert(GTP_MapCoordinates(rx, ry, &x, &y) && x == (int32_t)i && y == 240);
            if(kind == 2U && i >= PALETTE_START_X)
            {
                point(0U, 0U, rx, ry); report(1U);
            }
        }
        report(0U);
        if(kind == 2U)
        {
            for(i = PALETTE_START_X; i < 800U; i++)
                assert(pixels[240U * 800U + i] == CL_BLACK);
            assert(!GTP_MapCoordinates(800U, 240U, &x, &y));
        }
    }
    /* Short taps, movement, multiple fingers and bus failures must not accept
     * a target or reuse samples from before the interruption. */
    calibration_begin(5U);
    point(0U, 0U, 80U, 80U); report(1U); report(0U);
    assert(g_cal_point == 0U);
    point(0U, 0U, 80U, 80U); report(1U);
    point(0U, 0U, 130U, 80U); report(1U); report(1U); report(0U);
    assert(g_cal_point == 0U);
    point(0U, 0U, 80U, 80U); point(1U, 1U, 720U, 80U); report(2U);
    report(1U); report(1U); report(1U); report(0U);
    assert(g_cal_point == 0U);
    point(0U, 0U, 80U, 80U); report(1U); report(1U); report(1U);
    fail_read = 1U; advance(10U); fail_read = 0U; report(0U);
    assert(g_cal_point == 0U);
    calibration_tap(80U, 80U);
    assert(g_cal_point == 1U);
    GTP_IRQ_Disable();
    assert(!GTP_CalibrationIsReady() && g_cal_active == 0U);

    /* Keep circular averaging correct even though centre taps are no longer
     * a mandatory calibration step. */
    calibration_begin(5U);
    point(0U, 0U, 799U, 80U); report(1U);
    point(0U, 0U, 0U, 80U); report(1U);
    point(0U, 0U, 1U, 80U); report(1U); report(0U);
    assert(g_cal_raw_x[0] == 0 && g_cal_point == 1U);

    /* Inconsistent corners must still fail. Removing the centre gate must
     * not turn every group of taps into an accepted transform. */
    calibration_begin(5U);
    for(i = 0U; i < GTP_CAL_POINT_COUNT; i++)
        calibration_tap(g_cal_target_x[i] + (i == 3U ? 200U : 0U), g_cal_target_y[i]);
    assert(!GTP_CalibrationIsReady() && g_cal_active == 2U);
    i = draw_calls;
    point(0U, 0U, 300U, 200U); report(1U); report(0U);
    assert(draw_calls == i); /* failure page continues raw diagnostics only */
    calibration_begin(5U);
    for(i = 0U; i < GTP_CAL_POINT_COUNT; i++) calibration_tap(80U, 80U);
    assert(!GTP_CalibrationIsReady() && g_cal_active == 2U);
    /* Retry after failure, then leave and re-enter without losing a valid fit. */
    GTP_CalibrationStart();
    for(i = 0U; i < GTP_CAL_POINT_COUNT; i++)
        calibration_tap(g_cal_target_x[i], g_cal_target_y[i]);
    assert(GTP_CalibrationIsReady());
    assert(!GTP_MapCoordinates(830U, 240U, &x, &y));
    assert(GTP_MapCoordinates(800U, 240U, &x, &y) && x == 799);
    GTP_IRQ_Disable();
    Palette_Init(5U); GTP_IRQ_Enable();
    assert(GTP_CalibrationIsReady());
    GTP_IRQ_Disable();
}

static void test_field_calibration(void)
{
    /* Recorded FIELD-CAL-2 taps from the user's 2026-09-05 serial log. The
     * old three-point fit missed P4 by (14,-36) and never entered the board. */
    static const uint16_t field_raw[4][2] = {
        {323U,420U}, {499U,385U}, {333U,51U}, {494U,56U}
    };
    unsigned i;
    int32_t x, y;
    calibration_begin(5U);
    for(i = 0U; i < 4U; i++) calibration_tap(field_raw[i][0], field_raw[i][1]);
    assert(GTP_CalibrationIsReady() && g_cal_active == 0U && g_cal_point == 4U);
    assert(g_cal_wrap_axis == 1U && g_cal_wrap_period == 800U);
    for(i = 0U; i < 4U; i++)
    {
        assert(GTP_MapCoordinates(field_raw[i][0], field_raw[i][1], &x, &y));
        assert(abs(x - g_cal_target_x[i]) <= 24 && abs(y - g_cal_target_y[i]) <= 24);
    }
    assert(GTP_MapCoordinates(799U, 240U, &x, &y));
    i = x;
    assert(GTP_MapCoordinates(0U, 240U, &x, &y) && abs(x - (int32_t)i) <= 2);

    /* A centre contact being split into two IDs must no longer reject a fifth
     * calibration point. Keep both raw records for diagnosis, but only one drawing pointer. */
    point(0U, 0U, 779U, 247U); report(1U);
    point(1U, 1U, 64U, 245U); report(2U);
    assert(g_gtp_active_mask == 1U && g_cal_active == 0U);
    report(0U);
    assert(GTP_CalibrationIsReady() && g_gtp_active_mask == 0U);
    GTP_IRQ_Disable();
    puts("PASS: recorded field corners enter the board after P4; seam reports remain available");
}

static void seam_board(void)
{
    unsigned i;
    calibration_begin(5U);
    for(i = 0U; i < 4U; i++)
        calibration_tap((1199U - g_cal_target_x[i]) % 800U, 479U - g_cal_target_y[i]);
    assert(GTP_CalibrationIsReady() && g_cal_wrap_axis == 1U);
}

static void timed_report(unsigned ms, uint8_t count)
{
    unsigned i;
    for(i = 0U; i < ms; i += 10U) GTP_Tick10ms();
    report(count);
}

static void test_single_field_path(void)
{
    unsigned before, i, x, y;
    static const uint16_t vertical[][3] = {
        {81,454,454}, {81,411,395}, {82,270,263}, {82,182,176},
        {82,113,105}, {81,68,58}, {83,37,37}
    };
    seam_board(); reset_board(); Palette_Init(5U);
    /* Actual raw coordinates from the rising-screen-Y diagonal. Timing is
     * modeled; the field trace did not contain every intermediate frame. */
    sized_point(0U, 0U, 142U, 419U, 27U); timed_report(10U, 1U);
    before = draw_calls;
    sized_point(0U, 0U, 94U, 378U, 27U); timed_report(20U, 1U);
    assert(draw_calls == before); /* Do not ink an unreliable seam fragment. */
    sized_point(0U, 0U, 86U, 367U, 27U);
    sized_point(1U, 1U, 798U, 352U, 12U); timed_report(10U, 2U);
    sized_point(0U, 0U, 65U, 334U, 27U); timed_report(20U, 2U);
    sized_point(0U, 1U, 797U, 350U, 12U); timed_report(10U, 1U);
    assert(draw_calls == before && g_gtp_active_mask == 1U);
    sized_point(0U, 1U, 717U, 283U, 12U); timed_report(20U, 1U);
    assert(g_gtp_active_mask == 1U && pixels[139U * 800U + 355U] == CL_WHITE);
    for(x = 257U; x <= 482U; x++)
    {
        unsigned found = 0U;
        for(y = 60U; y <= 196U; y++) found |= pixels[y * 800U + x] == CL_BLACK;
        assert(found);
    }
    report(0U);

    reset_board(); Palette_Init(5U);
    sized_point(0U, 0U, 81U, 454U, 28U); timed_report(10U, 1U);
    for(i = 0U; i < sizeof(vertical) / sizeof(vertical[0]); i++)
    {
        sized_point(0U, 1U, 798U, vertical[i][2], 7U); /* shuffled slots */
        sized_point(1U, 0U, vertical[i][0], vertical[i][1], 28U); timed_report(20U, 2U);
        assert(g_gtp_active_mask == 1U);
    }
    sized_point(0U, 0U, 83U, 12U, 28U); timed_report(10U, 1U); report(0U);
    for(y = 25U; y <= 467U; y++)
    {
        unsigned found = 0U;
        for(x = 313U; x <= 323U; x++) found |= pixels[y * 800U + x] == CL_BLACK;
        assert(found);
        for(x = 380U; x <= 450U; x++) assert(pixels[y * 800U + x] == CL_WHITE);
    }
    GTP_IRQ_Disable();
    puts("PASS: single field vertical stroke, stationary ghost, deferred diagonal bridge without backtracking");
}

static void single_pair(uint16_t lx, uint16_t ly, uint16_t hx, uint16_t hy, unsigned flip)
{
    sized_point(0U, 1U, hx, flip ? 479U - hy : hy, 19U);
    sized_point(1U, 0U, lx, flip ? 479U - ly : ly, 20U);
    timed_report(20U, 2U);
}

static void test_single_lifecycle(void)
{
    unsigned reverse, flip, x, y, kind;
    seam_board();
    /* Reverse both the stroke and its Y direction. The frozen intermediate
     * coordinates must contribute no kink or second branch to the bridge. */
    for(reverse = 0U; reverse < 2U; reverse++)
        for(flip = 0U; flip < 2U; flip++)
        {
            int32_t ax, ay, bx, by;
            reset_board(); Palette_Init(5U); brush.shape = LINE_SINGLE_PIXCEL;
            GTP_MapCoordinates(176U, flip ? 436U : 43U, &ax, &ay);
            GTP_MapCoordinates(693U, flip ? 236U : 243U, &bx, &by);
            if(!reverse)
            {
                sized_point(0U, 0U, 176U, flip ? 436U : 43U, 20U); timed_report(10U, 1U);
                single_pair(93U, 134U, 793U, 155U, flip);
                single_pair(74U, 160U, 790U, 165U, flip);
                sized_point(0U, 1U, 772U, flip ? 290U : 189U, 19U); timed_report(20U, 1U);
                sized_point(0U, 1U, 693U, flip ? 236U : 243U, 19U); timed_report(20U, 1U);
            }
            else
            {
                sized_point(0U, 1U, 693U, flip ? 236U : 243U, 19U); timed_report(10U, 1U);
                sized_point(0U, 1U, 772U, flip ? 290U : 189U, 19U); timed_report(20U, 1U);
                single_pair(74U, 160U, 790U, 165U, flip);
                single_pair(93U, 134U, 793U, 155U, flip);
                sized_point(0U, 0U, 176U, flip ? 436U : 43U, 20U); timed_report(20U, 1U);
            }
            assert(g_gtp_active_mask == 1U);
            report(0U);
            for(x = ax; x <= (unsigned)bx; x++)
            {
                unsigned found = 0U;
                for(y = 0U; y < 480U; y++) if(pixels[y * 800U + x] == CL_BLACK)
                {
                    found = 1U;
                    assert(abs(((int)y - ay) * (bx - ax) - ((int)x - ax) * (by - ay)) <=
                           abs(bx - ax) + abs(by - ay));
                }
                assert(found);
            }
        }

    /* No overlap frame at high speed: reliable endpoints still join. */
    reset_board(); Palette_Init(5U);
    sized_point(0U, 0U, 210U, 239U, 30U); timed_report(10U, 1U);
    sized_point(0U, 0U, 150U, 239U, 30U); timed_report(10U, 1U);
    sized_point(0U, 1U, 650U, 239U, 30U); timed_report(20U, 1U);
    assert(g_gtp_active_mask == 1U);
    for(x = 249U; x <= 549U; x++) assert(pixels[240U * 800U + x] == CL_BLACK);
    report(0U);

    /* If the old fragment freezes, a known fragment reaching the far side
     * can finish the bridge even when both hardware IDs remain present. */
    reset_board(); Palette_Init(5U);
    sized_point(0U, 0U, 160U, 239U, 30U); timed_report(10U, 1U);
    single_pair(81U, 239U, 798U, 239U, 0U);
    single_pair(81U, 239U, 700U, 239U, 0U);
    assert(g_gtp_single.hw == 1U && !g_gtp_single.bridge && pre_x[0] == 499);
    report(0U);
    reset_board(); Palette_Init(5U);
    sized_point(0U, 0U, 81U, 239U, 30U); timed_report(10U, 1U);
    single_pair(81U, 239U, 798U, 239U, 0U);
    single_pair(81U, 239U, 700U, 239U, 0U);
    assert(g_gtp_single.hw == 1U && pre_x[0] == 499);
    report(0U);

    /* Cancelled pending ink cannot leak into another gesture. */
    for(kind = 0U; kind < 4U; kind++)
    {
        reset_board(); Palette_Init(5U);
        sized_point(0U, 0U, 160U, 239U, 30U); timed_report(10U, 1U);
        sized_point(0U, 0U, 81U, 239U, 30U); timed_report(10U, 1U);
        assert(g_gtp_single.bridge);
        if(kind == 0U) report(0U);
        if(kind == 1U) { fail_ack = 1U; report(1U); fail_ack = 0U; }
        if(kind == 2U) advance(200U);
        if(kind == 3U) { GTP_IRQ_Disable(); GTP_IRQ_Enable(); }
        sized_point(0U, 1U, 650U, 239U, 30U); timed_report(10U, 1U);
        assert(pixels[240U * 800U + 400U] == CL_WHITE);
        report(0U);
    }

    /* A vertical stroke in the band stays at one position on ID handoff;
     * leaving the band must restore calibrated absolute coordinates. */
    reset_board(); Palette_Init(5U);
    sized_point(0U, 0U, 81U, 400U, 28U); timed_report(10U, 1U);
    single_pair(81U, 380U, 798U, 380U, 0U);
    sized_point(0U, 1U, 798U, 370U, 7U); timed_report(20U, 1U);
    assert(pre_x[0] == 318);
    sized_point(0U, 1U, 798U, 340U, 7U); timed_report(20U, 1U);
    assert(pre_x[0] == 318);
    sized_point(0U, 1U, 700U, 300U, 7U); timed_report(20U, 1U);
    assert(pre_x[0] == 499 && pre_y[0] == 179);
    report(0U);
    GTP_IRQ_Disable();
    puts("PASS: four diagonal bridges, fast handoff, frozen primary, pending cancellation, vertical handoff");
}

int main(void)
{
    memcpy(registers + 0x8140U, "911\0\x34\x12", 6U);
    registers[0x8048U] = 0x20U; registers[0x8049U] = 3U;
    registers[0x804AU] = 0xE0U; registers[0x804BU] = 1U;
    registers[0x804DU] = 1U;
    assert(GTP_Init_Panel() == 0);
    test_geometry(); test_frames(); test_buttons(); test_brushes(); test_calibration(); test_field_calibration(); test_single_field_path(); test_single_lifecycle();
    bus_addr = 0x28U;
    assert(GTP_Init_Panel() == 0 && g_gtp_address == 0x28U);
    fail_read = 1U;
    assert(GTP_Init_Panel() != 0);
    GTP_IRQ_Enable();
    assert(g_gtp_enabled == 0U);
    puts("PASS: geometry, frame handshake, single pointer, complete-frame validation, failures, timeout, buttons, brush bounds, measured calibration, seam continuity, address probe");
    return 0;
}
