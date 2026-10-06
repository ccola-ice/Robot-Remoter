#include "diag_menu.h"
#include "bsp_fsmc_lcd.h"
#include "ui_menu.h"
#include "ui_menu_catalog.h"
#include "control_link.h"
#include "multi_button_user.h"
#include "ui_pages.h"
#include "app_config.h"
#include "platform_nrf.h"
#include "bsp_gpio_digital_channel.h"
#include "ff.h"
#include "file_browser.h"
#include "image_viewer.h"
#include "image_gif.h"
#include "system_feedback.h"
#include "text_reader.h"

extern FATFS fs_sdcard;
#include "bsp_SysTick.h"
#include "bsp_rtc.h"
#include "gps_service.h"
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
#define PARAM_VISIBLE_ROWS    GUI_PARAM_VISIBLE_ROWS
/* 参数列表按“全局项、各通道五个校准字段、末尾操作项”连续编号，导航和格式化共用此布局。 */
#define PARAM_GLOBAL_COUNT    7U
#define PARAM_CALIBRATION_CHANNELS 6U
#define PARAM_CHANNEL_START   PARAM_GLOBAL_COUNT
#define PARAM_CHANNEL_FIELDS  5U
#define PARAM_ACTION_START    (PARAM_CHANNEL_START + PARAM_CALIBRATION_CHANNELS * PARAM_CHANNEL_FIELDS)
#define PARAM_ITEM_COUNT      (PARAM_ACTION_START + 3U)

/* 页面状态只记录当前所在层级；各设置页另有独立的选择、编辑和草稿状态。 */
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
    MENU_PAGE_FILE_VIEWER,
    MENU_PAGE_PARAMETER_SETTINGS,
    MENU_PAGE_SYSTEM_SETTINGS,
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

/* 实体按键使用环形队列；长按连发只保留一个待处理方向，避免快速输入积压。 */
static MenuKey repeat_key;
static uint8_t repeat_pending;
/* 日历可从分类页或系统设置进入，单独保存返回位置，不随浏览月份变化。 */
static GuiCalendarState calendar_state;
static MenuPage calendar_return_page = MENU_PAGE_CATEGORY;
/* 亮度和声音保存为页面草稿；system_backup 是进入单项编辑时的值，用于 BACK 撤销。 */
static uint8_t system_selected, system_editing, system_brightness, system_sound, system_backup;
static char system_status[96];
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
/* page_dirty 请求重绘当前内容，page_changed 额外重建整页；
 * refresh_due 只触发动态数据刷新，由主循环统一提交 LCD。 */
static uint8_t page_dirty;
static uint8_t page_changed;
static uint8_t refresh_due;
static MenuPage current_page;
/* 无线页区分用户草稿与模块回读值；回读有效性单独记录，读失败不能显示为已应用。 */
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
enum { FILE_VIEW_UNSUPPORTED, FILE_VIEW_IMAGE, FILE_VIEW_TEXT, FILE_VIEW_GIF };
/* 文件解码期间借助回调继续处理后台任务；取消标志锁存到本次文件操作结束。 */
static void (*file_background_service)(void);
static uint8_t file_operation_cancelled;
static char file_view_path[FILE_BROWSER_PATH_LENGTH];
static char file_view_name[GUI_FILE_NAME_LENGTH];
static char file_view_status[128];
static uint8_t file_view_kind, file_image_pending;
/* GIF 分离暂停状态、到期换帧请求和解码期间收到的暂停请求，避免在解码栈内切换页面。 */
static uint8_t gif_paused, gif_step_pending, gif_toggle_pending;
static uint32_t gif_due_ms;
static FileImageInfo gif_info;
static FileImageResult gif_result;
static file_text_result_t file_text_result;
/* 草稿与单项备份分离：确认单项不等于持久化，dirty 记录是否仍有未保存内容。 */
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

/* 接收方发布完整显示快照并记录接收时刻；传入空指针时保持原状态。 */
void menu_robot_telemetry_update(const GuiRobotTelemetry *telemetry)
{
    if(telemetry != 0) {
        robot_telemetry = *telemetry;
        robot_telemetry_received = telemetry->link_online != 0U;
        get_tick_count(&robot_last_receive_ms);
    }
}

/* 启动时清空遥测内容，并以最大帧龄表示尚未收到有效数据。 */
static void menu_robot_telemetry_reset(void)
{
    memset(&robot_telemetry, 0, sizeof(robot_telemetry));
    robot_telemetry.packet_age_ms = 65535U;
    robot_telemetry_received = 0U;
}

/* 在主循环中计算接收帧龄，在线状态变化时请求刷新；界面不会自行推断机器人运动结果。 */
static void menu_robot_telemetry_service(void)
{
    unsigned long now;
    uint32_t age;
    uint8_t was_online = robot_telemetry.link_online;
    get_tick_count(&now);
    age = (uint32_t)(now - robot_last_receive_ms);
    /* 未收到遥测时用最大值表示未知帧龄；帧龄达到 1 秒即离线，并请求刷新状态。 */
    if(!robot_telemetry_received) age = 65535U;
    robot_telemetry.packet_age_ms = age > 65535U ? 65535U : (uint16_t)age;
    if(age >= 1000U) robot_telemetry.link_online = 0U;
    if(was_online != robot_telemetry.link_online) refresh_due = 1U;
}

