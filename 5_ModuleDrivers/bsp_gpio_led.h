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


/** ����LED������ĺ꣬
	* ��LED�͵�ƽ��������LED_ON=0,LED_OFF=1
	* ��LED�ߵ�ƽ��������LED_ON=1,LED_OFF=0
	*/
#define LED_ON  0
#define LED_OFF 1

/* ���κ꣬��������������һ��ʹ�� */
#define LED1(a)	    if (a)	\
					GPIO_SetBits(LED1_GPIO_PORT,LED1_PIN);\
					else		\
					GPIO_ResetBits(LED1_GPIO_PORT,LED1_PIN)

#define LED2(a)	    if (a)	\
					GPIO_SetBits(LED2_GPIO_PORT,LED2_PIN);\
					else		\
					GPIO_ResetBits(LED2_GPIO_PORT,LED2_PIN)

                                        
/* ֱ�Ӳ����Ĵ����ķ�������IO */
#define	digitalHi(p,i)			    {p->BSRRL=i;}		//����Ϊ�ߵ�ƽ
#define digitalLo(p,i)			    {p->BSRRH=i;}		//����͵�ƽ
#define digitalToggle(p,i)	        {p->ODR ^=i;}		//�����ת״̬

/* �������IO�ĺ� */
#define LED1_TOGGLE		    digitalToggle(LED1_GPIO_PORT,LED1_PIN)
#define LED1_OFF			digitalHi(LED1_GPIO_PORT,LED1_PIN)
#define LED1_ON				digitalLo(LED1_GPIO_PORT,LED1_PIN)

#define LED2_TOGGLE		    digitalToggle(LED2_GPIO_PORT,LED2_PIN)
#define LED2_OFF			digitalHi(LED2_GPIO_PORT,LED2_PIN)
#define LED2_ON				digitalLo(LED2_GPIO_PORT,LED2_PIN)

void LED_GPIO_Config(void);




#endif
