#include "menu.h"
#include "gui.h"

#define MENU_ITEM_COUNT       4U
#define MENU_EVENT_QUEUE_SIZE 8U
#define MENU_REFRESH_TICKS    10U

typedef enum
{
    MENU_PAGE_HOME = 0,
    MENU_PAGE_SYSTEM_INFO,
    MENU_PAGE_MONITOR,
    MENU_PAGE_GPS,
    MENU_PAGE_DRAW_BOARD
} MenuPage;

static const MenuPage menu_items[MENU_ITEM_COUNT] =
{
    MENU_PAGE_SYSTEM_INFO,
    MENU_PAGE_MONITOR,
    MENU_PAGE_GPS,
    MENU_PAGE_DRAW_BOARD
};

static MenuKey event_queue[MENU_EVENT_QUEUE_SIZE];
static uint8_t event_read_index;
static uint8_t event_write_index;
static uint8_t selected_item;
static uint8_t refresh_tick_count;
static uint8_t page_dirty;
static uint8_t page_changed;
static uint8_t refresh_due;
static MenuPage current_page;

static uint8_t menu_get_key(MenuKey *key)
{
    if(event_read_index == event_write_index)
    {
        return 0;
    }

    *key = event_queue[event_read_index];
    event_read_index = (uint8_t)((event_read_index + 1U) % MENU_EVENT_QUEUE_SIZE);
    return 1;
}

static void menu_handle_home_key(MenuKey key)
{
    switch(key)
    {
        case MENU_KEY_LEFT:
            selected_item = (selected_item == 0U) ?
                            (MENU_ITEM_COUNT - 1U) : (selected_item - 1U);
            page_dirty = 1;
            break;

        case MENU_KEY_RIGHT:
            selected_item = (uint8_t)((selected_item + 1U) % MENU_ITEM_COUNT);
            page_dirty = 1;
            break;

        case MENU_KEY_OK:
            current_page = menu_items[selected_item];
            page_dirty = 1;
            page_changed = 1;
            break;

        case MENU_KEY_BACK:
        default:
            break;
    }
}

static void menu_handle_page_key(MenuKey key)
{
    if(key == MENU_KEY_BACK)
    {
        current_page = MENU_PAGE_HOME;
        page_dirty = 1;
        page_changed = 1;
    }
}

static void menu_draw_current_page(void)
{
    if(page_changed)
    {
        page_changed = 0;
        gui_prepare_page();
    }

    switch(current_page)
    {
        case MENU_PAGE_HOME:
            main_menu(selected_item);
            break;

        case MENU_PAGE_SYSTEM_INFO:
            system_basic_information();
            break;

        case MENU_PAGE_MONITOR:
            mpu6050_euler_information();
            break;

        case MENU_PAGE_GPS:
            system_data_read_and_set();
            break;

        case MENU_PAGE_DRAW_BOARD:
            Draw_Board();
            break;

        default:
            current_page = MENU_PAGE_HOME;
            main_menu(selected_item);
            break;
    }
}

static void menu_refresh_dynamic_page(void)
{
    if(current_page == MENU_PAGE_MONITOR)
    {
        mpu6050_euler_information();
    }
    else if(current_page == MENU_PAGE_GPS)
    {
        system_data_read_and_set();
    }
}

void menu_init(void)
{
    event_read_index = 0;
    event_write_index = 0;
    selected_item = 0;
    refresh_tick_count = 0;
    refresh_due = 0;
    current_page = MENU_PAGE_HOME;
    page_dirty = 1;
    page_changed = 1;
}

void menu_post_key(MenuKey key)
{
    uint8_t next_index = (uint8_t)((event_write_index + 1U) % MENU_EVENT_QUEUE_SIZE);

    if(next_index == event_read_index)
    {
        return;
    }

    event_queue[event_write_index] = key;
    event_write_index = next_index;
}

void menu_tick_10ms(void)
{
    if(++refresh_tick_count >= MENU_REFRESH_TICKS)
    {
        refresh_tick_count = 0;
        refresh_due = 1;
    }
}

void menu_process(void)
{
    MenuKey key;

    while(menu_get_key(&key))
    {
        if(current_page == MENU_PAGE_HOME)
        {
            menu_handle_home_key(key);
        }
        else
        {
            menu_handle_page_key(key);
        }
    }

    if(page_dirty)
    {
        page_dirty = 0;
        refresh_due = 0;
        menu_draw_current_page();
    }
    else if(refresh_due)
    {
        refresh_due = 0;
        menu_refresh_dynamic_page();
    }
}