/* 只比较 RF 功率位，将寄存器值映射到四档菜单索引，忽略寄存器内其他配置位。 */
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

/* 回读模块当前配置供界面对照；失败仅撤销回读有效标志，不覆盖用户正在编辑的草稿。 */
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

/* 进入无线页时从运行配置建立草稿，修正显示索引范围，并同步一次硬件回读值。 */
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

/* 按方向修改选中的无线草稿项，枚举和频道采用首尾循环；此阶段不写 Flash 或模块。 */
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

/* 选择态负责导航和执行操作，编辑态负责改值；OK/BACK 均只结束单项编辑，应用保存另行执行。 */
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
                /* 先保存并校验参数，再下发无线模块；保存失败时恢复运行配置。 */
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

/* 参数页维护独立草稿，调整和恢复默认值都要经过保存操作才作用于运行参数。 */
static void menu_param_copy_from_runtime(void)
{
    memcpy(&param_edit, (const void *)&param, sizeof(param_edit));
    param_edit.writeFlag = FM_FLAG;
    param_edit.version = FM_VERSION;
    param_edit.version_time = FM_TIME;
}

/* 每次进入参数页重新载入运行参数并清除编辑状态，先前未保存的草稿不跨页面保留。 */
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

/* 只在选择越过可见范围时滚动，使当前项始终落在六行显示窗口内。 */
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
    /* 菜单仅列出实际使用的字段；历史字段和未使用字段继续保留在
     * 持久化结构中以兼容已有记录，不再列入菜单。 */
    return item < PARAM_ITEM_COUNT;
}

/* 将全局编号转换为标签和值；通道区按通道号与字段号拆分，操作项显示执行提示。 */
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
     * param_Config 按字节紧凑排列，并直接作为 SPI Flash 存储镜像。
     * 其中 float 成员并非四字节对齐，若直接通过 float 指针访问，
     * ARMCC 可能在未对齐地址上生成 VLDR/VSTR，进而触发 HardFault。
     * 使用 volatile 字节拷贝，确保对紧凑结构的访问始终以字节进行，
     * 再通过已对齐的局部 float 变量执行 FPU 运算。
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

/* 根据字段类型限幅或切换草稿值；达到边界且数值未变时仅更新提示，不新增未保存修改。 */
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
                /* 保持 param_sanitize 要求的严格大小关系。
                 * 编辑到端点时，也不能导致保存时将校准值重置。 */
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

/* 单项编辑保存进入时的草稿快照，BACK 只撤销本项编辑，保留此前未保存的修改。 */
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
                /* 写入失败恢复运行参数，草稿继续保留，方便用户重试保存。 */
                /* 校验若修复了草稿，先返回界面供用户查看；再次保存才提交修复后的参数。 */
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

static uint8_t menu_get_key(MenuKey *key);

void menu_set_background_service(void (*service)(void))
{
    file_background_service = service;
}

/* 文件操作期间服务后台和取消键；动画暂停请求留到当前帧结束后处理。 */
static uint8_t menu_file_service(void *context)
{
    MenuKey key;
    (void)context;
    if(file_background_service) file_background_service();
    while(menu_get_key(&key)) {
        if(key == MENU_KEY_BACK) file_operation_cancelled = 1U;
        else if(key == MENU_KEY_OK && current_page == MENU_PAGE_FILE_VIEWER &&
                file_view_kind == FILE_VIEW_GIF) gif_toggle_pending = 1U;
    }
    repeat_pending = 0U;
    return file_operation_cancelled == 0U;
}

/* 开始一次新的读取/翻页操作时清除上次取消和连发请求，避免旧事件取消新任务。 */
static void menu_file_begin(void)
{
    file_operation_cancelled = 0U;
    repeat_pending = 0U;
}

/* 进入存储浏览前关闭旧预览，注册磁盘和协作回调，再显示虚拟驱动器根目录。 */
static void menu_browser_load_drives(void)
{
    file_text_close();
    file_gif_close();
    /* 即使启动时未插卡，也注册文件系统；实际介质初始化在打开目录时完成。 */
    f_mount(&fs_sdcard, "0:", 0U);
    menu_file_begin();
    file_browser_init(menu_file_service, NULL);
    file_text_set_service(menu_file_service, NULL);
}

/* 比较最后一个点后的扩展名时忽略大小写；extension 使用带点的小写字符串，不修改原路径。 */
static uint8_t menu_file_extension_is(const char *path, const char *extension)
{
    const char *dot = strrchr(path, '.');
    uint8_t a, b;
    if(!dot) return 0U;
    do {
        a = (uint8_t)*dot++; b = (uint8_t)*extension++;
        if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if(a != b) return 0U;
    } while(a);
    return 1U;
}

/* 扩展名只决定选择哪种预览器，实际内容是否合法仍由对应解码器检查。 */
static uint8_t menu_file_kind(const char *path)
{
    static const char * const text_extensions[] = {
        ".txt", ".log", ".md", ".csv", ".ini", ".cfg", ".json",
        ".xml", ".yaml", ".yml", ".c", ".h", ".cpp", ".py"
    };
    uint8_t i;
    if(menu_file_extension_is(path, ".bmp") || menu_file_extension_is(path, ".jpg") ||
       menu_file_extension_is(path, ".jpeg") || menu_file_extension_is(path, ".jpe") ||
       menu_file_extension_is(path, ".png")) return FILE_VIEW_IMAGE;
    if(menu_file_extension_is(path, ".gif")) return FILE_VIEW_GIF;
    for(i = 0U; i < sizeof(text_extensions) / sizeof(text_extensions[0]); i++)
        if(menu_file_extension_is(path, text_extensions[i])) return FILE_VIEW_TEXT;
    return FILE_VIEW_UNSUPPORTED;
}

