#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../1_App/system/system_feedback.c"
#include "backlight_driver.inc"

volatile param_Config param;
RCC_TypeDef test_rcc;
GPIO_TypeDef test_gpioa, test_gpiof;
TIM_TypeDef test_tim14;
static uint32_t irq_mask, pclk1 = 42000000UL;
static unsigned beep_level, beep_writes, beep_init, pwm_init;
static unsigned ahb_flags, apb_flags, compare;
static TIM_TimeBaseInitTypeDef configured_base;
static TIM_OCInitTypeDef configured_output;

uint32_t __get_PRIMASK(void) { return irq_mask; }
void __disable_irq(void) { irq_mask = 1U; }
void __set_PRIMASK(uint32_t value) { irq_mask = value; }
void RCC_AHB1PeriphClockCmd(uint32_t clock, FunctionalState state)
{
    assert(state == ENABLE); ahb_flags |= clock;
}
void RCC_APB1PeriphClockCmd(uint32_t clock, FunctionalState state)
{
    assert(state == ENABLE); apb_flags |= clock;
}
void RCC_GetClocksFreq(RCC_ClocksTypeDef *clocks) { clocks->PCLK1_Frequency = pclk1; }
void GPIO_StructInit(GPIO_InitTypeDef *gpio) { memset(gpio, 0, sizeof(*gpio)); }
void GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *gpio)
{
    assert(gpio->GPIO_OType == GPIO_OType_PP && gpio->GPIO_PuPd == GPIO_PuPd_NOPULL);
    assert(gpio->GPIO_Speed == GPIO_Speed_25MHz);
    if(port == GPIOA) {
        assert(irq_mask == 1U);
        assert(gpio->GPIO_Pin == GPIO_Pin_15 && gpio->GPIO_Mode == GPIO_Mode_OUT);
        beep_init++;
    } else {
        assert(port == GPIOF);
        assert(gpio->GPIO_Pin == GPIO_Pin_9 && gpio->GPIO_Mode == GPIO_Mode_AF);
    }
}
void GPIO_SetBits(GPIO_TypeDef *port, uint16_t pin)
{
    assert(port == GPIOA && pin == GPIO_Pin_15);
    beep_level = 1U; beep_writes++;
}
void GPIO_ResetBits(GPIO_TypeDef *port, uint16_t pin)
{
    assert(port == GPIOA && pin == GPIO_Pin_15);
    beep_level = 0U; beep_writes++;
}
void GPIO_PinAFConfig(GPIO_TypeDef *port, uint16_t pin, uint16_t af)
{
    assert(port == GPIOF && pin == GPIO_PinSource9 && af == GPIO_AF_TIM14);
}
void TIM_TimeBaseStructInit(TIM_TimeBaseInitTypeDef *base) { memset(base, 0, sizeof(*base)); }
void TIM_TimeBaseInit(TIM_TypeDef *timer, TIM_TimeBaseInitTypeDef *base)
{
    assert(timer == TIM14); configured_base = *base; pwm_init++;
}
void TIM_OCStructInit(TIM_OCInitTypeDef *output) { memset(output, 0, sizeof(*output)); }
void TIM_OC1Init(TIM_TypeDef *timer, TIM_OCInitTypeDef *output)
{
    assert(timer == TIM14); configured_output = *output;
}
void TIM_OC1PreloadConfig(TIM_TypeDef *timer, uint16_t state)
{
    assert(timer == TIM14 && state == TIM_OCPreload_Enable);
}
void TIM_ARRPreloadConfig(TIM_TypeDef *timer, FunctionalState state)
{
    assert(timer == TIM14 && state == ENABLE);
}
void TIM_GenerateEvent(TIM_TypeDef *timer, uint16_t event)
{
    assert(timer == TIM14 && event == TIM_EventSource_Update);
}
void TIM_Cmd(TIM_TypeDef *timer, FunctionalState state)
{
    assert(timer == TIM14 && state == ENABLE);
}
void TIM_SetCompare1(TIM_TypeDef *timer, uint16_t value)
{
    assert(timer == TIM14 && value <= 1000U); compare = value;
}

static void test_beep(void)
{
    unsigned tick, writes;
    system_settings_tick_1ms();
    system_key_beep_stop();
    assert(beep_writes == 0U);
    param.keySound = OFF;
    system_key_beep();
    assert(beep_writes == 0U && beep_init == 0U && irq_mask == 0U);
    param.keySound = ON;
    system_key_beep();
    assert(beep_init == 1U && beep_level == 1U && irq_mask == 0U);
    for(tick = 1U; tick <= 40U; tick++) {
        system_settings_tick_1ms();
        assert(beep_level == (tick == 40U ? 0U : !(tick & 1U)));
    }
    writes = beep_writes;
    for(tick = 0U; tick < 100U; tick++) system_settings_tick_1ms();
    assert(beep_writes == writes && beep_level == 0U);
    system_key_beep();
    for(tick = 0U; tick < 20U; tick++) system_settings_tick_1ms();
    system_key_beep();
    assert(key_beep_ticks == 40U && beep_init == 1U);
    irq_mask = 1U;
    system_key_beep_stop();
    assert(irq_mask == 1U && key_beep_ticks == 0U && beep_level == 0U);
    irq_mask = 0U;
    writes = beep_writes;
    system_settings_tick_1ms();
    assert(writes == beep_writes); /* 诊断运行时不会再写蜂鸣器引脚。 */
    system_key_beep();
    param.keySound = OFF;
    system_key_beep();
    assert(key_beep_ticks == 0U && beep_level == 0U);
    assert((ahb_flags & RCC_AHB1Periph_GPIOA) != 0U);
}

static void test_backlight(void)
{
    test_rcc.CFGR = 5UL << 10;
    LCD_SetBrightness(100U);
    assert(pwm_init == 1U && configured_base.TIM_Prescaler == 83U);
    assert(configured_base.TIM_Period == 999U);
    assert(configured_output.TIM_OCMode == TIM_OCMode_PWM1);
    assert(configured_output.TIM_OutputState == TIM_OutputState_Enable);
    assert(configured_output.TIM_OCPolarity == TIM_OCPolarity_High);
    assert(compare == 1000U && LCD_GetBrightness() == 100U);
    LCD_SetBrightness(35U);
    assert(compare == 350U && LCD_GetBrightness() == 35U && pwm_init == 1U);
    ILI9806G_BackLed_Control(DISABLE);
    assert(compare == 0U && LCD_GetBrightness() == 0U);
    ILI9806G_BackLed_Control(ENABLE);
    assert(compare == 350U && LCD_GetBrightness() == 35U);
    LCD_SetBrightness(10U);
    assert(compare == 100U && LCD_GetBrightness() == 10U);
    LCD_SetBrightness(255U);
    assert(compare == 1000U && LCD_GetBrightness() == 100U);
    assert((ahb_flags & RCC_AHB1Periph_GPIOF) != 0U);
    assert(apb_flags == RCC_APB1Periph_TIM14);
    /* APB1 不分频时，定时器不能误用两倍总线时钟。 */
    lcd_backlight_pwm_ready = 0U;
    test_rcc.CFGR = 0U;
    pclk1 = 16000000UL;
    LCD_SetBrightness(50U);
    assert(configured_base.TIM_Prescaler == 15U && compare == 500U);
}

int main(void)
{
    test_beep();
    test_backlight();
    puts("system settings: PWM duty, APB1 clocks, mute and nonblocking beep passed");
    return 0;
}
