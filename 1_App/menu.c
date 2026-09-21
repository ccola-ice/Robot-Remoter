#include "diagnostics.h"
#include "bsp_fsmc_lcd.h"
#include "menu.h"
#include "menu_catalog.h"
#include "control_link.h"
#include "multi_button_user.h"
#include "gui.h"
#include "param.h"
#include "platform_nrf.h"
#include "bsp_gpio_digital_channel.h"
#include "ff.h"
#include "bsp_SysTick.h"
#include "bsp_rtc.h"
#include "nmea_decode_test.h"
#include "nmea/nmea.h"

extern nmeaTIME beiJingTime;

#include <stdio.h>
#include <string.h>

#define MENU_ITEM_COUNT       MENU_ENTRY_COUNT
#define MENU_EVENT_QUEUE_SIZE 8U
#define MENU_REFRESH_TICKS    5U
#define CLOCK_REFRESH_TICKS   20U
#define NRF_MENU_ITEM_COUNT   6U
#define NRF_SETTING_COUNT     4U
#define BROWSER_MAX_ENTRIES   64U
#define BROWSER_VISIBLE_ROWS  6U
#define BROWSER_PATH_LENGTH   256U
#define PARAM_VISIBLE_ROWS    GUI_PARAM_VISIBLE_ROWS
#define PARAM_GLOBAL_COUNT    7U
#define PARAM_CALIBRATION_CHANNELS 6U
#define PARAM_CHANNEL_START   PARAM_GLOBAL_COUNT
#define PARAM_CHANNEL_FIELDS  5U
#define PARAM_ACTION_START    (PARAM_CHANNEL_START + PARAM_CALIBRATION_CHANNELS * PARAM_CHANNEL_FIELDS)
#define PARAM_ITEM_COUNT      (PARAM_ACTION_START + 3U)

typedef enum
{
    MENU_PAGE_HOME = 0,
    MENU_PAGE_CATEGORY,
    MENU_PAGE_SYSTEM_INFO,
    MENU_PAGE_MONITOR,
    MENU_PAGE_DIGITAL_CHANNELS,
    MENU_PAGE_IMU,
    MENU_PAGE_GPS,
    MENU_PAGE_NRF,
    MENU_PAGE_CALENDAR,
    MENU_PAGE_FILE_BROWSER,
    MENU_PAGE_PARAMETER_SETTINGS,
    MENU_PAGE_DIAGNOSTICS,
    MENU_PAGE_EEPROM,
    MENU_PAGE_ROBOT_CONTROL
} MenuPage;

static const MenuPage menu_items[MENU_ITEM_COUNT] =
{
#define MENU_TARGET(page, label, hint) MENU_PAGE_##page,
    MENU_ENTRY_LIST(MENU_TARGET)
#undef MENU_TARGET
};

static const uint8_t nrf_power_register[4] = {0x09U, 0x0bU, 0x0dU, 0x0fU};
static const char * const nrf_status_text[] =
{
    "\xd0\xde\xb8\xc4\xb5\xc4\xca\xc7\xb2\xdd\xb8\xe5\xa3\xac\xd3\xa6\xd3\xc3\xb2\xa2\xb1\xa3\xb4\xe6\xba\xf3\xc9\xfa\xd0\xa7",
    "\xd2\xd1\xb1\xa3\xb4\xe6\xa3\xac\xce\xde\xcf\xdf\xc4\xa3\xbf\xe9\xb6\xc1\xbb\xd8\xd2\xbb\xd6\xc2",
    "\xce\xde\xcf\xdf\xc4\xa3\xbf\xe9\xbc\xec\xb2\xe9\xcd\xa8\xb9\xfd",
    "\xce\xde\xcf\xdf\xc4\xa3\xbf\xe9\xbc\xec\xb2\xe9\xca\xa7\xb0\xdc",
    "\xd2\xd1\xb1\xa3\xb4\xe6\xa3\xac\xb5\xab\xc4\xa3\xbf\xe9\xb6\xc1\xbb\xd8\xb2\xbb\xd2\xbb\xd6\xc2",
    "\xb2\xdd\xb8\xe5\xd2\xd1\xd0\xde\xb8\xc4\xa3\xac\xc7\xeb\xd1\xa1\xd4\xf1\xd3\xa6\xd3\xc3\xb2\xa2\xb1\xa3\xb4\xe6",
    "\xb1\xa3\xb4\xe6\xd0\xa3\xd1\xe9\xca\xa7\xb0\xdc\xa3\xac\xd4\xcb\xd0\xd0\xc9\xe8\xd6\xc3\xd2\xd1\xbb\xd6\xb8\xb4"
};

