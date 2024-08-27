#ifndef __BSP_SPI_FLASH_H
#define __BSP_SPI_FLASH_H
#include "stm32f4xx.h"
#include <stdio.h>

/*�ȴ���ʱʱ��*/
#define  FLASH_TIME_OUT                  0X000007FF     //2048
#define  FLASH_ID                       0XEF4018       //W25Q128 ID
#define  DUMMY_BYTE                      0xFF

#define FLASH_PageSize                  4096
#define FLASH_PerWritePageSize          4096
#define FLASH_SECTOR_SIZE  				4096

/*�����-��ͷ*******************************/
#define W25X_WriteEnable		        0x06 
#define W25X_WriteDisable		        0x04 
#define W25X_ReadStatusReg		        0x05 
#define W25X_WriteStatusReg		        0x01 
#define W25X_ReadData			        0x03 
#define W25X_FastReadData		        0x0B 
#define W25X_FastReadDual		        0x3B 
#define W25X_PageProgram		        0x02 
#define W25X_BlockErase			        0xD8 
#define W25X_SectorErase		        0x20 
#define W25X_ChipErase			        0xC7 
#define W25X_PowerDown			        0xB9 
#define W25X_ReleasePowerDown	        0xAB 
#define W25X_DeviceID			        0xAB 
#define W25X_ManufactDeviceID           0x90 
#define W25X_JedecDeviceID		        0x9F 

#define WIP_Flag                        0x01  /* Write In Progress (WIP) flag */

/*SPI1�ӿ�*/
#define FLASH_SPI                               SPI1
#define FLASH_SPI_CLK                           RCC_APB2Periph_SPI1
#define FLASH_SPI_CLK_INIT                      RCC_APB2PeriphClockCmd

#define FLASH_SPI_SCK_GPIO_PORT                 GPIOA
#define FLASH_SPI_SCK_GPIO_CLK                  RCC_AHB1Periph_GPIOA
#define FLASH_SPI_SCK_GPIO_CLK_INIT             RCC_AHB1PeriphClockCmd
#define FLASH_SPI_SCK_GPIO_PIN                  GPIO_Pin_5
#define FLASH_SPI_SCK_AF                        GPIO_AF_SPI1
#define FLASH_SPI_SCK_SOURCE                    GPIO_PinSource5

#define FLASH_SPI_MISO_GPIO_PORT                GPIOA
#define FLASH_SPI_MISO_GPIO_CLK                 RCC_AHB1Periph_GPIOA
#define FLASH_SPI_MISO_GPIO_CLK_INIT            RCC_AHB1PeriphClockCmd
#define FLASH_SPI_MISO_GPIO_PIN                 GPIO_Pin_6
#define FLASH_SPI_MISO_AF                       GPIO_AF_SPI1
#define FLASH_SPI_MISO_SOURCE                   GPIO_PinSource6

#define FLASH_SPI_MOSI_GPIO_PORT                GPIOA
#define FLASH_SPI_MOSI_GPIO_CLK                 RCC_AHB1Periph_GPIOA
#define FLASH_SPI_MOSI_GPIO_CLK_INIT            RCC_AHB1PeriphClockCmd
#define FLASH_SPI_MOSI_GPIO_PIN                 GPIO_Pin_7
#define FLASH_SPI_MOSI_AF                       GPIO_AF_SPI1
#define FLASH_SPI_MOSI_SOURCE                   GPIO_PinSource7

#define FLASH_SPI_CS_GPIO_PORT                  GPIOF
#define FLASH_SPI_CS_GPIO_CLK                   RCC_AHB1Periph_GPIOF
#define FLASH_SPI_CS_GPIO_CLK_INIT              RCC_AHB1PeriphClockCmd
#define FLASH_SPI_CS_GPIO_PIN                   GPIO_Pin_11

#define FLASH_SPI_CS_LOW                        GPIO_ResetBits(FLASH_SPI_CS_GPIO_PORT,FLASH_SPI_CS_GPIO_PIN)
#define FLASH_SPI_CS_HIGH                       GPIO_SetBits(FLASH_SPI_CS_GPIO_PORT,FLASH_SPI_CS_GPIO_PIN)
/************************************************************/

/*��Ϣ���*/
#define FLASH_DEBUG_ON         1
#define FLASH_INFO(fmt,arg...)           printf("<<-FLASH-INFO->> "fmt"\n",##arg)
#define FLASH_ERROR(fmt,arg...)          printf("<<-FLASH-ERROR->> "fmt"\n",##arg)
#define FLASH_DEBUG(fmt,arg...)          do{if(FLASH_DEBUG_ON)printf("<<-FLASH-DEBUG->> [%d]"fmt"\n",__LINE__, ##arg);}while(0)

static void FLASH_SPI_GPIO_Config(void);
static void FLASH_SPI_MODE_Config(void);
void FLASH_SPI_Init(void);

static uint8_t FLASH_Send_Byte(uint8_t byte);
static uint8_t FLASH_Receive_Byte(void);

uint8_t  FLASH_Read_DeviceID(void);
uint32_t FLASH_Read_FlashID(void);										  
void FLASH_Erase_Sectors(uint32_t addr);
void FLASH_Read_Data(uint8_t * data,uint32_t ReadAddr,uint16_t size);
void FLASH_Write_Page_v1(uint32_t addr,uint8_t * data,uint16_t size);
void FLASH_Write_Page_v2(uint32_t addr,uint8_t * data,uint16_t size);
void FLASH_Write_Page_v3(uint8_t * data,uint32_t WriteAddr,uint16_t size);
void FLASH_Write_Data(uint8_t * data, uint32_t WriteAddr, uint16_t size);

void FLASH_PowerDown(void);
void FLASH_Wakeup(void);
										  
static void Flash_Write_Enable(void);
static void Flash_Wait_For_Standby(void);
static uint8_t FLASH_Error_CallBack(uint8_t code);


#endif /* __BSP_SPI_FLASH_H */
