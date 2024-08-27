#ifndef __BSP_BASIC_TIM_H
#define	__BSP_BASIC_TIM_H
#include "stm32f4xx.h"

#define BASIC_TIM           		TIM6
#define BASIC_TIM_CLK       		RCC_APB1Periph_TIM6

#define BASIC_TIM_IRQn		        TIM6_DAC_IRQn
#define BASIC_TIM_IRQHandler        TIM6_DAC_IRQHandler


void BASIC_TIM6_Configuration(uint16_t arr,uint16_t psc);

#endif /* __BSP_BASIC_TIM_H */

