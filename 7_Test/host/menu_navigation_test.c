#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "menu.h"
#include "menu_catalog.h"
#include "control_link.h"
#include "gui.h"

#define WHITE 0xffffU
#define BLACK 0U
#include "menu_state.inc"

static unsigned draws, inhibits, resumes, diagnostics, eeproms;
static unsigned partial_updates, full_updates, commits;
static uint8_t transaction;
static unsigned nrf_loads, browser_loads, param_loads, raw_draws, output_draws, snapshots;
static void menu_nrf_load_settings(void) { nrf_loads++; }
static void menu_browser_load_drives(void) { browser_loads++; }
static void menu_param_load(void) { param_loads++; }
/* These page-specific editors are outside this navigation extraction. */
static void menu_handle_nrf_key(MenuKey key) { (void)key; assert(0); }
static void menu_handle_browser_key(MenuKey key) { (void)key; assert(0); }
static void menu_handle_param_key(MenuKey key) { (void)key; assert(0); }
static void diagnostics_menu(void) { diagnostics++; menu_post_key(MENU_KEY_OK); }
static void eeprom_menu(void) { eeproms++; menu_post_key(MENU_KEY_RIGHT); }
static void user_BUTTON_resume(void) { resumes++; }
static void LCD_SetBackColor(unsigned color) { (void)color; }
static void LCD_SetTextColor(unsigned color) { (void)color; }
static void menu_draw_current_page(void)
{
    if(page_changed) { full_updates++; transaction = 1U; }
    else assert(transaction == 2U);
    draws++; page_changed = 0U;
}
static void menu_refresh_dynamic_page(void) { assert(transaction == 2U); }
static void menu_robot_telemetry_reset(void) {}
static void menu_robot_telemetry_service(void) {}
void gui_clock_overlay(void) { assert(transaction != 0U); }
static void LCD_BeginUpdate(void) { partial_updates++; transaction = 2U; }
static void LCD_EndPage(void) { assert(transaction != 0U); transaction = 0U; commits++; }
void control_link_inhibit(void) { inhibits++; }
void control_link_get_snapshot(ControlLinkSnapshot *snapshot)
{ memset(snapshot, 0, sizeof(*snapshot)); snapshots++; }
void channel_monitor_page(void) { raw_draws++; }
void channel_output_monitor_page(const ControlLinkSnapshot *snapshot)
{ assert(snapshot && !snapshot->sent); output_draws++; }

#include "menu_functions.inc"

static void press(MenuKey key)
{ menu_post_key(key); menu_process(); }

static void open_group(unsigned group)
{
    menu_init(); menu_process();
    while(group--) press(MENU_KEY_RIGHT);
    press(MENU_KEY_OK);
    assert(current_page == MENU_PAGE_CATEGORY && !menu_control_active());
}

static void test_all_routes(void)
{
    static const MenuPage expected[] = {
        MENU_PAGE_ROBOT_CONTROL, MENU_PAGE_MONITOR, MENU_PAGE_DIGITAL_CHANNELS,
        MENU_PAGE_PARAMETER_SETTINGS, MENU_PAGE_NRF, MENU_PAGE_SYSTEM_INFO,
        MENU_PAGE_IMU, MENU_PAGE_GPS, MENU_PAGE_FILE_BROWSER,
        MENU_PAGE_CATEGORY, MENU_PAGE_CATEGORY
    };
    unsigned entry, step;
    for(entry = 0U; entry < sizeof(expected) / sizeof(expected[0]); entry++) {
        unsigned group = entry < 3U ? 0U : entry < 5U ? 1U : 2U;
        unsigned first = group == 0U ? 0U : group == 1U ? 3U : 5U;
        open_group(group);
        for(step = first; step < entry; step++) press(MENU_KEY_RIGHT);
        assert(selected_item == entry);
        press(MENU_KEY_OK);
        assert(current_page == expected[entry]);
        assert(menu_control_active() == (entry == 0U));
        if(entry == 3U || entry == 4U || entry == 8U) continue;
        if(current_page != MENU_PAGE_CATEGORY) press(MENU_KEY_BACK);
        assert(current_page == MENU_PAGE_CATEGORY && selected_item == entry);
        assert(!menu_control_active());
        press(MENU_KEY_BACK);
        assert(current_page == MENU_PAGE_HOME && selected_group == group);
        press(MENU_KEY_OK);
        assert(current_page == MENU_PAGE_CATEGORY && selected_item == entry);
    }
    assert(nrf_loads == 1U && browser_loads == 1U && param_loads == 1U);
    assert(diagnostics == 1U && eeproms == 1U && resumes == 2U);
}

