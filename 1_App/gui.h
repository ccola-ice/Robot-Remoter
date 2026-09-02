#ifndef __GUI_H_
#define __GUI_H_

#include <stdint.h>

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

typedef struct
{
    char label[GUI_PARAM_LABEL_LENGTH];
    char value[GUI_PARAM_VALUE_LENGTH];
} GuiParamRow;

void gui_prepare_page(void);

void gui_clock_overlay(void);

void gui_boot_begin(void);

void gui_boot_update(uint8_t percent, uint8_t stage,
                     const char *status_text, uint8_t warning);

void gui_boot_finish(void);

void system_basic_information(void);

void channel_monitor_page(void);

void digital_channel_monitor_page(const uint8_t *raw_values,
                                  const uint8_t *stable_values);

void imu6050_information(void);

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

void Draw_Board(void);

#endif



