#ifndef __BSP_GENERAL_TIM2_H
#define	__BSP_GENERAL_TIM2_H
#include "stm32f4xx.h"

#define GENERAL_TIM2           		    TIM2
#define GENERAL_TIM2_CLK       		    RCC_APB1Periph_TIM2

#define GENERAL_TIM2_IRQn		        TIM2_IRQn
#define GENERAL_TIM2_IRQHandler         TIM2_IRQHandler


void GENERAL_TIM2_InitConfiguration(uint16_t arr,uint16_t psc);

#endif

