#include "menu.h"
#include "gui.h"
#include "param.h"
#include "platform_nrf.h"
#include "ff.h"

#include <stdio.h>
#include <string.h>

#define MENU_ITEM_COUNT       7U
#define MENU_EVENT_QUEUE_SIZE 8U
#define MENU_REFRESH_TICKS    5U
#define NRF_MENU_ITEM_COUNT   6U
#define NRF_SETTING_COUNT     4U
#define BROWSER_MAX_ENTRIES   64U
#define BROWSER_VISIBLE_ROWS  6U
#define BROWSER_PATH_LENGTH   256U

typedef enum
{
    MENU_PAGE_HOME = 0,
    MENU_PAGE_SYSTEM_INFO,
    MENU_PAGE_MONITOR,
    MENU_PAGE_IMU,
    MENU_PAGE_GPS,
    MENU_PAGE_DRAW_BOARD,
    MENU_PAGE_NRF,
    MENU_PAGE_FILE_BROWSER
} MenuPage;

static const MenuPage menu_items[MENU_ITEM_COUNT] =
{
    MENU_PAGE_SYSTEM_INFO,
    MENU_PAGE_MONITOR,
    MENU_PAGE_IMU,
    MENU_PAGE_GPS,
    MENU_PAGE_DRAW_BOARD,
    MENU_PAGE_NRF,
    MENU_PAGE_FILE_BROWSER
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
static GuiFileEntry browser_entries[BROWSER_MAX_ENTRIES];
static char browser_path[BROWSER_PATH_LENGTH];
static char browser_status[80];
static uint8_t browser_item_count;
static uint8_t browser_selected_item;
static uint8_t browser_first_visible;
static uint8_t browser_virtual_root;
static uint16_t browser_revision;

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

static void menu_browser_set_status(const char *text)
{
    strncpy(browser_status, text, sizeof(browser_status) - 1U);
    browser_status[sizeof(browser_status) - 1U] = '\0';
}

static void menu_browser_load_drives(void)
{
    memset(browser_entries, 0, sizeof(browser_entries));
    strcpy(browser_path, "Available volumes");
    strcpy(browser_entries[0].name, "SD Card [0:]");
    strcpy(browser_entries[1].name, "SPI Flash [1:]");
    browser_entries[0].is_directory = 1U;
    browser_entries[1].is_directory = 1U;
    browser_item_count = 2U;
    browser_selected_item = 0U;
    browser_first_visible = 0U;
    browser_virtual_root = 1U;
    browser_revision++;
    menu_browser_set_status("Select a volume and press OK");
}

static uint8_t menu_browser_load_directory(void)
{
    DIR directory;
    FILINFO file_info;
    FRESULT result;
    char long_name[_MAX_LFN + 1U];
    const char *source_name;

    browser_item_count = 0U;
    result = f_opendir(&directory, browser_path);
    if(result != FR_OK)
    {
        sprintf(browser_status, "Open failed - FatFs error %u", (uint16_t)result);
        browser_selected_item = 0U;
        browser_first_visible = 0U;
        browser_virtual_root = 0U;
        browser_revision++;
        return 0U;
    }

    while(browser_item_count < BROWSER_MAX_ENTRIES)
    {
        memset(&file_info, 0, sizeof(file_info));
        long_name[0] = '\0';
        file_info.lfname = long_name;
        file_info.lfsize = sizeof(long_name);
        result = f_readdir(&directory, &file_info);
        if((result != FR_OK) || (file_info.fname[0] == '\0'))
        {
            break;
        }

        source_name = (long_name[0] != '\0') ? long_name : file_info.fname;
        if((strcmp(source_name, ".") == 0) || (strcmp(source_name, "..") == 0))
        {
            continue;
        }

        strncpy(browser_entries[browser_item_count].name, source_name,
                GUI_FILE_NAME_LENGTH - 1U);
        browser_entries[browser_item_count].name[GUI_FILE_NAME_LENGTH - 1U] = '\0';
        browser_entries[browser_item_count].size = file_info.fsize;
        browser_entries[browser_item_count].date = file_info.fdate;
        browser_entries[browser_item_count].time = file_info.ftime;
        browser_entries[browser_item_count].is_directory =
            ((file_info.fattrib & AM_DIR) != 0U) ? 1U : 0U;
        browser_item_count++;
    }

    f_closedir(&directory);
    browser_selected_item = 0U;
    browser_first_visible = 0U;
    browser_virtual_root = 0U;
    browser_revision++;
    if(result != FR_OK)
    {
        sprintf(browser_status, "Read failed - FatFs error %u", (uint16_t)result);
    }
    else if(browser_item_count >= BROWSER_MAX_ENTRIES)
    {
        menu_browser_set_status("Showing the first 64 entries");
    }
    else
    {
        sprintf(browser_status, "%u item(s)", (uint16_t)browser_item_count);
    }
    return 1U;
}

static void menu_browser_adjust_window(void)
{
    if(browser_selected_item < browser_first_visible)
    {
        browser_first_visible = browser_selected_item;
    }
    else if(browser_selected_item >=
            (uint8_t)(browser_first_visible + BROWSER_VISIBLE_ROWS))
    {
        browser_first_visible =
            (uint8_t)(browser_selected_item - BROWSER_VISIBLE_ROWS + 1U);
    }
}

static void menu_browser_enter_selected(void)
{
    size_t path_length;
    size_t name_length;

    if(browser_item_count == 0U)
    {
        return;
    }

    if(browser_virtual_root != 0U)
    {
        strcpy(browser_path, (browser_selected_item == 0U) ? "0:" : "1:");
        menu_browser_load_directory();
        page_dirty = 1U;
        page_changed = 1U;
        return;
    }

    if(browser_entries[browser_selected_item].is_directory != 0U)
    {
        path_length = strlen(browser_path);
        name_length = strlen(browser_entries[browser_selected_item].name);
        if((path_length + name_length + 2U) >= sizeof(browser_path))
        {
            menu_browser_set_status("Path is too long");
            page_dirty = 1U;
            return;
        }

        browser_path[path_length++] = '/';
        memcpy(&browser_path[path_length],
               browser_entries[browser_selected_item].name, name_length + 1U);
        menu_browser_load_directory();
        page_dirty = 1U;
        page_changed = 1U;
    }
    else
    {
        sprintf(browser_status, "File: %.42s | %lu bytes",
                browser_entries[browser_selected_item].name,
                browser_entries[browser_selected_item].size);
        page_dirty = 1U;
    }
}

static void menu_browser_go_back(void)
{
    char *last_separator;

    if(browser_virtual_root != 0U)
    {
        current_page = MENU_PAGE_HOME;
        page_dirty = 1U;
        page_changed = 1U;
        return;
    }

    last_separator = strrchr(browser_path, '/');
    if(last_separator == NULL)
    {
        menu_browser_load_drives();
    }
    else
    {
        *last_separator = '\0';
        menu_browser_load_directory();
    }
    page_dirty = 1U;
    page_changed = 1U;
}

static void menu_handle_browser_key(MenuKey key)
{
    switch(key)
    {
        case MENU_KEY_LEFT:
            if(browser_item_count != 0U)
            {
                browser_selected_item = (browser_selected_item == 0U) ?
                    (browser_item_count - 1U) : (browser_selected_item - 1U);
                menu_browser_adjust_window();
                page_dirty = 1U;
            }
            break;

        case MENU_KEY_RIGHT:
            if(browser_item_count != 0U)
            {
                browser_selected_item =
                    (uint8_t)((browser_selected_item + 1U) % browser_item_count);
                menu_browser_adjust_window();
                page_dirty = 1U;
            }
            break;

        case MENU_KEY_OK:
            menu_browser_enter_selected();
            break;

        case MENU_KEY_BACK:
            menu_browser_go_back();
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
            else if(current_page == MENU_PAGE_FILE_BROWSER)
            {
                menu_browser_load_drives();
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

    if(current_page == MENU_PAGE_FILE_BROWSER)
    {
        menu_handle_browser_key(key);
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
            channel_monitor_page();
            break;

        case MENU_PAGE_IMU:
            imu6050_information();
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

        case MENU_PAGE_FILE_BROWSER:
            file_browser_page(browser_path, browser_entries, browser_item_count,
                              browser_selected_item, browser_first_visible,
                              browser_revision, browser_status);
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
        channel_monitor_page();
    }
    else if(current_page == MENU_PAGE_IMU)
    {
        imu6050_information();
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
