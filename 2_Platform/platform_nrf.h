#include "stm32f4xx.h"
#include "bsp_spi_nrf.h"
#include "bsp_usart_debug.h"



uint8_t nrf24l01_check(void);
void nrf24l01_apply_settings(uint8_t enabled, uint8_t channel,
                            uint8_t power_register, uint8_t data_rate);
uint8_t nrf24l01_read_runtime(uint8_t *enabled, uint8_t *channel,
                             uint8_t *power_register, uint8_t *data_rate);
void nrf24l01_receive(void);
void nrf24l01_show(void);
void nrf24l01_send(void);


