#ifndef _BSP_USART_EXPAND_H
#define	_BSP_USART_EXPAND_H
#include "stm32f4xx.h"


/************************************************************/
#define EXPAND_USART                            UART4
#define EXPAND_USART_CLK                        RCC_APB1Periph_UART4
#define EXPAND_USART_BAUDRATE                   115200  //´®¿Ú²¨ÌØÂÊ

#define EXPAND_USART_RX_GPIO_PORT               GPIOA
#define EXPAND_USART_RX_GPIO_CLK                RCC_AHB1Periph_GPIOA
#define EXPAND_USART_RX_PIN                     GPIO_Pin_1
#define EXPAND_USART_RX_AF                      GPIO_AF_UART4
#define EXPAND_USART_RX_SOURCE                  GPIO_PinSource1

#define EXPAND_USART_TX_GPIO_PORT               GPIOA
#define EXPAND_USART_TX_GPIO_CLK                RCC_AHB1Periph_GPIOA
#define EXPAND_USART_TX_PIN                     GPIO_Pin_0
#define EXPAND_USART_TX_AF                      GPIO_AF_UART4
#define EXPAND_USART_TX_SOURCE                  GPIO_PinSource0

#define EXPAND_USART_IRQHandler                 UART4_IRQHandler
#define EXPAND_USART_IRQ                 		UART4_IRQn
/************************************************************/


void EXPAND_USART_Config(void);


#endif /* _BSP_USART_EXPAND_H */
