#include "bsp_fsmc_sram.h"

/* ��ʼ��SRAM��IO */
static void SRAM_GPIO_Config(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
 
                         /*��ַ�ź���*/
  RCC_AHB1PeriphClockCmd(FSMC_A0_GPIO_CLK | FSMC_A1_GPIO_CLK | FSMC_A2_GPIO_CLK | 
                         FSMC_A3_GPIO_CLK | FSMC_A4_GPIO_CLK | FSMC_A5_GPIO_CLK |
                         FSMC_A6_GPIO_CLK | FSMC_A7_GPIO_CLK | FSMC_A8_GPIO_CLK |
                         FSMC_A9_GPIO_CLK | FSMC_A10_GPIO_CLK| FSMC_A11_GPIO_CLK| 
												 FSMC_A12_GPIO_CLK| FSMC_A13_GPIO_CLK|FSMC_A14_GPIO_CLK|
												 FSMC_A15_GPIO_CLK|FSMC_A16_GPIO_CLK|FSMC_A17_GPIO_CLK|FSMC_A18_GPIO_CLK|
                         /*�����ź���*/
                         FSMC_D0_GPIO_CLK | FSMC_D1_GPIO_CLK | FSMC_D2_GPIO_CLK | 
                         FSMC_D3_GPIO_CLK | FSMC_D4_GPIO_CLK | FSMC_D5_GPIO_CLK |
                         FSMC_D6_GPIO_CLK | FSMC_D7_GPIO_CLK | FSMC_D8_GPIO_CLK |
                         FSMC_D9_GPIO_CLK | FSMC_D10_GPIO_CLK| FSMC_D11_GPIO_CLK|
                         FSMC_D12_GPIO_CLK| FSMC_D13_GPIO_CLK| FSMC_D14_GPIO_CLK|
                         FSMC_D15_GPIO_CLK|  
                         /*�����ź���*/
                         FSMC_CS_GPIO_CLK  | FSMC_WE_GPIO_CLK | FSMC_OE_GPIO_CLK |
                         FSMC_UDQM_GPIO_CLK|FSMC_LDQM_GPIO_CLK, ENABLE);

  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
  
  /* A��ַ�ź��� */
  GPIO_InitStructure.GPIO_Pin = FSMC_A0_GPIO_PIN; 
  GPIO_Init(FSMC_A0_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A0_GPIO_PORT,FSMC_A0_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_A1_GPIO_PIN; 
  GPIO_Init(FSMC_A1_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A1_GPIO_PORT,FSMC_A1_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_A2_GPIO_PIN; 
  GPIO_Init(FSMC_A2_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A2_GPIO_PORT,FSMC_A2_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_A3_GPIO_PIN; 
  GPIO_Init(FSMC_A3_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A3_GPIO_PORT,FSMC_A3_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_A4_GPIO_PIN; 
  GPIO_Init(FSMC_A4_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A4_GPIO_PORT,FSMC_A4_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_A5_GPIO_PIN; 
  GPIO_Init(FSMC_A5_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A5_GPIO_PORT,FSMC_A5_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_A6_GPIO_PIN; 
  GPIO_Init(FSMC_A6_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A6_GPIO_PORT,FSMC_A6_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_A7_GPIO_PIN; 
  GPIO_Init(FSMC_A7_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A7_GPIO_PORT,FSMC_A7_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_A8_GPIO_PIN; 
  GPIO_Init(FSMC_A8_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A8_GPIO_PORT,FSMC_A8_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_A9_GPIO_PIN; 
  GPIO_Init(FSMC_A9_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A9_GPIO_PORT,FSMC_A9_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_A10_GPIO_PIN; 
  GPIO_Init(FSMC_A10_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A10_GPIO_PORT,FSMC_A10_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_A11_GPIO_PIN; 
  GPIO_Init(FSMC_A11_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A11_GPIO_PORT,FSMC_A11_GPIO_PinSource,FSMC_GPIO_AF);
	
  GPIO_InitStructure.GPIO_Pin = FSMC_A12_GPIO_PIN; 
  GPIO_Init(FSMC_A12_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A12_GPIO_PORT,FSMC_A12_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_A13_GPIO_PIN; 
  GPIO_Init(FSMC_A13_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A13_GPIO_PORT,FSMC_A13_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_A14_GPIO_PIN; 
  GPIO_Init(FSMC_A14_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A14_GPIO_PORT,FSMC_A14_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_A15_GPIO_PIN; 
  GPIO_Init(FSMC_A15_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A15_GPIO_PORT,FSMC_A15_GPIO_PinSource,FSMC_GPIO_AF);  
	
  GPIO_InitStructure.GPIO_Pin = FSMC_A16_GPIO_PIN; 
  GPIO_Init(FSMC_A16_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A16_GPIO_PORT,FSMC_A16_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_A17_GPIO_PIN; 
  GPIO_Init(FSMC_A17_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A17_GPIO_PORT,FSMC_A17_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_A18_GPIO_PIN; 
  GPIO_Init(FSMC_A18_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_A18_GPIO_PORT,FSMC_A18_GPIO_PinSource,FSMC_GPIO_AF);
    
  /* D�����ź��� */
  GPIO_InitStructure.GPIO_Pin = FSMC_D0_GPIO_PIN; 
  GPIO_Init(FSMC_D0_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_D0_GPIO_PORT,FSMC_D0_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_D1_GPIO_PIN; 
  GPIO_Init(FSMC_D1_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_D1_GPIO_PORT,FSMC_D1_GPIO_PinSource,FSMC_GPIO_AF);
    
  GPIO_InitStructure.GPIO_Pin = FSMC_D2_GPIO_PIN; 
  GPIO_Init(FSMC_D2_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_D2_GPIO_PORT,FSMC_D2_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_D3_GPIO_PIN; 
  GPIO_Init(FSMC_D3_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_D3_GPIO_PORT,FSMC_D3_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_D4_GPIO_PIN; 
  GPIO_Init(FSMC_D4_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_D4_GPIO_PORT,FSMC_D4_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_D5_GPIO_PIN; 
  GPIO_Init(FSMC_D5_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_D5_GPIO_PORT,FSMC_D5_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_D6_GPIO_PIN; 
  GPIO_Init(FSMC_D6_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_D6_GPIO_PORT,FSMC_D6_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_D7_GPIO_PIN; 
  GPIO_Init(FSMC_D7_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_D7_GPIO_PORT,FSMC_D7_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_D8_GPIO_PIN; 
  GPIO_Init(FSMC_D8_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_D8_GPIO_PORT,FSMC_D8_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_D9_GPIO_PIN; 
  GPIO_Init(FSMC_D9_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_D9_GPIO_PORT,FSMC_D9_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_D10_GPIO_PIN; 
  GPIO_Init(FSMC_D10_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_D10_GPIO_PORT,FSMC_D10_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_D11_GPIO_PIN; 
  GPIO_Init(FSMC_D11_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_D11_GPIO_PORT,FSMC_D11_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_D12_GPIO_PIN; 
  GPIO_Init(FSMC_D12_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_D12_GPIO_PORT,FSMC_D12_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_D13_GPIO_PIN; 
  GPIO_Init(FSMC_D13_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_D13_GPIO_PORT,FSMC_D13_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_D14_GPIO_PIN; 
  GPIO_Init(FSMC_D14_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_D14_GPIO_PORT,FSMC_D14_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_D15_GPIO_PIN; 
  GPIO_Init(FSMC_D15_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_D15_GPIO_PORT,FSMC_D15_GPIO_PinSource,FSMC_GPIO_AF);
  
  /* �����ź��� */
  GPIO_InitStructure.GPIO_Pin = FSMC_CS_GPIO_PIN; 
  GPIO_Init(FSMC_CS_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_CS_GPIO_PORT,FSMC_CS_GPIO_PinSource,FSMC_GPIO_AF);
    
  GPIO_InitStructure.GPIO_Pin = FSMC_WE_GPIO_PIN; 
  GPIO_Init(FSMC_WE_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_WE_GPIO_PORT,FSMC_WE_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_OE_GPIO_PIN; 
  GPIO_Init(FSMC_OE_GPIO_PORT, &GPIO_InitStructure);   
  GPIO_PinAFConfig(FSMC_OE_GPIO_PORT,FSMC_OE_GPIO_PinSource,FSMC_GPIO_AF);  
  
  GPIO_InitStructure.GPIO_Pin = FSMC_UDQM_GPIO_PIN; 
  GPIO_Init(FSMC_UDQM_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_UDQM_GPIO_PORT,FSMC_UDQM_GPIO_PinSource,FSMC_GPIO_AF);
  
  GPIO_InitStructure.GPIO_Pin = FSMC_LDQM_GPIO_PIN; 
  GPIO_Init(FSMC_LDQM_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(FSMC_LDQM_GPIO_PORT,FSMC_LDQM_GPIO_PinSource,FSMC_GPIO_AF); 
}				  			


/* ��ʼ��FSMC���� */
void SRAM_FSMC_Config(void)
{	
	FSMC_NORSRAMInitTypeDef  FSMC_NORSRAMInitStruct; 
	FSMC_NORSRAMTimingInitTypeDef  FSMC_NORSRAMTimingStruct; 
	
	//��ʼ��SRAM��ص�����
	SRAM_GPIO_Config();
	
	//ʹ��FSMCʱ��
	RCC_AHB3PeriphClockCmd(RCC_AHB3Periph_FSMC,ENABLE);
	
	//��ʼ��ʱ��ṹ��
	FSMC_NORSRAMTimingStruct.FSMC_AccessMode = FSMC_AccessMode_A;
	//һ��HCLK���ڳ���Ϊ1/168΢��=0.0059 = 6ns 	
	//6>0   54>25  54+6>55
	FSMC_NORSRAMTimingStruct.FSMC_AddressSetupTime = 0; //(0+1)��HCLK����=6
	FSMC_NORSRAMTimingStruct.FSMC_DataSetupTime = 8;		//(8+1)��HCLK���� = 54
	//����ʱ�����ø��첽SRAM�޹�
	FSMC_NORSRAMTimingStruct.FSMC_AddressHoldTime = 0;
	FSMC_NORSRAMTimingStruct.FSMC_BusTurnAroundDuration = 0;
	FSMC_NORSRAMTimingStruct.FSMC_CLKDivision = 0;
	FSMC_NORSRAMTimingStruct.FSMC_DataLatency = 0;	
	
	//��ʼ��FSMC�ṹ��
	//��������SRAM�洢��û���õ�
	FSMC_NORSRAMInitStruct.FSMC_WaitSignal = FSMC_WaitSignal_Disable  ;
	FSMC_NORSRAMInitStruct.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState  ;
	FSMC_NORSRAMInitStruct.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low ;
	FSMC_NORSRAMInitStruct.FSMC_WrapMode = FSMC_WrapMode_Disable  ;
	FSMC_NORSRAMInitStruct.FSMC_WriteBurst = FSMC_WriteBurst_Disable ;

	//��������SRAM�洢����������
	FSMC_NORSRAMInitStruct.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable ;
	FSMC_NORSRAMInitStruct.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable;
	FSMC_NORSRAMInitStruct.FSMC_Bank = FSMC_Bank1_NORSRAM4;
	FSMC_NORSRAMInitStruct.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
	FSMC_NORSRAMInitStruct.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
	FSMC_NORSRAMInitStruct.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
	FSMC_NORSRAMInitStruct.FSMC_MemoryType = FSMC_MemoryType_SRAM;
	FSMC_NORSRAMInitStruct.FSMC_WriteOperation = FSMC_WriteOperation_Enable ;
	FSMC_NORSRAMInitStruct.FSMC_ReadWriteTimingStruct = &FSMC_NORSRAMTimingStruct;
	FSMC_NORSRAMInitStruct.FSMC_WriteTimingStruct = &FSMC_NORSRAMTimingStruct;
	
	FSMC_NORSRAMInit(&FSMC_NORSRAMInitStruct);
	
	//ʹ�����裬NORSRAM4
	FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM4,ENABLE);										
}
	  		

///**
//  * @brief  �ԡ��֡�Ϊ��λ��sdramд������ 
//  * @param  pBuffer: ָ�����ݵ�ָ�� 
//  * @param  uwWriteAddress: Ҫд���SRAM�ڲ���ַ
//  * @param  uwBufferSize: Ҫд�����ݴ�С
//  * @retval None.
//  */
void SRAM_WriteBuffer(uint32_t* pBuffer, uint32_t uwWriteAddress, uint32_t uwBufferSize)
{
  __IO uint32_t write_pointer = (uint32_t)uwWriteAddress;
 
  /* ѭ��д������ */
  for (; uwBufferSize != 0; uwBufferSize--) 
  {
    /* �������ݵ�SRAM */
    *(uint32_t *) (SRAM_BASE_ADDR + write_pointer) = *pBuffer++;

    /* ��ַ����*/
    write_pointer += 4;
  }
    
}

///**
//  * @brief  ��SRAM�ж�ȡ���� 
//  * @param  pBuffer: ָ��洢���ݵ�buffer
//  * @param  ReadAddress: Ҫ��ȡ���ݵĵ�ʮ
//  * @param  uwBufferSize: Ҫ��ȡ�����ݴ�С
//  * @retval None.
//  */
void SRAM_ReadBuffer(uint32_t* pBuffer, uint32_t uwReadAddress, uint32_t uwBufferSize)
{
  __IO uint32_t write_pointer = (uint32_t)uwReadAddress;
  
  
  /*��ȡ���� */
  for(; uwBufferSize != 0x00; uwBufferSize--)
  {
   *pBuffer++ = *(__IO uint32_t *)(SRAM_BASE_ADDR + write_pointer );
    
   /* ��ַ����*/
    write_pointer += 4;
  } 
}


//��ʹ����Ļʱ ʧ��LCD_CS���� ��ֹӰ��
void Disable_LCD_CS()
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_AHB1PeriphClockCmd ( RCC_AHB1Periph_GPIOG, ENABLE); 
														   
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;   
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; 
	GPIO_Init(GPIOG, &GPIO_InitStructure);	
	GPIO_SetBits(GPIOG,GPIO_Pin_10);
}




