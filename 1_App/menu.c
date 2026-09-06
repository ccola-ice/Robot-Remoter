#include "menu.h"
#include "gui.h"
#include "gt9xx.h"
#include "param.h"
#include "platform_nrf.h"
#include "bsp_gpio_digital_channel.h"
#include "ff.h"

#include <stdio.h>
#include <string.h>

#define MENU_ITEM_COUNT       9U
#define MENU_EVENT_QUEUE_SIZE 8U
#define MENU_REFRESH_TICKS    5U
#define CLOCK_REFRESH_TICKS   20U
#define NRF_MENU_ITEM_COUNT   6U
#define NRF_SETTING_COUNT     4U
#define BROWSER_MAX_ENTRIES   64U
#define BROWSER_VISIBLE_ROWS  6U
#define BROWSER_PATH_LENGTH   256U
#define PARAM_VISIBLE_ROWS    GUI_PARAM_VISIBLE_ROWS
#define PARAM_GLOBAL_COUNT    18U
#define PARAM_CHANNEL_START   PARAM_GLOBAL_COUNT
#define PARAM_CHANNEL_FIELDS  5U
#define PARAM_ACTION_START    (PARAM_CHANNEL_START + chNum * PARAM_CHANNEL_FIELDS)
#define PARAM_ITEM_COUNT      (PARAM_ACTION_START + 3U)

typedef enum
{
    MENU_PAGE_HOME = 0,
    MENU_PAGE_SYSTEM_INFO,
    MENU_PAGE_MONITOR,
    MENU_PAGE_DIGITAL_CHANNELS,
    MENU_PAGE_IMU,
    MENU_PAGE_GPS,
    MENU_PAGE_DRAW_BOARD,
    MENU_PAGE_NRF,
    MENU_PAGE_FILE_BROWSER,
    MENU_PAGE_PARAMETER_SETTINGS
} MenuPage;

static const MenuPage menu_items[MENU_ITEM_COUNT] =
{
    MENU_PAGE_SYSTEM_INFO,
    MENU_PAGE_MONITOR,
    MENU_PAGE_DIGITAL_CHANNELS,
    MENU_PAGE_IMU,
    MENU_PAGE_GPS,
    MENU_PAGE_DRAW_BOARD,
    MENU_PAGE_NRF,
    MENU_PAGE_FILE_BROWSER,
    MENU_PAGE_PARAMETER_SETTINGS
};

static const uint8_t nrf_power_register[4] = {0x09U, 0x0bU, 0x0dU, 0x0fU};
static const char * const nrf_status_text[] =
{
    "Ready",
    "Settings saved; register readback OK",
    "Module check passed",
    "Module check FAILED",
    "Settings saved; register readback FAILED",
    "Unsaved change; run Apply & Save",
    "SPI Flash save verification FAILED"
};

static MenuKey event_queue[MENU_EVENT_QUEUE_SIZE];
static uint8_t event_read_index;
static uint8_t event_write_index;
static uint8_t selected_item;
static uint8_t refresh_tick_count;
static uint8_t clock_refresh_tick_count;
static uint8_t clock_refresh_due;
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
static param_Config param_edit;
static param_Config param_edit_backup;
static GuiParamRow param_rows[PARAM_VISIBLE_ROWS];
static char param_status[80];
static uint8_t param_selected_item;
static uint8_t param_first_visible;
static uint8_t param_editing;
static uint8_t param_dirty;
static uint8_t param_dirty_before_edit;
static uint16_t param_revision;

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
    param_Config runtime_backup;

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
                memcpy(&runtime_backup, (const void *)&param,
                       sizeof(runtime_backup));
                param.NRF_Mode = nrf_enabled;
                param.NRF_Channel = nrf_channel;
                param.NRF_Power = nrf_power_register[nrf_power_index];
                param.NRF_DataRate = nrf_data_rate;
                if(write_param() != 0U)
                {
                    memcpy((void *)&param, &runtime_backup,
                           sizeof(runtime_backup));
                    nrf_status = 6U;
                    page_dirty = 1U;
                    break;
                }
                nrf24l01_apply_settings(param.NRF_Mode, param.NRF_Channel,
                                        param.NRF_Power, param.NRF_DataRate);
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

static void menu_param_set_status(const char *text)
{
    strncpy(param_status, text, sizeof(param_status) - 1U);
    param_status[sizeof(param_status) - 1U] = '\0';
}

