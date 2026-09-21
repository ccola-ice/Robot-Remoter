#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "menu.h"
static uint8_t pins[4] = {1, 1, 1, 1};
static unsigned events[4], repeats[4], attempts[4];
static uint32_t now_ms, repeat_times[64];
static uint8_t queue_busy, repeat_pending, auto_consume = 1U;
static MenuKey pending_key;
static void BUTTON_GPIO_Config(void) {}
static uint8_t GPIO_ReadInputDataBit(unsigned port, unsigned pin)
{ assert(port == 0U && pin < 4U); return pins[pin]; }
static int get_tick_count(unsigned long *now) { *now = now_ms; return 0; }
void menu_post_key(MenuKey key) { events[key]++; }
void menu_post_repeat(MenuKey key)
{
    assert(key == MENU_KEY_LEFT || key == MENU_KEY_RIGHT);
    attempts[key]++;
    if(queue_busy || repeat_pending) return;
    pending_key = key; repeat_pending = 1U;
}
#include "multi_button_user.c"

enum { MENU_PAGE_HOME, MENU_PAGE_MONITOR, MENU_PAGE_ROBOT_CONTROL };
static unsigned current_page, refresh_tick_count, clock_refresh_tick_count;
static uint8_t refresh_due, clock_refresh_due;
#include "menu_tick.inc"

static void consume_repeat(void)
{
    if(repeat_pending && user_BUTTON_repeat_held((uint8_t)pending_key)) {
        assert(repeats[pending_key] < 64U);
        repeat_times[repeats[pending_key]++] = now_ms;
        events[pending_key]++;
    }
    repeat_pending = 0U;
}
static void ticks(unsigned count)
{
    while(count--) {
        now_ms += 10U;
        button_ticks();
        if(auto_consume) consume_repeat();
    }
}
static void reset_input(uint32_t start)
{
    memset(pins,1,sizeof(pins));
    user_BUTTON_resume();
    memset(events,0,sizeof(events));
    memset(repeats,0,sizeof(repeats));
    memset(attempts,0,sizeof(attempts));
    memset(repeat_times,0,sizeof(repeat_times));
    now_ms=start; queue_busy=repeat_pending=0U; auto_consume=1U;
}

static void test_press_edges_and_bounce(void)
{
    unsigned pin;
    static const MenuKey keys[4] = {MENU_KEY_OK, MENU_KEY_BACK, MENU_KEY_LEFT, MENU_KEY_RIGHT};
    for(pin = 0; pin < 4U; pin++) {
        MenuKey key = keys[pin];
        reset_input(0U);
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
        ticks(40U); /* A short hold does not trigger a repeat. */
        assert(events[key] == 2U);
        if(key == MENU_KEY_OK || key == MENU_KEY_BACK) {
            ticks(200U); /* Actions and BACK remain edge-only. */
            assert(events[key] == 2U);
        }
        pins[pin] = 1U; ticks(10U);
        assert(events[key] == 2U);
    }
}

static void test_repeat_timing_and_acceleration(uint32_t start)
{
    uint32_t pressed;
    reset_input(start);
    pins[2]=0U; ticks(3U); pressed=now_ms;
    assert(events[MENU_KEY_LEFT]==1U);
    ticks(50U); assert(repeats[MENU_KEY_LEFT]==0U);
    ticks(1U); assert(repeats[MENU_KEY_LEFT]==1U);
    assert((uint32_t)(repeat_times[0]-pressed)==510U);
    ticks(11U); assert(repeats[MENU_KEY_LEFT]==1U);
    ticks(1U); assert(repeats[MENU_KEY_LEFT]==2U);
    assert((uint32_t)(repeat_times[1]-repeat_times[0])==120U);
    while((uint32_t)(now_ms-pressed)<1500U) ticks(1U);
    assert(repeats[MENU_KEY_LEFT]==9U);
    ticks(3U); assert(repeats[MENU_KEY_LEFT]==10U);
    assert((uint32_t)(repeat_times[9]-repeat_times[8])==60U);
    ticks(5U); assert(repeats[MENU_KEY_LEFT]==10U);
    ticks(1U); assert(repeats[MENU_KEY_LEFT]==11U);
    /* Raw release suppresses a due repeat before the three-sample up debounce. */
    ticks(5U); pins[2]=1U;
    assert(!user_BUTTON_repeat_held(MENU_KEY_LEFT));
    ticks(1U); assert(repeats[MENU_KEY_LEFT]==11U);
    ticks(100U); assert(repeats[MENU_KEY_LEFT]==11U);
    assert(!user_BUTTON_repeat_held(MENU_KEY_OK) && !user_BUTTON_repeat_held(255U));
}

