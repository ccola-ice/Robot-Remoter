#include "bsp_spi_flash.h"

volatile uint32_t flash_count_wait = FLASH_TIME_OUT;

 /**
  * @brief  W25Q128 SPI1 GPIO��ʼ��
  * @param  ��
  * @retval ��
  */
static void FLASH_SPI_GPIO_Config(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    
    /* ʹ��SPI1��ӦGPIO����ʱ�� */
    FLASH_SPI_MISO_GPIO_CLK_INIT( FLASH_SPI_MISO_GPIO_CLK, ENABLE);
    FLASH_SPI_MOSI_GPIO_CLK_INIT( FLASH_SPI_MOSI_GPIO_CLK, ENABLE);
    FLASH_SPI_SCK_GPIO_CLK_INIT(  FLASH_SPI_SCK_GPIO_CLK,  ENABLE);
    FLASH_SPI_CS_GPIO_CLK_INIT(   FLASH_SPI_CS_GPIO_CLK,   ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    
    /* ���� PXx���� �� SPI1 ���� F4XXϵ������*/
    GPIO_PinAFConfig(FLASH_SPI_MISO_GPIO_PORT, FLASH_SPI_MISO_SOURCE, FLASH_SPI_MISO_AF);
    GPIO_PinAFConfig(FLASH_SPI_MOSI_GPIO_PORT, FLASH_SPI_MOSI_SOURCE, FLASH_SPI_MOSI_AF);
    GPIO_PinAFConfig(FLASH_SPI_SCK_GPIO_PORT,  FLASH_SPI_SCK_SOURCE,  FLASH_SPI_SCK_AF);

    /* SPI1 SCK GPIO��ʼ�� */
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Pin = FLASH_SPI_SCK_GPIO_PIN  ;
    GPIO_Init(FLASH_SPI_SCK_GPIO_PORT, &GPIO_InitStructure);
    /* SPI1 MOSI GPIO��ʼ�� */
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Pin = FLASH_SPI_MOSI_GPIO_PIN  ;
    GPIO_Init(FLASH_SPI_MOSI_GPIO_PORT, &GPIO_InitStructure);
    /* SPI1 MISO GPIO��ʼ�� F4XXϵ�е�MISO GPIOҲֱ������Ϊ�����������Ҳ���Զ�ȡIO״̬*/
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Pin = FLASH_SPI_MISO_GPIO_PIN  ;
    GPIO_Init(FLASH_SPI_MISO_GPIO_PORT, &GPIO_InitStructure);
    /* SPI1 CS GPIO��ʼ�� �������CS����*/
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Pin = FLASH_SPI_CS_GPIO_PIN  ;
    GPIO_Init(FLASH_SPI_CS_GPIO_PORT, &GPIO_InitStructure);
}


 /**
  * @brief  W25Q128 SPI1 ����ģʽ��ʼ��
  * @param  ��
  * @retval ��
  */
static void FLASH_SPI_MODE_Config(void)
{
    SPI_InitTypeDef   SPI_InitStructure;
    
    /* ʹ��SPI1ʱ�� */
    FLASH_SPI_CLK_INIT(FLASH_SPI_CLK, ENABLE);
    
    /* SPI1 ��ʼ�� mode 0*/
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2 ;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge ;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
    SPI_InitStructure.SPI_CRCPolynomial = 7;//���д��
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_Init(FLASH_SPI,&SPI_InitStructure);

    /* ʹ�� SPI1 */
    SPI_Cmd(FLASH_SPI,ENABLE);
}


//W25Q128 SPI1��ʼ��
void FLASH_SPI_Init(void)
{
    FLASH_SPI_GPIO_Config();
	FLASH_SPI_CS_HIGH;
    FLASH_SPI_MODE_Config();
}


 /**
  * @brief  ʹ��SPI����һ���ֽڵ�����
  * @param  byte��Ҫ���͵�����
  * @retval ���ؽ��յ�������
  */
static uint8_t FLASH_Send_Byte(uint8_t byte)
{
    uint8_t return_data;
    
    flash_count_wait =  FLASH_TIME_OUT;
    /* �ȴ����ͻ�����Ϊ�գ�TXE�¼� */
    while(SPI_I2S_GetFlagStatus(FLASH_SPI, SPI_I2S_FLAG_TXE) == RESET)
    {
        flash_count_wait --;
        if(flash_count_wait == 0)
        {
            return FLASH_Error_CallBack(1);
        }
    }
    /* д�����ݼĴ�������Ҫд�������д�뷢�ͻ����� */
    SPI_I2S_SendData(FLASH_SPI,byte);
    
    flash_count_wait =  FLASH_TIME_OUT;
    /* �ȴ����ջ������ǿգ�RXNE�¼� */
    while(SPI_I2S_GetFlagStatus(FLASH_SPI, SPI_I2S_FLAG_RXNE) == RESET)
    {
        flash_count_wait --;
        if(flash_count_wait == 0)
        {
            return FLASH_Error_CallBack(2);
        }
    }  
    /* ��ȡ���ݼĴ�������ȡ���ջ��������� */
    return_data = SPI_I2S_ReceiveData(FLASH_SPI);
    
    return return_data;
}


 /**
  * @brief  ʹ��SPI��ȡһ���ֽڵ�����
  * @param  ��
  * @retval ���ؽ��յ�������
  */
static uint8_t FLASH_Receive_Byte(void)
{
  return (FLASH_Send_Byte(0xFF));//Dummy_Byte
}

 /**
  * @brief  ��ȡFlash Device ID [ID7-ID0]
  * @param  ��
  * @retval ����Device ID
  */
uint8_t FLASH_Read_DeviceID(void)
{
    uint8_t DeviceID_Value;
    
    //����CS�źţ���ʼͨ��
    FLASH_SPI_CS_LOW;
    
    //дָ�����W25X_ReleasePowerDown����3��Dummy Bytes
    FLASH_Send_Byte(W25X_ReleasePowerDown);
    FLASH_Send_Byte(0xFF);
    FLASH_Send_Byte(0xFF);
    FLASH_Send_Byte(0xFF);
    
    //���ն�ȡ��������
    DeviceID_Value = FLASH_Receive_Byte();
    
    //����CS�źţ�ֹͣͨ��
    FLASH_SPI_CS_HIGH;
    
    return DeviceID_Value;
}

 /**
  * @brief  ��ȡFLASH ID
  * @param 	��
  * @retval FLASH ID
  */
uint32_t FLASH_Read_FlashID(void)
{
  uint32_t Temp = 0, Temp0 = 0, Temp1 = 0, Temp2 = 0;

  //����CS�źţ���ʼͨ��
  FLASH_SPI_CS_LOW;

  //дָ�����W25X_JedecDeviceID����3��Dummy Bytes
  FLASH_Send_Byte(W25X_JedecDeviceID);

  Temp0 = FLASH_Send_Byte(0xFF);
  Temp1 = FLASH_Send_Byte(0xFF);
  Temp2 = FLASH_Send_Byte(0xFF);

  //����CS�źţ�ֹͣͨ��
  FLASH_SPI_CS_HIGH;

  //�������������
  Temp = (Temp0 << 16) | (Temp1 << 8) | Temp2;

  return Temp;
}

 /**
  * @brief  ����������ÿ�����ٲ���4096�ֽڣ����еĴ洢��Ԫ����д1��
  * @param  SectorAddr,������뵽Ҫ�������������׵�ַ 0,4096,......
  * @retval ��
  */
void FLASH_Erase_Sectors(uint32_t SectorAddr)
{
    uint8_t addr_low,addr_mid,addr_high;
    
    addr_low  = (SectorAddr >> 0)  & 0x000000FF;
    addr_mid  = (SectorAddr >> 8)  & 0x000000FF;
    addr_high = (SectorAddr >> 16) & 0x000000FF;
    
    /* дʹ�� */
    Flash_Write_Enable();
	Flash_Wait_For_Standby();
    
    /* �������� */
    //����CS�źţ���ʼͨ��
    FLASH_SPI_CS_LOW;
    
    //���Ͳ�������ָ�����W25X_SectorErase
    FLASH_Send_Byte(W25X_SectorErase);
    
    //����Ҫ�����ĵ�ַ
    FLASH_Send_Byte(addr_high);
    FLASH_Send_Byte(addr_mid);
    FLASH_Send_Byte(addr_low);

    //����CS�źţ�ֹͣͨ��
    FLASH_SPI_CS_HIGH;
    
    //�ȴ��ڲ�ʱ����� ��W25Q128Ҫ����д��/����/��������ǰ����Ҫ��W25Q128�Ƿ�Ϊ����״̬��ֻ�п���״̬���ܽ��в�����
    Flash_Wait_For_Standby();    
}

 /**
  * @brief  ����FLASH��������Ƭ����
  * @param  ��
  * @retval ��
  */
void FLASH_Erase_Bulk(void)
{
	/* дʹ�� */
	Flash_Write_Enable();

	/* ���� Erase */
	//����CS�źţ���ʼͨ��
	FLASH_SPI_CS_LOW;
	
	/* �����������ָ��*/
	FLASH_Send_Byte(W25X_ChipErase);
	
	//����CS�źţ�ֹͣͨ��
	FLASH_SPI_CS_HIGH;

	//�ȴ��������
	Flash_Wait_For_Standby();
}


 /**
  * @brief  ��ȡFLASH����
  * @param  data���洢Ҫ��ȡ�����ݵ�ָ�� 
            ReadAddr��Ҫ��ȡ�����ݵĵ�ַ
            size��Ҫ��ȡ�����ݳ��ȣ����ֽ�Ϊ��λ������ܶ�0xFFFFFF=16,777,215���ֽڣ�
  * @retval ��
  */
void FLASH_Read_Data(uint8_t * data,uint32_t ReadAddr,uint16_t size)
{
    uint8_t addr_low,addr_mid,addr_high;
    
    addr_low  = (ReadAddr >> 0)  & 0x000000FF;
    addr_mid  = (ReadAddr >> 8)  & 0x000000FF;
    addr_high = (ReadAddr >> 16) & 0x000000FF;
    
    //����CS�źţ���ʼͨ��
    FLASH_SPI_CS_LOW;
    
    //����ָ�����W25X_ReadData
    FLASH_Send_Byte(W25X_ReadData);
    
    //����Ҫ��ȡ���ݵĵ�ַ
    FLASH_Send_Byte(addr_high);
    FLASH_Send_Byte(addr_mid);
    FLASH_Send_Byte(addr_low);

    while(size--)
    {
        *data = FLASH_Receive_Byte();
        data++;
    }
    
    //����CS�źţ�ֹͣͨ��
    FLASH_SPI_CS_HIGH;     
}


 /**
  * @brief д������v1.0   ���ݳ��Ȳ�����256 ���ñ�����д������ǰ��Ҫ�Ȳ�������
  * @param  addr��Ҫд������ݵ�Flash��ַ 
            data��Ҫд������� ��ָ��
            size��Ҫд������ݳ��ȣ����ֽ�Ϊ��λ ������256
  * @retval
  */
void FLASH_Write_Page_v1(uint32_t addr,uint8_t * data,uint16_t size)
{
    uint8_t addr_low,addr_mid,addr_high;
    
    addr_low  = (addr >> 0)  & 0x000000FF;
    addr_mid  = (addr >> 8)  & 0x000000FF;
    addr_high = (addr >> 16) & 0x000000FF;
    
    /* дʹ�� */
    Flash_Write_Enable();
    
    //����CS�źţ���ʼͨ��
    FLASH_SPI_CS_LOW;
    
    //����ָ�����W25X_PageProgram
    FLASH_Send_Byte(W25X_PageProgram);
    
    //����Ҫд��Flash�ĵ�ַ
    FLASH_Send_Byte(addr_high);
    FLASH_Send_Byte(addr_mid);
    FLASH_Send_Byte(addr_low);

    //����Ҫд������� 
    while(size--)
    {
       FLASH_Send_Byte(*data);
       data++;
    }
    
    //����CS�źţ�ֹͣͨ��
    FLASH_SPI_CS_HIGH;
    
    //�ȴ��ڲ�ʱ����� ��W25Q128Ҫ����д��/����/��������ǰ����Ҫ��W25Q128�Ƿ�Ϊ����״̬��ֻ�п���״̬���ܽ��в�����
    Flash_Wait_For_Standby();   
}


 /**
  * @brief д������v2.0  ���ݳ������4096 Ч�ʵͣ�ռ����Դ�� ���ñ�����д������ǰ��Ҫ�Ȳ�������
  * @param  addr��Ҫд������ݵ�Flash��ַ 
            data��Ҫд������� ��ָ��
            size��Ҫд������ݳ��ȣ����Ȳ������ƣ����4096
  * @retval
  */
void FLASH_Write_Page_v2(uint32_t addr,uint8_t * data,uint16_t size)
{
    uint8_t addr_low,addr_mid,addr_high;
    
    while(size--)
    {
        addr_low  = (addr >> 0)  & 0x000000FF;
        addr_mid  = (addr >> 8)  & 0x000000FF;
        addr_high = (addr >> 16) & 0x000000FF;
        
        /* дʹ�� */
        Flash_Write_Enable();
        
        //����CS�źţ���ʼͨ��
        FLASH_SPI_CS_LOW;
        
        //����ָ�����W25X_PageProgram
        FLASH_Send_Byte(W25X_PageProgram);
        
        //����Ҫд��Flash�ĵ�ַ
        FLASH_Send_Byte(addr_high);
        FLASH_Send_Byte(addr_mid);
        FLASH_Send_Byte(addr_low);

        //����Ҫд������� 
        FLASH_Send_Byte(*data);
        data++;
        
        //��Ϊ���ڵ��߼���ÿ����һ���ֽڵ����ݶ��ȷ��͸��ֽڵĴ洢��Ԫ��ַ�����Դ洢��Ԫ��ַҪ����
        addr++;
        
        //����CS�źţ�ֹͣͨ��
        FLASH_SPI_CS_HIGH;
        
        //�ȴ��ڲ�ʱ����� ��W25Q128Ҫ����д��/����/��������ǰ����Ҫ��W25Q128�Ƿ�Ϊ����״̬��ֻ�п���״̬���ܽ��в�����
        Flash_Wait_For_Standby();
    }      
}


 /**
  * @brief д������v3.0 ��ҳд�� ���ݳ������4096 Ч�ʸ��ߣ�ռ����Դ�� �ҵ�ַ����,���Կ�����д�� ���ñ�����д������ǰ��Ҫ�Ȳ�������
  * @param  data��Ҫд������� ��ָ��
            WriteAddr��Ҫд������ݵ�Flash��ַ 
            size��Ҫд������ݳ��ȣ����Ȳ������ƣ����4096
  * @retval
  */
void FLASH_Write_Page_v3(uint8_t * data,uint32_t WriteAddr,uint16_t size)
{
    uint8_t addr_low,addr_mid,addr_high;
    uint32_t count = 0 ;
	
    Flash_Write_Enable();
    
	while(size--)
    {
        addr_low  = (WriteAddr >> 0)  & 0x000000FF;
        addr_mid  = (WriteAddr >> 8)  & 0x000000FF;
        addr_high = (WriteAddr >> 16) & 0x000000FF;
        
        count ++;
        
		if(size > FLASH_PerWritePageSize)
		{
			size = FLASH_PerWritePageSize;
			FLASH_ERROR("SPI_FLASH_PageWrite too large!");
		}	
		
        //�ֱ��ڵ�1,256*1+1,256*2+1,256*3+1...�Լ�addr��ַ4096����ʱ����if ����һ��W25X_PageProgram����
        //���У����˵�һ��֮�⣬ ����ÿ�ν���if��Ҫ�ȵȴ��ڲ�ʱ����ɣ���ΪFlash�涨ÿд��/����256���ֽھ�Ҫ�ȴ�д����ɣ��ٷ�����һ��256���ֽڵ�������׵�ַ��
        if(count == 1 || (count%256) == 1 || (WriteAddr%4096) == 0)
        {            
            /*������һ�ε�ҳд��ָ��(256���ֽ�)*/
            //����CS�źţ�ֹͣͨ��
            FLASH_SPI_CS_HIGH;
            
            /*���ﻹҪ����һ�εȴ�д��ʱ����ɣ��ȴ���һ��256������д����ڲ�ʱ�����*/
            //�ȴ��ڲ�ʱ����� ��W25Q128Ҫ����д��/����/��������ǰ����Ҫ��W25Q128�Ƿ�Ϊ����״̬��ֻ�п���״̬���ܽ��в�����
            Flash_Wait_For_Standby();
            
            /* дʹ�� */
            Flash_Write_Enable();
        
            //����CS�źţ���ʼͨ��
            FLASH_SPI_CS_LOW;
        
            //����ָ�����W25X_PageProgram
            FLASH_Send_Byte(W25X_PageProgram);
            
            //����Ҫд��Flash�ĵ�ַ
            FLASH_Send_Byte(addr_high);
            FLASH_Send_Byte(addr_mid);
            FLASH_Send_Byte(addr_low);   
        }
        
        //����Ҫд������� 
        FLASH_Send_Byte(*data);
        data++;
        //��Ϊ���ڵ��߼���ÿ����һ���ֽڵ����ݶ��ȷ��͸��ֽڵĴ洢��Ԫ��ַ�����Դ洢��Ԫ��ַҪ����
        WriteAddr++;
    
    }
    //����CS�źţ�ֹͣͨ��
    FLASH_SPI_CS_HIGH;
    //�ȴ��ڲ�ʱ����� ��W25Q128Ҫ����д��/����/��������ǰ����Ҫ��W25Q128�Ƿ�Ϊ����״̬��ֻ�п���״̬���ܽ��в�����
    Flash_Wait_For_Standby();     
}

 /**
  * @brief  ��FLASHд�����ݣ����ñ�����д������ǰ��Ҫ�Ȳ�������
  * @param	data��Ҫд�����ݵ� ָ��
  * @param  WriteAddr��д���ַ
  * @param  size��д�����ݳ���
  * @retval ��
  */
void FLASH_Write_Data(uint8_t * data, uint32_t WriteAddr, uint16_t size)
{
	  uint8_t NumOfPage = 0, NumOfSingle = 0, Addr = 0, count = 0, temp = 0;
		
		/*mod�������࣬��writeAddr��FLASH_PageSize��������������AddrֵΪ0*/
	  Addr = WriteAddr % FLASH_PageSize;
		
		/*��count������ֵ���պÿ��Զ��뵽ҳ��ַ*/
	  count = FLASH_PageSize - Addr;	
		/*�����Ҫд��������ҳ*/
	  NumOfPage =  size / FLASH_PageSize;
		/*mod�������࣬�����ʣ�಻��һҳ���ֽ���*/
	  NumOfSingle = size % FLASH_PageSize;

		 /* Addr=0,��WriteAddr �պð�ҳ���� aligned  */
	  if (Addr == 0) 
	  {
			/* size < FLASH_PageSize */
		if (NumOfPage == 0) 
		{
		  FLASH_Write_Page_v3(data, WriteAddr, size);
		}
		else /* size > FLASH_PageSize */
		{
				/*�Ȱ�����ҳ��д��*/
		  while (NumOfPage--)
		  {
			FLASH_Write_Page_v3(data, WriteAddr, FLASH_PageSize);
			WriteAddr +=  FLASH_PageSize;
			data += FLASH_PageSize;
		  }
				
				/*���ж���Ĳ���һҳ�����ݣ�����д��*/
		  FLASH_Write_Page_v3(data, WriteAddr, NumOfSingle);
		}
	  }
		/* ����ַ�� FLASH_PageSize ������  */
	  else 
	  {
			/* size < FLASH_PageSize */
		if (NumOfPage == 0) 
		{
				/*��ǰҳʣ���count��λ�ñ�NumOfSingleС��д����*/
		  if (NumOfSingle > count) 
		  {
			temp = NumOfSingle - count;
					
					/*��д����ǰҳ*/
			FLASH_Write_Page_v3(data, WriteAddr, count);
			WriteAddr +=  count;
			data += count;
					
					/*��дʣ�������*/
			FLASH_Write_Page_v3(data, WriteAddr, temp);
		  }
		  else /*��ǰҳʣ���count��λ����д��NumOfSingle������*/
		  {				
			FLASH_Write_Page_v3(data, WriteAddr, size);
		  }
		}
		else /* size > FLASH_PageSize */
		{
				/*��ַ����������count�ֿ������������������*/
		  size -= count;
		  NumOfPage =  size / FLASH_PageSize;
		  NumOfSingle = size % FLASH_PageSize;

		  FLASH_Write_Page_v3(data, WriteAddr, count);
		  WriteAddr +=  count;
		  data += count;
				
				/*������ҳ��д��*/
		  while (NumOfPage--)
		  {
			FLASH_Write_Page_v3(data, WriteAddr, FLASH_PageSize);
			WriteAddr +=  FLASH_PageSize;
			data += FLASH_PageSize;
		  }
				/*���ж���Ĳ���һҳ�����ݣ�����д��*/
		  if (NumOfSingle != 0)
		  {
			FLASH_Write_Page_v3(data, WriteAddr, NumOfSingle);
		  }
		}
	  }
}

//�������ģʽ
void FLASH_PowerDown(void)   
{ 
  /* ѡ�� FLASH: CS �� */
  FLASH_SPI_CS_LOW;

  /* ���� ���� ���� */
  FLASH_Send_Byte(W25X_PowerDown);

  /* ֹͣ�ź�  FLASH: CS �� */
  FLASH_SPI_CS_HIGH;
} 

//����
void FLASH_Wakeup(void)   
{
  /*ѡ�� FLASH: CS �� */
  FLASH_SPI_CS_LOW;

  /* ���� �ϵ� ���� */
  FLASH_Send_Byte(W25X_ReleasePowerDown);

  /* ֹͣ�ź� FLASH: CS �� */
  FLASH_SPI_CS_HIGH;                   //�ȴ�TRES1
}


 /**
  * @brief  дʹ��
  * @param  ��
  * @retval ��
  */
static void Flash_Write_Enable(void)
{
    //����CS�źţ���ʼͨ��
    FLASH_SPI_CS_LOW;
    
    //ָ�����W25X_WriteEnable
    FLASH_Send_Byte(W25X_WriteEnable);

    //����CS�źţ�ֹͣͨ��
    FLASH_SPI_CS_HIGH;
}


 /**
  * @brief  һֱ�ȴ���֪��Flash��Ϊ����״̬ 
  * @param  ��
  * @retval ��
  */
static void Flash_Wait_For_Standby(void)
{
    uint8_t Flash_status = 0;
    
    //����CS�źţ���ʼͨ��
    FLASH_SPI_CS_LOW;
    
    //����ָ�����W25X_ReadStatusReg
    FLASH_Send_Byte(W25X_ReadStatusReg);
    
    //���շ������ݣ����صľ���״̬�Ĵ�����ֵ
    Flash_status = FLASH_Receive_Byte();
    
    flash_count_wait =  FLASH_TIME_OUT;
    /*���FLASH��״̬�Ĵ��������λ(BUSYλ)�����BUSYλΪ1����æµ״̬������Ϊ����״̬*/
    while(1)
    {
        Flash_status = FLASH_Receive_Byte();
        if((Flash_status & WIP_Flag) == 0)
        {
            break;
        }
        flash_count_wait -- ;
        if(flash_count_wait == 0)
        {
            FLASH_Error_CallBack(3);
            break;
        }
    }                              
    
    //����CS�źţ�ֹͣͨ��
    FLASH_SPI_CS_HIGH;
}


//code���������
static uint8_t FLASH_Error_CallBack(uint8_t code)
{
    printf("\r\nSPI error occurred , code = %d",code);
    return code;
}