static void menu_param_copy_from_runtime(void)
{
    memcpy(&param_edit, (const void *)&param, sizeof(param_edit));
    param_edit.writeFlag = FM_FLAG;
    param_edit.version = FM_VERSION;
    param_edit.version_time = FM_TIME;
}

static void menu_param_load(void)
{
    menu_param_copy_from_runtime();
    param_selected_item = 0U;
    param_first_visible = 0U;
    param_editing = 0U;
    param_dirty = 0U;
    param_revision++;
    menu_param_set_status("Runtime parameters loaded");
}

static void menu_param_adjust_window(void)
{
    if(param_selected_item < param_first_visible)
    {
        param_first_visible = param_selected_item;
    }
    else if(param_selected_item >=
            (uint8_t)(param_first_visible + PARAM_VISIBLE_ROWS))
    {
        param_first_visible =
            (uint8_t)(param_selected_item - PARAM_VISIBLE_ROWS + 1U);
    }
}

static void menu_param_format_item(uint8_t item_index, GuiParamRow *row)
{
    static const char * const on_off_text[2] = {"OFF", "ON"};
    static const char * const model_text[3] = {"AIRPLANE", "CAR", "BOAT"};
    static const char * const hand_text[2] = {"RIGHT", "LEFT"};
    static const char * const rate_text[3] = {"250 Kbps", "1 Mbps", "2 Mbps"};
    static const int8_t power_dbm[4] = {-18, -12, -6, 0};
    uint8_t channel;
    uint8_t field;
    uint8_t power_index;

    row->label[0] = '\0';
    row->value[0] = '\0';

    switch(item_index)
    {
        case 0U:
            strcpy(row->label, "Firmware");
            sprintf(row->value, "%s / %s", FM_VERSION, FM_TIME);
            break;
        case 1U:
            strcpy(row->label, "TX battery warning");
            sprintf(row->value, "%.1f V", param_edit.warnBatVolt);
            break;
        case 2U:
            strcpy(row->label, "RX battery warning");
            sprintf(row->value, "%.1f V", param_edit.RecWarnBatVolt);
            break;
        case 3U:
            strcpy(row->label, "Battery calibration");
            sprintf(row->value, "%u", param_edit.batVoltAdjust);
            break;
        case 4U:
            strcpy(row->label, "Throttle hand");
            strcpy(row->value, hand_text[param_edit.throttlePreference ? 1U : 0U]);
            break;
        case 5U:
            strcpy(row->label, "Model type");
            strcpy(row->value, model_text[(param_edit.modelType <= 2U) ?
                                          param_edit.modelType : 0U]);
            break;
        case 6U:
            strcpy(row->label, "Trim step");
            sprintf(row->value, "%u", param_edit.PWMadjustUnit);
            break;
        case 7U:
            strcpy(row->label, "Key sound");
            strcpy(row->value, on_off_text[param_edit.keySound ? 1U : 0U]);
            break;
        case 8U:
            strcpy(row->label, "Boot image invert");
            strcpy(row->value, on_off_text[param_edit.onImage ? 1U : 0U]);
            break;
        case 9U:
            strcpy(row->label, "Clock alarm");
            strcpy(row->value, on_off_text[param_edit.clockMode ? 1U : 0U]);
            break;
        case 10U:
            strcpy(row->label, "Alarm time");
            sprintf(row->value, "%u x 5 min", param_edit.clockTime);
            break;
        case 11U:
            strcpy(row->label, "Startup throttle check");
            strcpy(row->value, on_off_text[param_edit.clockCheck ? 1U : 0U]);
            break;
        case 12U:
            strcpy(row->label, "Throttle protect");
            sprintf(row->value, "%u %%", param_edit.throttleProtect);
            break;
        case 13U:
            strcpy(row->label, "PPM output");
            strcpy(row->value, on_off_text[param_edit.PPM_Out ? 1U : 0U]);
            break;
        case 14U:
            strcpy(row->label, "NRF wireless");
            strcpy(row->value, on_off_text[param_edit.NRF_Mode ? 1U : 0U]);
            break;
        case 15U:
            strcpy(row->label, "NRF channel");
            sprintf(row->value, "%u / %u MHz", param_edit.NRF_Channel,
                    (uint16_t)(2400U + param_edit.NRF_Channel));
            break;
        case 16U:
            strcpy(row->label, "NRF TX power");
            power_index = menu_nrf_power_index(param_edit.NRF_Power);
            sprintf(row->value, "%d dBm", power_dbm[power_index]);
            break;
        case 17U:
            strcpy(row->label, "NRF air rate");
            strcpy(row->value, rate_text[(param_edit.NRF_DataRate <= 2U) ?
                                         param_edit.NRF_DataRate : 2U]);
            break;
        default:
            if(item_index < PARAM_ACTION_START)
            {
                channel = (uint8_t)((item_index - PARAM_CHANNEL_START) /
                                    PARAM_CHANNEL_FIELDS);
                field = (uint8_t)((item_index - PARAM_CHANNEL_START) %
                                  PARAM_CHANNEL_FIELDS);
                switch(field)
                {
                    case 0U:
                        sprintf(row->label, "CH%u lower limit", channel + 1U);
                        sprintf(row->value, "%u", param_edit.chLower[channel]);
                        break;
                    case 1U:
                        sprintf(row->label, "CH%u center", channel + 1U);
                        sprintf(row->value, "%u", param_edit.chMiddle[channel]);
                        break;
                    case 2U:
                        sprintf(row->label, "CH%u upper limit", channel + 1U);
                        sprintf(row->value, "%u", param_edit.chUpper[channel]);
                        break;
                    case 3U:
                        sprintf(row->label, "CH%u trim", channel + 1U);
                        sprintf(row->value, "%d", param_edit.PWMadjustValue[channel]);
                        break;
                    default:
                        sprintf(row->label, "CH%u reverse", channel + 1U);
                        strcpy(row->value,
                               on_off_text[param_edit.chReverse[channel] ? 1U : 0U]);
                        break;
                }
            }
            else if(item_index == PARAM_ACTION_START)
            {
                strcpy(row->label, "SAVE ALL PARAMETERS");
                strcpy(row->value, "Press OK");
            }
            else if(item_index == (PARAM_ACTION_START + 1U))
            {
                strcpy(row->label, "RELOAD CURRENT VALUES");
                strcpy(row->value, "Press OK");
            }
            else
            {
                strcpy(row->label, "RESTORE DEFAULTS");
                strcpy(row->value, "Not saved yet");
            }
            break;
    }
}

