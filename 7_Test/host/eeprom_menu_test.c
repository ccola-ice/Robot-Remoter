#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "menu.h"

#define DIAG_UI_BLACK  0U
#define DIAG_UI_WHITE  1U
#define DIAG_UI_BLUE   2U
#define DIAG_UI_GREY   3U
#define DIAG_UI_RED    4U

static uint8_t bytes[256];
static const int *keys;
static unsigned key_count, key_index, writes, read_error, mismatch, failure_shown;
static unsigned screen_draws;
static void diag_release(void) {}
static void diag_screen(const char *s) {(void)s; screen_draws++;}
static void diag_ui_fill(uint16_t x, uint16_t y, uint16_t width,
                         uint16_t height, uint8_t color)
{
    (void)x; (void)y; (void)width; (void)height; (void)color;
}
static void diag_ui_frame(uint16_t x, uint16_t y, uint16_t width,
                          uint16_t height, uint8_t color)
{
    (void)x; (void)y; (void)width; (void)height; (void)color;
}
static void diag_ui_text(uint16_t x, uint16_t y, uint16_t size,
                         uint8_t foreground, uint8_t background, const char *s)
{
    (void)x; (void)y; (void)size; (void)foreground; (void)background;
    if(strstr(s,"\xD0\xB4\xC8\xEB\xCA\xA7\xB0\xDC")) failure_shown=1U;
}
static int diag_key(void)
{
    assert(key_index < key_count); /* Reject unintended extra confirmation / stuck loops. */
    return keys[key_index++];
}
static void Delay_ms(unsigned ms) {(void)ms;}
static uint8_t EEPROM_Random_Read(uint8_t address, uint8_t *value)
{
    *value=bytes[address]; return (uint8_t)read_error;
}
static uint8_t EEPROM_Byte_Write(uint8_t address, uint8_t value)
{
    assert(address != 255U); /* Test-reserved byte is read only in the editor. */
    writes++; bytes[address]=value ^ mismatch; return 0U;
}
#include "eeprom_menu.c"

static void run(const int *events, unsigned size, unsigned fail_read, unsigned wrong_value)
{
    memset(bytes,0xa5,sizeof(bytes));
    keys=events; key_count=size; key_index=writes=failure_shown=screen_draws=0U;
    read_error=fail_read; mismatch=wrong_value;
    eeprom_menu();
    assert(key_index == key_count);
    assert(screen_draws == 1U); /* Key navigation must use partial redraws. */
}
#define RUN(events,r,m) run(events,sizeof(events)/sizeof(events[0]),r,m)
int main(void)
{
    static const int save[]={MENU_KEY_OK,MENU_KEY_RIGHT,MENU_KEY_OK,MENU_KEY_LEFT,
                             MENU_KEY_OK,MENU_KEY_OK,MENU_KEY_BACK};
    static const int discard[]={MENU_KEY_OK,MENU_KEY_RIGHT,MENU_KEY_OK,MENU_KEY_LEFT,
                                MENU_KEY_OK,MENU_KEY_BACK,MENU_KEY_BACK};
    static const int reserved[]={MENU_KEY_LEFT,MENU_KEY_OK,MENU_KEY_BACK};
    static const int unreadable[]={MENU_KEY_OK,MENU_KEY_BACK};
    unsigned i;
    RUN(save,0U,0U); assert(writes==1U && bytes[0]==0xb4U && !failure_shown);
    for(i=1;i<256;i++) assert(bytes[i]==0xa5U);
    RUN(discard,0U,0U); assert(!writes);
    for(i=0;i<256;i++) assert(bytes[i]==0xa5U);
    RUN(reserved,0U,0U); assert(!writes && bytes[255]==0xa5U);
    RUN(unreadable,1U,0U); assert(!writes);
    RUN(save,0U,1U); assert(writes==1U && failure_shown);
    puts("PASS: EEPROM editor explicit confirmation, cancel, reserved byte, read/write failure");
    return 0;
}
