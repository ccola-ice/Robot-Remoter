#ifndef __MENU_H_
#define __MENU_H_
#include "stm32f4xx.h"

typedef struct
{
    uint8_t current;
	uint8_t left;	
    uint8_t right;                                         
    uint8_t ok;                   
    uint8_t back;                     
	void (*current_operation)(void);  
}Menu_table;


void menu_button_set(void);















#endif



