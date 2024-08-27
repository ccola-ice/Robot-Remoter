#include "multi_button_user.h"
#include "multi_button.h"

/*user_add_header_file*/
#include "stm32f4xx.h"
#include "bsp_usart_debug.h"
#include "bsp_gpio_button.h"
/*user_add*/

/*user_add_define*/

/*user_add_define*/

/*user_add_handle*/
struct Button button_ok;
struct Button button_back;
struct Button button_left;
struct Button button_right;
/*user_add_handle*/

/*user_add_param*/
extern uint8_t button_num;

extern uint8_t Get_button_left;
extern uint8_t Get_button_right;
extern uint8_t Get_button_ok;
extern uint8_t Get_button_back;
/*user_add_param*/

/*user_add_funtion*/
void user_BUTTON_init(void)
{
	BUTTON_GPIO_Config();

	button_init(&button_ok,   	read_button_ok_gpio,   	0, 0);
	button_init(&button_back, 	read_button_back_gpio, 	0, 1);
	button_init(&button_left,  	read_button_left_gpio,  0, 2);
	button_init(&button_right,  read_button_right_gpio,	0, 3);

	button_attach(&button_ok,     PRESS_DOWN, 		 	button_ok_press_down_Handler);
	button_attach(&button_ok,     PRESS_UP, 			button_ok_press_up_Handler);
	button_attach(&button_ok,     LONG_PRESS_START, 	button_ok_long_press_start_Handler);
	button_attach(&button_ok,     SINGLE_CLICK, 		button_ok_single_click_Handler);
								  
	button_attach(&button_back,   PRESS_DOWN, 			button_back_press_down_Handler);
	button_attach(&button_back,   PRESS_UP, 			button_back_press_up_Handler);
	button_attach(&button_back,   LONG_PRESS_START, 	button_back_long_press_start_Handler);
	button_attach(&button_back,   SINGLE_CLICK, 		button_back_single_click_Handler);
	
	button_attach(&button_left,   PRESS_DOWN, 			button_left_press_down_Handler);
	button_attach(&button_left,   PRESS_UP, 			button_left_press_up_Handler);
	button_attach(&button_left,   LONG_PRESS_START, 	button_left_long_press_start_Handler);
	button_attach(&button_left,   SINGLE_CLICK, 		button_left_single_click_Handler);
	
	button_attach(&button_right,  PRESS_DOWN, 			button_right_press_down_Handler);
	button_attach(&button_right,  PRESS_UP, 			button_right_press_up_Handler);
	button_attach(&button_right,  LONG_PRESS_START, 	button_right_long_press_start_Handler);
	button_attach(&button_right,  SINGLE_CLICK, 		button_right_single_click_Handler);
	
	button_start(&button_ok);
	button_start(&button_back);
	button_start(&button_left);
	button_start(&button_right);
}

//读取按键电平接口
uint8_t read_button_ok_gpio(void)
{
	return GPIO_ReadInputDataBit(BUTTON_OK_GPIO_PORT, BUTTON_OK_PIN);
}

uint8_t read_button_back_gpio(void)
{
	return GPIO_ReadInputDataBit(BUTTON_BACK_GPIO_PORT, BUTTON_BACK_PIN);
}

uint8_t read_button_left_gpio(void)
{
	return GPIO_ReadInputDataBit(BUTTON_LEFT_GPIO_PORT, BUTTON_LEFT_PIN);
}

uint8_t read_button_right_gpio(void)
{
	return GPIO_ReadInputDataBit(BUTTON_RIGHT_GPIO_PORT, BUTTON_RIGHT_PIN);
}

//按键对应的动作处理函数接口
//button_ok
void button_ok_press_down_Handler(void *btn)
{	
//	Get_button_ok = 1;
	printf("---> button_ok press down! <---\r\n");
}

void button_ok_press_up_Handler(void *btn)
{
	printf("---> button_ok press up! <---\r\n");
}

void button_ok_single_click_Handler(void *btn)
{
	printf("---> button_ok single click! <---\r\n");
}

void button_ok_long_press_start_Handler(void *btn)
{
	printf("---> button_ok long press! <---\r\n");
}

//button_back
void button_back_press_down_Handler(void *btn)
{
//	Get_button_back = 1;
	printf("---> button_back press down! <---\r\n");
}

void button_back_press_up_Handler(void *btn)
{
	printf("***> button_back press up! <***\r\n");
}

void button_back_single_click_Handler(void *btn)
{

	printf("---> button_back single click! <---\r\n");
}

void button_back_long_press_start_Handler(void *btn)
{
	printf("---> button_back long press! <---\r\n");
}

//button_right
void button_right_press_down_Handler(void *btn)
{
//	Get_button_right = 1;
	printf("---> button_sub press down! <---\r\n");
}

void button_right_press_up_Handler(void *btn)
{
	printf("***> button_sub press up! <***\r\n");
}

void button_right_single_click_Handler(void *btn)
{
	printf("---> button_sub single click! <---\r\n");
}

void button_right_long_press_start_Handler(void *btn)
{
	printf("---> button_sub long press! <---\r\n");
}

//button_left
void button_left_press_down_Handler(void *btn)
{
//	Get_button_left = 1;
	printf("---> button_add press down! <---\r\n");
}

void button_left_press_up_Handler(void *btn)
{
	printf("***> button_add press up! <***\r\n");
}

void button_left_single_click_Handler(void *btn)
{

	printf("---> button_add single click! <---\r\n");
}

void button_left_long_press_start_Handler(void *btn)
{
	printf("---> button_add long press! <---\r\n");
}
/*user_add_funtion*/
