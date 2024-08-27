#ifndef __BSP_GENERAL_TIM3_H
#define __BSP_GENERAL_TIM3_H
#include "stm32f4xx.h"

#define GENERAL_TIM3           		    TIM3
#define GENERAL_TIM3_CLK       		    RCC_APB1Periph_TIM3

#define GENERAL_TIM3_IRQn		        TIM3_IRQn
#define GENERAL_TIM3_IRQHandler         TIM3_IRQHandler


void GENERAL_TIM3_InitConfiguration(uint16_t arr,uint16_t psc);

#endif

