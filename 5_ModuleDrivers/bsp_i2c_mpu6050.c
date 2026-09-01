#include "bsp_i2c_mpu6050.h"
#include "bsp_Systick.h"

void IIC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    IIC_SCL = 1;
    IIC_SDA = 1;
    Delay_us(5U);
    IIC_Bus_Recovery();
}

void IIC_Bus_Recovery(void)
{
    u8 pulse;

    /* Clock a slave out of an interrupted byte, then generate STOP. */
    SDA_IN();
    IIC_SCL = 1;
    Delay_us(5U);
    for(pulse = 0U; (pulse < 9U) && (READ_SDA == 0U); pulse++)
    {
        IIC_SCL = 0;
        Delay_us(5U);
        IIC_SCL = 1;
        Delay_us(5U);
    }
    SDA_OUT();
    IIC_Stop();
}

void IIC_Start(void)
{
    SDA_OUT();
    IIC_SDA = 1;
    IIC_SCL = 1;
    Delay_us(4U);
    IIC_SDA = 0;
    Delay_us(4U);
    IIC_SCL = 0;
}

void IIC_Stop(void)
{
    SDA_OUT();
    IIC_SCL = 0;
    IIC_SDA = 0;
    Delay_us(4U);
    IIC_SCL = 1;
    IIC_SDA = 1;
    Delay_us(4U);
}

u8 IIC_Wait_Ack(void)
{
    u16 timeout = 0U;

    SDA_IN();
    IIC_SDA = 1;
    Delay_us(1U);
    IIC_SCL = 1;
    Delay_us(1U);
    while(READ_SDA != 0U)
    {
        if(timeout++ >= 250U)
        {
            IIC_Stop();
            return 1U;
        }
        Delay_us(1U);
    }
    IIC_SCL = 0;
    return 0U;
}

void IIC_Ack(void)
{
    IIC_SCL = 0;
    SDA_OUT();
    IIC_SDA = 0;
    Delay_us(2U);
    IIC_SCL = 1;
    Delay_us(2U);
    IIC_SCL = 0;
}

void IIC_NAck(void)
{
    IIC_SCL = 0;
    SDA_OUT();
    IIC_SDA = 1;
    Delay_us(2U);
    IIC_SCL = 1;
    Delay_us(2U);
    IIC_SCL = 0;
}

void IIC_Send_Byte(u8 data)
{
    u8 bit_index;

    SDA_OUT();
    IIC_SCL = 0;
    for(bit_index = 0U; bit_index < 8U; bit_index++)
    {
        IIC_SDA = (data & 0x80U) >> 7;
        data <<= 1;
        Delay_us(2U);
        IIC_SCL = 1;
        Delay_us(2U);
        IIC_SCL = 0;
        Delay_us(2U);
    }
}

u8 IIC_Read_Byte(unsigned char acknowledge)
{
    u8 bit_index;
    u8 value = 0U;

    SDA_IN();
    for(bit_index = 0U; bit_index < 8U; bit_index++)
    {
        IIC_SCL = 0;
        Delay_us(2U);
        IIC_SCL = 1;
        value <<= 1;
        if(READ_SDA != 0U)
        {
            value++;
        }
        Delay_us(1U);
    }
    if(acknowledge == 0U)
    {
        IIC_NAck();
    }
    else
    {
        IIC_Ack();
    }
    return value;
}
