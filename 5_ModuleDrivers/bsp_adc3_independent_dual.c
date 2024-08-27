#include "bsp_adc3_independent_dual.h"

volatile uint16_t ADC3_Value[NUM_OF_ADC3CHANNEL]={0};

static void Independent_Mode_ADC3_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;	
    
    /*=====================ͨ��1======================*/	
    //ʹ�� GPIO ʱ��
    RCC_AHB1PeriphClockCmd(DUAL_ADC3_GPIO_CLK1,ENABLE);		
    GPIO_InitStructure.GPIO_Pin = DUAL_ADC3_GPIO_PIN1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;//������������
    GPIO_Init(DUAL_ADC3_GPIO_PORT1, &GPIO_InitStructure);

    /*=====================ͨ��2======================*/
    //ʹ�� GPIO ʱ��
    RCC_AHB1PeriphClockCmd(DUAL_ADC3_GPIO_CLK2,ENABLE);		
    GPIO_InitStructure.GPIO_Pin = DUAL_ADC3_GPIO_PIN2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;//������������	
    GPIO_Init(DUAL_ADC3_GPIO_PORT2, &GPIO_InitStructure);	

	/*=====================ͨ��3======================*/
    RCC_AHB1PeriphClockCmd(DUAL_ADC3_GPIO_CLK3,ENABLE);		
    GPIO_InitStructure.GPIO_Pin = DUAL_ADC3_GPIO_PIN3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;	
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;//������������
    GPIO_Init(DUAL_ADC3_GPIO_PORT3, &GPIO_InitStructure);
	
    /*=====================ͨ��n======================*/
    //�û��������
}

static void Independent_Mode_ADC3_MODE_Config(void)
{
    DMA_InitTypeDef DMA_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;
    ADC_CommonInitTypeDef ADC_CommonInitStructure;

    // ����ADCʱ��
    RCC_APB2PeriphClockCmd(DUAL_ADC3_CLK , ENABLE);
    
    //DMA Init �ṹ����� ��ʼ��--------------------------
    // ADC3ʹ��DMA2��������1��ͨ��2��������ֲ�̶�����
    // ����DMAʱ��
    RCC_AHB1PeriphClockCmd(DUAL_ADC3_DMA_CLK, ENABLE); 
    // �����ַΪ��ADC ���ݼĴ�����ַ
    DMA_InitStructure.DMA_PeripheralBaseAddr = DUAL_ADC3_DR_ADDR;	
    // �洢����ַ��ʵ���Ͼ���һ���ڲ�SRAM�ı���	
    DMA_InitStructure.DMA_Memory0BaseAddr = (u32)ADC3_Value;  
    // ���ݴ��䷽��Ϊ���赽�洢��	
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;	
    // ��������СΪ��ָһ�δ����������
    DMA_InitStructure.DMA_BufferSize = NUM_OF_ADC3CHANNEL;	
    // ����Ĵ���ֻ��һ������ַ���õ���
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    // �洢����ַ�̶�
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable; 
    // // �������ݴ�СΪ���֣��������ֽ� 
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord; 
    //	�洢�����ݴ�СҲΪ���֣����������ݴ�С��ͬ
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;	
    // ѭ������ģʽ
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    // DMA ����ͨ�����ȼ�Ϊ�ߣ���ʹ��һ��DMAͨ��ʱ�����ȼ����ò�Ӱ��
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    // ��ֹDMA FIFO	��ʹ��ֱ��ģʽ
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;  
    // FIFO ��С��FIFOģʽ��ֹʱ�������������	
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;  
    // ѡ�� DMA ͨ����ͨ������������
    DMA_InitStructure.DMA_Channel = DUAL_ADC3_DMA_CHANNEL; 
    //��ʼ��DMA�������൱��һ����Ĺܵ����ܵ������кܶ�ͨ��
    DMA_Init(DUAL_ADC3_DMA_STREAM, &DMA_InitStructure);
    // ʹ��DMA��
    DMA_Cmd(DUAL_ADC3_DMA_STREAM, ENABLE);

    //ADC Common �ṹ�� ���� ��ʼ��------------------------
    // ����ADCģʽ
    ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
    // ʱ��Ϊfpclk x��Ƶ	
    ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div4;
    // ��ֹDMAֱ�ӷ���ģʽ	
    ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
    // ����ʱ����	
    ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_20Cycles;  
    ADC_CommonInit(&ADC_CommonInitStructure);

    //ADC Init �ṹ�� ���� ��ʼ��--------------------------
    ADC_StructInit(&ADC_InitStructure);
    // ADC �ֱ���
    ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
    // ɨ��ģʽ����ͨ���ɼ���Ҫ	
    ADC_InitStructure.ADC_ScanConvMode = ENABLE; 
    // ����ת��	
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE; 
    //��ֹ�ⲿ���ش���
    ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    //�ⲿ����ͨ����������ʹ�������������ֵ��㸳ֵ����
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
    //�����Ҷ���	
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    //ת��ͨ��NUM_OF_CHANNEL��
    ADC_InitStructure.ADC_NbrOfConversion = NUM_OF_ADC3CHANNEL;                                    
    ADC_Init(DUAL_ADC3, &ADC_InitStructure);
    //---------------------------------------------------------------------------

    // ���� ADC ͨ��ת��˳��Ͳ���ʱ������
    ADC_RegularChannelConfig(DUAL_ADC3, DUAL_ADC3_CHANNEL1, 1, 
                             ADC_SampleTime_3Cycles);
    ADC_RegularChannelConfig(DUAL_ADC3, DUAL_ADC3_CHANNEL2, 2, 
                             ADC_SampleTime_3Cycles);
	ADC_RegularChannelConfig(DUAL_ADC3, DUAL_ADC3_CHANNEL3, 3, 
                             ADC_SampleTime_3Cycles);							 

    // ʹ��DMA���� after last transfer (Single-ADC mode)
    ADC_DMARequestAfterLastTransferCmd(DUAL_ADC3, ENABLE);
    // ʹ��ADC DMA
    ADC_DMACmd(DUAL_ADC3, ENABLE);
    // ʹ��ADC
    ADC_Cmd(DUAL_ADC3, ENABLE);
    // ��ʼADCת�����������
    ADC_SoftwareStartConv(DUAL_ADC3);
}

void Independent_Dual_ADC3_Init(void)
{
	Independent_Mode_ADC3_GPIO_Config();
	Independent_Mode_ADC3_MODE_Config();
}



