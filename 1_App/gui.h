#ifndef __GUI_H_
#define __GUI_H_

#include <stdint.h>

void gui_prepare_page(void);

void system_basic_information(void);

void mpu6050_euler_information(void);

void main_menu(uint8_t selected_item);

void nrf_settings_page(uint8_t selected_item, uint8_t editing,
                       uint8_t enabled, uint8_t channel,
                       uint8_t power_index, uint8_t data_rate,
                       const char *status_text, uint8_t runtime_valid,
                       uint8_t runtime_enabled, uint8_t runtime_channel,
                       uint8_t runtime_power_index, uint8_t runtime_data_rate);

void system_data_read_and_set(void);

void Draw_Board(void);

#endif



