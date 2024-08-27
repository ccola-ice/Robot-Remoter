#ifndef __BSP_I2C_EEPROM_H
#define __BSP_I2C_EEPROM_H

#include "stm32f4xx.h"
#include <stdio.h>

/*等待超时时间*/
#define EEPROM_TIME_OUT                         0X000007FF    //2048

/*STM32 的I2C1主机地址*/
#define STM32_I2C_OWN_ADDR                      0x77  //随便设的，只要与从设备地址不同即可。

/*
 * 1 0 1 0  A2 A1 A0 R/W
 * 1 0 1 0  0  0  0   0    = 0XA0
 * 1 0 1 0  0  0  0   1    = 0XA1 
 */
/*EEPROM的设备地址*/
#define EEPROM_I2C_ADDR                         0XA1

/*AT24C01/02每页有8个字节*/ 
//AT24C02一共256个地址，0-255，每个地址存储一个字节
//#define EEPROM_PAGE_SIZE                      8
/*AT24C04/08A/16A每页有16个字节*/
#define EEPROM_PAGE_SIZE                      16
/*AT24C32/64每页有8个字节*/
//#define EEPROM_PAGE_SIZE                     	32
/*AT24C256每页有64个字节*/
//#define EEPROM_PAGE_SIZE                      	64

/*STM32 I2C快速模式 */
#define I2C_SPEED                               400000

/*I2C接口*/
#define EEPROM_I2C                              I2C1
#define EEPROM_I2C_CLK                          RCC_APB1Periph_I2C1
#define EEPROM_I2C_CLK_INIT                     RCC_APB1PeriphClockCmd

#define EEPROM_I2C_SDA_GPIO_PORT                GPIOB
#define EEPROM_I2C_SDA_GPIO_CLK                 RCC_AHB1Periph_GPIOB
#define EEPROM_I2C_SDA_GPIO_CLK_INIT            RCC_AHB1PeriphClockCmd
#define EEPROM_I2C_SDA_GPIO_PIN                 GPIO_Pin_7
#define EEPROM_I2C_SDA_AF                       GPIO_AF_I2C1
#define EEPROM_I2C_SDA_SOURCE                   GPIO_PinSource7

#define EEPROM_I2C_SCL_GPIO_PORT                GPIOB
#define EEPROM_I2C_SCL_GPIO_CLK                 RCC_AHB1Periph_GPIOB
#define EEPROM_I2C_SCL_GPIO_CLK_INIT            RCC_AHB1PeriphClockCmd
#define EEPROM_I2C_SCL_GPIO_PIN                 GPIO_Pin_6
#define EEPROM_I2C_SCL_AF                       GPIO_AF_I2C1
#define EEPROM_I2C_SCL_SOURCE                   GPIO_PinSource6

/************************************************************/

static void EEPROM_I2C_GPIO_Config(void);
static void EEPROM_I2C_MODE_Config(void);
void EEPROM_I2C_Init(void);

uint8_t EEPROM_Byte_Write(uint8_t addr ,uint8_t data);
uint8_t EEPROM_Random_Read(uint8_t addr , uint8_t *data);
uint8_t EEPROM_Page_Write(uint8_t addr ,uint8_t * data ,uint8_t size);
uint8_t EEPROM_Sequential_Read(uint8_t addr , uint8_t *data ,uint16_t size);
uint8_t EEPROM_Sequential_Write(uint8_t addr ,uint8_t * data ,uint16_t size);
uint8_t EEPROM_Sequential_Write_Update(uint8_t addr ,uint8_t * data ,uint16_t size);

static uint8_t Error_CallBack(uint8_t code);
uint8_t Wait_For_Standby(void);

#endif /* __BSP_I2C_EEPROM_H */
