#ifndef __MULTI_BUTTON_USER_H
#define __MULTI_BUTTON_USER_H

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

void user_BUTTON_init(void);

uint8_t read_button_ok_gpio(uint8_t button_id);
uint8_t read_button_back_gpio(uint8_t button_id);
uint8_t read_button_left_gpio(uint8_t button_id);
uint8_t read_button_right_gpio(uint8_t button_id);

void button_ok_single_click_Handler(void *btn);
void button_back_single_click_Handler(void *btn);
void button_right_single_click_Handler(void *btn);
void button_left_single_click_Handler(void *btn);

#ifdef __cplusplus
}
#endif

#endif