static MenuKey repeat_key;
static uint8_t repeat_pending;
static GuiCalendarState calendar_state;
static MenuKey event_queue[MENU_EVENT_QUEUE_SIZE];
static uint8_t event_read_index;
static uint8_t event_write_index;
static uint8_t selected_item;
static uint8_t selected_group;
static uint8_t group_selection[MENU_GROUP_COUNT];
static uint8_t monitor_output_view;
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
static GuiRobotTelemetry robot_telemetry;

static unsigned long robot_last_receive_ms;
static uint8_t robot_telemetry_received;

void menu_robot_telemetry_update(const GuiRobotTelemetry *telemetry)
{
    if(telemetry != 0) {
        robot_telemetry = *telemetry;
        robot_telemetry_received = telemetry->link_online != 0U;
        get_tick_count(&robot_last_receive_ms);
    }
}

static void menu_robot_telemetry_reset(void)
{
    memset(&robot_telemetry, 0, sizeof(robot_telemetry));
    robot_telemetry.packet_age_ms = 65535U;
    robot_telemetry_received = 0U;
}

static void menu_robot_telemetry_service(void)
{
    unsigned long now;
    uint32_t age;
    uint8_t was_online = robot_telemetry.link_online;
    get_tick_count(&now);
    age = (uint32_t)(now - robot_last_receive_ms);
    if(!robot_telemetry_received) age = 65535U;
    robot_telemetry.packet_age_ms = age > 65535U ? 65535U : (uint16_t)age;
    if(age >= 1000U) robot_telemetry.link_online = 0U;
    if(was_online != robot_telemetry.link_online) refresh_due = 1U;
}

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
                nrf_status = (nrf_runtime_valid != 0U &&
                    nrf_runtime_enabled == nrf_enabled &&
                    nrf_runtime_channel == nrf_channel &&
                    nrf_runtime_power_index == nrf_power_index &&
                    nrf_runtime_data_rate == nrf_data_rate) ? 1U : 4U;
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
            current_page = MENU_PAGE_CATEGORY;
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
    menu_param_set_status("\xd2\xd1\xd4\xd8\xc8\xeb\xb5\xb1\xc7\xb0\xb2\xce\xca\xfd\xa3\xac\xbd\xf6\xcf\xd4\xca\xbe\xd2\xd1\xca\xb5\xcf\xd6\xcf\xee\xc4\xbf");
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

static uint8_t menu_param_supported(uint8_t item)
{
    /* The menu contains only consumed fields. Historic/unused fields stay in
     * the persistent structure for record compatibility, not in this catalog. */
    return item < PARAM_ITEM_COUNT;
}

