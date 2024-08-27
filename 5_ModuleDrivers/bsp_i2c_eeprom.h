#ifndef __BSP_I2C_EEPROM_H
#define __BSP_I2C_EEPROM_H

#include "stm32f4xx.h"
#include <stdio.h>

/*�ȴ���ʱʱ��*/
#define EEPROM_TIME_OUT                         0X000007FF    //2048

/*STM32 ��I2C1������ַ*/
#define STM32_I2C_OWN_ADDR                      0x77  //�����ģ�ֻҪ����豸��ַ��ͬ���ɡ�

/*
 * 1 0 1 0  A2 A1 A0 R/W
 * 1 0 1 0  0  0  0   0    = 0XA0
 * 1 0 1 0  0  0  0   1    = 0XA1 
 */
/*EEPROM���豸��ַ*/
#define EEPROM_I2C_ADDR                         0XA1

/*AT24C01/02ÿҳ��8���ֽ�*/ 
//AT24C02һ��256����ַ��0-255��ÿ����ַ�洢һ���ֽ�
//#define EEPROM_PAGE_SIZE                      8
/*AT24C04/08A/16Aÿҳ��16���ֽ�*/
#define EEPROM_PAGE_SIZE                      16
/*AT24C32/64ÿҳ��8���ֽ�*/
//#define EEPROM_PAGE_SIZE                     	32
/*AT24C256ÿҳ��64���ֽ�*/
//#define EEPROM_PAGE_SIZE                      	64

/*STM32 I2C����ģʽ */
#define I2C_SPEED                               400000

/*I2C�ӿ�*/
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
