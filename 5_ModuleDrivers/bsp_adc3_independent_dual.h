#ifndef __BSP_ADC3_DUAL_H
#define	__BSP_ADC3_DUAL_H
#include "stm32f4xx.h"

//�ɼ�2��ͨ��������
#define NUM_OF_ADC3CHANNEL              3

/*=====================ͨ��1 IO======================*/
// ADC IO�궨��
#define DUAL_ADC3_GPIO_PORT1        GPIOF
#define DUAL_ADC3_GPIO_PIN1         GPIO_Pin_6
#define DUAL_ADC3_GPIO_CLK1         RCC_AHB1Periph_GPIOF
#define DUAL_ADC3_CHANNEL1          ADC_Channel_4

/*=====================ͨ��2 IO======================*/
// ADC IO�궨��
#define DUAL_ADC3_GPIO_PORT2        GPIOF
#define DUAL_ADC3_GPIO_PIN2         GPIO_Pin_8
#define DUAL_ADC3_GPIO_CLK2         RCC_AHB1Periph_GPIOF
#define DUAL_ADC3_CHANNEL2          ADC_Channel_6

/*=====================ͨ��3 IO ======================*/
// ADC IO�궨��
#define DUAL_ADC3_GPIO_PORT3        GPIOF
#define DUAL_ADC3_GPIO_PIN3         GPIO_Pin_10
#define DUAL_ADC3_GPIO_CLK3         RCC_AHB1Periph_GPIOF
#define DUAL_ADC3_CHANNEL3          ADC_Channel_8

// ADC ��ź궨��
#define DUAL_ADC3                ADC3
#define DUAL_ADC3_CLK            RCC_APB2Periph_ADC3
// ADC DR�Ĵ����궨�壬ADCת���������ֵ����������
#define DUAL_ADC3_DR_ADDR        ((u32)ADC3+0x4c)

// ADC DMA ͨ���궨�壬��������ʹ��DMA����
#define DUAL_ADC3_DMA_CLK         RCC_AHB1Periph_DMA2
#define DUAL_ADC3_DMA_CHANNEL     DMA_Channel_2
#define DUAL_ADC3_DMA_STREAM      DMA2_Stream0

void Independent_Dual_ADC3_Init(void);

#endif



