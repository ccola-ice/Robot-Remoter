#include "bsp_usart_gps.h"
#include "bsp_usart_debug.h"

//DMA���ջ���
uint8_t gps_rbuff[GPS_RBUFF_SIZE];

//DMA���������־
volatile uint8_t GPS_TransferEnd = 0;
volatile uint8_t GPS_HalfTransferEnd = 0;

 /**
  * @brief  GPS_USART GPIO ����,����ģʽ���á�115200 8-N-1 ���жϽ���ģʽ
  * @param  ��
  * @retval ��
  */
void GPS_USART_Config(void)
{
  GPIO_InitTypeDef  GPIO_InitStructure;
  USART_InitTypeDef USART_InitStructure;
	
  /* ʹ�� USART ʱ�� */           
  RCC_APB2PeriphClockCmd(GPS_USART_CLK, ENABLE);
  RCC_AHB1PeriphClockCmd(GPS_USART_RX_GPIO_CLK|GPS_USART_TX_GPIO_CLK,ENABLE);

  /* ���� PXx �� USARTx_Tx*/
  GPIO_PinAFConfig(GPS_USART_RX_GPIO_PORT,GPS_USART_RX_SOURCE,GPS_USART_RX_AF);

  /* ���� PXx �� USARTx__Rx*/
  GPIO_PinAFConfig(GPS_USART_TX_GPIO_PORT,GPS_USART_TX_SOURCE,GPS_USART_TX_AF);
  
  /* ����Tx����Ϊ���ù���  */
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Pin = GPS_USART_TX_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  
  GPIO_Init(GPS_USART_TX_GPIO_PORT, &GPIO_InitStructure);

  /* ����Rx����Ϊ���ù��� */
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Pin = GPS_USART_RX_PIN;
  GPIO_Init(GPS_USART_RX_GPIO_PORT, &GPIO_InitStructure);
  
  /* ���ô���GPS_UART ģʽ */
  /* ���������ã�GPS_UART_BAUDRATE */
  USART_InitStructure.USART_BaudRate = GPS_USART_BAUDRATE;
  /* �ֳ�(����λ+У��λ)��8 */
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  /* ֹͣλ��1��ֹͣλ */
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  /* У��λѡ�񣺲�ʹ��У�� */
  USART_InitStructure.USART_Parity = USART_Parity_No;
  /* Ӳ�������ƣ���ʹ��Ӳ���� */
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  /* USARTģʽ���ƣ�ͬʱʹ�ܽ��պͷ��� */
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  /* ���USART��ʼ������ */
  USART_Init(GPS_USART, &USART_InitStructure); 
	
  //GPSģ��ʹ��DMA�������ݣ������� ����������жϣ����Բ���Ҫ���ô����жϡ�
  
  /* ʹ�ܴ��� */
  USART_Cmd(GPS_USART, ENABLE);
}

/**
  * @brief  GPS_Interrupt_Config ����GPSʹ�õ�DMA�ж� 
  * @param  None.
  * @retval None.
  */
