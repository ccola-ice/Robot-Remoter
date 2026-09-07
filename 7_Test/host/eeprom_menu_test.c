#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "menu.h"

static uint8_t bytes[256];
static const int *keys;
static unsigned key_count, key_index, writes, read_error, mismatch, failure_shown;
static void diag_release(void) {}
static void diag_screen(const char *s) {(void)s;}
static void diag_line(uint16_t y, const char *s)
{
    (void)y;
    if(strstr(s,"WRITE FAILED")) failure_shown=1U;
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
    keys=events; key_count=size; key_index=writes=failure_shown=0U;
    read_error=fail_read; mismatch=wrong_value;
    eeprom_menu();
    assert(key_index == key_count);
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
