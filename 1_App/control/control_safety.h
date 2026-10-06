#ifndef CONTROL_SAFETY_H
#define CONTROL_SAFETY_H
#include <stdint.h>

/* 使能流程的持久状态：neutral_since 记录回中计时起点，neutral_tracking 表示正在计时，
 * ready 表示允许下一次按键使能。
 * armed 在持续按键时保持；实例需先清零，并由同一控制循环维护。 */
typedef struct {
    uint32_t neutral_since;
    uint8_t neutral_tracking, ready, armed;
} ControlSafety;

/* 根据当前输入推进使能状态并返回 armed：now 为毫秒时刻，healthy 为外部汇总健康条件，
 * pressed 为使能键有效状态，neutral 表示所有运动轴均在中位死区内。
 * 发生故障、离开控制页面或缺少 ACK 后，都必须重新执行使能流程。
 * 上电或重新连接期间一直按住使能键，不会使车辆进入运动使能状态。 */
static __inline uint8_t control_safety_step(ControlSafety *s, uint32_t now,
                                          uint8_t healthy, uint8_t pressed,
                                          uint8_t neutral)
{
    if(!healthy) {
        s->neutral_tracking = s->ready = s->armed = 0U;
        return 0U;
    }
    if(!pressed) {
        /* 松键当轮立即停止输出，再根据中位条件判断是否可以重新准备使能。 */
        s->armed = 0U;
        /* 松开使能键且持续回中 500 ms 才进入就绪；中途偏离中心会重新计时。 */
        if(neutral) {
            if(!s->neutral_tracking) { s->neutral_since = now; s->neutral_tracking = 1U; }
            if((uint32_t)(now - s->neutral_since) >= 500U) s->ready = 1U;
        } else {
            s->neutral_tracking = s->ready = 0U;
        }
    } else {
        /* 就绪后在回中位置按键才能使能；使能后保持按键即可操作摇杆。 */
        if(!s->armed && s->ready && neutral) s->armed = 1U;
        s->neutral_tracking = s->ready = 0U;
    }
    return s->armed;
}
#endif
