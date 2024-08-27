#ifndef __BSP_BASIC_TIM6_H
#define	__BSP_BASIC_TIM6_H
#include "stm32f4xx.h"

#define BASIC_TIM7           		    TIM7
#define BASIC_TIM7_CLK       		    RCC_APB1Periph_TIM7

#define BASIC_TIM7_IRQn		            TIM7_IRQn
#define BASIC_TIM7_IRQHandler           TIM7_IRQHandler


void BASIC_TIM7_InitConfiguration(uint16_t arr,uint16_t psc);

#endif