static void menu_param_format_item(uint8_t item_index, GuiParamRow *row)
{
    static const char * const on_off_text[2] = {"\xb9\xd8\xb1\xd5", "\xbf\xaa\xc6\xf4"};
    static const char * const rate_text[3] = {"250 Kbps", "1 Mbps", "2 Mbps"};
    static const int8_t power_dbm[4] = {-18, -12, -6, 0};
    uint8_t channel, field, power_index;

    row->label[0] = '\0';
    row->value[0] = '\0';
    if(!menu_param_supported(item_index)) return;
    switch(item_index)
    {
        case 0U:
            strcpy(row->label, "\271\314\274\376\260\346\261\276");
            snprintf(row->value, sizeof(row->value), "%s", FM_VERSION);
            break;
        case 1U:
            strcpy(row->label, "\xb7\xa2\xc9\xe4\xb5\xcd\xd1\xb9\xcd\xa3\xbf\xd8");
            snprintf(row->value, sizeof(row->value), "%.1f V", param_edit.warnBatVolt);
            break;
        case 2U:
            strcpy(row->label, "\xb5\xe7\xb3\xd8\xb5\xe7\xd1\xb9\xd0\xa3\xd7\xbc");
            snprintf(row->value, sizeof(row->value), "%u", param_edit.batVoltAdjust);
            break;
        case 3U:
            strcpy(row->label, "\xce\xde\xcf\xdf\xca\xe4\xb3\xf6");
            strcpy(row->value, on_off_text[param_edit.NRF_Mode ? 1U : 0U]);
            break;
        case 4U:
            strcpy(row->label, "\xce\xde\xcf\xdf\xc6\xb5\xb5\xc0");
            snprintf(row->value, sizeof(row->value), "%u / %u MHz", param_edit.NRF_Channel,
                     (uint16_t)(2400U + param_edit.NRF_Channel));
            break;
        case 5U:
            strcpy(row->label, "\xb7\xa2\xc9\xe4\xb9\xa6\xc2\xca");
            power_index = menu_nrf_power_index(param_edit.NRF_Power);
            snprintf(row->value, sizeof(row->value), "%d dBm", power_dbm[power_index]);
            break;
        case 6U:
            strcpy(row->label, "\xce\xde\xcf\xdf\xcb\xd9\xc2\xca");
            strcpy(row->value, rate_text[param_edit.NRF_DataRate <= 2U ? param_edit.NRF_DataRate : 2U]);
            break;
        default:
            if(item_index < PARAM_ACTION_START)
            {
                channel = (uint8_t)((item_index - PARAM_CHANNEL_START) / PARAM_CHANNEL_FIELDS);
                field = (uint8_t)((item_index - PARAM_CHANNEL_START) % PARAM_CHANNEL_FIELDS);
                switch(field)
                {
                    case 0U:
                        snprintf(row->label, sizeof(row->label), "CH%u " "\xd0\xa3\xd7\xbc\xcf\xc2\xcf\xde", channel + 1U);
                        snprintf(row->value, sizeof(row->value), "%u", param_edit.chLower[channel]);
                        break;
                    case 1U:
                        snprintf(row->label, sizeof(row->label), "CH%u " "\xd0\xa3\xd7\xbc\xd6\xd0\xb5\xe3", channel + 1U);
                        snprintf(row->value, sizeof(row->value), "%u", param_edit.chMiddle[channel]);
                        break;
                    case 2U:
                        snprintf(row->label, sizeof(row->label), "CH%u " "\xd0\xa3\xd7\xbc\xc9\xcf\xcf\xde", channel + 1U);
                        snprintf(row->value, sizeof(row->value), "%u", param_edit.chUpper[channel]);
                        break;
                    case 3U:
                        snprintf(row->label, sizeof(row->label), "CH%u " "\xcd\xa8\xb5\xc0\xce\xa2\xb5\xf7", channel + 1U);
                        snprintf(row->value, sizeof(row->value), "%d", param_edit.PWMadjustValue[channel]);
                        break;
                    default:
                        snprintf(row->label, sizeof(row->label), "CH%u " "\xcd\xa8\xb5\xc0\xb7\xbd\xcf\xf2", channel + 1U);
                        strcpy(row->value, param_edit.chReverse[channel] ? "\xb7\xb4\xcf\xf2" : "\xd5\xfd\xcf\xf2");
                        break;
                }
            }
            else if(item_index == PARAM_ACTION_START)
            {
                strcpy(row->label, "\xb1\xa3\xb4\xe6\xc8\xab\xb2\xbf\xb2\xce\xca\xfd");
                strcpy(row->value, "\xb0\xb4\xc8\xb7\xb6\xa8\xd6\xb4\xd0\xd0");
            }
            else if(item_index == PARAM_ACTION_START + 1U)
            {
                strcpy(row->label, "\xd6\xd8\xd0\xc2\xd4\xd8\xc8\xeb\xb2\xce\xca\xfd");
                strcpy(row->value, "\xb0\xb4\xc8\xb7\xb6\xa8\xd6\xb4\xd0\xd0");
            }
            else
            {
                strcpy(row->label, "\xbb\xd6\xb8\xb4\xc4\xac\xc8\xcf\xb2\xce\xca\xfd");
                strcpy(row->value, "\xc8\xd4\xd0\xe8\xb1\xa3\xb4\xe6");
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
    param_Config before;
    uint8_t channel, field, power_index;
    uint16_t value, minimum, maximum;
    int trim;
    if(direction == 0 || param_selected_item == 0U || param_selected_item >= PARAM_ACTION_START) return;
    memcpy(&before, &param_edit, sizeof(before));
    switch(param_selected_item)
    {
        case 1U:
            menu_param_adjust_float(&param_edit.warnBatVolt, direction, 25, 50);
            break;
        case 2U:
            value = param_edit.batVoltAdjust;
            if(direction < 0) value = value <= 510U ? 500U : (uint16_t)(value - 10U);
            else value = value >= 1490U ? 1500U : (uint16_t)(value + 10U);
            param_edit.batVoltAdjust = value;
            break;
        case 3U:
            param_edit.NRF_Mode = (uint8_t)!param_edit.NRF_Mode;
            break;
        case 4U:
            if(direction < 0) { if(param_edit.NRF_Channel > 0U) param_edit.NRF_Channel--; }
            else if(param_edit.NRF_Channel < 125U) param_edit.NRF_Channel++;
            break;
        case 5U:
            power_index = menu_nrf_power_index(param_edit.NRF_Power);
            if(direction < 0) power_index = power_index == 0U ? 3U : power_index - 1U;
            else power_index = (uint8_t)((power_index + 1U) % 4U);
            param_edit.NRF_Power = nrf_power_register[power_index];
            break;
        case 6U:
            if(direction < 0) param_edit.NRF_DataRate = param_edit.NRF_DataRate == 0U ? 2U : param_edit.NRF_DataRate - 1U;
            else param_edit.NRF_DataRate = (uint8_t)((param_edit.NRF_DataRate + 1U) % 3U);
            break;
        default:
            channel = (uint8_t)((param_selected_item - PARAM_CHANNEL_START) / PARAM_CHANNEL_FIELDS);
            field = (uint8_t)((param_selected_item - PARAM_CHANNEL_START) % PARAM_CHANNEL_FIELDS);
            if(field < 3U)
            {
                /* Preserve the same strict ordering required by param_sanitize.
                 * Reaching an endpoint must not reset a calibration on Save. */
                if(param_edit.chLower[channel] >= param_edit.chMiddle[channel] ||
                   param_edit.chMiddle[channel] >= param_edit.chUpper[channel] ||
                   param_edit.chUpper[channel] > 4095U) {
                    menu_param_set_status("\xd0\xa3\xd7\xbc\xb7\xb6\xce\xa7\xce\xde\xd0\xa7\xa3\xac\xc7\xeb\xcf\xc8\xbb\xd6\xb8\xb4\xc4\xac\xc8\xcf\xb2\xce\xca\xfd");
                    param_revision++;
                    return;
                }
                if(field == 0U) {
                    value = param_edit.chLower[channel]; minimum = 0U;
                    maximum = param_edit.chMiddle[channel] - 1U;
                } else if(field == 1U) {
                    value = param_edit.chMiddle[channel]; minimum = param_edit.chLower[channel] + 1U;
                    maximum = param_edit.chUpper[channel] - 1U;
                } else {
                    value = param_edit.chUpper[channel]; minimum = param_edit.chMiddle[channel] + 1U;
                    maximum = 4095U;
                }
                if(direction < 0) value = value < 10U ? 0U : (uint16_t)(value - 10U);
                else value = (uint16_t)(value + 10U);
                if(value < minimum) value = minimum;
                if(value > maximum) value = maximum;
                if(field == 0U) param_edit.chLower[channel] = value;
                else if(field == 1U) param_edit.chMiddle[channel] = value;
                else param_edit.chUpper[channel] = value;
            }
            else if(field == 3U)
            {
                trim = param_edit.PWMadjustValue[channel] + direction;
                if(trim < -1000) trim = -1000;
                if(trim > 1000) trim = 1000;
                param_edit.PWMadjustValue[channel] = trim;
            }
            else param_edit.chReverse[channel] = (uint8_t)!param_edit.chReverse[channel];
            break;
    }
    param_revision++;
    if(memcmp(&before, &param_edit, sizeof(before)) == 0) {
        menu_param_set_status("\xd2\xd1\xb4\xef\xb5\xbd\xd4\xca\xd0\xed\xb7\xb6\xce\xa7\xa3\xac\xca\xfd\xd6\xb5\xce\xb4\xb8\xc4\xb1\xe4");
        return;
    }
    param_dirty = 1U;
    menu_param_set_status("\xb2\xce\xca\xfd\xd2\xd1\xd0\xde\xb8\xc4\xa3\xac\xcd\xea\xb3\xc9\xba\xf3\xc7\xeb\xd1\xa1\xd4\xf1\xb1\xa3\xb4\xe6\xc8\xab\xb2\xbf\xb2\xce\xca\xfd");
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
            if(memcmp(&param_edit, &param_edit_backup, sizeof(param_edit)) == 0)
                param_dirty = param_dirty_before_edit;
            param_revision++;
            menu_param_set_status(param_dirty ?
                "\xd0\xde\xb8\xc4\xd2\xd1\xc8\xb7\xc8\xcf\xa3\xac\xd1\xa1\xd4\xf1\xb1\xa3\xb4\xe6\xc8\xab\xb2\xbf\xb2\xce\xca\xfd\xba\xf3\xc9\xfa\xd0\xa7" :
                "\xb1\xe0\xbc\xad\xd2\xd1\xbd\xe1\xca\xf8\xa3\xac\xc3\xbb\xd3\xd0\xce\xb4\xb1\xa3\xb4\xe6\xd0\xde\xb8\xc4");
        }
        else if(key == MENU_KEY_BACK)
        {
            memcpy(&param_edit, &param_edit_backup, sizeof(param_edit));
            param_dirty = param_dirty_before_edit;
            param_editing = 0U;
            param_revision++;
            menu_param_set_status("\xd2\xd1\xc8\xa1\xcf\xfb\xb1\xbe\xcf\xee\xd0\xde\xb8\xc4");
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
                menu_param_set_status("\xb9\xcc\xbc\xfe\xb0\xe6\xb1\xbe\xce\xaa\xd6\xbb\xb6\xc1\xd0\xc5\xcf\xa2");
                param_revision++;
            }
            else if(!menu_param_supported(param_selected_item))
            {
                menu_param_set_status("\xb8\xc3\xb2\xce\xca\xfd\xb2\xbb\xbf\xc9\xb1\xe0\xbc\xad\xa3\xac\xb0\xb2\xc8\xab\xb1\xa3\xbb\xa4\xca\xbc\xd6\xd5\xc6\xf4\xd3\xc3");
                param_revision++;
            }
            else if(param_selected_item < PARAM_ACTION_START)
            {
                memcpy(&param_edit_backup, &param_edit, sizeof(param_edit));
                param_dirty_before_edit = param_dirty;
                param_editing = 1U;
                param_revision++;
                menu_param_set_status("\xd7\xf3\xd3\xd2\xbc\xfc\xd0\xde\xb8\xc4\xa3\xac\xc8\xb7\xb6\xa8\xbc\xfc\xc8\xb7\xc8\xcf\xa3\xac\xb7\xb5\xbb\xd8\xbc\xfc\xc8\xa1\xcf\xfb");
            }
            else if(param_selected_item == PARAM_ACTION_START)
            {
                if(param_sanitize(&param_edit)) {
                    menu_param_set_status("\xd2\xd1\xd0\xde\xb8\xb4\xce\xde\xd0\xa7\xca\xfd\xd6\xb5\xa3\xac\xc7\xeb\xbc\xec\xb2\xe9\xba\xf3\xd4\xd9\xb4\xce\xb1\xa3\xb4\xe6");
                    param_revision++;
                    break;
                }
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
                    menu_param_set_status("\xb2\xce\xca\xfd\xd2\xd1\xb1\xa3\xb4\xe6\xb2\xa2\xd0\xa3\xd1\xe9\xa3\xac\xd4\xcb\xd0\xd0\xd6\xb5\xd2\xd1\xb8\xfc\xd0\xc2");
                }
                else
                {
                    memcpy((void *)&param, &runtime_backup,
                           sizeof(runtime_backup));
                    param_dirty = 1U;
                    menu_param_set_status("\xb1\xa3\xb4\xe6\xca\xa7\xb0\xdc\xa3\xac\xd4\xcb\xd0\xd0\xb2\xce\xca\xfd\xd2\xd1\xbb\xd6\xb8\xb4\xa3\xac\xc7\xeb\xd6\xd8\xca\xd4");
                }
                param_revision++;
            }
            else if(param_selected_item == (PARAM_ACTION_START + 1U))
            {
                menu_param_copy_from_runtime();
                param_dirty = 0U;
                param_revision++;
                menu_param_set_status("\xd2\xd1\xd6\xd8\xd0\xc2\xd4\xd8\xc8\xeb\xd4\xcb\xd0\xd0\xb2\xce\xca\xfd\xa3\xac\xce\xb4\xb1\xa3\xb4\xe6\xd0\xde\xb8\xc4\xd2\xd1\xb3\xb7\xcf\xfa");
            }
            else
            {
                param_load_defaults(&param_edit);
                param_dirty = 1U;
                param_revision++;
                menu_param_set_status("\xd2\xd1\xd4\xd8\xc8\xeb\xc4\xac\xc8\xcf\xb2\xce\xca\xfd\xa3\xac\xd1\xa1\xd4\xf1\xb1\xa3\xb4\xe6\xba\xf3\xc9\xfa\xd0\xa7");
            }
            page_dirty = 1U;
            break;
        case MENU_KEY_BACK:
            current_page = MENU_PAGE_CATEGORY;
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
        current_page = MENU_PAGE_CATEGORY;
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

static void menu_calendar_refresh(void)
{
    memset(&calendar_state.now,0,sizeof(calendar_state.now));
    calendar_state.readable=RTC_ReadCalendar(&calendar_state.now)==0U;
    calendar_state.time_valid=calendar_state.readable && RTC_TimeIsValid();
    calendar_state.source=(uint8_t)RTC_TimeSource();
    calendar_state.gps_available=gps_time_is_fresh();
}

static void menu_calendar_load(void)
{
    memset(&calendar_state,0,sizeof(calendar_state));
    menu_calendar_refresh();
    if(calendar_state.readable) calendar_state.draft=calendar_state.now;
    else {
        calendar_state.draft.year=2000U; calendar_state.draft.month=1U;
        calendar_state.draft.day=1U; calendar_state.draft.weekday=6U;
    }
    calendar_state.year=calendar_state.draft.year;
    calendar_state.month=calendar_state.draft.month;
}

static void menu_draw_calendar(void)
{
    menu_calendar_refresh();
    calendar_page(&calendar_state);
}

static void menu_handle_calendar_key(MenuKey key)
{
    RtcCalendar date;
    uint8_t result;
    int year;
    if(key==MENU_KEY_BACK) {
        if(calendar_state.mode==CALENDAR_BROWSE) {
            current_page=MENU_PAGE_CATEGORY; page_changed=1U;
        } else {
            if(calendar_state.mode==CALENDAR_EDIT) calendar_state.status=CALENDAR_STATUS_CANCELED;
            calendar_state.mode=CALENDAR_BROWSE;
        }
    } else if(calendar_state.mode==CALENDAR_BROWSE) {
        if(key==MENU_KEY_LEFT || key==MENU_KEY_RIGHT)
            calendar_month_step(&calendar_state,key==MENU_KEY_RIGHT?1:-1);
        else if(key==MENU_KEY_OK) calendar_state.mode=CALENDAR_ACTIONS;
    } else if(calendar_state.mode==CALENDAR_ACTIONS) {
        if(key==MENU_KEY_LEFT)
            calendar_state.action=calendar_state.action==0U?CALENDAR_ACTION_COUNT-1U:calendar_state.action-1U;
        else if(key==MENU_KEY_RIGHT)
            calendar_state.action=(uint8_t)((calendar_state.action+1U)%CALENDAR_ACTION_COUNT);
        else if(key==MENU_KEY_OK) {
            menu_calendar_refresh();
            if(calendar_state.action==CALENDAR_TODAY) {
                if(calendar_state.readable) {
                    calendar_state.year=calendar_state.now.year;
                    calendar_state.month=calendar_state.now.month;
                }
                calendar_state.mode=CALENDAR_BROWSE;
            } else if(calendar_state.action==CALENDAR_SET_TIME) {
                if(calendar_state.readable) calendar_state.draft=calendar_state.now;
                calendar_state.field=0U; calendar_state.mode=CALENDAR_EDIT;
                calendar_state.status=CALENDAR_STATUS_NONE;
            } else {
                memset(&date,0,sizeof(date));
                year=beiJingTime.year+1900;
                if(gps_time_is_fresh() && year>=2000 && year<=2099 &&
                   beiJingTime.mon>=1 && beiJingTime.mon<=12 && beiJingTime.day>=1 && beiJingTime.day<=31 &&
                   beiJingTime.hour>=0 && beiJingTime.hour<24 && beiJingTime.min>=0 && beiJingTime.min<60 &&
                   beiJingTime.sec>=0 && beiJingTime.sec<60) {
                    date.year=(uint16_t)year; date.month=(uint8_t)beiJingTime.mon; date.day=(uint8_t)beiJingTime.day;
                    date.hour=(uint8_t)beiJingTime.hour; date.minute=(uint8_t)beiJingTime.min; date.second=(uint8_t)beiJingTime.sec;
                    if(RTC_CalendarValidate(&date)) {
                        result=RTC_SetCalendar(&date,RTC_TIME_GPS);
                        calendar_state.status=result?CALENDAR_STATUS_WRITE_FAILED:CALENDAR_STATUS_GPS_SAVED;
                        if(!result) {
                            calendar_state.year=date.year; calendar_state.month=date.month;
                            calendar_state.mode=CALENDAR_BROWSE;
                        }
                    } else calendar_state.status=CALENDAR_STATUS_GPS_WAIT;
                } else calendar_state.status=CALENDAR_STATUS_GPS_WAIT;
            }
        }
    } else if(calendar_state.mode==CALENDAR_EDIT) {
        if(key==MENU_KEY_LEFT || key==MENU_KEY_RIGHT) {
            calendar_edit_step(&calendar_state,key==MENU_KEY_RIGHT?1:-1);
            calendar_state.status=CALENDAR_STATUS_NONE;
        } else if(key==MENU_KEY_OK) {
            if(calendar_state.field<6U) calendar_state.field++;
            else {
                result=RTC_SetCalendar(&calendar_state.draft,RTC_TIME_MANUAL);
                calendar_state.status=result?CALENDAR_STATUS_WRITE_FAILED:CALENDAR_STATUS_MANUAL_SAVED;
                if(!result) {
                    calendar_state.year=calendar_state.draft.year;
                    calendar_state.month=calendar_state.draft.month;
                    calendar_state.mode=CALENDAR_BROWSE;
                }
            }
        }
    }
    page_dirty=1U;
}

static void menu_handle_home_key(MenuKey key)
{
    if(key == MENU_KEY_LEFT)
        selected_group = selected_group == 0U ? MENU_GROUP_COUNT - 1U : selected_group - 1U;
    else if(key == MENU_KEY_RIGHT)
        selected_group = (uint8_t)((selected_group + 1U) % MENU_GROUP_COUNT);
    else if(key == MENU_KEY_OK) {
        selected_item = group_selection[selected_group];
        current_page = MENU_PAGE_CATEGORY;
        page_changed = 1U;
    }
    page_dirty = 1U;
}

static void menu_handle_category_key(MenuKey key)
{
    uint8_t first = menu_group_first(selected_group);
    uint8_t count = menu_group_count(selected_group);
    switch(key)
    {
        case MENU_KEY_LEFT:
            selected_item = selected_item == first ? first + count - 1U : selected_item - 1U;
            group_selection[selected_group] = selected_item;
            page_dirty = 1;
            break;

        case MENU_KEY_RIGHT:
            selected_item = selected_item + 1U == first + count ? first : selected_item + 1U;
            group_selection[selected_group] = selected_item;
            page_dirty = 1;
            break;

        case MENU_KEY_OK:
            current_page = menu_items[selected_item];
            if(current_page == MENU_PAGE_DIAGNOSTICS || current_page == MENU_PAGE_EEPROM) {
                if(current_page == MENU_PAGE_DIAGNOSTICS) diagnostics_menu();
                else eeprom_menu();
                user_BUTTON_resume();
                LCD_SetBackColor(WHITE);
                LCD_SetTextColor(BLACK);
                /* Service screens consume raw keys; discard any pre-entry queued events. */
                event_read_index = event_write_index;
                current_page = MENU_PAGE_CATEGORY;
                page_dirty = 1U;
                page_changed = 1U;
                return;
            }
            if(current_page == MENU_PAGE_NRF)
            {
                menu_nrf_load_settings();
            }
            else if(current_page == MENU_PAGE_CALENDAR)
            {
                menu_calendar_load();
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
            current_page = MENU_PAGE_HOME;
            page_dirty = page_changed = 1U;
            break;
        default:
            break;
    }
}

static void menu_handle_page_key(MenuKey key)
{
    if(current_page == MENU_PAGE_CATEGORY) {
        menu_handle_category_key(key);
        return;
    }
    if(current_page == MENU_PAGE_MONITOR && key != MENU_KEY_BACK) {
        if(key == MENU_KEY_LEFT || key == MENU_KEY_RIGHT || key == MENU_KEY_OK) {
            monitor_output_view = (uint8_t)!monitor_output_view;
            page_dirty = page_changed = 1U;
        }
        return;
    }
    if(current_page == MENU_PAGE_NRF)
    {
        menu_handle_nrf_key(key);
        return;
    }

    if(current_page == MENU_PAGE_CALENDAR)
    {
        menu_handle_calendar_key(key);
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

    if(key == MENU_KEY_BACK)
    {
        current_page = MENU_PAGE_CATEGORY;
        page_dirty = 1;
        page_changed = 1;
    }
}

static void menu_draw_monitor(void)
{
    ControlLinkSnapshot snapshot;
    if(monitor_output_view) {
        control_link_get_snapshot(&snapshot);
        channel_output_monitor_page(&snapshot);
    } else channel_monitor_page();
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
            menu_group_page(selected_group);
            break;

        case MENU_PAGE_CATEGORY:
            main_menu(selected_item);
            break;

        case MENU_PAGE_SYSTEM_INFO:
            system_basic_information();
            break;

        case MENU_PAGE_MONITOR:
            menu_draw_monitor();
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

        case MENU_PAGE_NRF:
            nrf_settings_page(nrf_selected_item, nrf_editing, nrf_enabled,
                              nrf_channel, nrf_power_index, nrf_data_rate,
                              nrf_status_text[nrf_status], nrf_runtime_valid,
                              nrf_runtime_enabled, nrf_runtime_channel,
                              nrf_runtime_power_index, nrf_runtime_data_rate);
            break;

        case MENU_PAGE_CALENDAR:
            menu_draw_calendar();
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

        case MENU_PAGE_ROBOT_CONTROL:
            robot_control_page(&robot_telemetry);
            break;

        default:
            current_page = MENU_PAGE_HOME;
            menu_group_page(selected_group);
            break;
    }
}

static void menu_refresh_dynamic_page(void)
{
    if(current_page == MENU_PAGE_HOME)
    {
        menu_group_page(selected_group);
    }
    else if(current_page == MENU_PAGE_MONITOR)
    {
        menu_draw_monitor();
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
    else if(current_page == MENU_PAGE_CALENDAR)
    {
        menu_draw_calendar();
    }
    else if(current_page == MENU_PAGE_ROBOT_CONTROL)
    {
        robot_control_page(&robot_telemetry);
    }
}

void menu_init(void)
{
    repeat_pending = 0U;
    user_BUTTON_cancel_repeat();
    event_read_index = 0;
    event_write_index = 0;
    selected_item = 0;
    selected_group = monitor_output_view = 0U;
    group_selection[0] = menu_group_first(0U);
    group_selection[1] = menu_group_first(1U);
    group_selection[2] = menu_group_first(2U);
    refresh_tick_count = 0;
    clock_refresh_tick_count = 0U;
    clock_refresh_due = 1U;
    refresh_due = 0;
    current_page = MENU_PAGE_HOME;
    menu_robot_telemetry_reset();
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
    if(++refresh_tick_count >=
       ((current_page == MENU_PAGE_MONITOR || current_page == MENU_PAGE_ROBOT_CONTROL) ?
        2U : MENU_REFRESH_TICKS))
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

/* A repeat never queues behind a real press and expires when its key is released. */
void menu_post_repeat(MenuKey key)
{
    if(key != MENU_KEY_LEFT && key != MENU_KEY_RIGHT) return;
    if(current_page != MENU_PAGE_HOME && current_page != MENU_PAGE_CATEGORY &&
       current_page != MENU_PAGE_PARAMETER_SETTINGS && current_page != MENU_PAGE_NRF &&
       current_page != MENU_PAGE_FILE_BROWSER && current_page != MENU_PAGE_CALENDAR) return;
    if(event_read_index != event_write_index || repeat_pending) return;
    repeat_key = key;
    repeat_pending = 1U;
}

static uint32_t menu_key_context(void)
{
    return (uint32_t)current_page | ((uint32_t)param_editing << 8U) |
        ((uint32_t)nrf_editing << 9U) | ((uint32_t)calendar_state.mode << 10U) |
        ((uint32_t)calendar_state.field << 12U);
}

static void menu_dispatch_key(MenuKey key)
{
    uint32_t context = menu_key_context();
    if(current_page == MENU_PAGE_HOME) menu_handle_home_key(key);
    else {
        if(current_page == MENU_PAGE_ROBOT_CONTROL) control_link_inhibit();
        menu_handle_page_key(key);
    }
    if(menu_key_context() != context) {
        repeat_pending = 0U;
        user_BUTTON_cancel_repeat();
    }
}

void menu_process(void)
{
    MenuKey key;

    menu_robot_telemetry_service();
    if(event_read_index != event_write_index) repeat_pending = 0U;
    while(menu_get_key(&key)) menu_dispatch_key(key);
    if(repeat_pending) {
        key = repeat_key;
        repeat_pending = 0U;
        if(user_BUTTON_repeat_held((uint8_t)key)) menu_dispatch_key(key);
    }

    if(page_dirty)
    {
        page_dirty = 0;
        refresh_due = 0;
        if(!page_changed) LCD_BeginUpdate();
        menu_draw_current_page();
        gui_clock_overlay();
        LCD_EndPage();
        clock_refresh_due = 0U;
    }
    else if(refresh_due)
    {
        refresh_due = 0;
        LCD_BeginUpdate();
        menu_refresh_dynamic_page();
        if(clock_refresh_due) { clock_refresh_due = 0U; gui_clock_overlay(); }
        LCD_EndPage();
    }

    if(clock_refresh_due != 0U)
    {
        clock_refresh_due = 0U;
        LCD_BeginUpdate();
        gui_clock_overlay();
        LCD_EndPage();
    }
}

uint8_t menu_control_active(void)
{
    return current_page == MENU_PAGE_ROBOT_CONTROL;
}
