#ifndef CHANNEL_INPUT_H
#define CHANNEL_INPUT_H
#include <stdint.h>

/* Same calibrated input for the control path and its monitor. No UI filter. */
static __inline int16_t channel_input_normalize(uint16_t raw, uint16_t lower,
    uint16_t middle, uint16_t upper, int32_t trim, uint8_t reverse)
{
    int32_t value;
    if(raw > 4095U || lower >= middle || middle >= upper || upper > 4095U ||
       trim < -1000L || trim > 1000L) return 0;
    if(raw >= middle) value = ((int32_t)raw - middle) * 1000L / (upper - middle);
    else value = -((int32_t)middle - raw) * 1000L / (middle - lower);
    value += trim;
    if(reverse) value = -value;
    if(value > 1000L) value = 1000L;
    if(value < -1000L) value = -1000L;
    if(value >= -50L && value <= 50L) value = 0L;
    return (int16_t)value;
}
#endif
