#ifndef __MPU_EXTI_H
#define	__MPU_EXTI_H
#include "stm32f4xx.h"


#define MPU_INT_GPIO_PORT                GPIOB
#define MPU_INT_GPIO_CLK                 RCC_AHB1Periph_GPIOB
#define MPU_INT_GPIO_PIN                 GPIO_Pin_9
#define MPU_INT_EXTI_PORTSOURCE          EXTI_PortSourceGPIOB
#define MPU_INT_EXTI_PINSOURCE           EXTI_PinSource9
#define MPU_INT_EXTI_LINE                EXTI_Line9
#define MPU_INT_EXTI_IRQ                 EXTI9_5_IRQn

#define MPU_IRQHandler                   EXTI9_5_IRQHandler

void EXTI_MPU_Config(void);

#endif /* __EXTI_H */