static void menu_param_adjust_float(void *packed_field, int8_t direction,
                                    int16_t minimum_x10, int16_t maximum_x10)
{
    float value;
    int16_t value_x10;
    uint8_t byte_index;
    uint8_t *value_bytes = (uint8_t *)&value;
    volatile uint8_t *field_bytes = (volatile uint8_t *)packed_field;

    /*
     * param_Config is the byte-packed SPI Flash image.  Its float members are
     * not 4-byte aligned, so dereferencing a float pointer to either member
     * makes ARMCC emit VLDR/VSTR on an unaligned address and can HardFault.
     * Volatile byte copies keep the packed access explicitly byte-addressable
     * while the FPU arithmetic uses an aligned local float.
     */
    for(byte_index = 0U; byte_index < sizeof(value); byte_index++)
    {
        value_bytes[byte_index] = field_bytes[byte_index];
    }
    value_x10 = (int16_t)(value * 10.0f + 0.5f);

    value_x10 = (int16_t)(value_x10 + direction);
    if(value_x10 < minimum_x10)
    {
        value_x10 = minimum_x10;
    }
    else if(value_x10 > maximum_x10)
    {
        value_x10 = maximum_x10;
    }
    value = (float)value_x10 / 10.0f;
    for(byte_index = 0U; byte_index < sizeof(value); byte_index++)
    {
        field_bytes[byte_index] = value_bytes[byte_index];
    }
}

