#ifndef GUI_ANALOG_FILTER_H
#define GUI_ANALOG_FILTER_H

#include <stdint.h>

/* Display only: menu updates arrive every 50 ms. Keep fractional ADC bits
 * so the low-pass filter converges in both directions without integer bias. */
#define GUI_ANALOG_HYSTERESIS 8
#define GUI_NUMERIC_REFRESH_FRAMES 4U

typedef struct
{
    int32_t value_q8;
    uint16_t stable;
} GuiAnalogFilter;

static uint16_t gui_analog_filter_update(GuiAnalogFilter *filter,
                                         uint16_t raw, uint8_t reset)
{
    int32_t delta;
    int32_t rounded;
    if(raw > 4095U) raw = 4095U;
    if(reset != 0U)
    {
        filter->value_q8 = (int32_t)raw * 256L;
        filter->stable = raw;
        return raw;
    }
    delta = (int32_t)raw * 256L - filter->value_q8;
    /* Follow intentional movement faster; smooth small changes more heavily. */
    filter->value_q8 += delta / ((delta > 32768L || delta < -32768L) ? 2L : 4L);
    rounded = (filter->value_q8 + 128L) / 256L;
    delta = rounded - filter->stable;
    if(delta >= GUI_ANALOG_HYSTERESIS || delta <= -GUI_ANALOG_HYSTERESIS ||
       rounded == 0L || rounded == 4095L)
        filter->stable = (uint16_t)rounded;
    return filter->stable;
}

#endif
