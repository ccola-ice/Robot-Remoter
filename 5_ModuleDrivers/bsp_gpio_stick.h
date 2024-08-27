#ifndef __BSP_STICK_H
#define	__BSP_STICK_H
#include "stm32f4xx.h"

//STICK_1
#define STICK1_PIN                    GPIO_Pin_2                 
#define STICK1_GPIO_PORT              GPIOE                      
#define STICK1_GPIO_CLK               RCC_AHB1Periph_GPIOE
#define STICK1_GPIO_IRQn              EXTI2_IRQn
#define STICK1_EXTI_PortSource        EXTI_PortSourceGPIOE
#define STICK1_EXTI_PinSource         EXTI_PinSource2
#define STICK1_EXTI_LINE              EXTI_Line2

//STICK_2
#define STICK2_PIN                  GPIO_Pin_3                 
#define STICK2_GPIO_PORT            GPIOE                      
#define STICK2_GPIO_CLK             RCC_AHB1Periph_GPIOE
#define STICK2_GPIO_IRQn            EXTI3_IRQn
#define STICK2_EXTI_PortSource      EXTI_PortSourceGPIOE
#define STICK2_EXTI_PinSource       EXTI_PinSource3
#define STICK2_EXTI_LINE            EXTI_Line3

//STICK_3
#define STICK3_PIN                   GPIO_Pin_3                 
#define STICK3_GPIO_PORT             GPIOD                      
#define STICK3_GPIO_CLK              RCC_AHB1Periph_GPIOD
#define STICK3_GPIO_IRQn             EXTI3_IRQn
#define STICK3_EXTI_PortSource       EXTI_PortSourceGPIOD
#define STICK3_EXTI_PinSource        EXTI_PinSource3
#define STICK3_EXTI_LINE             EXTI_Line3
										 

#define STICK_ON	0
#define STICK_OFF	1
                              
void STICK_GPIO_Config(void);

uint8_t STICK_Scan(GPIO_TypeDef* GPIOx,uint16_t GPIO_Pin);


#endif
