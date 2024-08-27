#include "bsp_gpio_led.h"   

 /**
  * @brief  配置控制LED的IO
  * @param  无
  * @retval 无
  */
void LED_GPIO_Config(void)
{		
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd (LED1_GPIO_CLK|LED2_GPIO_CLK, ENABLE); 
														   
    GPIO_InitStructure.GPIO_Pin = LED1_PIN;	
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;   
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; 
    GPIO_Init(LED1_GPIO_PORT, &GPIO_InitStructure);	
													   
    GPIO_InitStructure.GPIO_Pin = LED2_PIN;	
    GPIO_Init(LED2_GPIO_PORT, &GPIO_InitStructure);

    GPIO_SetBits(LED1_GPIO_PORT,LED1_PIN);
    GPIO_SetBits(LED2_GPIO_PORT,LED2_PIN);
}
