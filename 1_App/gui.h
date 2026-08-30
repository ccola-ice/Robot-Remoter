#ifndef __GUI_H_
#define __GUI_H_

#include <stdint.h>

void gui_prepare_page(void);

void system_basic_information(void);

void mpu6050_euler_information(void);

void main_menu(uint8_t selected_item);

void system_data_read_and_set(void);

void uGUI(void);

void Draw_Board(void);

#endif



