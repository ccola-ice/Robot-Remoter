#ifndef __BSP_GENERAL_TIM4_H
#define	__BSP_GENERAL_TIM4_H
#include "stm32f4xx.h"

#define GENERAL_TIM4_GPIO_CLK                   RCC_AHB1Periph_GPIOD
#define GENERAL_TIM4_GPIO_CLK_CMD               RCC_AHB1PeriphClockCmd
#define GENERAL_TIM4_GPIO_PORT                  GPIOD
#define GENERAL_TIM4_CH1_GPIO_PIN               GPIO_Pin_12
#define GENERAL_TIM4_CH1_GPIO_PIN_SOURCE        GPIO_PinSource12
#define GENERAL_TIM4_CH2_GPIO_PIN               GPIO_Pin_13
#define GENERAL_TIM4_CH2_GPIO_PIN_SOURCE        GPIO_PinSource13
#define GENERAL_TIM4_PORT_AF                    GPIO_AF_TIM4

#define GENERAL_TIM4           		            TIM4
#define GENERAL_TIM4_CLK       		            RCC_APB1Periph_TIM4
#define GENERAL_TIM4_CLK_CMD                    RCC_APB1PeriphClockCmd

#define GENERAL_TIM4_IRQn		                TIM4_IRQn
#define GENERAL_TIM4_IRQHandler                 TIM4_IRQHandler

extern int circle_count;

void GENERAL_TIM4_InitConfiguration(uint16_t arr,uint16_t psc);

int Read_Encoder_TIM4(void);


#endif

