#ifndef __BSP_GPIO_DIGITAL_CHANNEL_H
#define __BSP_GPIO_DIGITAL_CHANNEL_H

#include "stm32f4xx.h"

#define DIGITAL_CHANNEL_COUNT       6U
#define DIGITAL_CHANNEL_DEBOUNCE_MS 30U
#define DIGITAL_CHANNEL_BUTTON_ACTIVE_LEVEL 0U

#define DCH1_GPIO_PORT GPIOD
#define DCH1_GPIO_PIN  GPIO_Pin_6
#define DCH2_GPIO_PORT GPIOD
#define DCH2_GPIO_PIN  GPIO_Pin_3
#define DCH3_GPIO_PORT GPIOA
#define DCH3_GPIO_PIN  GPIO_Pin_8
#define DCH4_GPIO_PORT GPIOD
#define DCH4_GPIO_PIN  GPIO_Pin_7
#define DCH5_GPIO_PORT GPIOE
#define DCH5_GPIO_PIN  GPIO_Pin_3
#define DCH6_GPIO_PORT GPIOE
#define DCH6_GPIO_PIN  GPIO_Pin_2

typedef enum
{
    DIGITAL_CHANNEL_BUTTON = 0,
    DIGITAL_CHANNEL_TOGGLE
} DigitalChannelType;

/* Schematic mapping: DCH1..DCH6 = PD6, PD3, PA8, PD7, PE3, PE2. */
void digital_channel_init(void);

/* Call every 10 ms. Raw values update immediately; stable values debounce. */
void digital_channel_update_10ms(void);

/* Channel index is zero based: 0 means DCH1 and 5 means DCH6. */
uint8_t digital_channel_get_raw(uint8_t channel_index);
uint8_t digital_channel_get_stable(uint8_t channel_index);
DigitalChannelType digital_channel_get_type(uint8_t channel_index);

void digital_channel_get_snapshot(uint8_t raw_values[DIGITAL_CHANNEL_COUNT],
                                  uint8_t stable_values[DIGITAL_CHANNEL_COUNT]);

#endif
