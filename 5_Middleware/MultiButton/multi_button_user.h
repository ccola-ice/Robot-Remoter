#ifndef __MULTI_BUTTON_USER_H
#define __MULTI_BUTTON_USER_H
#include "stdint.h"
#include "string.h"

#ifdef __cplusplus
extern "C" {
#endif
/*user_add*/
void user_BUTTON_init(void);

uint8_t read_button_ok_gpio(void);
uint8_t read_button_back_gpio(void);
uint8_t read_button_left_gpio(void);
uint8_t read_button_right_gpio(void);

void button_ok_press_down_Handler(void *btn);
void button_ok_press_up_Handler(void *btn);
void button_ok_single_click_Handler(void *btn);
void button_ok_long_press_start_Handler(void *btn);
	
void button_back_press_down_Handler(void *btn);
void button_back_press_up_Handler(void *btn);
void button_back_single_click_Handler(void *btn);
void button_back_long_press_start_Handler(void *btn);

void button_right_press_down_Handler(void *btn);
void button_right_press_up_Handler(void *btn);
void button_right_single_click_Handler(void *btn);
void button_right_long_press_start_Handler(void *btn);

void button_left_press_down_Handler(void *btn);
void button_left_press_up_Handler(void *btn);
void button_left_single_click_Handler(void *btn);
void button_left_long_press_start_Handler(void *btn);


/*user_add*/

#ifdef __cplusplus
}
#endif
		 
#endif
