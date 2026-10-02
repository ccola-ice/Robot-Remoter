#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "menu.h"

#define WHITE 1U
#define DIAG_UI_BLACK 0U
#define DIAG_UI_WHITE 1U
#define DIAG_UI_BLUE 2U
#define DIAG_UI_GREY 3U
#define DIAG_UI_RED 4U
#define KEY_BIT(key) (1U << (key))

typedef struct {
    uint32_t begin, end;
    uint8_t keys;
} KeySpan;
static const KeySpan *timeline;
static unsigned timeline_count;
static uint32_t now_ms;
static uint8_t raw_keys, diag_transition_pending, diag_line_cache[1];
static unsigned writes, screen_draws;
static uint8_t eeprom_bytes[256];

static uint8_t sampled_keys(void)
{
    unsigned i;
    if(!timeline) return raw_keys;
    assert(now_ms < 10000U);
    for(i = 0U; i < timeline_count; i++)
        if(now_ms >= timeline[i].begin && now_ms < timeline[i].end)
            return timeline[i].keys;
    return 0U;
}
static uint8_t read_button_left_gpio(uint8_t unused)
{ (void)unused; return !(sampled_keys() & KEY_BIT(MENU_KEY_LEFT)); }
static uint8_t read_button_right_gpio(uint8_t unused)
{ (void)unused; return !(sampled_keys() & KEY_BIT(MENU_KEY_RIGHT)); }
static uint8_t read_button_ok_gpio(uint8_t unused)
{ (void)unused; return !(sampled_keys() & KEY_BIT(MENU_KEY_OK)); }
static uint8_t read_button_back_gpio(uint8_t unused)
{ (void)unused; return !(sampled_keys() & KEY_BIT(MENU_KEY_BACK)); }
static int get_tick_count(unsigned long *ticks) { *ticks = now_ms; return 0; }
static void Delay_ms(uint32_t ms) { now_ms += ms; }
static void LCD_EndPage(void) {}
static void LCD_BeginPage(uint16_t color) { (void)color; screen_draws++; }
static void LCD_SetBackColor(uint16_t color) { (void)color; }
static void diag_ui_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t c)
{ (void)x; (void)y; (void)w; (void)h; (void)c; }
static void diag_ui_frame(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t c)
{ (void)x; (void)y; (void)w; (void)h; (void)c; }
static void diag_ui_text(uint16_t x, uint16_t y, uint16_t size,
                         uint8_t fg, uint8_t bg, const char *text)
{ (void)x; (void)y; (void)size; (void)fg; (void)bg; (void)text; }
static uint8_t EEPROM_Random_Read(uint8_t address, uint8_t *value)
{ *value = eeprom_bytes[address]; return 0U; }
static uint8_t EEPROM_Byte_Write(uint8_t address, uint8_t value)
{ assert(address != 255U); writes++; eeprom_bytes[address] = value; return 0U; }

#include "diag_key.inc"
#include "eeprom_menu.c"

static void reset_at(uint32_t tick)
{
    timeline = NULL;
    timeline_count = 0U;
    raw_keys = 0U;
    now_ms = tick;
    writes = screen_draws = 0U;
    memset(eeprom_bytes, 0xa5, sizeof(eeprom_bytes));
    diag_reset_keys();
}
static int sample(uint8_t mask, uint32_t tick)
{
    raw_keys = mask;
    now_ms = tick;
    return diag_key();
}
static void press(uint8_t key, uint32_t tick)
{
    assert(sample(KEY_BIT(key), tick) == -1);
    assert(sample(KEY_BIT(key), tick + 10U) == -1);
    assert(sample(KEY_BIT(key), tick + 20U) == key);
}
static void release(uint32_t tick)
{
    assert(sample(0U, tick) == -1);
    assert(sample(0U, tick + 10U) == -1);
    assert(sample(0U, tick + 20U) == -1);
}
static void check_repeat(uint8_t key, uint32_t start)
{
    uint8_t mask = KEY_BIT(key);
    reset_at(start);
    press(key, start);
    assert(sample(mask, start + 519U) == -1);
    assert(sample(mask, start + 520U) == key);
    assert(sample(mask, start + 639U) == -1);
    assert(sample(mask, start + 640U) == key);
    assert(sample(mask, start + 1470U) == key);
    assert(sample(mask, start + 1519U) == -1);
    assert(sample(mask, start + 1530U) == key);
    assert(sample(mask, start + 1589U) == -1);
    assert(sample(mask, start + 1590U) == key);
    /* A delayed foreground poll produces one event and no catch-up burst. */
    assert(sample(mask, start + 9000U) == key);
    assert(sample(mask, start + 9000U) == -1);
    assert(sample(mask, start + 9059U) == -1);
    assert(sample(mask, start + 9060U) == key);
    /* Raw release stops a repeat even before release debounce completes. */
    assert(sample(0U, start + 9120U) == -1);
    release(start + 9130U);
    press(key, start + 9200U);
    assert(sample(mask, start + 9719U) == -1);
    assert(sample(mask, start + 9720U) == key);
}