/* 把文本读取结果和当前字节位置转换为页面提示，同时保留结果供 OK 重试或切换编码。 */
static void menu_file_text_status(file_text_result_t result)
{
    const file_text_page_t *text = file_text_get_page();
    if(result == FILE_TEXT_IO_ERROR) {
        snprintf(file_view_status, sizeof(file_view_status), "\xb6\xc1\xc8\xa1\xca\xa7\xb0\xdc\x20\x45\x25\x75\xa3\xac\xbc\xec\xb2\xe9\x20\x53\x44\x20\xbf\xa8\xa3\xbb\x4f\x4b\x20\xd6\xd8\xca\xd4",
                 text->fatfs_error);
    } else if(result == FILE_TEXT_INVALID) {
        strcpy(file_view_status, "\xce\xc4\xbc\xfe\xb8\xf1\xca\xbd\xbb\xf2\xb1\xe0\xc2\xeb\xce\xde\xd0\xa7\xa3\xac\x4f\x4b\x20\xd6\xd8\xca\xd4");
    } else if(result == FILE_TEXT_NOT_OPEN) {
        strcpy(file_view_status, "\xce\xc4\xbc\xfe\xce\xb4\xb4\xf2\xbf\xaa\xa3\xac\x4f\x4b\x20\xd6\xd8\xca\xd4");
    } else if(result == FILE_TEXT_CANCELLED) {
        strcpy(file_view_status, "\xd2\xd1\xc8\xa1\xcf\xfb\xb6\xc1\xc8\xa1");
    } else if(!text->file_size) {
        strcpy(file_view_status, "\xbf\xd5\xce\xc4\xb5\xb5");
    } else {
        snprintf(file_view_status, sizeof(file_view_status), "\x25\x6c\x75\x20\x2f\x20\x25\x6c\x75\x20\xd7\xd6\xbd\xda\x25\x73",
            (unsigned long)text->end_offset, (unsigned long)text->file_size,
            text->has_next ? "" : "\x20\x20\xd2\xd1\xb5\xbd\xce\xc4\xb5\xb5\xc4\xa9\xce\xb2");
    }
    file_text_result = result;
}

/* 退出预览时释放文本/GIF 文件并撤销待绘制请求，返回原文件列表位置。 */
static void menu_file_return(void)
{
    file_text_close();
    file_gif_close();
    gif_step_pending = gif_paused = gif_toggle_pending = 0U;
    file_image_pending = 0U;
    current_page = MENU_PAGE_FILE_BROWSER;
    page_dirty = page_changed = 1U;
}

/* 左右移动条目，BACK 上移目录；确认普通文件后按类型进入预览，空目录允许重新读取。 */
static void menu_handle_browser_key(MenuKey key)
{
    const FileBrowserState *browser = file_browser_state();
    FileBrowserEnterResult result;
    menu_file_begin();
    if(key == MENU_KEY_LEFT || key == MENU_KEY_RIGHT) {
        file_browser_move(key == MENU_KEY_LEFT ? -1 : 1);
    } else if(key == MENU_KEY_BACK) {
        if(file_browser_back()) {
            current_page = MENU_PAGE_CATEGORY;
            page_changed = 1U;
        }
    } else if(key == MENU_KEY_OK) {
        if(!browser->virtual_root && !browser->visible_count) {
            file_browser_refresh();
            page_dirty = 1U;
            return;
        }
        if(browser->visible_count) {
            strncpy(file_view_name, browser->entries[browser->selected_row].name,
                    sizeof(file_view_name) - 1U);
            file_view_name[sizeof(file_view_name) - 1U] = '\0';
        }
        result = file_browser_enter(file_view_path, sizeof(file_view_path));
        if(result == FILE_BROWSER_ENTER_FILE) {
            /* 长文件名可能显示截断，但文件类型优先使用完整的磁盘路径别名。 */
            file_view_kind = menu_file_kind(file_view_path);
            if(file_view_kind == FILE_VIEW_UNSUPPORTED)
                file_view_kind = menu_file_kind(file_view_name);
            current_page = MENU_PAGE_FILE_VIEWER;
            file_image_pending = file_view_kind == FILE_VIEW_IMAGE || file_view_kind == FILE_VIEW_GIF;
            gif_paused = gif_step_pending = gif_toggle_pending = 0U;
            if(file_view_kind == FILE_VIEW_TEXT) {
                menu_file_text_status(file_text_open(file_view_path));
                if(file_operation_cancelled) menu_file_return();
            }
            page_changed = 1U;
        }
    }
    page_dirty = 1U;
}

