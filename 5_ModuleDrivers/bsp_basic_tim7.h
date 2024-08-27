#ifndef __BSP_GENERAL_TIM6_H
#define	__BSP_GENERAL_TIM6_H
#include "stm32f4xx.h"

#define GENERAL_TIM7           		    TIM7
#define GENERAL_TIM7_CLK       		    RCC_APB1Periph_TIM7

#define GENERAL_TIM7_IRQn		        TIM7_IRQn
#define GENERAL_TIM7_IRQHandler         TIM7_IRQHandler


void GENERAL_TIM7_InitConfiguration(uint16_t arr,uint16_t psc);

#endif

