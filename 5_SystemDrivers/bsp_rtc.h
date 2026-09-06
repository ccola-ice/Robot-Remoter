#ifndef __BSP_RTC_H__
#define __BSP_RTC_H__
#include "stm32f4xx.h"


// 时钟源宏定义
#define RTC_CLOCK_SOURCE_LSE      
//#define RTC_CLOCK_SOURCE_LSI

// 异步分频因子
#define ASYNCHPREDIV         0X7F
// 同步分频因子
#define SYNCHPREDIV          0XFF


#define RTC_H12_AMorPM             RTC_H12_AM

// 时间格式宏定义
#define RTC_Format_BINorBCD         RTC_Format_BIN

// 备份域寄存器宏定义
#define RTC_BKP_DRX                 RTC_BKP_DR0
// 写入到备份寄存器的数据宏定义
#define RTC_BKP_DATA                0X32F3
 
                                  
uint8_t RTC_CLK_Config(void);
void RTC_TimeAndDate_Set(void);
void RTC_TimeAndDate_Show(void);
uint8_t RTC_SynchronizeCalendar(uint16_t year, uint8_t month, uint8_t day,
                                uint8_t hour, uint8_t minute, uint8_t second);

uint8_t RTC_Config(void);

#endif
