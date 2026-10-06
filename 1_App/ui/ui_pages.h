#ifndef UI_PAGES_H
#define UI_PAGES_H

#include <stdint.h>
#include "boot_status.h"
#include "ui_calendar.h"

#define GUI_FILE_NAME_LENGTH 128U
#define GUI_PARAM_VISIBLE_ROWS 6U
#define GUI_PARAM_LABEL_LENGTH 24U
#define GUI_PARAM_VALUE_LENGTH 24U

/* 浏览器传入的显示条目：name 以零结尾；date/time 保留 FatFs 的打包日期时间，size 单位为字节。 */
typedef struct
{
    char name[GUI_FILE_NAME_LENGTH];
    uint32_t size;
    uint16_t date;
    uint16_t time;
    uint8_t is_directory;
} GuiFileEntry;

/* 按字库支持的编码范围检查文件名，返回 1 表示编码可显示；name 必须为有效的零结尾字符串。 */
uint8_t gui_file_name_can_render(const char *name);

/* 菜单层先将参数格式化为定长文本行，页面层只负责布局和变化比较。 */
typedef struct
{
    char label[GUI_PARAM_LABEL_LENGTH];
    char value[GUI_PARAM_VALUE_LENGTH];
} GuiParamRow;

/* 机器人遥测的显示模型，数值已转换为字段后缀所示的米、秒、度和伏等单位。
 * 它不代表 NRF 硬件 ACK，也不参与本机运动使能判定。 */
typedef struct GuiRobotTelemetry
{
    float speed_mps;
    float position_x_m;
    float position_y_m;
    float position_z_m;
    float acceleration_x_mps2;
    float acceleration_y_mps2;
    float acceleration_z_mps2;
    float roll_deg;
    float pitch_deg;
    float yaw_deg;
    float voltage_v;
    double latitude_deg;
    double longitude_deg;
    float gps_altitude_m;
    /* 接收计数与帧龄用于说明遥测时效；菜单用 65535 ms 表示未知或超出显示范围。 */
    uint32_t packet_count;
    uint16_t packet_age_ms;
    uint8_t battery_percent;
    uint8_t satellites;
    /* gps_fix 为无定位/2D/3D 状态，link_online 决定页面是否显示远端数值。 */
    uint8_t gps_fix;
    uint8_t link_online;
} GuiRobotTelemetry;

typedef struct ControlLinkSnapshot ControlLinkSnapshot;

/* 常规页面统一调用约定：切页先 prepare，再绘制页面与顶栏，最后由菜单提交 LCD。
 * 页面内部保留上一帧缓存；prepare 使首次绘制路径重新生效。 */
void gui_prepare_page(void);

/* 叠加公共顶栏，不结束本次 LCD 更新；静态页面也应周期调用以更新时钟。 */
void gui_clock_overlay(void);
/* state 包含实时日期、草稿和交互模式；绘制不会调整时间或写入 RTC。 */
void calendar_page(const GuiCalendarState *state);

/* 启动界面依次调用 begin/update/finish；update 的 item 为本次变化的检查项。 */
void gui_boot_begin(void);

void gui_boot_update(const BootReport *report, uint8_t item);

/* 显示结果并保存 report 引用，报告对象须保持有效，供后续系统信息页面读取。 */
void gui_boot_finish(const BootReport *report);

void system_basic_information(void);

/* 原始输入视图自行读取 ADC；输出视图使用同一次控制采样与已提交发送帧的快照。 */
void channel_monitor_page(void);
void channel_output_monitor_page(const ControlLinkSnapshot *snapshot);
/* selected_group 为分类索引；main_menu 的 selected_item 则为 MENU_ENTRY_* 全局入口编号。 */
void menu_group_page(uint8_t selected_group);

/* 两个数组各含六路电平，0 为有效低电平；空指针或非 0/1 值在页面中显示为未知。 */
void digital_channel_monitor_page(const uint8_t *raw_values,
                                  const uint8_t *stable_values);

void imu6050_information(void);

/* 遥测由参数传入，本机摇杆与控制状态由页面读取；空指针不绘制。 */
void robot_control_page(const GuiRobotTelemetry *telemetry);

void main_menu(uint8_t selected_item);

/* entries 包含 item_count 项，selected_item 与 first_visible 都是该数组内的索引。
 * 内容更新须改变 revision；完整目录的全局位置另由 file_browser_position 显示。 */
void file_browser_page(const char *path, const GuiFileEntry *entries,
                       uint8_t item_count, uint8_t selected_item,
                       uint8_t first_visible, uint16_t revision,
                       const char *status_text);

void file_browser_position(uint32_t selected, uint32_t total, uint8_t retry);
/* lines 按字节步长 stride 存放多行文本，可为空以仅绘制预览框架；clear_content 决定是否清空内容区。
 * text_mode：0 为图片/普通预览，1 为文本翻页，2 为 GIF，决定页脚操作提示。 */
void file_preview_page(const char *name, const char *kind, const char *lines,
                       uint16_t stride, uint8_t line_count, const char *status,
                       const char *position, uint8_t clear_content, uint8_t text_mode);

/* rows 只含当前窗口，selected_row 为窗口内索引，first_visible 为全列表起始位置。
 * editing 表示单项正在改值，dirty 表示尚未保存；两种状态独立显示。 */
void parameter_settings_page(const GuiParamRow *rows, uint8_t visible_count,
                             uint8_t selected_row, uint8_t first_visible,
                             uint8_t total_items, uint8_t editing,
                             uint8_t dirty, uint16_t revision,
                             const char *status_text);

/* rows 固定提供五项；selected 为行号，editing/dirty 由系统设置草稿状态计算。 */
void system_settings_page(const GuiParamRow *rows, uint8_t selected,
                          uint8_t editing, uint8_t dirty, const char *status);

/* 前半组值为编辑草稿，runtime_* 为模块回读结果；runtime_valid 为 0 时不能展示旧回读为有效。 */
void nrf_settings_page(uint8_t selected_item, uint8_t editing,
                       uint8_t enabled, uint8_t channel,
                       uint8_t power_index, uint8_t data_rate,
                       const char *status_text, uint8_t runtime_valid,
                       uint8_t runtime_enabled, uint8_t runtime_channel,
                       uint8_t runtime_power_index, uint8_t runtime_data_rate);

/* 历史名称保留：当前接口仅展示 GPS 服务快照，不负责校时或修改系统时间。 */
void system_data_read_and_set(void);


#endif



