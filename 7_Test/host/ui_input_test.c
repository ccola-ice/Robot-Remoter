#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "menu.h"
static uint8_t pins[4] = {1, 1, 1, 1};
static unsigned events[4];
static void BUTTON_GPIO_Config(void) {}
static uint8_t GPIO_ReadInputDataBit(unsigned port, unsigned pin)
{ (void)port; return pins[pin]; }
void menu_post_key(MenuKey key) { events[key]++; }
#include "multi_button_user.c"

enum { MENU_PAGE_HOME, MENU_PAGE_MONITOR, MENU_PAGE_ROBOT_CONTROL };
static unsigned current_page, refresh_tick_count, clock_refresh_tick_count;
static uint8_t refresh_due, clock_refresh_due;
#include "menu_tick.inc"

static void ticks(unsigned count) { while(count--) button_ticks(); }

int main(void)
{
    unsigned pin, i;
    static const MenuKey keys[4] = {MENU_KEY_OK, MENU_KEY_BACK, MENU_KEY_LEFT, MENU_KEY_RIGHT};
    user_BUTTON_init();
    for(pin = 0; pin < 4U; pin++) {
        MenuKey key = keys[pin];
        pins[pin] = 0U; ticks(2U);
        assert(events[key] == 0U);
        pins[pin] = 1U; ticks(1U); /* Contact bounce. */
        pins[pin] = 0U; ticks(2U);
        assert(events[key] == 0U);
        ticks(1U);
        assert(events[key] == 1U && pins[pin] == 0U);
        pins[pin] = 1U; ticks(3U);
        pins[pin] = 0U; ticks(3U); /* Second press inside click timeout. */
        assert(events[key] == 2U);
        ticks(100U); /* Holding it must never re-emit PRESS_DOWN. */
        assert(events[key] == 2U);
        pins[pin] = 1U; ticks(10U);
        assert(events[key] == 2U);
    }
    pins[0] = pins[1] = 0U; /* Keys consumed by a blocking service menu. */
    user_BUTTON_resume();
    ticks(100U);
    assert(events[MENU_KEY_OK] == 2U && events[MENU_KEY_BACK] == 2U);
    pins[0] = pins[1] = 1U; ticks(10U);
    pins[0] = 0U; ticks(3U);
    assert(events[MENU_KEY_OK] == 3U);
    for(current_page = MENU_PAGE_HOME; current_page <= MENU_PAGE_ROBOT_CONTROL; current_page++) {
        unsigned count = current_page == MENU_PAGE_HOME ? 5U : 2U;
        refresh_tick_count = 0U;
        refresh_due = 0U;
        for(i = 1U; i < count; i++) { menu_tick_10ms(); assert(!refresh_due); }
        menu_tick_10ms();
        assert(refresh_due);
    }
    puts("UI input tests passed: press-edge debounce, rapid second hold, service return suppression, 20 ms analog scheduling.");
    return 0;
}
