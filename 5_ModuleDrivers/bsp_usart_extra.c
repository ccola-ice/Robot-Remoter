#include "bsp_usart_extra.h"

 /**
  * @brief  ������չ����(����4)Ƕ�������жϿ�����NVIC
  * @param  ��
  * @retval ��
  */
static void NVIC_EXPAND_USART_Configuration(void)
{
  NVIC_InitTypeDef NVIC_InitStructure;
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
  
  NVIC_InitStructure.NVIC_IRQChannel = EXPAND_USART_IRQ;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
}

 /**
  * @brief  EXPAND_USART GPIO ����,����ģʽ���á�115200 8-N-1 ���жϽ���ģʽ
  * @param  ��
  * @retval ��
  */
void EXPAND_USART_Config(void)
{
  GPIO_InitTypeDef  GPIO_InitStructure;
  USART_InitTypeDef USART_InitStructure;
	
  RCC_AHB1PeriphClockCmd(EXPAND_USART_RX_GPIO_CLK|EXPAND_USART_TX_GPIO_CLK,ENABLE);          
  RCC_APB1PeriphClockCmd(EXPAND_USART_CLK, ENABLE);
  
  GPIO_PinAFConfig(EXPAND_USART_RX_GPIO_PORT,EXPAND_USART_RX_SOURCE,EXPAND_USART_RX_AF);
  GPIO_PinAFConfig(EXPAND_USART_TX_GPIO_PORT,EXPAND_USART_TX_SOURCE,EXPAND_USART_TX_AF);

  /* GPIO��ʼ�� */
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;  
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  
  /* ����Tx����Ϊ���ù��� */
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Pin  = EXPAND_USART_TX_PIN  ;  
  GPIO_Init(EXPAND_USART_TX_GPIO_PORT, &GPIO_InitStructure);

  /* ����Rx����Ϊ���ù��� */
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Pin  = EXPAND_USART_RX_PIN;
  GPIO_Init(EXPAND_USART_RX_GPIO_PORT, &GPIO_InitStructure);
    
  USART_InitStructure.USART_BaudRate = EXPAND_USART_BAUDRATE;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  USART_Init(EXPAND_USART, &USART_InitStructure); 
	
  /* Ƕ�������жϿ�����NVIC���� */
  NVIC_EXPAND_USART_Configuration();
  
  /* ʹ�ܴ��ڽ����ж� */
  USART_ITConfig(EXPAND_USART, USART_IT_RXNE, ENABLE);
	
  /* ʹ�ܴ��� */
  USART_Cmd(EXPAND_USART, ENABLE);
}




