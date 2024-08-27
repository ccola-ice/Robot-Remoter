#ifndef __BSP_GPIO_H
#define	__BSP_GPIO_H
#include "stm32f4xx.h"

//LED1
#define LED1_PIN                  GPIO_Pin_8                 
#define LED1_GPIO_PORT            GPIOB                      
#define LED1_GPIO_CLK             RCC_AHB1Periph_GPIOB

//LED2
#define LED2_PIN                  GPIO_Pin_13                 
#define LED2_GPIO_PORT            GPIOC                      
#define LED2_GPIO_CLK             RCC_AHB1Periph_GPIOC


/** 控制LED灯亮灭的宏，
	* 若LED低电平亮，设置LED_ON=0,LED_OFF=1
	* 若LED高电平亮，设置LED_ON=1,LED_OFF=0
	*/
#define LED_ON  0
#define LED_OFF 1

/* 带参宏，可以像内联函数一样使用 */
#define LED1(a)	    if (a)	\
					GPIO_SetBits(LED1_GPIO_PORT,LED1_PIN);\
					else		\
					GPIO_ResetBits(LED1_GPIO_PORT,LED1_PIN)

#define LED2(a)	    if (a)	\
					GPIO_SetBits(LED2_GPIO_PORT,LED2_PIN);\
					else		\
					GPIO_ResetBits(LED2_GPIO_PORT,LED2_PIN)

                                        
/* 直接操作寄存器的方法控制IO */
#define	digitalHi(p,i)			    {p->BSRRL=i;}		//设置为高电平
#define digitalLo(p,i)			    {p->BSRRH=i;}		//输出低电平
#define digitalToggle(p,i)	        {p->ODR ^=i;}		//输出反转状态

/* 定义控制IO的宏 */
#define LED1_TOGGLE		    digitalToggle(LED1_GPIO_PORT,LED1_PIN)
#define LED1_OFF			digitalHi(LED1_GPIO_PORT,LED1_PIN)
#define LED1_ON				digitalLo(LED1_GPIO_PORT,LED1_PIN)

#define LED2_TOGGLE		    digitalToggle(LED2_GPIO_PORT,LED2_PIN)
#define LED2_OFF			digitalHi(LED2_GPIO_PORT,LED2_PIN)
#define LED2_ON				digitalLo(LED2_GPIO_PORT,LED2_PIN)

void LED_GPIO_Config(void);




#endif
