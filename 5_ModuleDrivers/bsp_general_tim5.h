#ifndef __BSP_GENERAL_TIM5_H
#define __BSP_GENERAL_TIM5_H
#include "stm32f4xx.h"

#define GENERAL_TIM5           		    TIM5
#define GENERAL_TIM5_CLK       		    RCC_APB1Periph_TIM5

#define GENERAL_TIM5_IRQn		        TIM5_IRQn
#define GENERAL_TIM5_IRQHandler         TIM5_IRQHandler


void GENERAL_TIM5_InitConfiguration(uint16_t arr,uint16_t psc);


#endif