static void test_wrap_and_memory(void)
{
    unsigned group, count;
    static const unsigned first[] = {0U, 3U, 5U};
    static const unsigned size[] = {3U, 2U, 6U};
    menu_init(); menu_process();
    press(MENU_KEY_LEFT); assert(selected_group == 2U);
    press(MENU_KEY_RIGHT); assert(selected_group == 0U);
    for(group = 0U; group < 3U; group++) {
        press(MENU_KEY_OK);
        assert(selected_item == first[group]);
        press(MENU_KEY_LEFT); assert(selected_item == first[group] + size[group] - 1U);
        press(MENU_KEY_RIGHT); assert(selected_item == first[group]);
        for(count = 0U; count < size[group] - 1U; count++) press(MENU_KEY_RIGHT);
        press(MENU_KEY_BACK); press(MENU_KEY_RIGHT);
    }
    for(group = 0U; group < 3U; group++) {
        press(MENU_KEY_OK); assert(selected_item == first[group] + size[group] - 1U);
        press(MENU_KEY_BACK); press(MENU_KEY_RIGHT);
    }
}

static void test_monitor_is_observer(void)
{
    unsigned i, original_inhibits = inhibits;
    static const MenuKey toggles[] = {MENU_KEY_RIGHT, MENU_KEY_LEFT, MENU_KEY_OK};
    open_group(0U); press(MENU_KEY_RIGHT); press(MENU_KEY_OK);
    assert(current_page == MENU_PAGE_MONITOR);
    menu_draw_monitor(); assert(raw_draws == 1U && snapshots == 0U);
    for(i = 0U; i < 12U; i++) {
        press(toggles[i % 3U]); menu_draw_monitor();
        assert(current_page == MENU_PAGE_MONITOR && !menu_control_active());
    }
    assert(snapshots == 6U && output_draws == 6U && raw_draws == 7U);
    assert(inhibits == original_inhibits);
    press(MENU_KEY_BACK); assert(selected_item == MENU_ENTRY_MONITOR);
    press(MENU_KEY_LEFT); press(MENU_KEY_OK); assert(menu_control_active());
    press(MENU_KEY_BACK); assert(!menu_control_active() && inhibits == original_inhibits + 1U);
}

static void test_render_transactions(void)
{
    unsigned partial_before, full_before, commits_before, i;
    menu_init(); menu_process();
    full_before = full_updates; partial_before = partial_updates;
    press(MENU_KEY_RIGHT);
    assert(partial_updates == partial_before + 1U && full_updates == full_before);
    press(MENU_KEY_OK);
    assert(full_updates == full_before + 1U);
    partial_before = partial_updates; commits_before = commits;
    for(i = 0U; i < 6U; i++) menu_post_key(i & 1U ? MENU_KEY_LEFT : MENU_KEY_RIGHT);
    menu_process();
    assert(partial_updates == partial_before + 1U && commits == commits_before + 1U);
    partial_before = partial_updates;
    refresh_due = clock_refresh_due = 1U;
    menu_process();
    assert(partial_updates == partial_before + 1U && transaction == 0U);
    partial_before = partial_updates;
    clock_refresh_due = 1U; menu_process();
    assert(partial_updates == partial_before + 1U && transaction == 0U);
}

int main(void)
{
    test_all_routes(); test_wrap_and_memory(); test_monitor_is_observer(); test_render_transactions();
    assert(draws > 0U);
    puts("Menu navigation: all 11 routes, category wrap/selection memory, service event discard, monitor isolation and robot exit: PASS");
    return 0;
}
