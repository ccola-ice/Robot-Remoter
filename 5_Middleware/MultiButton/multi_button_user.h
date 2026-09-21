#ifndef __MULTI_BUTTON_USER_H
#define __MULTI_BUTTON_USER_H

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

void user_BUTTON_init(void);
void user_BUTTON_resume(void);
/* Cancel held navigation when a page/editor context changes. */
void user_BUTTON_cancel_repeat(void);
/* Check again when consuming a deferred repeat; a raw release wins immediately. */
uint8_t user_BUTTON_repeat_held(uint8_t key);

uint8_t read_button_ok_gpio(uint8_t button_id);
uint8_t read_button_back_gpio(uint8_t button_id);
uint8_t read_button_left_gpio(uint8_t button_id);
uint8_t read_button_right_gpio(uint8_t button_id);

void button_ok_press_down_Handler(void *btn);
void button_back_press_down_Handler(void *btn);
void button_right_press_down_Handler(void *btn);
void button_left_press_down_Handler(void *btn);

#ifdef __cplusplus
}
#endif

#endif
