#ifndef __BSP_GPIO_BUTTON_H
#define	__BSP_GPIO_BUTTON_H
#include "stm32f4xx.h"

//BUTTON_OK
#define BUTTON_OK_PIN                    GPIO_Pin_15                 
#define BUTTON_OK_GPIO_PORT              GPIOG                      
#define BUTTON_OK_GPIO_CLK               RCC_AHB1Periph_GPIOG
#define BUTTON_OK_GPIO_IRQn              EXTI15_10_IRQn
#define BUTTON_OK_EXTI_PortSource        EXTI_PortSourceGPIOG
#define BUTTON_OK_EXTI_PinSource         EXTI_PinSource15
#define BUTTON_OK_EXTI_LINE              EXTI_Line15

//BUTTON_BACK
#define BUTTON_BACK_PIN                  GPIO_Pin_14                 
#define BUTTON_BACK_GPIO_PORT            GPIOG                      
#define BUTTON_BACK_GPIO_CLK             RCC_AHB1Periph_GPIOG
#define BUTTON_BACK_GPIO_IRQn            EXTI15_10_IRQn
#define BUTTON_BACK_EXTI_PortSource      EXTI_PortSourceGPIOG
#define BUTTON_BACK_EXTI_PinSource       EXTI_PinSource14
#define BUTTON_BACK_EXTI_LINE            EXTI_Line14

//BUTTON_ADD
#define BUTTON_LEFT_PIN                   GPIO_Pin_13                 
#define BUTTON_LEFT_GPIO_PORT             GPIOG                      
#define BUTTON_LEFT_GPIO_CLK              RCC_AHB1Periph_GPIOG
#define BUTTON_LEFT_GPIO_IRQn             EXTI15_10_IRQn
#define BUTTON_LEFT_EXTI_PortSource       EXTI_PortSourceGPIOG
#define BUTTON_LEFT_EXTI_PinSource        EXTI_PinSource13
#define BUTTON_LEFT_EXTI_LINE             EXTI_Line13

//BUTTON_SUB                             
#define BUTTON_RIGHT_PIN                   GPIO_Pin_11                 
#define BUTTON_RIGHT_GPIO_PORT             GPIOG                      
#define BUTTON_RIGHT_GPIO_CLK              RCC_AHB1Periph_GPIOG
#define BUTTON_RIGHT_GPIO_IRQn             EXTI15_10_IRQn
#define BUTTON_RIGHT_EXTI_PortSource       EXTI_PortSourceGPIOG
#define BUTTON_RIGHT_EXTI_PinSource        EXTI_PinSource11
#define BUTTON_RIGHT_EXTI_LINE             EXTI_Line11
/*******************************************************/


/** 按键按下标志宏
	* 若按键按下为高电平，设置KEY_ON=1，KEY_OFF=0
	* 若按键按下为低电平，设置KEY_ON=0，KEY_OFF=1
	*/
#define BUTTON_ON	0
#define BUTTON_OFF	1

void BUTTON_GPIO_Config(void);

uint8_t BUTTON_Scan(GPIO_TypeDef* GPIOx,uint16_t GPIO_Pin);

#endif
