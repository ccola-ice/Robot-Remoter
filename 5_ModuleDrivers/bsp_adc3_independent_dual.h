#ifndef __BSP_ADC3_DUAL_H
#define	__BSP_ADC3_DUAL_H
#include "stm32f4xx.h"

//采集2个通道的数据
#define NUM_OF_ADC3CHANNEL              3

/*=====================通道1 IO======================*/
// ADC IO宏定义
#define DUAL_ADC3_GPIO_PORT1        GPIOF
#define DUAL_ADC3_GPIO_PIN1         GPIO_Pin_6
#define DUAL_ADC3_GPIO_CLK1         RCC_AHB1Periph_GPIOF
#define DUAL_ADC3_CHANNEL1          ADC_Channel_4

/*=====================通道2 IO======================*/
// ADC IO宏定义
#define DUAL_ADC3_GPIO_PORT2        GPIOF
#define DUAL_ADC3_GPIO_PIN2         GPIO_Pin_8
#define DUAL_ADC3_GPIO_CLK2         RCC_AHB1Periph_GPIOF
#define DUAL_ADC3_CHANNEL2          ADC_Channel_6

/*=====================通道3 IO ======================*/
// ADC IO宏定义
#define DUAL_ADC3_GPIO_PORT3        GPIOF
#define DUAL_ADC3_GPIO_PIN3         GPIO_Pin_10
#define DUAL_ADC3_GPIO_CLK3         RCC_AHB1Periph_GPIOF
#define DUAL_ADC3_CHANNEL3          ADC_Channel_8

// ADC 序号宏定义
#define DUAL_ADC3                ADC3
#define DUAL_ADC3_CLK            RCC_APB2Periph_ADC3
// ADC DR寄存器宏定义，ADC转换后的数字值则存放在这里
#define DUAL_ADC3_DR_ADDR        ((u32)ADC3+0x4c)

// ADC DMA 通道宏定义，这里我们使用DMA传输
#define DUAL_ADC3_DMA_CLK         RCC_AHB1Periph_DMA2
#define DUAL_ADC3_DMA_CHANNEL     DMA_Channel_2
#define DUAL_ADC3_DMA_STREAM      DMA2_Stream0

void Independent_Dual_ADC3_Init(void);

#endif