/* 共用 BACK 关闭预览；OK 对图片重载、对 GIF 暂停/重播、对文本重试或轮换编码。 */
static void menu_handle_file_viewer_key(MenuKey key)
{
    const file_text_page_t *text;
    file_text_result_t result;
    if(key == MENU_KEY_BACK) {
        menu_file_return();
        return;
    }
    menu_file_begin();
    if(file_view_kind == FILE_VIEW_IMAGE && key == MENU_KEY_OK) {
        file_image_pending = 1U;
    } else if(file_view_kind == FILE_VIEW_GIF && key == MENU_KEY_OK) {
        if(!file_gif_is_open() || file_gif_finished()) {
            file_gif_close();
            file_image_pending = 1U;
            gif_paused = 0U;
        } else {
            unsigned long now;
            gif_paused = !gif_paused;
            get_tick_count(&now);
            gif_due_ms = (uint32_t)now + file_gif_delay_ms();
        }
    } else if(file_view_kind == FILE_VIEW_TEXT) {
        text = file_text_get_page();
        result = file_text_result;
        if(key == MENU_KEY_OK) {
            if(file_text_result != FILE_TEXT_OK && file_text_result != FILE_TEXT_END)
                result = file_text_open(file_view_path);
            else
                result = file_text_set_encoding((file_text_encoding_t)
                    (((unsigned)text->encoding + 1U) % (FILE_TEXT_ENCODING_UTF16_BE + 1U)));
        } else if(key == MENU_KEY_LEFT) result = file_text_previous();
        else if(key == MENU_KEY_RIGHT) result = file_text_next();
        menu_file_text_status(result);
        if(file_operation_cancelled) menu_file_return();
    }
    page_dirty = 1U;
}

/* 把文件服务的窗口、错误状态和全目录位置映射到界面，显示索引与磁盘索引分别传入。 */
static void menu_draw_browser(void)
{
    const FileBrowserState *browser = file_browser_state();
    const char *path = browser->virtual_root ? "\xb4\xe6\xb4\xa2\xc9\xe8\xb1\xb8" : browser->path;
    char status[96];
    if(browser->virtual_root) strcpy(status, "\x4f\x4b\x20\xe4\xaf\xc0\xc0\x20\x53\x44\x20\xbf\xa8\xa3\xac\xd6\xa7\xb3\xd6\xcd\xbc\xc6\xac\xba\xcd\xce\xc4\xb5\xb5");
    else if(browser->status == FILE_BROWSER_IO_ERROR)
        snprintf(status, sizeof(status), "\xb6\xc1\xc8\xa1\xca\xa7\xb0\xdc\x20\x45\x25\x75\xa3\xac\xbc\xec\xb2\xe9\x20\x53\x44\x20\xbf\xa8\xa3\xbb\x4f\x4b\x20\xd6\xd8\xca\xd4", (unsigned)browser->result);
    else if(browser->status == FILE_BROWSER_PATH_LIMIT) strcpy(status, "\xc4\xbf\xc2\xbc\xb9\xfd\xc9\xee\xbb\xf2\xc2\xb7\xbe\xb6\xb9\xfd\xb3\xa4");
    else if(browser->status == FILE_BROWSER_CANCELLED) strcpy(status, "\xd2\xd1\xc8\xa1\xcf\xfb\xb6\xc1\xc8\xa1\xa3\xac\x4f\x4b\x20\xd6\xd8\xca\xd4");
    else if(!browser->total_count) strcpy(status, "\xbf\xd5\xc4\xbf\xc2\xbc\xa3\xac\x4f\x4b\x20\xd6\xd8\xd0\xc2\xb6\xc1\xc8\xa1");
    else status[0] = '\0';
    file_browser_page(path, browser->entries, browser->visible_count, browser->selected_row,
                      0U, browser->revision, status);
    file_browser_position(browser->visible_count ? browser->first_index + browser->selected_row + 1U : 0U,
                          browser->total_count, !browser->virtual_root && !browser->visible_count);
}


/* 将解码错误归类为可读提示；预览页保留文件路径，用户可在故障排除后按 OK 重试。 */
static void menu_image_error(FileImageResult result)
{
    if(result == FILE_IMAGE_IO) strcpy(file_view_status, "\xb6\xc1\xc8\xa1\xca\xa7\xb0\xdc\xa3\xac\xc7\xeb\xbc\xec\xb2\xe9 SD \xbf\xa8\xa3\xbbOK \xd6\xd8\xca\xd4");
    else if(result == FILE_IMAGE_TOO_LARGE) strcpy(file_view_status, "\xcd\xbc\xc6\xac\xb9\xfd\xb4\xf3\xa3\xac\xc7\xeb\xcb\xf5\xd0\xa1\xba\xf3\xd6\xd8\xca\xd4");
    else if(result == FILE_IMAGE_NO_MEMORY) strcpy(file_view_status, "\xcd\xe2\xb2\xbf\xc4\xda\xb4\xe6\xb2\xbb\xbf\xc9\xd3\xc3\xa3\xac\xc7\xeb\xbc\xec\xb2\xe9\xd3\xb2\xbc\xfe\xd7\xd4\xbc\xec");
    else if(result == FILE_IMAGE_UNSUPPORTED) strcpy(file_view_status, "\xb2\xbb\xd6\xa7\xb3\xd6\xb4\xcb\xcd\xbc\xc6\xac\xb1\xe0\xc2\xeb\xa3\xac\xc7\xeb\xd7\xaa\xbb\xbb\xce\xaa\xc6\xd5\xcd\xa8 PNG / JPG / BMP");
    else strcpy(file_view_status, "\xcd\xbc\xc6\xac\xca\xfd\xbe\xdd\xcb\xf0\xbb\xb5\xbb\xf2\xce\xc4\xbc\xfe\xb2\xbb\xcd\xea\xd5\xfb\xa3\xbbOK \xd6\xd8\xca\xd4");
}

