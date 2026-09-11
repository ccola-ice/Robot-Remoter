#ifndef __GUI_H_
#define __GUI_H_

#include <stdint.h>
#include "boot_status.h"

#define GUI_FILE_NAME_LENGTH 128U
#define GUI_PARAM_VISIBLE_ROWS 6U
#define GUI_PARAM_LABEL_LENGTH 24U
#define GUI_PARAM_VALUE_LENGTH 24U

typedef struct
{
    char name[GUI_FILE_NAME_LENGTH];
    uint32_t size;
    uint16_t date;
    uint16_t time;
    uint8_t is_directory;
} GuiFileEntry;

uint8_t gui_file_name_can_render(const char *name);

typedef struct
{
    char label[GUI_PARAM_LABEL_LENGTH];
    char value[GUI_PARAM_VALUE_LENGTH];
} GuiParamRow;

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
    uint32_t packet_count;
    uint16_t packet_age_ms;
    uint8_t battery_percent;
    uint8_t satellites;
    uint8_t gps_fix;
    uint8_t link_online;
} GuiRobotTelemetry;

void gui_prepare_page(void);

void gui_clock_overlay(void);

void gui_boot_begin(void);

void gui_boot_update(const BootReport *report, uint8_t item);

void gui_boot_finish(const BootReport *report);

void system_basic_information(void);

void channel_monitor_page(void);

void digital_channel_monitor_page(const uint8_t *raw_values,
                                  const uint8_t *stable_values);

void imu6050_information(void);

void robot_control_page(const GuiRobotTelemetry *telemetry);

void main_menu(uint8_t selected_item);

void file_browser_page(const char *path, const GuiFileEntry *entries,
                       uint8_t item_count, uint8_t selected_item,
                       uint8_t first_visible, uint16_t revision,
                       const char *status_text);

void parameter_settings_page(const GuiParamRow *rows, uint8_t visible_count,
                             uint8_t selected_row, uint8_t first_visible,
                             uint8_t total_items, uint8_t editing,
                             uint8_t dirty, uint16_t revision,
                             const char *status_text);

void nrf_settings_page(uint8_t selected_item, uint8_t editing,
                       uint8_t enabled, uint8_t channel,
                       uint8_t power_index, uint8_t data_rate,
                       const char *status_text, uint8_t runtime_valid,
                       uint8_t runtime_enabled, uint8_t runtime_channel,
                       uint8_t runtime_power_index, uint8_t runtime_data_rate);

void system_data_read_and_set(void);


#endif



