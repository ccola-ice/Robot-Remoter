#include "bsp_gpio_stick.h"   
#include "bsp_Systick.h"

/**
  * @brief  配置拨杆用到的I/O口
  * @param  无
  * @retval 无
  */
void STICK_GPIO_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_AHB1PeriphClockCmd(STICK1_GPIO_CLK|STICK2_GPIO_CLK|STICK3_GPIO_CLK,ENABLE);
	
	GPIO_InitStructure.GPIO_Pin = STICK1_PIN; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN; 
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(STICK1_GPIO_PORT, &GPIO_InitStructure);   

	GPIO_InitStructure.GPIO_Pin = STICK2_PIN; 
	GPIO_Init(STICK2_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = STICK3_PIN; 
	GPIO_Init(STICK3_GPIO_PORT, &GPIO_InitStructure);
}

uint8_t STICK_Scan(GPIO_TypeDef* GPIOx,uint16_t GPIO_Pin)
{			
	if(GPIO_ReadInputDataBit(GPIOx,GPIO_Pin) == STICK_ON )  
	{	
		//Delay_us(100);
		if(GPIO_ReadInputDataBit(GPIOx,GPIO_Pin) == STICK_ON)
		{
			return 	STICK_ON;
		}
		else 
			return 2;	 
	}
	
	else if(GPIO_ReadInputDataBit(GPIOx,GPIO_Pin) == STICK_OFF )  
	{	 
		//Delay_us(100);
		if(GPIO_ReadInputDataBit(GPIOx,GPIO_Pin) == STICK_OFF)
		{
			return 	STICK_OFF;
		}
		else 
			return 2; 
	}
	else 
		return 2;
}


