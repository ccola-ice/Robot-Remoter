#include "system_feedback.h"
#include "app_config.h"

#define SYSTEM_KEY_BEEP_MS 40U

/* ticks 同时由按键事件和 1 ms 中断访问；ready 表示 PA15 是否已完成惰性初始化。 */
static volatile uint8_t key_beep_ticks;
static uint8_t key_beep_ready;

/* 按键事件只启动/续接短声，实际翻转由节拍完成，使事件处理不等待发声结束。 */
void system_key_beep(void)
{
    uint32_t interrupt_mask;
    if(param.keySound == 0U) {
        system_key_beep_stop();
        return;
    }
    /* 与 1 ms 节拍共同访问蜂鸣器状态，更新时屏蔽中断并保留调用前的屏蔽状态。 */
    interrupt_mask = __get_PRIMASK();
    __disable_irq();
    if(key_beep_ready == 0U) {
        GPIO_InitTypeDef gpio;
        /* PA15 为无源蜂鸣器，拉低为静音；与硬件测试的接线一致。 */
        RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
        GPIO_ResetBits(GPIOA, GPIO_Pin_15);
        GPIO_StructInit(&gpio);
        gpio.GPIO_Pin = GPIO_Pin_15;
        gpio.GPIO_Mode = GPIO_Mode_OUT;
        gpio.GPIO_OType = GPIO_OType_PP;
        gpio.GPIO_Speed = GPIO_Speed_25MHz;
        gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
        GPIO_Init(GPIOA, &gpio);
        key_beep_ready = 1U;
    }
    GPIO_SetBits(GPIOA, GPIO_Pin_15);
    key_beep_ticks = SYSTEM_KEY_BEEP_MS;
    __set_PRIMASK(interrupt_mask);
}

/* 取消计时和静音放在同一临界区，避免中断在停止过程中重新改变输出电平。 */
void system_key_beep_stop(void)
{
    uint32_t interrupt_mask = __get_PRIMASK();
    __disable_irq();
    key_beep_ticks = 0U;
    if(key_beep_ready != 0U) GPIO_ResetBits(GPIOA, GPIO_Pin_15);
    __set_PRIMASK(interrupt_mask);
}

void system_settings_tick_1ms(void)
{
    if(key_beep_ticks == 0U) return;
    key_beep_ticks--;
    /* 每毫秒翻转形成 500 Hz 短声；结束后固定低电平，无忙等延时。 */
    if(key_beep_ticks == 0U || (key_beep_ticks & 1U) != 0U)
        GPIO_ResetBits(GPIOA, GPIO_Pin_15);
    else
        GPIO_SetBits(GPIOA, GPIO_Pin_15);
}
