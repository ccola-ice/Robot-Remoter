#ifndef GUI_ROBOT_FILTER_H
#define GUI_ROBOT_FILTER_H
#include <stdint.h>

/* 仅用于显示滤波。三点中值滤波抑制单次 ADC 尖峰，再平滑小幅变化；
 * 对明显的操纵变化，在两帧内跟随。 */
typedef struct {
    uint16_t history[3], stable;
    int32_t value_q8;
    uint8_t next;
} GuiRobotFilter;

static __inline uint16_t gui_robot_filter_update(GuiRobotFilter *f,
                                                uint16_t raw, uint8_t reset)
{
    uint16_t a, b, c, median;
    int32_t delta, rounded;
    if(raw > 4095U) raw = 4095U;
    if(reset) {
        f->history[0] = f->history[1] = f->history[2] = f->stable = raw;
        f->next = 0U;
        f->value_q8 = (int32_t)raw * 256L;
        return raw;
    }
    f->history[f->next] = raw;
    f->next = (uint8_t)((f->next + 1U) % 3U);
    a = f->history[0]; b = f->history[1]; c = f->history[2];
    median = a > b ? (b > c ? b : (a > c ? c : a))
                   : (a > c ? a : (b > c ? c : b));
    delta = (int32_t)median * 256L - f->value_q8;
    if(delta >= 65536L || delta <= -65536L) f->value_q8 = (int32_t)median * 256L;
    else f->value_q8 += delta / 4L;
    rounded = (f->value_q8 + 128L) / 256L;
    delta = rounded - f->stable;
    if(delta >= 12L || delta <= -12L || rounded == 0L || rounded == 4095L)
        f->stable = (uint16_t)rounded;
    return f->stable;
}
#endif