/* 每次主循环最多准备一帧，不因积压时间连赶多帧。 */
static void menu_gif_tick(void)
{
    unsigned long now;
    if(current_page != MENU_PAGE_FILE_VIEWER || file_view_kind != FILE_VIEW_GIF ||
       file_image_pending || gif_paused || !file_gif_is_open() || file_gif_finished()) return;
    get_tick_count(&now);
    if((int32_t)((uint32_t)now - gif_due_ms) >= 0) {
        gif_step_pending = 1U;
        page_dirty = 1U;
    }
}

/* 首帧打开 GIF 并取得共享解码工作区，后续只在换帧请求到达时解码；取消则返回列表。 */
static void menu_draw_gif(void)
{
    FileImageResult result = FILE_IMAGE_OK;
    uint32_t capacity;
    void *workspace;
    unsigned long now;
    uint8_t first = file_image_pending;
    if(first || gif_step_pending) {
        file_image_pending = gif_step_pending = 0U;
        menu_file_begin();
        if(first) {
            file_preview_page(file_view_name, "GIF / \xb6\xaf\xbb\xad", NULL, 0U, 0U,
                              "\xd5\xfd\xd4\xda\xb6\xc1\xc8\xa1\xb6\xaf\xbb\xad\xa3\xac\x42\x41\x43K \xbf\xc9\xc8\xa1\xcf\xfb", "", 1U, 2U);
            gui_clock_overlay();
            LCD_EndPage();
            LCD_BeginUpdate();
            workspace = file_image_get_workspace(&capacity);
            result = file_gif_open(file_view_path, 24U, 136U, 752U, 280U, 0xffffU,
                                  &gif_info, workspace, capacity, menu_file_service, NULL);
        } else result = file_gif_next();
        if(result == FILE_IMAGE_CANCELLED) {
            menu_file_return();
            gui_prepare_page();
            menu_draw_browser();
            page_dirty = page_changed = 0U;
            return;
        }
        get_tick_count(&now);
        /* 从解码完成时刻安排下一帧，慢速存储只降低播放速度，不连续追赶积压帧。 */
        gif_due_ms = (uint32_t)now + file_gif_delay_ms();
        gif_result = result;
        if(result != FILE_IMAGE_OK) menu_image_error(result);
    }
    if(gif_result == FILE_IMAGE_OK) {
        snprintf(file_view_status, sizeof(file_view_status), "%lu x %lu -> %u x %u  %s",
                 (unsigned long)gif_info.width, (unsigned long)gif_info.height,
                 gif_info.drawn_width, gif_info.drawn_height,
                 file_gif_finished() ? "\xb2\xa5\xb7\xc5\xbd\xe1\xca\xf8\xa3\xacOK \xd6\xd8\xb2\xa5" : gif_paused ? "\xd2\xd1\xd4\xdd\xcd\xa3" : "\xd5\xfd\xd4\xda\xb2\xa5\xb7\xc5");
    }
    file_preview_page(file_view_name, "GIF / \xb6\xaf\xbb\xad", NULL, 0U, 0U,
                      file_view_status, "", 0U, 2U);
}

