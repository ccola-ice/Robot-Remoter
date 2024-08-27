#ifndef __BSP_I2C_TOUCH_H
#define	__BSP_I2C_TOUCH_H
#include "stm32f4xx.h"

/* STM32 I2C ����ģʽ */
#define I2C_Speed              400000

/* �����ַֻҪ��STM32��ҵ�I2C������ַ��һ������ */
#define I2C_OWN_ADDRESS7       0xA0

/*�趨ʹ�õĵ�����IIC�豸��ַ*/
#define GTP_ADDRESS            0xBA

#define I2CT_FLAG_TIMEOUT         ((uint32_t)0x1000)
#define I2CT_LONG_TIMEOUT         ((uint32_t)(10 * I2CT_FLAG_TIMEOUT))

/*I2C����*/
#define GTP_I2C_SCL_PIN                  GPIO_Pin_4                 
#define GTP_I2C_SCL_GPIO_PORT            GPIOC                       
#define GTP_I2C_SCL_GPIO_CLK             RCC_AHB1Periph_GPIOC
#define GTP_I2C_SCL_SOURCE               GPIO_PinSource4

#define GTP_I2C_SDA_PIN                  GPIO_Pin_5                 
#define GTP_I2C_SDA_GPIO_PORT            GPIOC                     
#define GTP_I2C_SDA_GPIO_CLK             RCC_AHB1Periph_GPIOC
#define GTP_I2C_SDA_SOURCE               GPIO_PinSource5

/*��λ����*/
#define GTP_RST_GPIO_PORT                GPIOG
#define GTP_RST_GPIO_CLK                 RCC_AHB1Periph_GPIOG
#define GTP_RST_GPIO_PIN                 GPIO_Pin_8
/*�ж�����*/
#define GTP_INT_GPIO_PORT                GPIOB
#define GTP_INT_GPIO_CLK                 RCC_AHB1Periph_GPIOB
#define GTP_INT_GPIO_PIN                 GPIO_Pin_12
#define GTP_INT_EXTI_PORTSOURCE          EXTI_PortSourceGPIOB
#define GTP_INT_EXTI_PINSOURCE           EXTI_PinSource12
#define GTP_INT_EXTI_LINE                EXTI_Line12
#define GTP_INT_EXTI_IRQ                 EXTI15_10_IRQn
#define GTP_IRQHandler                   EXTI15_10_IRQHandler


//���IICʹ�õĺ�
#define I2C_SCL_1()  GPIO_SetBits(GTP_I2C_SCL_GPIO_PORT, GTP_I2C_SCL_PIN)		/* SCL = 1 */
#define I2C_SCL_0()  GPIO_ResetBits(GTP_I2C_SCL_GPIO_PORT, GTP_I2C_SCL_PIN)		/* SCL = 0 */

#define I2C_SDA_1()  GPIO_SetBits(GTP_I2C_SDA_GPIO_PORT, GTP_I2C_SDA_PIN)		/* SDA = 1 */
#define I2C_SDA_0()  GPIO_ResetBits(GTP_I2C_SDA_GPIO_PORT, GTP_I2C_SDA_PIN)		/* SDA = 0 */

#define I2C_SDA_READ()  GPIO_ReadInputDataBit(GTP_I2C_SDA_GPIO_PORT, GTP_I2C_SDA_PIN)	/* ��SDA����״̬ */

//�����ӿ�
void I2C_Touch_Init(void);
uint32_t I2C_WriteBytes(uint8_t ClientAddr,uint8_t* pBuffer,  uint8_t NumByteToWrite);
uint32_t I2C_ReadBytes(uint8_t ClientAddr,uint8_t* pBuffer, uint16_t NumByteToRead);
void I2C_ResetChip(void);
void I2C_GTP_IRQDisable(void);
void I2C_GTP_IRQEnable(void);

#endif /* __I2C_TOUCH_H */
