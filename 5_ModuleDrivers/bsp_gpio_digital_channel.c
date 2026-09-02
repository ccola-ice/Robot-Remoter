#include "bsp_gpio_digital_channel.h"

#define DIGITAL_CHANNEL_DEBOUNCE_SAMPLES 3U

static GPIO_TypeDef * const channel_port[DIGITAL_CHANNEL_COUNT] =
{
    DCH1_GPIO_PORT, DCH2_GPIO_PORT, DCH3_GPIO_PORT,
    DCH4_GPIO_PORT, DCH5_GPIO_PORT, DCH6_GPIO_PORT
};

static const uint16_t channel_pin[DIGITAL_CHANNEL_COUNT] =
{
    DCH1_GPIO_PIN, DCH2_GPIO_PIN, DCH3_GPIO_PIN,
    DCH4_GPIO_PIN, DCH5_GPIO_PIN, DCH6_GPIO_PIN
};

static const DigitalChannelType channel_type[DIGITAL_CHANNEL_COUNT] =
{
    DIGITAL_CHANNEL_BUTTON,
    DIGITAL_CHANNEL_TOGGLE,
    DIGITAL_CHANNEL_BUTTON,
    DIGITAL_CHANNEL_BUTTON,
    DIGITAL_CHANNEL_TOGGLE,
    DIGITAL_CHANNEL_TOGGLE
};

static uint8_t channel_raw[DIGITAL_CHANNEL_COUNT];
static uint8_t channel_candidate[DIGITAL_CHANNEL_COUNT];
static uint8_t channel_stable[DIGITAL_CHANNEL_COUNT];
static uint8_t channel_candidate_count[DIGITAL_CHANNEL_COUNT];

static uint8_t digital_channel_read_pin(uint8_t channel_index)
{
    return (GPIO_ReadInputDataBit(channel_port[channel_index],
                                  channel_pin[channel_index]) != Bit_RESET) ?
           1U : 0U;
}

void digital_channel_init(void)
{
    GPIO_InitTypeDef gpio;
    uint8_t channel_index;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA |
                           RCC_AHB1Periph_GPIOD |
                           RCC_AHB1Periph_GPIOE, ENABLE);

    gpio.GPIO_Mode = GPIO_Mode_IN;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;

    gpio.GPIO_Pin = GPIO_Pin_8;
    GPIO_Init(GPIOA, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_Init(GPIOD, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_Init(GPIOE, &gpio);

    for(channel_index = 0U;
        channel_index < DIGITAL_CHANNEL_COUNT;
        channel_index++)
    {
        channel_raw[channel_index] = digital_channel_read_pin(channel_index);
        channel_candidate[channel_index] = channel_raw[channel_index];
        channel_stable[channel_index] = channel_raw[channel_index];
        channel_candidate_count[channel_index] =
            DIGITAL_CHANNEL_DEBOUNCE_SAMPLES;
    }
}

void digital_channel_update_10ms(void)
{
    uint8_t channel_index;
    uint8_t sample;

    for(channel_index = 0U;
        channel_index < DIGITAL_CHANNEL_COUNT;
        channel_index++)
    {
        sample = digital_channel_read_pin(channel_index);
        channel_raw[channel_index] = sample;

        if(sample != channel_candidate[channel_index])
        {
            channel_candidate[channel_index] = sample;
            channel_candidate_count[channel_index] = 1U;
        }
        else if(channel_candidate_count[channel_index] <
                DIGITAL_CHANNEL_DEBOUNCE_SAMPLES)
        {
            channel_candidate_count[channel_index]++;
        }

        if(channel_candidate_count[channel_index] >=
           DIGITAL_CHANNEL_DEBOUNCE_SAMPLES)
        {
            channel_stable[channel_index] = channel_candidate[channel_index];
        }
    }
}

uint8_t digital_channel_get_raw(uint8_t channel_index)
{
    if(channel_index >= DIGITAL_CHANNEL_COUNT)
    {
        return 0U;
    }
    return channel_raw[channel_index];
}

uint8_t digital_channel_get_stable(uint8_t channel_index)
{
    if(channel_index >= DIGITAL_CHANNEL_COUNT)
    {
        return 0U;
    }
    return channel_stable[channel_index];
}

DigitalChannelType digital_channel_get_type(uint8_t channel_index)
{
    if(channel_index >= DIGITAL_CHANNEL_COUNT)
    {
        return DIGITAL_CHANNEL_BUTTON;
    }
    return channel_type[channel_index];
}

void digital_channel_get_snapshot(uint8_t raw_values[DIGITAL_CHANNEL_COUNT],
                                  uint8_t stable_values[DIGITAL_CHANNEL_COUNT])
{
    uint8_t channel_index;

    for(channel_index = 0U;
        channel_index < DIGITAL_CHANNEL_COUNT;
        channel_index++)
    {
        raw_values[channel_index] = channel_raw[channel_index];
        stable_values[channel_index] = channel_stable[channel_index];
    }
}