static void menu_param_adjust(int8_t direction)
{
    uint8_t channel;
    uint8_t field;
    uint8_t power_index;
    uint16_t value;
    int trim;

    switch(param_selected_item)
    {
        case 1U:
            menu_param_adjust_float(&param_edit.warnBatVolt, direction, 25, 50);
            break;
        case 2U:
            menu_param_adjust_float(&param_edit.RecWarnBatVolt, direction, 30, 300);
            break;
        case 3U:
            value = param_edit.batVoltAdjust;
            if(direction < 0)
            {
                value = (value <= 500U) ? 500U :
                        (uint16_t)((value < 510U) ? 500U : value - 10U);
            }
            else
            {
                value = (value >= 1500U) ? 1500U :
                        (uint16_t)((value > 1490U) ? 1500U : value + 10U);
            }
            param_edit.batVoltAdjust = value;
            break;
        case 4U:
            param_edit.throttlePreference =
                (uint8_t)!param_edit.throttlePreference;
            break;
        case 5U:
            if(direction < 0)
            {
                param_edit.modelType = (param_edit.modelType == 0U) ?
                                       2U : (param_edit.modelType - 1U);
            }
            else
            {
                param_edit.modelType = (uint8_t)((param_edit.modelType + 1U) % 3U);
            }
            break;
        case 6U:
            if(direction < 0)
            {
                if(param_edit.PWMadjustUnit > 1U)
                {
                    param_edit.PWMadjustUnit--;
                }
            }
            else if(param_edit.PWMadjustUnit < 100U)
            {
                param_edit.PWMadjustUnit++;
            }
            break;
        case 7U:
            param_edit.keySound = (uint8_t)!param_edit.keySound;
            break;
        case 8U:
            param_edit.onImage = (uint8_t)!param_edit.onImage;
            break;
        case 9U:
            param_edit.clockMode = (uint8_t)!param_edit.clockMode;
            break;
        case 10U:
            if(direction < 0)
            {
                if(param_edit.clockTime > 1U)
                {
                    param_edit.clockTime--;
                }
            }
            else if(param_edit.clockTime < 255U)
            {
                param_edit.clockTime++;
            }
            break;
        case 11U:
            param_edit.clockCheck = (uint8_t)!param_edit.clockCheck;
            break;
        case 12U:
            if(direction < 0)
            {
                if(param_edit.throttleProtect > 0U)
                {
                    param_edit.throttleProtect--;
                }
            }
            else if(param_edit.throttleProtect < 100U)
            {
                param_edit.throttleProtect++;
            }
            break;
        case 13U:
            param_edit.PPM_Out = (uint8_t)!param_edit.PPM_Out;
            break;
        case 14U:
            param_edit.NRF_Mode = (uint8_t)!param_edit.NRF_Mode;
            break;
        case 15U:
            if(direction < 0)
            {
                if(param_edit.NRF_Channel > 0U)
                {
                    param_edit.NRF_Channel--;
                }
            }
            else if(param_edit.NRF_Channel < 125U)
            {
                param_edit.NRF_Channel++;
            }
            break;
        case 16U:
            power_index = menu_nrf_power_index(param_edit.NRF_Power);
            if(direction < 0)
            {
                power_index = (power_index == 0U) ? 3U : (power_index - 1U);
            }
            else
            {
                power_index = (uint8_t)((power_index + 1U) % 4U);
            }
            param_edit.NRF_Power = nrf_power_register[power_index];
            break;
        case 17U:
            if(direction < 0)
            {
                param_edit.NRF_DataRate = (param_edit.NRF_DataRate == 0U) ?
                                          2U : (param_edit.NRF_DataRate - 1U);
            }
            else
            {
                param_edit.NRF_DataRate =
                    (uint8_t)((param_edit.NRF_DataRate + 1U) % 3U);
            }
            break;
        default:
            if((param_selected_item >= PARAM_CHANNEL_START) &&
               (param_selected_item < PARAM_ACTION_START))
            {
                channel = (uint8_t)((param_selected_item - PARAM_CHANNEL_START) /
                                    PARAM_CHANNEL_FIELDS);
                field = (uint8_t)((param_selected_item - PARAM_CHANNEL_START) %
                                  PARAM_CHANNEL_FIELDS);
                if(field == 0U)
                {
                    value = param_edit.chLower[channel];
                    if(direction < 0)
                    {
                        value = (value < 10U) ? 0U : (uint16_t)(value - 10U);
                    }
                    else
                    {
                        value = (uint16_t)(value + 10U);
                        if(value > param_edit.chMiddle[channel])
                        {
                            value = param_edit.chMiddle[channel];
                        }
                    }
                    param_edit.chLower[channel] = value;
                }
                else if(field == 1U)
                {
                    value = param_edit.chMiddle[channel];
                    if(direction < 0)
                    {
                        value = (value < 10U) ? 0U : (uint16_t)(value - 10U);
                        if(value < param_edit.chLower[channel])
                        {
                            value = param_edit.chLower[channel];
                        }
                    }
                    else
                    {
                        value = (value > 4085U) ? 4095U : (uint16_t)(value + 10U);
                        if(value > param_edit.chUpper[channel])
                        {
                            value = param_edit.chUpper[channel];
                        }
                    }
                    param_edit.chMiddle[channel] = value;
                }
                else if(field == 2U)
                {
                    value = param_edit.chUpper[channel];
                    if(direction < 0)
                    {
                        value = (value < 10U) ? 0U : (uint16_t)(value - 10U);
                        if(value < param_edit.chMiddle[channel])
                        {
                            value = param_edit.chMiddle[channel];
                        }
                    }
                    else
                    {
                        value = (value > 4085U) ? 4095U : (uint16_t)(value + 10U);
                    }
                    param_edit.chUpper[channel] = value;
                }
                else if(field == 3U)
                {
                    trim = param_edit.PWMadjustValue[channel] + direction;
                    if(trim < -1000)
                    {
                        trim = -1000;
                    }
                    else if(trim > 1000)
                    {
                        trim = 1000;
                    }
                    param_edit.PWMadjustValue[channel] = trim;
                }
                else
                {
                    param_edit.chReverse[channel] =
                        (uint8_t)!param_edit.chReverse[channel];
                }
            }
            break;
    }

    param_dirty = 1U;
    param_revision++;
    menu_param_set_status("Unsaved change; select SAVE when finished");
}

