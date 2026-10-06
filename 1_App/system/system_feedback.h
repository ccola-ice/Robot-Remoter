#ifndef SYSTEM_FEEDBACK_H
#define SYSTEM_FEEDBACK_H

/* 按键音独立于长按连发；调用者只在实际按键事件时触发。
 * 按设置决定是否发声，重复调用会重新开始短声计时；GPIO 在首次发声时初始化。 */
void system_key_beep(void);
/* 进入硬件诊断前停止短声，取消计时并拉低 PA15，防止节拍中断继续翻转引脚。
 * 不改变引脚模式；需要临时配置蜂鸣器的诊断代码负责保存和恢复 GPIO 设置。 */
void system_key_beep_stop(void);
/* 由 1 ms 中断调用，只更新计时和 GPIO，不阻塞主循环。 */
void system_settings_tick_1ms(void);

#endif
