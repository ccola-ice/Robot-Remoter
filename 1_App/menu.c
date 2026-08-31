#include "menu.h"
#include "gui.h"
#include "param.h"
#include "platform_nrf.h"

#define MENU_ITEM_COUNT       5U
#define MENU_EVENT_QUEUE_SIZE 8U
#define MENU_REFRESH_TICKS    10U
#define NRF_MENU_ITEM_COUNT   6U
#define NRF_SETTING_COUNT     4U

typedef enum
{
    MENU_PAGE_HOME = 0,
    MENU_PAGE_SYSTEM_INFO,
    MENU_PAGE_MONITOR,
    MENU_PAGE_GPS,
    MENU_PAGE_DRAW_BOARD,
    MENU_PAGE_NRF
} MenuPage;

static const MenuPage menu_items[MENU_ITEM_COUNT] =
{
    MENU_PAGE_SYSTEM_INFO,
    MENU_PAGE_MONITOR,
    MENU_PAGE_GPS,
    MENU_PAGE_DRAW_BOARD,
    MENU_PAGE_NRF
};

static const uint8_t nrf_power_register[4] = {0x09U, 0x0bU, 0x0dU, 0x0fU};
static const char * const nrf_status_text[] =
{
    "Ready",
    "Settings saved; register readback OK",
    "Module check passed",
    "Module check FAILED",
    "Settings saved; register readback FAILED",
    "Unsaved change; run Apply & Save"
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
static uint8_t nrf_selected_item;
static uint8_t nrf_editing;
static uint8_t nrf_enabled;
static uint8_t nrf_channel;
static uint8_t nrf_power_index;
static uint8_t nrf_data_rate;
static uint8_t nrf_status;
static uint8_t nrf_runtime_valid;
static uint8_t nrf_runtime_enabled;
static uint8_t nrf_runtime_channel;
static uint8_t nrf_runtime_power_index;
static uint8_t nrf_runtime_data_rate;

static uint8_t menu_nrf_power_index(uint8_t power_register)
{
    uint8_t i;

    for(i = 0; i < 4U; i++)
    {
        if((power_register & 0x06U) == (nrf_power_register[i] & 0x06U))
        {
            return i;
        }
    }
    return 0U;
}

static void menu_nrf_refresh_runtime(void)
{
    uint8_t runtime_power;

    if(nrf24l01_read_runtime(&nrf_runtime_enabled, &nrf_runtime_channel,
                             &runtime_power, &nrf_runtime_data_rate) == 0U)
    {
        nrf_runtime_power_index = menu_nrf_power_index(runtime_power);
        nrf_runtime_valid = 1U;
    }
    else
    {
        nrf_runtime_valid = 0U;
    }
}

static void menu_nrf_load_settings(void)
{
    nrf_selected_item = 0U;
    nrf_editing = 0U;
    nrf_enabled = (param.NRF_Mode != 0U) ? 1U : 0U;
    nrf_channel = (param.NRF_Channel <= 125U) ? param.NRF_Channel : 40U;
    nrf_power_index = menu_nrf_power_index(param.NRF_Power);
    nrf_data_rate = (param.NRF_DataRate <= 2U) ? param.NRF_DataRate : 2U;
    nrf_status = 0U;
    menu_nrf_refresh_runtime();
}

static void menu_nrf_adjust(int8_t direction)
{
    switch(nrf_selected_item)
    {
        case 0U:
            nrf_enabled = (uint8_t)!nrf_enabled;
            break;

        case 1U:
            if(direction < 0)
            {
                nrf_channel = (nrf_channel == 0U) ? 125U : (nrf_channel - 1U);
            }
            else
            {
                nrf_channel = (nrf_channel >= 125U) ? 0U : (nrf_channel + 1U);
            }
            break;

        case 2U:
            if(direction < 0)
            {
                nrf_power_index = (nrf_power_index == 0U) ? 3U : (nrf_power_index - 1U);
            }
            else
            {
                nrf_power_index = (uint8_t)((nrf_power_index + 1U) % 4U);
            }
            break;

        case 3U:
            if(direction < 0)
            {
                nrf_data_rate = (nrf_data_rate == 0U) ? 2U : (nrf_data_rate - 1U);
            }
            else
            {
                nrf_data_rate = (uint8_t)((nrf_data_rate + 1U) % 3U);
            }
            break;

        default:
            break;
    }

    nrf_status = 5U;
}

static void menu_handle_nrf_key(MenuKey key)
{
    if(nrf_editing != 0U)
    {
        if(key == MENU_KEY_LEFT)
        {
            menu_nrf_adjust(-1);
        }
        else if(key == MENU_KEY_RIGHT)
        {
            menu_nrf_adjust(1);
        }
        else if((key == MENU_KEY_OK) || (key == MENU_KEY_BACK))
        {
            nrf_editing = 0U;
        }
        page_dirty = 1U;
        return;
    }

    switch(key)
    {
        case MENU_KEY_LEFT:
            nrf_selected_item = (nrf_selected_item == 0U) ?
                                (NRF_MENU_ITEM_COUNT - 1U) : (nrf_selected_item - 1U);
            page_dirty = 1U;
            break;

        case MENU_KEY_RIGHT:
            nrf_selected_item = (uint8_t)((nrf_selected_item + 1U) % NRF_MENU_ITEM_COUNT);
            page_dirty = 1U;
            break;

        case MENU_KEY_OK:
            if(nrf_selected_item < NRF_SETTING_COUNT)
            {
                nrf_editing = 1U;
            }
            else if(nrf_selected_item == 4U)
            {
                param.NRF_Mode = nrf_enabled;
                param.NRF_Channel = nrf_channel;
                param.NRF_Power = nrf_power_register[nrf_power_index];
                param.NRF_DataRate = nrf_data_rate;
                nrf24l01_apply_settings(param.NRF_Mode, param.NRF_Channel,
                                        param.NRF_Power, param.NRF_DataRate);
                write_param();
                menu_nrf_refresh_runtime();
                nrf_status = (nrf_runtime_valid != 0U) ? 1U : 4U;
            }
            else
            {
                nrf_status = (nrf24l01_check() == 0U) ? 2U : 3U;
                nrf24l01_apply_settings(param.NRF_Mode, param.NRF_Channel,
                                        param.NRF_Power, param.NRF_DataRate);
                menu_nrf_refresh_runtime();
            }
            page_dirty = 1U;
            break;

        case MENU_KEY_BACK:
            current_page = MENU_PAGE_HOME;
            page_dirty = 1U;
            page_changed = 1U;
            break;

        default:
            break;
    }
}

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
            if(current_page == MENU_PAGE_NRF)
            {
                menu_nrf_load_settings();
            }
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
    if(current_page == MENU_PAGE_NRF)
    {
        menu_handle_nrf_key(key);
        return;
    }

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

        case MENU_PAGE_NRF:
            nrf_settings_page(nrf_selected_item, nrf_editing, nrf_enabled,
                              nrf_channel, nrf_power_index, nrf_data_rate,
                              nrf_status_text[nrf_status], nrf_runtime_valid,
                              nrf_runtime_enabled, nrf_runtime_channel,
                              nrf_runtime_power_index, nrf_runtime_data_rate);
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
