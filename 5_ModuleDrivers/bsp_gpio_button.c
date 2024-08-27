#include "bsp_gpio_button.h"   

/**
  * @brief  配置按键用到的I/O口
  * @param  无
  * @retval 无
  */
void BUTTON_GPIO_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_AHB1PeriphClockCmd(BUTTON_OK_GPIO_CLK|BUTTON_BACK_GPIO_CLK|BUTTON_LEFT_GPIO_CLK|BUTTON_RIGHT_GPIO_CLK,ENABLE);
	
	GPIO_InitStructure.GPIO_Pin = BUTTON_OK_PIN; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN; 
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(BUTTON_OK_GPIO_PORT, &GPIO_InitStructure);   

	GPIO_InitStructure.GPIO_Pin = BUTTON_BACK_PIN; 
	GPIO_Init(BUTTON_BACK_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = BUTTON_LEFT_PIN; 
	GPIO_Init(BUTTON_LEFT_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = BUTTON_RIGHT_PIN; 
	GPIO_Init(BUTTON_RIGHT_GPIO_PORT, &GPIO_InitStructure);
}

/**
  * @brief   检测是否有按键按下     
  *	@param 	 GPIOx:具体的端口, x可以是（A...K） 
  *	@param 	 GPIO_PIN:具体的端口位， 可以是GPIO_PIN_x（x可以是0...15）
  * @retval  按键的状态
  *	@arg     BUTTON_ON:按键按下
  *	@arg     BUTTON_OFF:按键没按下
  */
uint8_t BUTTON_Scan(GPIO_TypeDef* GPIOx,uint16_t GPIO_Pin)
{			
	/*检测是否有按键按下 */
	if(GPIO_ReadInputDataBit(GPIOx,GPIO_Pin) == BUTTON_ON )  
	{	 
		/*等待按键释放 */
		while(GPIO_ReadInputDataBit(GPIOx,GPIO_Pin) == BUTTON_ON);   
		return 	BUTTON_ON;	 
	}
	else
		return  BUTTON_OFF;
}

