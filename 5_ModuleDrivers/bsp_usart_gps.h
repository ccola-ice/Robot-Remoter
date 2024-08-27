#ifndef _BSP_USART_GPS_H
#define	_BSP_USART_GPS_H
#include "stm32f4xx.h"
#include "nmea/nmea.h"

//#define __GPS_DEBUG

/*******************************************************/
#define GPS_USART                             USART6
#define GPS_USART_CLK                         RCC_APB2Periph_USART6
#define GPS_USART_BAUDRATE                    9600

#define GPS_USART_RX_GPIO_PORT                GPIOC
#define GPS_USART_RX_GPIO_CLK                 RCC_AHB1Periph_GPIOC
#define GPS_USART_RX_PIN                      GPIO_Pin_7
#define GPS_USART_RX_AF                       GPIO_AF_USART6
#define GPS_USART_RX_SOURCE                   GPIO_PinSource7

#define GPS_USART_TX_GPIO_PORT                GPIOC
#define GPS_USART_TX_GPIO_CLK                 RCC_AHB1Periph_GPIOC
#define GPS_USART_TX_PIN                      GPIO_Pin_6
#define GPS_USART_TX_AF                       GPIO_AF_USART6
#define GPS_USART_TX_SOURCE                   GPIO_PinSource6
/*******************************************************/


/*******************************************************/
//GPS USART6对应的DMA
#define GPS_USART_DMA_STREAM        DMA2_Stream1
#define GPS_USART_DMA_CLK           RCC_AHB1Periph_DMA2
#define GPS_USART_DMA_CHANNEL       DMA_Channel_5

/* 外设标志 */
#define GPS_DMA_IT_HT               DMA_IT_HTIF1
#define GPS_DMA_IT_TC               DMA_IT_TCIF1

#define GPS_DMA_IRQn                DMA2_Stream1_IRQn         //GPS中断源
#define GPS_DMA_IRQHANDLER          DMA2_Stream1_IRQHandler   //GPS使用的DMA中断服务函数

#define GPS_DR_Base                 (USART6_BASE+0x04)        //串口的数据寄存器地址
#define GPS_DATA_ADDR               GPS_DR_Base               //GPS使用的串口的数据寄存器地址
#define GPS_RBUFF_SIZE              512                       //串口接收缓冲区大小
#define HALF_GPS_RBUFF_SIZE         (GPS_RBUFF_SIZE/2)        //串口接收缓冲区一半  

extern uint8_t gps_rbuff[GPS_RBUFF_SIZE];
extern volatile uint8_t GPS_TransferEnd ;
extern volatile uint8_t GPS_HalfTransferEnd;
/*******************************************************/


void GPS_USART_Config(void);
void GPS_DMA_Config(void);

void trace(const char *str, int str_size);
void error(const char *str, int str_size);
void gps_info(const char *str, int str_size);
void GMTconvert(nmeaTIME *SourceTime, nmeaTIME *ConvertTime, uint8_t GMT,uint8_t AREA) ;




#endif /* _BSP_USART_GPS_H */


