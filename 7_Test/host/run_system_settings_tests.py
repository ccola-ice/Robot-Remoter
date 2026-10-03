from pathlib import Path
import subprocess
import sys
import tempfile

host = Path(__file__).resolve().parent
root = host.parents[1]
compiler = sys.argv[1] if len(sys.argv) > 1 else 'gcc'
with tempfile.TemporaryDirectory(prefix='remoter-system-settings-') as folder:
    tmp = Path(folder)
    (tmp / 'stm32f4xx.h').write_text('''#ifndef STM32_TEST_H
#define STM32_TEST_H
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef enum { DISABLE = 0, ENABLE = 1 } FunctionalState;
typedef struct { uint32_t CFGR; } RCC_TypeDef;
typedef struct { uint32_t tag; } GPIO_TypeDef;
typedef struct { uint32_t tag; } TIM_TypeDef;
extern RCC_TypeDef test_rcc;
extern GPIO_TypeDef test_gpioa, test_gpiof;
extern TIM_TypeDef test_tim14;
#define RCC (&test_rcc)
#define GPIOA (&test_gpioa)
#define GPIOF (&test_gpiof)
#define TIM14 (&test_tim14)
#define GPIO_Pin_9 0x0200U
#define GPIO_Pin_15 0x8000U
#define GPIO_Mode_OUT 1U
#define GPIO_Mode_AF 2U
#define GPIO_OType_PP 0U
#define GPIO_PuPd_NOPULL 0U
#define GPIO_Speed_25MHz 1U
#define GPIO_PinSource9 9U
#define GPIO_AF_TIM14 9U
#define RCC_AHB1Periph_GPIOA 1U
#define RCC_AHB1Periph_GPIOF 32U
#define RCC_APB1Periph_TIM14 256U
#define RCC_CFGR_PPRE1 0x1c00U
#define TIM_CounterMode_Up 0U
#define TIM_CKD_DIV1 0U
#define TIM_OCMode_PWM1 0x60U
#define TIM_OutputState_Enable 1U
#define TIM_OCPolarity_High 0U
#define TIM_OCPreload_Enable 8U
#define TIM_EventSource_Update 1U
#define ILI9806G_BK_CLK RCC_AHB1Periph_GPIOF
#define ILI9806G_BK_PORT GPIOF
#define ILI9806G_BK_PIN GPIO_Pin_9
typedef struct {
    uint32_t GPIO_Pin, GPIO_Mode, GPIO_OType, GPIO_PuPd, GPIO_Speed;
} GPIO_InitTypeDef;
typedef struct {
    uint32_t TIM_Prescaler, TIM_Period, TIM_CounterMode, TIM_ClockDivision;
} TIM_TimeBaseInitTypeDef;
typedef struct {
    uint32_t TIM_OCMode, TIM_OutputState, TIM_OCPolarity, TIM_Pulse;
} TIM_OCInitTypeDef;
typedef struct { uint32_t PCLK1_Frequency; } RCC_ClocksTypeDef;
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t);
void RCC_AHB1PeriphClockCmd(uint32_t, FunctionalState);
void RCC_APB1PeriphClockCmd(uint32_t, FunctionalState);
void RCC_GetClocksFreq(RCC_ClocksTypeDef *);
void GPIO_StructInit(GPIO_InitTypeDef *);
void GPIO_Init(GPIO_TypeDef *, GPIO_InitTypeDef *);
void GPIO_SetBits(GPIO_TypeDef *, uint16_t);
void GPIO_ResetBits(GPIO_TypeDef *, uint16_t);
void GPIO_PinAFConfig(GPIO_TypeDef *, uint16_t, uint16_t);
void TIM_TimeBaseStructInit(TIM_TimeBaseInitTypeDef *);
void TIM_TimeBaseInit(TIM_TypeDef *, TIM_TimeBaseInitTypeDef *);
void TIM_OCStructInit(TIM_OCInitTypeDef *);
void TIM_OC1Init(TIM_TypeDef *, TIM_OCInitTypeDef *);
void TIM_OC1PreloadConfig(TIM_TypeDef *, uint16_t);
void TIM_ARRPreloadConfig(TIM_TypeDef *, FunctionalState);
void TIM_GenerateEvent(TIM_TypeDef *, uint16_t);
void TIM_Cmd(TIM_TypeDef *, FunctionalState);
void TIM_SetCompare1(TIM_TypeDef *, uint16_t);
#endif
''')
    driver = (root / '5_ModuleDrivers/bsp_fsmc_lcd.c').read_bytes().decode('gbk')
    start = driver.index('static uint8_t lcd_backlight_brightness')
    end = driver.index('\n}', driver.index('void ILI9806G_BackLed_Control(', start)) + 2
    (tmp / 'backlight_driver.inc').write_text(driver[start:end], encoding='utf8')
    exe = tmp / 'system-settings-test.exe'
    subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I', str(tmp), '-I', str(root / '5_ModuleDrivers'),
                    str(host / 'system_settings_test.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
