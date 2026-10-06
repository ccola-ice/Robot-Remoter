#ifndef UI_ANALOG_FILTER_H
#define UI_ANALOG_FILTER_H

#include <stdint.h>

/* 仅用于显示，菜单每 20 ms 更新一次。保留 ADC 数值的小数部分，
 * 使低通滤波在两个变化方向上均能收敛，避免整数截断造成偏差。 */
#define GUI_ANALOG_HYSTERESIS 8
#define GUI_NUMERIC_REFRESH_FRAMES 10U /* 仅用于远端遥测显示，周期为 200 ms。 */

/* 每个 ADC 通道独立保存 Q8 平滑累积值和最后一次对外显示值，二者不能混作同一状态。 */
typedef struct
{
    int32_t value_q8;
    uint16_t stable;
} GuiAnalogFilter;

/* raw 为 12 位 ADC 输入；切页首次调用时 reset 非零，从当前样本初始化而不沿用旧滤波历史。 */
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
    /* 输出迟滞屏蔽小幅数值跳动，但量程两端直接更新，保证能显示到达端点。 */
    if(delta >= GUI_ANALOG_HYSTERESIS || delta <= -GUI_ANALOG_HYSTERESIS ||
       rounded == 0L || rounded == 4095L)
        filter->stable = (uint16_t)rounded;
    return filter->stable;
}

#endif