int main(void)
{
    unsigned key;
    static const KeySpan browse[] = {
        {50U, 1850U, KEY_BIT(MENU_KEY_RIGHT)},
        {2000U, 2100U, KEY_BIT(MENU_KEY_BACK)}
    };
    static const KeySpan protected_edit[] = {
        {50U, 1200U, KEY_BIT(MENU_KEY_OK)},
        {1400U, 2100U, KEY_BIT(MENU_KEY_BACK)},
        {2300U, 2400U, KEY_BIT(MENU_KEY_BACK)}
    };
    static const KeySpan release_wait[] = {
        {0U, 100U, KEY_BIT(MENU_KEY_OK)},
        {110U, 120U, KEY_BIT(MENU_KEY_OK)}
    };
    check_repeat(MENU_KEY_LEFT, 0U);
    check_repeat(MENU_KEY_RIGHT, 0U);
    check_repeat(MENU_KEY_RIGHT, UINT32_MAX - 200U);

    reset_at(0U);
    assert(sample(KEY_BIT(MENU_KEY_RIGHT), 0U) == -1);
    assert(sample(KEY_BIT(MENU_KEY_RIGHT), 10U) == -1);
    assert(sample(0U, 20U) == -1);
    assert(sample(KEY_BIT(MENU_KEY_RIGHT), 30U) == -1);
    assert(sample(0U, 40U) == -1);
    press(MENU_KEY_RIGHT, 50U);

    for(key = MENU_KEY_OK; key <= MENU_KEY_BACK; key++) {
        reset_at(0U);
        press((uint8_t)key, 0U);
        assert(sample(KEY_BIT(key), 520U) == -1);
        assert(sample(KEY_BIT(key), 2000U) == -1);
        release(2010U);
        press((uint8_t)key, 2040U);
    }

    /* A fresh confirmation wins over a due navigation repeat. */
    reset_at(0U);
    press(MENU_KEY_LEFT, 0U);
    assert(sample(KEY_BIT(MENU_KEY_LEFT) | KEY_BIT(MENU_KEY_OK), 500U) == -1);
    assert(sample(KEY_BIT(MENU_KEY_LEFT) | KEY_BIT(MENU_KEY_OK), 510U) == -1);
    assert(sample(KEY_BIT(MENU_KEY_LEFT) | KEY_BIT(MENU_KEY_OK), 520U) == MENU_KEY_OK);

    /* No held key may leak across a page transition, even mid-debounce. */
    for(key = MENU_KEY_LEFT; key <= MENU_KEY_BACK; key++) {
        reset_at(0U);
        assert(sample(KEY_BIT(key), 0U) == -1);
        diag_screen("NEXT");
        assert(sample(KEY_BIT(key), 10U) == -1);
        assert(sample(KEY_BIT(key), 20U) == -1);
        assert(sample(KEY_BIT(key), 2000U) == -1);
        release(2010U);
        press((uint8_t)key, 2040U);
    }
    reset_at(0U);
    press(MENU_KEY_RIGHT, 0U);
    assert(sample(KEY_BIT(MENU_KEY_RIGHT), 520U) == MENU_KEY_RIGHT);
    diag_screen("NEXT");
    assert(sample(KEY_BIT(MENU_KEY_RIGHT), 3000U) == -1);

    reset_at(0U);
    timeline = release_wait;
    timeline_count = sizeof(release_wait) / sizeof(release_wait[0]);
    diag_release();
    assert(now_ms == 150U);
    timeline = NULL;
    press(MENU_KEY_OK, 160U);

    /* Exercise actual EEPROM navigation and confirmation with physical holds. */
    reset_at(0U);
    timeline = browse;
    timeline_count = sizeof(browse) / sizeof(browse[0]);
    eeprom_menu();
    assert(eeprom_ui_address >= 10U && writes == 0U && screen_draws == 1U);
    reset_at(0U);
    timeline = protected_edit;
    timeline_count = sizeof(protected_edit) / sizeof(protected_edit[0]);
    eeprom_menu();
    assert(now_ms == 2320U && writes == 0U && screen_draws == 1U);
    assert(eeprom_bytes[0] == 0xa5U);
    puts("PASS: diagnostic/EEPROM key debounce, accelerated hold, release, wrap, page reset, safe confirmation");
    return 0;
}
