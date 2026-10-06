#ifndef CHANNEL_INPUT_H
#define CHANNEL_INPUT_H
#include <stdint.h>

/* 将 12 位 ADC 输入换算为 -1000..1000 的控制量，trim 与返回值使用相同刻度，
 * reverse 非零表示反向；原始值或校准参数非法时返回 0，不在此处修改参数。
 * 控制链路与通道监视共用此计算，不应用界面显示滤波；零也可代表正常中位。 */
static __inline int16_t channel_input_normalize(uint16_t raw, uint16_t lower,
    uint16_t middle, uint16_t upper, int32_t trim, uint8_t reverse)
{
    int32_t value;
    if(raw > 4095U || lower >= middle || middle >= upper || upper > 4095U ||
       trim < -1000L || trim > 1000L) return 0;
    /* 以校准中点分段缩放，允许摇杆两侧行程不对称，统一输出到 ±1000。 */
    if(raw >= middle) value = ((int32_t)raw - middle) * 1000L / (upper - middle);
    else value = -((int32_t)middle - raw) * 1000L / (middle - lower);
    value += trim;
    if(reverse) value = -value;
    if(value > 1000L) value = 1000L;
    if(value < -1000L) value = -1000L;
    /* 微调和反向处理后再应用 ±5% 中位死区，同时供运动控制和回中判定使用。 */
    if(value >= -50L && value <= 50L) value = 0L;
    return (int16_t)value;
}
#endif
