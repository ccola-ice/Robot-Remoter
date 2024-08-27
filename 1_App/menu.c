#include "menu.h"
#include "gui.h"
#include "bsp_gpio_button.h"
#include "bsp_usart_debug.h"
#include "bsp_fsmc_lcd.h"

void (*current_operation_index)(); //执行当前操作函数
uint8_t func_index = 0;

uint8_t Get_button_left 	= 0;
uint8_t Get_button_right 	= 0;
uint8_t Get_button_ok 		= 0;
uint8_t Get_button_back 	= 0;

extern uint8_t display_flag;		

//{current left right ok back}
Menu_table table[100] =
{
        {0, 0,0,1,4,  (*system_basic_information)},  //主界面
        
        {1, 1,1,2,0,  (*mpu6050_euler_information)},  //
        {2, 2,2,3,1,  (*main_menu)},  //
				
		{3, 3,3,4,2,  (*system_data_read_and_set)},
		
		{4, 4,4,0,3,  (*Draw_Board)},		
};

void menu_button_set(void)
{
    Get_button_left 	=   BUTTON_Scan(BUTTON_LEFT_GPIO_PORT,BUTTON_LEFT_PIN);
    Get_button_right    =   BUTTON_Scan(BUTTON_RIGHT_GPIO_PORT,BUTTON_RIGHT_PIN);
    Get_button_ok 		=   BUTTON_Scan(BUTTON_OK_GPIO_PORT,BUTTON_OK_PIN);
    Get_button_back 	=   BUTTON_Scan(BUTTON_BACK_GPIO_PORT,BUTTON_BACK_PIN);
		
	if (Get_button_left == 0)
    {
		display_flag = 1;
		func_index = table[func_index].left;
    }
	if (Get_button_right == 0)
    {
		display_flag = 1;
		func_index = table[func_index].right;
    }
	if (Get_button_ok == 0)
    {
		display_flag = 1;
		func_index = table[func_index].ok;
    }
	if (Get_button_back == 0)
    {
		display_flag = 1;
        func_index = table[func_index].back;
    }

    current_operation_index = table[func_index].current_operation;
    (*current_operation_index)(); //执行当前操作函数
	
}

