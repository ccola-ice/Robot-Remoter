#ifndef __MYIIC_H
#define __MYIIC_H

#include "common.h"

/* PB10: SCL, PB11: SDA. */
#define SDA_IN()  {GPIOB->MODER &= ~(3U << (11U * 2U));}
#define SDA_OUT() {GPIOB->MODER &= ~(3U << (11U * 2U)); GPIOB->MODER |= 1U << (11U * 2U);}
#define IIC_SCL   PBout(10)
#define IIC_SDA   PBout(11)
#define READ_SDA  PBin(11)

void IIC_Init(void);
void IIC_Bus_Recovery(void);
void IIC_Start(void);
void IIC_Stop(void);
void IIC_Send_Byte(u8 data);
u8 IIC_Read_Byte(unsigned char acknowledge);
u8 IIC_Wait_Ack(void);
void IIC_Ack(void);
void IIC_NAck(void);

void IIC_Write_One_Byte(u8 device_address, u8 address, u8 data);
u8 IIC_Read_One_Byte(u8 device_address, u8 address);

#endif
