#ifndef SYSTEM_SETTINGS_H
#define SYSTEM_SETTINGS_H

/* 按键音独立于长按连发；调用者只在实际按键事件时触发。 */
void system_key_beep(void);
/* 进入硬件诊断前停止短声，释放 PA15 的时序控制。 */
void system_key_beep_stop(void);
/* 由 1 ms 中断调用，只更新计时和 GPIO，不阻塞主循环。 */
void system_settings_tick_1ms(void);

#endif