/* 统一绘制文本、静态图片和动画预览；静态图片只在首次进入或手动重试时读取。 */
static void menu_draw_file_viewer(void)
{
    const file_text_page_t *text;
    static const char * const encodings[] = {"UTF-8", "GBK", "UTF-16 LE", "UTF-16 BE"};
    char position[40], kind[48];
    FileImageInfo image;
    FileImageResult result;
    static const char unsupported[][75] = {
        "\xb4\xcb\xb8\xf1\xca\xbd\xd4\xdd\xb2\xbb\xd6\xa7\xb3\xd6\xd4\xa4\xc0\xc0\xa1\xa3",
        "",
        "\xcd\xbc\xc6\xac\xa3\xbaJPG / JPEG / BMP / PNG / GIF",
        "\xce\xc4\xb5\xb5\xa3\xba\x54\x58\x54\x20\x2f\x20\x4c\x4f\x47\x20\x2f\x20\x4d\x44\x20\x2f\x20\x43\x53\x56\x20\x2f\x20\x49\x4e\x49\x20\x2f\x20\x4a\x53\x4f\x4e\x20\xb5\xc8\xb4\xbf\xce\xc4\xb1\xbe",
        "",
        "\x50\x44\x46\x20\x2f\x20\x57\x6f\x72\x64\x20\xc7\xeb\xcf\xc8\xd7\xaa\xbb\xbb\xb3\xc9\x20\x54\x58\x54\x20\xbb\xf2\x20\x4a\x50\x47\x20\x2f\x20\x42\x4d\x50\xa1\xa3",
        "",
        "\x42\x41\x43\x4b\x20\xb7\xb5\xbb\xd8\xce\xc4\xbc\xfe\xc1\xd0\xb1\xed\xa1\xa3"
    };
    if(file_view_kind == FILE_VIEW_GIF) {
        menu_draw_gif();
    } else if(file_view_kind == FILE_VIEW_TEXT) {
        text = file_text_get_page();
        if(text->page_number)
            snprintf(position, sizeof(position), "\xb5\xda\x20\x25\x6c\x75\x20\xd2\xb3\x25\x73", (unsigned long)text->page_number,
                     text->has_next ? "" : "\x20\x2f\x20\xc4\xa9\xd2\xb3");
        else position[0] = '\0';
        snprintf(kind, sizeof(kind), "\xce\xc4\xb5\xb5\x20\x2f\x20\x25\x73", encodings[text->encoding]);
        file_preview_page(file_view_name, kind, &text->lines[0][0], sizeof(text->lines[0]),
            text->line_count, file_view_status, position, 1U, 1U);
    } else if(file_view_kind == FILE_VIEW_IMAGE) {
        if(file_image_pending) {
            file_image_pending = 0U;
            file_preview_page(file_view_name, "\xcd\xbc\xc6\xac\x20\x2f\x20\xd7\xd4\xb6\xaf\xcb\xf5\xb7\xc5", NULL, 0U, 0U,
                "\xd5\xfd\xd4\xda\xb6\xc1\xc8\xa1\xcd\xbc\xc6\xac\xa3\xac\x42\x41\x43\x4b\x20\xbf\xc9\xc8\xa1\xcf\xfb", "", 1U, 0U);
            /* 先显示读取提示，再在保留帧中合成图片，避免直接写屏造成撕裂。 */
            gui_clock_overlay();
            LCD_EndPage();
            LCD_BeginUpdate();
            menu_file_begin();
            memset(&image, 0, sizeof(image));
            result = file_image_draw(file_view_path, 24U, 136U, 752U, 280U,
                                     &image, menu_file_service, NULL);
            if(result == FILE_IMAGE_CANCELLED) {
                menu_file_return();
                /* 丢弃尚未完成的图片，直接合成列表，取消时不提交半张图片。 */
                gui_prepare_page();
                menu_draw_browser();
                page_dirty = page_changed = 0U;
                return;
            }
            if(result == FILE_IMAGE_OK) {
                snprintf(file_view_status, sizeof(file_view_status), "\x25\x6c\x75\x20\x78\x20\x25\x6c\x75\x20\x2d\x3e\x20\x25\x75\x20\x78\x20\x25\x75\x20\xcf\xf1\xcb\xd8",
                    (unsigned long)image.width, (unsigned long)image.height,
                    image.drawn_width, image.drawn_height);
            } else menu_image_error(result);
            file_preview_page(file_view_name, "\xcd\xbc\xc6\xac\x20\x2f\x20\xd7\xd4\xb6\xaf\xcb\xf5\xb7\xc5", NULL, 0U, 0U,
                              file_view_status, "", result != FILE_IMAGE_OK, 0U);
        }
    } else {
        file_preview_page(file_view_name, "\xce\xc4\xbc\xfe\xb8\xf1\xca\xbd", &unsupported[0][0], sizeof(unsupported[0]),
                          sizeof(unsupported) / sizeof(unsupported[0]), "\xd6\xbb\xb6\xc1\xe4\xaf\xc0\xc0\xa3\xac\xce\xc4\xbc\xfe\xc4\xda\xc8\xdd\xb2\xbb\xbb\xe1\xd0\xde\xb8\xc4", "", 1U, 0U);
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

/* 只更新 RTC 实时快照和 GPS 可用性，保留浏览月份及用户正在编辑的日期草稿。 */
static void menu_calendar_refresh(void)
{
    memset(&calendar_state.now,0,sizeof(calendar_state.now));
    calendar_state.readable=RTC_ReadCalendar(&calendar_state.now)==0U;
    calendar_state.time_valid=calendar_state.readable && RTC_TimeIsValid();
    calendar_state.source=(uint8_t)RTC_TimeSource();
    calendar_state.gps_available=gps_time_is_fresh();
}

/* 进入日历时用 RTC 初始化浏览与编辑日期；读不到 RTC 时以 2000-01-01 作为可编辑起点。 */
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

/* 日历分为浏览、操作菜单和时间编辑三种状态；手动时间只在最后的保存项写入 RTC。 */
static void menu_handle_calendar_key(MenuKey key)
{
    RtcCalendar date;
    uint8_t result;
    int year;
    if(key==MENU_KEY_BACK) {
        if(calendar_state.mode==CALENDAR_BROWSE) {
            current_page=calendar_return_page; page_changed=1U;
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
                /* GPS 校时同时检查接收时效和日期范围，避免把陈旧或无效时间写入 RTC。 */
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


/* 系统设置只编辑屏幕和声音，不修改通道校准或无线参数。 */
static void menu_system_load(void)
{
    system_selected = system_editing = 0U;
    system_brightness = param.screenBrightness;
    system_sound = param.keySound;
    strcpy(system_status, "\xc1\xc1\xb6\xc8\xbc\xb4\xca\xb1\xd4\xa4\xc0\xc0\xa3\xac\xb1\xa3\xb4\xe6\xba\xf3\xbf\xaa\xbb\xfa\xc9\xfa\xd0\xa7");
}

/* 亮度修改立即预览，BACK 编辑时撤销本项、离页时恢复已保存亮度；声音草稿在保存后生效。 */
static void menu_handle_system_key(MenuKey key)
{
    uint8_t old_brightness, old_sound;
    int value;
    if(key == MENU_KEY_BACK) {
        if(system_editing) {
            if(system_selected == 0U) system_brightness = system_backup;
            else system_sound = system_backup;
            system_editing = 0U;
            LCD_SetBrightness(system_brightness);
            strcpy(system_status, "\xd2\xd1\xc8\xa1\xcf\xfb\xb1\xbe\xcf\xee\xd0\xde\xb8\xc4");
        } else {
            LCD_SetBrightness(param.screenBrightness);
            current_page = MENU_PAGE_CATEGORY;
            page_changed = 1U;
        }
    } else if(key == MENU_KEY_LEFT || key == MENU_KEY_RIGHT) {
        if(!system_editing)
            system_selected = (uint8_t)((system_selected + (key == MENU_KEY_RIGHT ? 1U : 4U)) % 5U);
        else {
            if(system_selected == 0U) {
                value = (int)system_brightness + (key == MENU_KEY_RIGHT ? 5 : -5);
                if(value < 10) value = 10;
                if(value > 100) value = 100;
                system_brightness = (uint8_t)value;
                LCD_SetBrightness(system_brightness);
            } else system_sound = !system_sound;
            strcpy(system_status, "OK \xc8\xb7\xc8\xcf\xb1\xbe\xcf\xee\xa3\xac\x42\x41\x43K \xb3\xb7\xcf\xfa\xa3\xbb\xc7\xeb\xb1\xa3\xb4\xe6\xc9\xe8\xd6\xc3");
        }
    } else if(key == MENU_KEY_OK) {
        if(system_editing) system_editing = 0U;
        else if(system_selected < 2U) {
            system_backup = system_selected == 0U ? system_brightness : system_sound;
            system_editing = 1U;
        } else if(system_selected == 2U) {
            calendar_return_page = MENU_PAGE_SYSTEM_SETTINGS;
            menu_calendar_load();
            current_page = MENU_PAGE_CALENDAR;
            page_changed = 1U;
        } else if(system_selected == 3U) {
            old_brightness = param.screenBrightness; old_sound = param.keySound;
            param.screenBrightness = system_brightness; param.keySound = system_sound;
            if(write_param() == 0U) strcpy(system_status, "\xcf\xb5\xcd\xb3\xc9\xe8\xd6\xc3\xd2\xd1\xb1\xa3\xb4\xe6");
            else {
                param.screenBrightness = old_brightness; param.keySound = old_sound;
                strcpy(system_status, "\xb1\xa3\xb4\xe6\xca\xa7\xb0\xdc\xa3\xac\xd0\xde\xb8\xc4\xc9\xd0\xce\xb4\xb1\xa3\xb4\xe6\xa3\xbbOK \xd6\xd8\xca\xd4");
            }
        } else {
            system_brightness = 100U; system_sound = 1U;
            LCD_SetBrightness(system_brightness);
            strcpy(system_status, "\xcf\xb5\xcd\xb3\xc4\xac\xc8\xcf\xd6\xb5\xd2\xd1\xd4\xd8\xc8\xeb\xa3\xac\xc7\xeb\xb1\xa3\xb4\xe6\xc9\xe8\xd6\xc3");
        }
    }
    page_dirty = 1U;
}

/* 将系统设置草稿生成五行视图，未保存状态通过草稿与运行配置比较得出。 */
static void menu_draw_system(void)
{
    GuiParamRow rows[5];
    static const char * const labels[] = {"\xc6\xc1\xc4\xbb\xc1\xc1\xb6\xc8", "\xb0\xb4\xbc\xfc\xc9\xf9\xd2\xf4", "\xc8\xd5\xc6\xda\xca\xb1\xbc\xe4", "\xb1\xa3\xb4\xe6\xc9\xe8\xd6\xc3", "\xbb\xd6\xb8\xb4\xc4\xac\xc8\xcf"};
    uint8_t row;
    memset(rows, 0, sizeof(rows));
    for(row = 0U; row < 5U; ++row) strcpy(rows[row].label, labels[row]);
    snprintf(rows[0].value, sizeof(rows[0].value), "%u %%", system_brightness);
    strcpy(rows[1].value, system_sound ? "\xbf\xaa\xc6\xf4" : "\xb9\xd8\xb1\xd5");
    strcpy(rows[2].value, "\xc8\xd5\xc0\xfa / \xd0\xa3\xca\xb1");
    strcpy(rows[3].value, "\xd0\xb4\xc8\xeb\xb4\xe6\xb4\xa2");
    strcpy(rows[4].value, "\xc6\xc1\xc4\xbb / \xc9\xf9\xd2\xf4");
    system_settings_page(rows, system_selected, system_editing,
        system_brightness != param.screenBrightness || system_sound != param.keySound, system_status);
}

/* 每个分类分别记住上次选中项，从首页重新进入时恢复原位置。 */
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

/* 分类内循环导航；进入功能前初始化其状态，独占式诊断返回后重新接管按键和页面。 */
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
                system_key_beep_stop();
                if(current_page == MENU_PAGE_DIAGNOSTICS) diagnostics_menu();
                else eeprom_menu();
                user_BUTTON_resume();
                LCD_SetBackColor(WHITE);
                LCD_SetTextColor(BLACK);
                /* 服务页面直接读取按键，返回时丢弃进入页面前排队的旧事件。 */
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
                calendar_return_page = MENU_PAGE_CATEGORY;
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
            else if(current_page == MENU_PAGE_SYSTEM_SETTINGS) menu_system_load();
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

/* 先把按键交给具有独立状态机的页面，其余只读页面统一用 BACK 返回所属分类。 */
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

    if(current_page == MENU_PAGE_SYSTEM_SETTINGS) {
        menu_handle_system_key(key);
        return;
    }

    if(current_page == MENU_PAGE_CALENDAR)
    {
        menu_handle_calendar_key(key);
        return;
    }

    if(current_page == MENU_PAGE_FILE_VIEWER)
    {
        menu_handle_file_viewer_key(key);
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

/* 输出视图读取控制任务的完整快照，原始视图直接显示 ADC；两种视图使用各自的绘制缓存。 */
static void menu_draw_monitor(void)
{
    ControlLinkSnapshot snapshot;
    if(monitor_output_view) {
        control_link_get_snapshot(&snapshot);
        channel_output_monitor_page(&snapshot);
    } else channel_monitor_page();
}

/* 切换页面时使静态布局和绘制缓存重新初始化，同页操作沿用已有画面做局部更新。 */
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

        case MENU_PAGE_SYSTEM_SETTINGS:
            menu_draw_system();
            break;

        case MENU_PAGE_FILE_BROWSER:
            menu_draw_browser();
            break;

        case MENU_PAGE_FILE_VIEWER:
            menu_draw_file_viewer();
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

/* 周期刷新只访问含实时数据的页面，设置页和文件页保持到用户操作或换帧请求才更新。 */
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

/* 建立首页、分类选择和首次重绘状态；调用前 LCD 应已完成初始化。 */
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

    /* 环形队列预留一个空位区分空与满；满时丢弃新事件，避免覆盖尚未处理的按键。 */
    if(next_index == event_read_index)
    {
        return;
    }

    system_key_beep();
    event_queue[event_write_index] = key;
    event_write_index = next_index;
}

/* 定时入口只置刷新标志：通道监视和机器人控制页每 20 ms 更新，其他动态页每 50 ms 更新。 */
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

/* 长按连发不排在实体按下事件之后；松开对应按键后立即失效。 */
void menu_post_repeat(MenuKey key)
{
    if(key != MENU_KEY_LEFT && key != MENU_KEY_RIGHT) return;
    if(current_page != MENU_PAGE_HOME && current_page != MENU_PAGE_CATEGORY &&
       current_page != MENU_PAGE_PARAMETER_SETTINGS && current_page != MENU_PAGE_NRF &&
       current_page != MENU_PAGE_SYSTEM_SETTINGS &&
       current_page != MENU_PAGE_FILE_BROWSER && current_page != MENU_PAGE_FILE_VIEWER &&
       current_page != MENU_PAGE_CALENDAR) return;
    if(event_read_index != event_write_index || repeat_pending) return;
    repeat_key = key;
    repeat_pending = 1U;
}

/* 压缩决定方向键含义的页面、编辑模式和字段位置，用于检测连发是否跨越了操作上下文。 */
static uint32_t menu_key_context(void)
{
    return (uint32_t)current_page | ((uint32_t)param_editing << 8U) |
        ((uint32_t)nrf_editing << 9U) | ((uint32_t)calendar_state.mode << 10U) |
        ((uint32_t)calendar_state.field << 12U) | ((uint32_t)system_editing << 16U);
}

/* 集中路由按键；机器人控制页上的菜单操作先撤销运动使能，再处理导航。 */
static void menu_dispatch_key(MenuKey key)
{
    uint32_t context = menu_key_context();
    if(current_page == MENU_PAGE_HOME) menu_handle_home_key(key);
    else {
        if(current_page == MENU_PAGE_ROBOT_CONTROL) control_link_inhibit();
        menu_handle_page_key(key);
    }
    /* 页面或编辑字段变化后取消旧连发，防止一次长按继续操作新页面或下一字段。 */
    if(menu_key_context() != context) {
        repeat_pending = 0U;
        user_BUTTON_cancel_repeat();
    }
}

/* 主循环入口：依次推进输入、动画和重绘；所有常规页面绘制在此统一结束并提交。 */
void menu_process(void)
{
    MenuKey key;

    menu_robot_telemetry_service();
    /* 优先消费实体按键，再检查仍被按住的连发键，避免积压的连发拖延返回操作。 */
    if(event_read_index != event_write_index) repeat_pending = 0U;
    while(menu_get_key(&key)) menu_dispatch_key(key);
    if(repeat_pending) {
        key = repeat_key;
        repeat_pending = 0U;
        if(user_BUTTON_repeat_held((uint8_t)key)) menu_dispatch_key(key);
    }
    if(gif_toggle_pending && current_page == MENU_PAGE_FILE_VIEWER && file_view_kind == FILE_VIEW_GIF) {
        gif_toggle_pending = 0U;
        menu_handle_file_viewer_key(MENU_KEY_OK);
    }
    menu_gif_tick();

    /* 显式页面变更优先于周期刷新；绘制内容和顶栏时钟后，一次提交完整更新。 */
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

    /* 静态页面也需要走独立的时钟更新路径，避免没有页面变更时顶栏停止走时。 */
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
