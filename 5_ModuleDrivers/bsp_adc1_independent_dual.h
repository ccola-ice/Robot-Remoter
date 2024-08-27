#ifndef __BSP_ADC_DUAL_H
#define	__BSP_ADC_DUAL_H
#include "stm32f4xx.h"

//�ɼ�3��ͨ��������
#define NUM_OF_ADC1CHANNEL              7

/*=====================ͨ��1 IO======================*/
// ADC IO�궨��
#define DUAL_ADC_GPIO_PORT1    GPIOA
#define DUAL_ADC_GPIO_PIN1     GPIO_Pin_4
#define DUAL_ADC_GPIO_CLK1     RCC_AHB1Periph_GPIOA
#define DUAL_ADC_CHANNEL1      ADC_Channel_4

/*=====================ͨ��2 IO ======================*/
// ADC IO�궨��
#define DUAL_ADC_GPIO_PORT2     GPIOB
#define DUAL_ADC_GPIO_PIN2      GPIO_Pin_0
#define DUAL_ADC_GPIO_CLK2      RCC_AHB1Periph_GPIOB
#define DUAL_ADC_CHANNEL2       ADC_Channel_8

/*=====================ͨ��3 IO ======================*/
// ADC IO�궨��
#define DUAL_ADC_GPIO_PORT3     GPIOB
#define DUAL_ADC_GPIO_PIN3      GPIO_Pin_1
#define DUAL_ADC_GPIO_CLK3      RCC_AHB1Periph_GPIOB
#define DUAL_ADC_CHANNEL3       ADC_Channel_9

/*=====================ͨ��4 IO ======================*/
// ADC IO�궨��
#define DUAL_ADC_GPIO_PORT4     GPIOC
#define DUAL_ADC_GPIO_PIN4      GPIO_Pin_0
#define DUAL_ADC_GPIO_CLK4      RCC_AHB1Periph_GPIOC
#define DUAL_ADC_CHANNEL4       ADC_Channel_10

/*=====================ͨ��5 IO ======================*/
// ADC IO�궨��
#define DUAL_ADC_GPIO_PORT5     GPIOC
#define DUAL_ADC_GPIO_PIN5      GPIO_Pin_1
#define DUAL_ADC_GPIO_CLK5      RCC_AHB1Periph_GPIOC
#define DUAL_ADC_CHANNEL5       ADC_Channel_11

/*=====================ͨ��6 IO ======================*/
// ADC IO�궨��
#define DUAL_ADC_GPIO_PORT6     GPIOC
#define DUAL_ADC_GPIO_PIN6      GPIO_Pin_2
#define DUAL_ADC_GPIO_CLK6      RCC_AHB1Periph_GPIOC
#define DUAL_ADC_CHANNEL6       ADC_Channel_12

/*=====================ͨ��7 IO ======================*/
// ADC IO�궨��  Battery_ADC
#define DUAL_ADC_GPIO_PORT7     GPIOC
#define DUAL_ADC_GPIO_PIN7      GPIO_Pin_3
#define DUAL_ADC_GPIO_CLK7      RCC_AHB1Periph_GPIOC
#define DUAL_ADC_CHANNEL7       ADC_Channel_13


// ADC ��ź궨��
#define DUAL_ADC1                ADC1
#define DUAL_ADC1_CLK            RCC_APB2Periph_ADC1
// ADC DR�Ĵ����궨�壬ADCת���������ֵ����������
#define DUAL_ADC1_DR_ADDR        ((u32)ADC1+0x4c)

// ADC DMA ͨ���궨�壬��������ʹ��DMA����
#define DUAL_ADC1_DMA_CLK         RCC_AHB1Periph_DMA2
#define DUAL_ADC1_DMA_CHANNEL     DMA_Channel_0
#define DUAL_ADC1_DMA_STREAM      DMA2_Stream4

void Independent_Dual_ADC1_Init(void);

#endif



