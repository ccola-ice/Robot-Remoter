#include "bsp_SysTick.h"

static volatile uint32_t g_ul_ms_ticks;

void SysTick_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    g_ul_ms_ticks = 0U;
    if(SysTick_Config(SystemCoreClock / 1000U)) {
        while(1) {}
    }
}

/* Microsecond delays use the CPU counter, independently of interrupt priority.
 * Chunk long delays so the target cycle count always fits in uint32_t. */
void Delay_us(__IO uint32_t nTime)
{
    while(nTime != 0U) {
        uint32_t chunk = nTime > 1000000U ? 1000000U : nTime;
        uint32_t cycles = chunk * (SystemCoreClock / 1000000U);
        uint32_t start = DWT->CYCCNT;
        while((uint32_t)(DWT->CYCCNT - start) < cycles) {}
        nTime -= chunk;
    }
}

int get_tick_count(unsigned long *count)
{
    if(count == 0) return -1;
    *count = g_ul_ms_ticks;
    return 0;
}

void TimeStamp_Increment(void)
{
    g_ul_ms_ticks++;
}