static void test_second_hold_and_release_bounce(void)
{
    unsigned count;
    reset_input(0U);
    pins[3]=0U; ticks(3U); pins[3]=1U; ticks(3U);
    pins[3]=0U; ticks(3U); ticks(51U);
    assert(events[MENU_KEY_RIGHT]==3U && repeats[MENU_KEY_RIGHT]==1U);
    count=events[MENU_KEY_RIGHT];
    pins[3]=1U; ticks(1U); pins[3]=0U; ticks(1U);
    assert(events[MENU_KEY_RIGHT]==count); /* A bounce is not another press. */
    pins[3]=1U; ticks(3U); ticks(100U);
    assert(events[MENU_KEY_RIGHT]==count);
}

static void test_busy_and_delayed_foreground(void)
{
    unsigned count;
    reset_input(0U);
    pins[2]=0U; ticks(3U); ticks(51U);
    count=repeats[MENU_KEY_LEFT];
    queue_busy=1U; ticks(100U);
    assert(repeats[MENU_KEY_LEFT]==count && attempts[MENU_KEY_LEFT]>count);
    queue_busy=0U; ticks(6U);
    assert(repeats[MENU_KEY_LEFT]==count+1U); /* No replay of dropped attempts. */
    count=repeats[MENU_KEY_LEFT];
    now_ms+=5000U;
    button_ticks(); consume_repeat();
    assert(repeats[MENU_KEY_LEFT]==count+1U);
    button_ticks(); consume_repeat();
    assert(repeats[MENU_KEY_LEFT]==count+1U); /* Same wall clock cannot catch up. */
    ticks(5U); assert(repeats[MENU_KEY_LEFT]==count+1U);
    ticks(1U); assert(repeats[MENU_KEY_LEFT]==count+2U);
    auto_consume=0U; ticks(30U);
    assert(repeat_pending); /* Only one pending request, regardless of hold duration. */
    count=repeats[MENU_KEY_LEFT];
    pins[2]=1U; consume_repeat();
    assert(repeats[MENU_KEY_LEFT]==count && !repeat_pending);
}

static void test_context_cancel_and_service_resume(void)
{
    unsigned count;
    reset_input(0U);
    pins[2]=0U; ticks(3U); ticks(51U);
    count=events[MENU_KEY_LEFT];
    user_BUTTON_cancel_repeat(); ticks(100U);
    assert(events[MENU_KEY_LEFT]==count && !user_BUTTON_repeat_held(MENU_KEY_LEFT));
    pins[2]=1U; ticks(3U); pins[2]=0U; ticks(3U); ticks(51U);
    assert(events[MENU_KEY_LEFT]==count+2U);
    reset_input(0U);
    memset(pins,0,sizeof(pins)); /* All navigation keys held in a blocking service. */
    user_BUTTON_resume();
    ticks(200U);
    assert(!events[MENU_KEY_OK] && !events[MENU_KEY_BACK] && !events[MENU_KEY_LEFT] && !events[MENU_KEY_RIGHT]);
    assert(!attempts[MENU_KEY_LEFT] && !attempts[MENU_KEY_RIGHT]);
    memset(pins,1,sizeof(pins)); ticks(3U);
    pins[2]=0U; ticks(3U); ticks(51U);
    assert(events[MENU_KEY_LEFT]==2U && repeats[MENU_KEY_LEFT]==1U);
}

int main(void)
{
    unsigned i;
    user_BUTTON_init();
    test_press_edges_and_bounce();
    test_repeat_timing_and_acceleration(0U);
    test_repeat_timing_and_acceleration(0xffffff00UL);
    test_second_hold_and_release_bounce();
    test_busy_and_delayed_foreground();
    test_context_cancel_and_service_resume();
    for(current_page = MENU_PAGE_HOME; current_page <= MENU_PAGE_ROBOT_CONTROL; current_page++) {
        unsigned count = current_page == MENU_PAGE_HOME ? 5U : 2U;
        refresh_tick_count = 0U;
        refresh_due = 0U;
        for(i = 1U; i < count; i++) { menu_tick_10ms(); assert(!refresh_due); }
        menu_tick_10ms();
        assert(refresh_due);
    }
    puts("UI input: debounce/short press, direction repeat 510/120/60 ms, wrap, raw release, backlog drop, context/service suppression and 20 ms scheduling passed.");
    return 0;
}
