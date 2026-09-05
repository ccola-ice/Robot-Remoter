#ifndef TOUCH_TEST_I2C_H
#define TOUCH_TEST_I2C_H
#include <stdint.h>
#define GTP_ADDRESS 0xBAU
void I2C_Touch_Init(void);
void I2C_ResetChip(void);
void I2C_GTP_IRQDisable(void);
void I2C_GTP_IRQEnable(void);
void I2C_GTP_SetInterruptTrigger(uint8_t trigger);
uint32_t I2C_WriteBytes(uint8_t addr, uint8_t *data, uint8_t size);
uint32_t I2C_ReadBytes(uint8_t addr, uint8_t *data, uint16_t size);
#endif
