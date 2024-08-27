#include "bsp_gpio_button.h"   

/**
  * @brief  ���ð����õ���I/O��
  * @param  ��
  * @retval ��
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
  * @brief   ����Ƿ��а�������     
  *	@param 	 GPIOx:����Ķ˿�, x�����ǣ�A...K�� 
  *	@param 	 GPIO_PIN:����Ķ˿�λ�� ������GPIO_PIN_x��x������0...15��
  * @retval  ������״̬
  *	@arg     BUTTON_ON:��������
  *	@arg     BUTTON_OFF:����û����
  */
uint8_t BUTTON_Scan(GPIO_TypeDef* GPIOx,uint16_t GPIO_Pin)
{			
	/*����Ƿ��а������� */
	if(GPIO_ReadInputDataBit(GPIOx,GPIO_Pin) == BUTTON_ON )  
	{	 
		/*�ȴ������ͷ� */
		while(GPIO_ReadInputDataBit(GPIOx,GPIO_Pin) == BUTTON_ON);   
		return 	BUTTON_ON;	 
	}
	else
		return  BUTTON_OFF;
}

