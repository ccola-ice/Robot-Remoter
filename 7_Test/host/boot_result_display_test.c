#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "boot_status.h"

#define LCD_X_LENGTH 800U
#define LCD_Y_LENGTH 480U
#define BLACK 0x0000U
#define WHITE 0xffffU
#define GREEN 0x07e0U
#define RED 0xf800U
#define YELLOW 0xffe0U
#define BLUE2 0x001fU
#define GREY 0x8410U

typedef struct {
    uint16_t width, height;
} TestFont;

typedef struct {
    uint16_t x, y, width, height, color;
    char text[128];
} TextCall;

static const TestFont Font8x16 = {8U, 16U};
static const TestFont Font16x32 = {16U, 32U};
static const TestFont *active_font;
static uint16_t text_color, background_color;
static TextCall calls[BOOT_ITEM_COUNT + 8U];
static unsigned call_count;

static void LCD_SetFont(const TestFont *font) { active_font = font; }
static void LCD_SetTextColor(uint16_t color) { text_color = color; }
static void LCD_SetBackColor(uint16_t color) { background_color = color; }

static void ILI9806G_Clear(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    assert(x == 0U && y == 0U);
    assert(width == LCD_X_LENGTH && height == LCD_Y_LENGTH);
    assert(text_color == BLACK && background_color == BLACK);
    call_count = 0U;
}

static void ILI9806G_DispString_EN(uint16_t x, uint16_t y, const char *text)
{
    TextCall *call;
    size_t length = strlen(text);
    unsigned i;
    assert(active_font != NULL);
    assert(call_count < sizeof(calls) / sizeof(calls[0]));
    assert(length < sizeof(calls[0].text));
    assert(x + length * active_font->width <= LCD_X_LENGTH);
    assert(y + active_font->height <= LCD_Y_LENGTH);
    assert(background_color == BLACK);
    call = &calls[call_count++];
    call->x = x;
    call->y = y;
    call->width = (uint16_t)(length * active_font->width);
    call->height = active_font->height;
    call->color = text_color;
    strcpy(call->text, text);
    /* Check real output rectangles, including both failure lines and the footer. */
    for(i = 0U; i + 1U < call_count; i++) {
        const TextCall *previous = &calls[i];
        assert(call->x >= previous->x + previous->width ||
               previous->x >= call->x + call->width ||
               call->y >= previous->y + previous->height ||
               previous->y >= call->y + call->height);
    }
}

static void gui_boot_not_tested_label(uint16_t x, uint16_t y)
{
    assert(x + 48U <= LCD_X_LENGTH);
    assert(y + 16U <= LCD_Y_LENGTH);
}

#include "boot_result_display_impl.inc"

static const TextCall *at_row(uint16_t y)
{
    unsigned i;
    for(i = 0U; i < call_count; i++)
        if(calls[i].y == y) return &calls[i];
    return NULL;
}

static const TextCall *required_row(uint16_t y)
{
    const TextCall *call = at_row(y);
    assert(call != NULL);
    return call;
}

static void expect_summary(const char *prefix, uint16_t color)
{
    const TextCall *summary = at_row(446U);
    assert(summary != NULL);
    assert(summary->x == 20U && summary->height == 16U);
    assert(summary->color == color);
    assert(strncmp(summary->text, prefix, strlen(prefix)) == 0);
    assert(summary == &calls[call_count - 1U]);
}

static void populate_report(BootReport *report, int failed_item, const char *detail)
{
    unsigned i;
    char shared_detail[128];
    boot_report_reset(report);
    for(i = 0U; i < BOOT_ITEM_COUNT; i++) {
        snprintf(shared_detail, sizeof(shared_detail), "Later success item %u", i);
        if((int)i == failed_item) {
            snprintf(shared_detail, sizeof(shared_detail), "%s", detail);
            assert(boot_report_record(report, (BootItem)i, BOOT_FAIL, shared_detail, 1U));
        } else {
            assert(boot_report_record(report, (BootItem)i, BOOT_PASS, shared_detail, 1U));
        }
    }
    memset(shared_detail, 'x', sizeof(shared_detail));
}

