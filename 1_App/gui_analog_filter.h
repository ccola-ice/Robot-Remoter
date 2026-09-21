#ifndef GUI_ANALOG_FILTER_H
#define GUI_ANALOG_FILTER_H

#include <stdint.h>

/* 仅用于显示，菜单每 20 ms 更新一次。保留 ADC 数值的小数部分，
 * 使低通滤波在两个变化方向上均能收敛，避免整数截断造成偏差。 */
#define GUI_ANALOG_HYSTERESIS 8
#define GUI_NUMERIC_REFRESH_FRAMES 10U /* 仅用于远端遥测显示，周期为 200 ms。 */

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
    /* 明显的操纵变化必须在当前帧显示，不等待多帧逐步收敛。
     * 仅对噪声幅度范围内的小变化进行低通平滑。 */
    if(delta >= 8192L || delta <= -8192L)
        filter->value_q8 = (int32_t)raw * 256L;
    else
        filter->value_q8 += delta / ((delta >= 4096L || delta <= -4096L) ? 2L : 4L);
    rounded = (filter->value_q8 + 128L) / 256L;
    delta = rounded - filter->stable;
    if(delta >= GUI_ANALOG_HYSTERESIS || delta <= -GUI_ANALOG_HYSTERESIS ||
       rounded == 0L || rounded == 4095L)
        filter->stable = (uint16_t)rounded;
    return filter->stable;
}

#endif