static void menu_handle_param_key(MenuKey key)
{
    param_Config runtime_backup;

    if(param_editing != 0U)
    {
        if(key == MENU_KEY_LEFT)
        {
            menu_param_adjust(-1);
        }
        else if(key == MENU_KEY_RIGHT)
        {
            menu_param_adjust(1);
        }
        else if(key == MENU_KEY_OK)
        {
            param_editing = 0U;
            param_revision++;
            menu_param_set_status("Edit confirmed; select SAVE to store it");
        }
        else if(key == MENU_KEY_BACK)
        {
            memcpy(&param_edit, &param_edit_backup, sizeof(param_edit));
            param_dirty = param_dirty_before_edit;
            param_editing = 0U;
            param_revision++;
            menu_param_set_status("Edit cancelled");
        }
        page_dirty = 1U;
        return;
    }

    switch(key)
    {
        case MENU_KEY_LEFT:
            param_selected_item = (param_selected_item == 0U) ?
                                  (PARAM_ITEM_COUNT - 1U) :
                                  (param_selected_item - 1U);
            menu_param_adjust_window();
            page_dirty = 1U;
            break;
        case MENU_KEY_RIGHT:
            param_selected_item =
                (uint8_t)((param_selected_item + 1U) % PARAM_ITEM_COUNT);
            menu_param_adjust_window();
            page_dirty = 1U;
            break;
        case MENU_KEY_OK:
            if(param_selected_item == 0U)
            {
                menu_param_set_status("Firmware information is read-only");
                param_revision++;
            }
            else if(param_selected_item < PARAM_ACTION_START)
            {
                memcpy(&param_edit_backup, &param_edit, sizeof(param_edit));
                param_dirty_before_edit = param_dirty;
                param_editing = 1U;
                param_revision++;
                menu_param_set_status("Editing: LEFT/RIGHT change, OK confirm");
            }
            else if(param_selected_item == PARAM_ACTION_START)
            {
                param_edit.writeFlag = FM_FLAG;
                param_edit.version = FM_VERSION;
                param_edit.version_time = FM_TIME;
                memcpy(&runtime_backup, (const void *)&param,
                       sizeof(runtime_backup));
                memcpy((void *)&param, &param_edit, sizeof(param_edit));
                if(write_param() == 0U)
                {
                    nrf24l01_apply_settings(param.NRF_Mode, param.NRF_Channel,
                                            param.NRF_Power, param.NRF_DataRate);
                    param_dirty = 0U;
                    menu_param_set_status("Saved and verified in SPI Flash");
                }
                else
                {
                    memcpy((void *)&param, &runtime_backup,
                           sizeof(runtime_backup));
                    param_dirty = 1U;
                    menu_param_set_status("SAVE FAILED: SPI Flash verify error");
                }
                param_revision++;
            }
            else if(param_selected_item == (PARAM_ACTION_START + 1U))
            {
                menu_param_copy_from_runtime();
                param_dirty = 0U;
                param_revision++;
                menu_param_set_status("Current runtime values reloaded");
            }
            else
            {
                param_load_defaults(&param_edit);
                param_dirty = 1U;
                param_revision++;
                menu_param_set_status("Defaults loaded; select SAVE to store them");
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
    browser_entries[0].is_directory = 1U;
    browser_item_count = 1U;
    browser_selected_item = 0U;
    browser_first_visible = 0U;
    browser_virtual_root = 1U;
    browser_revision++;
    menu_browser_set_status("Press OK to browse the SD card");
}

static uint8_t menu_browser_load_directory(void)
{
    DIR directory;
    FILINFO file_info;
    FRESULT result;
    FRESULT space_result;
    FATFS *volume_fs = NULL;
    DWORD free_clusters = 0UL;
    DWORD free_kb = 0UL;
    /* CP936 may need two bytes for each FatFs LFN character. */
    static char long_name[_MAX_LFN * 2U + 1U];
    const char *source_name;
    uint8_t long_name_renderable;

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

        long_name_renderable = gui_file_name_can_render(long_name);
        source_name = file_info.fname;
        if((long_name[0] != '\0') &&
           (long_name_renderable != 0U))
        {
            source_name = long_name;
        }
        else if(long_name[0] != '\0')
        {
            printf("[FILE] LFN is outside the installed GB2312 font; "
                   "using SFN for display/open\r\n");
        }
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
    space_result = f_getfree(browser_path, &free_clusters, &volume_fs);
    if((space_result == FR_OK) && (volume_fs != NULL))
    {
        free_kb = free_clusters * volume_fs->csize * volume_fs->ssize / 1024UL;
        printf("[FILE] volume=%c: free_clusters=%lu free_kb=%lu\r\n",
               browser_path[0], (unsigned long)free_clusters,
               (unsigned long)free_kb);
    }
    else
    {
        printf("[FILE] f_getfree(%c:) failed: %u\r\n",
               browser_path[0], (uint16_t)space_result);
    }
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
        strcpy(browser_path, "0:");
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
        sprintf(browser_status, "Selected file | %lu bytes",
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
            else if(current_page == MENU_PAGE_PARAMETER_SETTINGS)
            {
                menu_param_load();
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

    if(current_page == MENU_PAGE_PARAMETER_SETTINGS)
    {
        menu_handle_param_key(key);
        return;
    }

    if(current_page == MENU_PAGE_DRAW_BOARD && key == MENU_KEY_OK)
    {
        GTP_CalibrationStart();
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

        case MENU_PAGE_DIGITAL_CHANNELS:
        {
            uint8_t raw_values[DIGITAL_CHANNEL_COUNT];
            uint8_t stable_values[DIGITAL_CHANNEL_COUNT];
            digital_channel_get_snapshot(raw_values, stable_values);
            digital_channel_monitor_page(raw_values, stable_values);
            break;
        }

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

        case MENU_PAGE_PARAMETER_SETTINGS:
        {
            uint8_t row;
            uint8_t visible_count =
                (uint8_t)(PARAM_ITEM_COUNT - param_first_visible);
            if(visible_count > PARAM_VISIBLE_ROWS)
            {
                visible_count = PARAM_VISIBLE_ROWS;
            }
            for(row = 0U; row < visible_count; row++)
            {
                menu_param_format_item((uint8_t)(param_first_visible + row),
                                       &param_rows[row]);
            }
            parameter_settings_page(param_rows, visible_count,
                                    (uint8_t)(param_selected_item -
                                              param_first_visible),
                                    param_first_visible, PARAM_ITEM_COUNT,
                                    param_editing, param_dirty, param_revision,
                                    param_status);
            break;
        }

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
    else if(current_page == MENU_PAGE_DIGITAL_CHANNELS)
    {
        uint8_t raw_values[DIGITAL_CHANNEL_COUNT];
        uint8_t stable_values[DIGITAL_CHANNEL_COUNT];
        digital_channel_get_snapshot(raw_values, stable_values);
        digital_channel_monitor_page(raw_values, stable_values);
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
    clock_refresh_tick_count = 0U;
    clock_refresh_due = 1U;
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

    if(++clock_refresh_tick_count >= CLOCK_REFRESH_TICKS)
    {
        clock_refresh_tick_count = 0U;
        clock_refresh_due = 1U;
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
        gui_clock_overlay();
        clock_refresh_due = 0U;
    }
    else if(refresh_due)
    {
        refresh_due = 0;
        menu_refresh_dynamic_page();
    }

    if(clock_refresh_due != 0U)
    {
        clock_refresh_due = 0U;
        gui_clock_overlay();
    }
}