static void expect_failure(const BootReport *report, BootItem item)
{
    char label[100];
    const TextCall *detail;
    gui_boot_finish(report);
    assert(boot_last_report == report);
    snprintf(label, sizeof(label), "Failed: %s", boot_item_names[item]);
    assert(at_row(394U) != NULL && at_row(394U)->color == RED);
    assert(strcmp(required_row(394U)->text, label) == 0);
    detail = at_row(414U);
    assert(detail != NULL && detail->color == RED);
    assert(strcmp(detail->text, report->items[item].detail) == 0);
    assert(at_row(398U) == NULL);
    expect_summary("CHECKS FAILED / INCOMPLETE", RED);
}

static void failure_regressions(void)
{
    BootReport report;
    unsigned i;
    char long_detail[128];
    const char *sd_detail = "SD open failed: FR_DENIED (7)";
    populate_report(&report, BOOT_SD_WRITE, sd_detail);
    assert(report.items[BOOT_INTERNAL_MEMORY].state == BOOT_PASS);
    expect_failure(&report, BOOT_SD_WRITE);
    assert(strcmp(required_row(414U)->text, sd_detail) == 0);
    assert(strstr(required_row(370U)->text, "PASS 26  FAIL 1") != NULL);

    /* An earlier failure remains first even when recorded after the SD failure. */
    boot_report_reset(&report);
    assert(boot_report_record(&report, BOOT_SD_WRITE, BOOT_FAIL, sd_detail, 1U));
    for(i = 0U; i < BOOT_ITEM_COUNT; i++) {
        if(i == BOOT_SD_WRITE) continue;
        assert(boot_report_record(&report, (BootItem)i,
               i == BOOT_FLASH_WRITE ? BOOT_FAIL : BOOT_PASS,
               i == BOOT_FLASH_WRITE ? "Flash verify mismatch" : "Passed", 1U));
    }
    expect_failure(&report, BOOT_FLASH_WRITE);
    assert(strstr(required_row(370U)->text, "FAIL 2") != NULL);

    /* Exercise every actual item label and the longest stored detail. */
    memset(long_detail, 'A', sizeof(long_detail));
    long_detail[sizeof(long_detail) - 1U] = '\0';
    for(i = 0U; i < BOOT_ITEM_COUNT; i++) {
        populate_report(&report, (int)i, long_detail);
        expect_failure(&report, (BootItem)i);
        assert(strlen(required_row(414U)->text) == sizeof(report.items[i].detail) - 1U);
    }
}

static void no_failure_regressions(void)
{
    BootReport report;
    unsigned i;
    populate_report(&report, -1, "");
    gui_boot_finish(&report);
    assert(at_row(394U) == NULL && at_row(414U) == NULL);
    assert(at_row(398U) != NULL);
    assert(strncmp(required_row(398U)->text, "Open Hardware Tests", 19U) == 0);
    expect_summary("ALL LISTED CHECKS PASSED", GREEN);

    boot_report_reset(&report);
    for(i = 0U; i < BOOT_ITEM_COUNT; i++)
        assert(boot_report_record(&report, (BootItem)i, BOOT_NOT_TESTED, "Not tested", 0U));
    gui_boot_finish(&report);
    assert(at_row(394U) == NULL && at_row(414U) == NULL && at_row(398U) != NULL);
    expect_summary("CHECKS COMPLETE - untested items remain", YELLOW);

    boot_report_reset(&report);
    gui_boot_finish(&report);
    assert(at_row(394U) == NULL && at_row(414U) == NULL && at_row(398U) != NULL);
    expect_summary("CHECKS FAILED / INCOMPLETE", RED);
}

int main(void)
{
    failure_regressions();
    no_failure_regressions();
    puts("PASS: boot failure details, later successes, first failure, text bounds and summaries");
    return 0;
}
