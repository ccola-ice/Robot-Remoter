#ifndef __BSP_SYSTICK_H
#define __BSP_SYSTICK_H

#include "stm32f4xx.h"

void SysTick_Init(void);
void Delay_us(__IO uint32_t nTime);
int get_tick_count(unsigned long *count);
void TimeStamp_Increment(void);

#define Delay_ms(x) Delay_us(1000*x)	 //µ¥Î»ms

#endif /* __BSP_SYSTICK_H */

