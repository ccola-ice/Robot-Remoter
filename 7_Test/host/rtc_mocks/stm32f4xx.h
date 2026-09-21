#ifndef RTC_TEST_STM32_H
#define RTC_TEST_STM32_H
#include <stdint.h>
#define __IO volatile
typedef enum { DISABLE=0, ENABLE=1 } FunctionalState;
typedef enum { RESET=0, SET=1 } FlagStatus;
typedef enum { ERROR=0, SUCCESS=1 } ErrorStatus;
typedef struct { volatile uint32_t TR,DR,CR,PRER,ISR; } RTC_TestRegisters;
typedef struct { volatile uint32_t BDCR; } RCC_TestRegisters;
extern RTC_TestRegisters rtc_test_registers;
extern RCC_TestRegisters rcc_test_registers;
#define RTC (&rtc_test_registers)
#define RCC (&rcc_test_registers)
typedef struct { uint8_t RTC_Hours,RTC_Minutes,RTC_Seconds,RTC_H12; } RTC_TimeTypeDef;
typedef struct { uint8_t RTC_WeekDay,RTC_Month,RTC_Date,RTC_Year; } RTC_DateTypeDef;
typedef struct { uint32_t RTC_HourFormat,RTC_AsynchPrediv,RTC_SynchPrediv; } RTC_InitTypeDef;
#define RCC_BDCR_LSEON 0x1U
#define RCC_BDCR_LSERDY 0x2U
#define RCC_BDCR_LSEBYP 0x4U
#define RCC_BDCR_RTCSEL 0x300U
#define RCC_BDCR_RTCEN 0x8000U
#define RCC_RTCCLKSource_LSE 0x100U
#define RCC_RTCCLKSource_LSI 0x200U
#define RCC_LSE_ON 1U
#define RCC_FLAG_LSERDY 0x41U
#define RCC_APB1Periph_PWR 0x10000000U
#define RTC_CR_FMT 0x40U
#define RTC_CR_BYPSHAD 0x20U
#define RTC_ISR_INIT 0x80U
#define RTC_ISR_RSF 0x20U
#define RTC_PRER_PREDIV_A 0x7f0000U
#define RTC_PRER_PREDIV_S 0x1fffU
#define RTC_Format_BCD 1U
#define RTC_H12_PM 0x40U
#define RTC_HourFormat_24 0U
#define RTC_BKP_DR0 0U
#define RTC_BKP_DR1 1U
#define RTC_BKP_DR2 2U
void RCC_APB1PeriphClockCmd(uint32_t peripheral, FunctionalState state);
void PWR_BackupAccessCmd(FunctionalState state);
void RCC_LSEConfig(uint8_t state);
FlagStatus RCC_GetFlagStatus(uint8_t flag);
void RCC_BackupResetCmd(FunctionalState state);
void RCC_RTCCLKConfig(uint32_t source);
void RCC_RTCCLKCmd(FunctionalState state);
uint32_t RTC_ReadBackupRegister(uint32_t index);
void RTC_WriteBackupRegister(uint32_t index,uint32_t value);
void RTC_WriteProtectionCmd(FunctionalState state);
ErrorStatus RTC_EnterInitMode(void);
void RTC_ExitInitMode(void);
ErrorStatus RTC_WaitForSynchro(void);
ErrorStatus RTC_Init(RTC_InitTypeDef *init);
void RTC_GetTime(uint32_t format,RTC_TimeTypeDef *value);
void RTC_GetDate(uint32_t format,RTC_DateTypeDef *value);
#endif