static void GPS_DMA_Interrupt_Config(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

	//DMA Channel Interrupt ENABLE
	NVIC_InitStructure.NVIC_IRQChannel = GPS_DMA_IRQn; 
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

/**
  * @brief  GPS_DMA_Config gps dma��������
  * @param  ��
  * @retval ��
  */
void GPS_DMA_Config(void)
{
    DMA_InitTypeDef DMA_InitStructure;

    /* ����DMAʱ�� */ 
    RCC_AHB1PeriphClockCmd(GPS_USART_DMA_CLK, ENABLE); 
    
    /* ��λ��ʼ��DMA������ */ 
    DMA_DeInit(GPS_USART_DMA_STREAM);
  
    /* ȷ��DMA��������λ��� */
    while (DMA_GetCmdStatus(GPS_USART_DMA_STREAM) != DISABLE)  {}
  
    /*usart ��Ӧdma��ͨ����������*/	
    DMA_InitStructure.DMA_Channel = GPS_USART_DMA_CHANNEL;  
    /*����DMAԴ���������ݼĴ�����ַ  Դ��ַ   */
    DMA_InitStructure.DMA_PeripheralBaseAddr = GPS_DATA_ADDR;	 
    /*�ڴ��ַ(Ҫ����ı�����ָ��)   Ŀ�ĵ�ַ */
    DMA_InitStructure.DMA_Memory0BaseAddr = (u32)gps_rbuff;
    /*���򣺴����赽�ڴ�*/		
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;	
    /*�����СDMA_BufferSize=SENDBUFF_SIZE*/	
    DMA_InitStructure.DMA_BufferSize = GPS_RBUFF_SIZE;
    /*�����ַ����*/	    
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable; 
    /*�ڴ��ַ����*/
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;	
    /*�������ݵ�λ*/	
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    /*�ڴ����ݵ�λ 8bit*/
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;	
    /*DMAģʽ������ѭ��*/
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;	 
    /*���ȼ�����*/	
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;      
    /*����FIFO*/
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;        
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;    
    /*�洢��ͻ������ 16������*/
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;    
    /*����ͻ������ 1������*/
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;    
    /*����DMA2��������7*/		   
    DMA_Init(GPS_USART_DMA_STREAM, &DMA_InitStructure);    
    
    /*�����ж����ȼ�*/
    GPS_DMA_Interrupt_Config();
    
    DMA_ITConfig(GPS_USART_DMA_STREAM,DMA_IT_HT|DMA_IT_TC,ENABLE);  //����DMA������ɺ�����ж�
      
     /*ʹ��DMA*/
     DMA_Cmd(GPS_USART_DMA_STREAM, ENABLE);
  
    /* �ȴ�DMA��������Ч*/
    while(DMA_GetCmdStatus(GPS_USART_DMA_STREAM) != ENABLE){ }   
    
    /* ���� ���� �� DMA �������� */
     USART_DMACmd(GPS_USART, USART_DMAReq_Rx, ENABLE);
}



/**
  * @brief  trace �ڽ���ʱ��������GPS���
  * @param  str: Ҫ������ַ�����str_size:���ݳ���
  * @retval ��
  */
void trace(const char *str, int str_size)
{
  #ifdef __GPS_DEBUG    //��gps_config.h�ļ���������꣬�Ƿ����������Ϣ
    uint16_t i;
    printf("\r\nTrace: ");
    for(i=0;i<str_size;i++)
      printf("%c",*(str+i));
  
    printf("\n");
  #endif
}

/**
  * @brief  error �ڽ������ʱ�����ʾ��Ϣ
  * @param  str: Ҫ������ַ�����str_size:���ݳ���
  * @retval ��
  */
void error(const char *str, int str_size)
{
    #ifdef __GPS_DEBUG   //��gps_config.h�ļ���������꣬�Ƿ����������Ϣ

    uint16_t i;
    printf("\r\nError: ");
    for(i=0;i<str_size;i++)
      printf("%c",*(str+i));
    printf("\n");
    #endif
}

/**
  * @brief  error �ڽ������ʱ�����ʾ��Ϣ
  * @param  str: Ҫ������ַ�����str_size:���ݳ���
  * @retval ��
  */
void gps_info(const char *str, int str_size)
{

    uint16_t i;
    printf("\r\nInfo: ");
    for(i=0;i<str_size;i++)
    printf("%c",*(str+i));
    printf("\n");
}

/******************************************************************************************************** 
**    ��������:            bit  IsLeapYear(uint8_t iYear) 
**    ��������:            �ж�����(�������2000�Ժ�����) 
**    ��ڲ�����           iYear    ��λ���� 
**    ���ڲ���:            uint8_t  1:Ϊ����    0:Ϊƽ�� 
********************************************************************************************************/ 
static uint8_t IsLeapYear(uint8_t iYear) 
{ 
    uint16_t    Year; 
    Year    =    2000+iYear; 
    if((Year&3)==0) 
    { 
        return ((Year%400==0) || (Year%100!=0)); 
    } 
     return 0; 
} 

/******************************************************************************************************** 
**    ��������:            void    GMTconvert(uint8_t *DT,uint8_t GMT,uint8_t AREA) 
**    ��������:            ��������ʱ�任�������ʱ��ʱ�� 
**    ��ڲ�����           *DT:    ��ʾ����ʱ������� ��ʽ YY,MM,DD,HH,MM,SS 
**                        GMT:    ʱ���� 
**                        AREA:    1(+)���� W0(-)���� 
********************************************************************************************************/ 
void GMTconvert(nmeaTIME *SourceTime, nmeaTIME *ConvertTime, uint8_t GMT,uint8_t AREA) 
{ 
    uint32_t    YY,MM,DD,hh,mm,ss;        //������ʱ�����ݴ���� 
     
    if(GMT==0)    return;                //�������0ʱ��ֱ�ӷ��� 
    if(GMT>12)    return;                //ʱ�����Ϊ12 �����򷵻�         

    YY    =    SourceTime->year;                //��ȡ�� 
    MM    =    SourceTime->mon;                 //��ȡ�� 
    DD    =    SourceTime->day;                 //��ȡ�� 
    hh    =    SourceTime->hour;                //��ȡʱ 
    mm    =    SourceTime->min;                 //��ȡ�� 
    ss    =    SourceTime->sec;                 //��ȡ�� 

    if(AREA)                        //��(+)ʱ������ 
    { 
        if(hh+GMT<24)    hh    +=    GMT;//������������ʱ�䴦��ͬһ�������Сʱ���� 
        else                        //����Ѿ����ڸ�������ʱ��1����������ڴ��� 
        { 
            hh    =    hh+GMT-24;        //�ȵó�ʱ�� 
            if(MM==1 || MM==3 || MM==5 || MM==7 || MM==8 || MM==10)    //���·�(12�µ�������) 
            { 
                if(DD<31)    DD++; 
                else 
                { 
                    DD    =    1; 
                    MM    ++; 
                } 
            } 
            else if(MM==4 || MM==6 || MM==9 || MM==11)                //С�·�2�µ�������) 
            { 
                if(DD<30)    DD++; 
                else 
                { 
                    DD    =    1; 
                    MM    ++; 
                } 
            } 
            else if(MM==2)    //����2�·� 
            { 
                if((DD==29) || (DD==28 && IsLeapYear(YY)==0))        //��������������2��29�� ���߲�����������2��28�� 
                { 
                    DD    =    1; 
                    MM    ++; 
                } 
                else    DD++; 
            } 
            else if(MM==12)    //����12�·� 
            { 
                if(DD<31)    DD++; 
                else        //�������һ�� 
                {               
                    DD    =    1; 
                    MM    =    1; 
                    YY    ++; 
                } 
            } 
        } 
    } 
    else 
    {     
        if(hh>=GMT)    hh    -=    GMT;    //������������ʱ�䴦��ͬһ�������Сʱ���� 
        else                        //����Ѿ����ڸ�������ʱ��1����������ڴ��� 
        { 
            hh    =    hh+24-GMT;        //�ȵó�ʱ�� 
            if(MM==2 || MM==4 || MM==6 || MM==8 || MM==9 || MM==11)    //�����Ǵ��·�(1�µ�������) 
            { 
                if(DD>1)    DD--; 
                else 
                { 
                    DD    =    31; 
                    MM    --; 
                } 
            } 
            else if(MM==5 || MM==7 || MM==10 || MM==12)                //������С�·�2�µ�������) 
            { 
                if(DD>1)    DD--; 
                else 
                { 
                    DD    =    30; 
                    MM    --; 
                } 
            } 
            else if(MM==3)    //�����ϸ�����2�·� 
            { 
                if((DD==1) && IsLeapYear(YY)==0)                    //�������� 
                { 
                    DD    =    28; 
                    MM    --; 
                } 
                else    DD--; 
            } 
            else if(MM==1)    //����1�·� 
            { 
                if(DD>1)    DD--; 
                else        //�����һ�� 
                {               
                    DD    =    31; 
                    MM    =    12; 
                    YY    --; 
                } 
            } 
        } 
    }         

    ConvertTime->year   =    YY;                //������ 
    ConvertTime->mon    =    MM;                //������ 
    ConvertTime->day    =    DD;                //������ 
    ConvertTime->hour   =    hh;                //����ʱ 
    ConvertTime->min    =    mm;                //���·� 
    ConvertTime->sec    =    ss;                //������ 
}


