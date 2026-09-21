#include "bsp_rtc.h"
#include "bsp_SysTick.h"
#include <string.h>

#define RTC_SOURCE_TAG 0x43544d00UL
#define RTC_SOURCE_REGISTER RTC_BKP_DR1
#define RTC_SOURCE_CHECK_REGISTER RTC_BKP_DR2
#define RTC_EXPECTED_PRER (((uint32_t)ASYNCHPREDIV << 16) | SYNCHPREDIV)

static uint8_t rtc_ready;

static void rtc_backup_access(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
}

static uint8_t rtc_clock_running(void)
{
    return (RCC->BDCR & RCC_BDCR_RTCSEL) == RCC_RTCCLKSource_LSE &&
           (RCC->BDCR & (RCC_BDCR_RTCEN | RCC_BDCR_LSERDY | RCC_BDCR_LSEON)) ==
               (RCC_BDCR_RTCEN | RCC_BDCR_LSERDY | RCC_BDCR_LSEON) &&
           !(RCC->BDCR & RCC_BDCR_LSEBYP) && !(RTC->ISR & RTC_ISR_INIT) &&
           (RTC->PRER & (RTC_PRER_PREDIV_A | RTC_PRER_PREDIV_S)) == RTC_EXPECTED_PRER &&
           !(RTC->CR & RTC_CR_FMT);
}

static void rtc_mark_source(RtcTimeSource source)
{
    uint32_t value = source == RTC_TIME_UNSET ? 0UL : RTC_SOURCE_TAG | (uint32_t)source;
    /* Commit source last; a reset while setting time must not retain trust. */
    RTC_WriteBackupRegister(RTC_SOURCE_REGISTER, 0UL);
    RTC_WriteBackupRegister(RTC_SOURCE_CHECK_REGISTER, ~value);
    RTC_WriteBackupRegister(RTC_SOURCE_REGISTER, value);
}

static RtcTimeSource rtc_saved_source(void)
{
    uint32_t value = RTC_ReadBackupRegister(RTC_SOURCE_REGISTER);
    if(RTC_ReadBackupRegister(RTC_BKP_DRX) != RTC_BKP_DATA ||
       RTC_ReadBackupRegister(RTC_SOURCE_CHECK_REGISTER) != ~value) return RTC_TIME_UNSET;
    if(value == (RTC_SOURCE_TAG | RTC_TIME_MANUAL)) return RTC_TIME_MANUAL;
    if(value == (RTC_SOURCE_TAG | RTC_TIME_GPS)) return RTC_TIME_GPS;
    return RTC_TIME_UNSET;
}

static uint8_t rtc_can_access(void)
{
    if(!rtc_ready) return 0U;
    if(rtc_clock_running()) return 1U;
    /* A detected oscillator/configuration interruption loses the claim of
     * continuous time even if the hardware starts running again later. */
    if(rtc_saved_source() != RTC_TIME_UNSET) {
        rtc_backup_access();
        rtc_mark_source(RTC_TIME_UNSET);
    }
    return 0U;
}

static ErrorStatus rtc_wait_sync(void)
{
    /* In bypass mode no shadow-register synchronization is needed. */
    return RTC->CR & RTC_CR_BYPSHAD ? SUCCESS : RTC_WaitForSynchro();
}

static uint8_t rtc_bcd_decode(uint8_t bcd, uint8_t *value)
{
    if((bcd & 15U) > 9U || (bcd >> 4) > 9U) return 0U;
    *value = (uint8_t)((bcd >> 4) * 10U + (bcd & 15U));
    return 1U;
}

static uint32_t rtc_bcd_encode(uint8_t value)
{
    return (uint32_t)((value / 10U) * 16U + value % 10U);
}

static uint8_t rtc_read_pair(RtcCalendar *value)
{
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;
    uint8_t year;
    /* Reading time latches the date shadow; reading date releases that latch. */
    RTC_GetTime(RTC_Format_BCD, &time);
    RTC_GetDate(RTC_Format_BCD, &date);
    memset(value, 0, sizeof(*value));
    if(!rtc_bcd_decode(time.RTC_Hours, &value->hour) ||
       !rtc_bcd_decode(time.RTC_Minutes, &value->minute) ||
       !rtc_bcd_decode(time.RTC_Seconds, &value->second) ||
       !rtc_bcd_decode(date.RTC_Year, &year) ||
       !rtc_bcd_decode(date.RTC_Month, &value->month) ||
       !rtc_bcd_decode(date.RTC_Date, &value->day)) return RTC_CALENDAR_READ_ERROR;
    value->year = 2000U + year;
    if(RTC->CR & RTC_CR_FMT) {
        if(!value->hour || value->hour > 12U) return RTC_CALENDAR_READ_ERROR;
        value->hour = (uint8_t)(value->hour % 12U + (time.RTC_H12 == RTC_H12_PM ? 12U : 0U));
    }
    if(!RTC_CalendarValidate(value)) return RTC_CALENDAR_READ_ERROR;
    value->weekday = RTC_CalendarWeekday(value->year, value->month, value->day);
    return RTC_CALENDAR_OK;
}

static uint8_t rtc_read_raw(RtcCalendar *value)
{
    RtcCalendar first, second;
    uint8_t attempt;
    /* Two identical snapshots also work with a legacy BYPSHAD setting, and
     * tolerate a date rollover between accesses without returning mixed days. */
    for(attempt = 0U; attempt < 3U; attempt++) {
        if(rtc_read_pair(&first) != RTC_CALENDAR_OK ||
           rtc_read_pair(&second) != RTC_CALENDAR_OK) continue;
        if(memcmp(&first, &second, sizeof(first)) == 0) {
            *value = second;
            return RTC_CALENDAR_OK;
        }
    }
    return RTC_CALENDAR_READ_ERROR;
}

uint8_t RTC_ReadCalendar(RtcCalendar *calendar)
{
    if(!calendar) return RTC_CALENDAR_ARGUMENT_ERROR;
    memset(calendar, 0, sizeof(*calendar));
    if(!rtc_can_access()) return RTC_CALENDAR_CLOCK_ERROR;
    if(!(RTC->CR & RTC_CR_BYPSHAD) && !(RTC->ISR & RTC_ISR_RSF)) return RTC_CALENDAR_READ_ERROR;
    return rtc_read_raw(calendar);
}

RtcTimeSource RTC_TimeSource(void)
{
    RtcCalendar current;
    if(RTC_ReadCalendar(&current) != RTC_CALENDAR_OK) return RTC_TIME_UNSET;
    return rtc_saved_source();
}

uint8_t RTC_TimeIsValid(void)
{
    return RTC_TimeSource() != RTC_TIME_UNSET;
}

static uint32_t rtc_seconds(const RtcCalendar *calendar)
{
    uint32_t days = calendar->day - 1U;
    uint16_t year;
    uint8_t month;
    for(year = 2000U; year < calendar->year; year++) days += year % 4U == 0U ? 366UL : 365UL;
    for(month = 1U; month < calendar->month; month++) days += RTC_CalendarDaysInMonth(calendar->year, month);
    return ((days * 24UL + calendar->hour) * 60UL + calendar->minute) * 60UL + calendar->second;
}

static uint8_t rtc_write_calendar(const RtcCalendar *calendar, RtcTimeSource source)
{
    RtcCalendar verify;
    uint32_t target_seconds, read_seconds;
    rtc_backup_access();
    RTC_WriteProtectionCmd(DISABLE);
    if(RTC_EnterInitMode() == ERROR) {
        RTC_ExitInitMode();
        RTC_WriteProtectionCmd(ENABLE);
        return RTC_CALENDAR_WRITE_ERROR;
    }
    rtc_mark_source(RTC_TIME_UNSET);
    /* One INIT interval freezes the counter while BOTH registers change.
     * Two separate RTC_SetDate/RTC_SetTime calls expose a mixed date/time. */
    RTC->TR = rtc_bcd_encode(calendar->hour) << 16 |
              rtc_bcd_encode(calendar->minute) << 8 | rtc_bcd_encode(calendar->second);
    RTC->DR = rtc_bcd_encode((uint8_t)(calendar->year - 2000U)) << 16 |
              (uint32_t)RTC_CalendarWeekday(calendar->year, calendar->month, calendar->day) << 13 |
              rtc_bcd_encode(calendar->month) << 8 | rtc_bcd_encode(calendar->day);
    RTC_ExitInitMode();
    RTC_WriteProtectionCmd(ENABLE);
    if(rtc_wait_sync() == ERROR || rtc_read_raw(&verify) != RTC_CALENDAR_OK)
        return RTC_CALENDAR_WRITE_ERROR;
    target_seconds = rtc_seconds(calendar);
    read_seconds = rtc_seconds(&verify);
    if(read_seconds != target_seconds && read_seconds != target_seconds + 1UL)
        return RTC_CALENDAR_WRITE_ERROR;
    RTC_WriteBackupRegister(RTC_BKP_DRX, RTC_BKP_DATA);
    rtc_mark_source(source);
    return RTC_CALENDAR_OK;
}

uint8_t RTC_SetCalendar(const RtcCalendar *calendar, RtcTimeSource source)
{
    if(!RTC_CalendarValidate(calendar) || (source != RTC_TIME_MANUAL && source != RTC_TIME_GPS))
        return RTC_CALENDAR_ARGUMENT_ERROR;
    if(!rtc_can_access()) return RTC_CALENDAR_CLOCK_ERROR;
    return rtc_write_calendar(calendar, source);
}

static uint8_t rtc_start_lse(void)
{
    uint16_t elapsed;
    /* ST's RCC_LSEConfig first turns the oscillator OFF. Do not call it on
     * every normal reset: that would interrupt a retained running calendar. */
    if(!(RCC->BDCR & RCC_BDCR_LSEON) || (RCC->BDCR & RCC_BDCR_LSEBYP)) RCC_LSEConfig(RCC_LSE_ON);
    for(elapsed = 0U; elapsed < 3000U; elapsed++) {
        if(RCC_GetFlagStatus(RCC_FLAG_LSERDY) != RESET) return RTC_CALENDAR_OK;
        Delay_ms(1U);
    }
    return RTC_CALENDAR_CLOCK_ERROR;
}

uint8_t RTC_Config(void)
{
    RTC_InitTypeDef init;
    RtcCalendar retained, current;
    static const RtcCalendar baseline = {2000U,1U,1U,6U,0U,0U,0U};
    uint32_t old_bdcr, old_prer, old_cr, old_isr, backup[20];
    uint8_t had_calendar = 0U, reset_domain = 0U, repair, i;
    RtcTimeSource source;

    rtc_ready = 0U;
    rtc_backup_access();
    old_bdcr = RCC->BDCR; old_prer = RTC->PRER; old_cr = RTC->CR; old_isr = RTC->ISR;
    source = rtc_saved_source();
    if((old_bdcr & RCC_BDCR_RTCEN) && (old_bdcr & RCC_BDCR_RTCSEL)) {
        (void)rtc_wait_sync();
        if(rtc_read_raw(&retained) == RTC_CALENDAR_OK) had_calendar = 1U;
    }
    if(rtc_start_lse() != RTC_CALENDAR_OK) return RTC_CALENDAR_CLOCK_ERROR;

    if((old_bdcr & RCC_BDCR_RTCSEL) && (old_bdcr & RCC_BDCR_RTCSEL) != RCC_RTCCLKSource_LSE) {
        /* RTCSEL cannot be replaced by OR-ing a new source. Preserve the
         * representable civil time and unrelated backup registers first. */
        for(i = 0U; i < 20U; i++) backup[i] = RTC_ReadBackupRegister(i);
        RCC_BackupResetCmd(ENABLE);
        RCC_BackupResetCmd(DISABLE);
        reset_domain = 1U;
        if(rtc_start_lse() != RTC_CALENDAR_OK) return RTC_CALENDAR_CLOCK_ERROR;
        for(i = 3U; i < 20U; i++) RTC_WriteBackupRegister(i, backup[i]);
    }
    if((RCC->BDCR & RCC_BDCR_RTCSEL) != RCC_RTCCLKSource_LSE) RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
    RCC_RTCCLKCmd(ENABLE);
    if(rtc_wait_sync() == ERROR) return RTC_CALENDAR_CLOCK_ERROR;
    if(!had_calendar && !reset_domain && (old_bdcr & RCC_BDCR_RTCSEL) == RCC_RTCCLKSource_LSE &&
       rtc_read_raw(&retained) == RTC_CALENDAR_OK) had_calendar = 1U;

    repair = reset_domain || !(old_bdcr & RCC_BDCR_RTCEN) ||
        !(old_bdcr & RCC_BDCR_LSERDY) || (old_bdcr & RCC_BDCR_LSEBYP) ||
        (old_prer & (RTC_PRER_PREDIV_A | RTC_PRER_PREDIV_S)) != RTC_EXPECTED_PRER ||
        (old_cr & RTC_CR_FMT) || (old_isr & RTC_ISR_INIT);
    if(repair || !had_calendar) {
        rtc_mark_source(RTC_TIME_UNSET);
        init.RTC_AsynchPrediv = ASYNCHPREDIV;
        init.RTC_SynchPrediv = SYNCHPREDIV;
        init.RTC_HourFormat = RTC_HourFormat_24;
        if(RTC_Init(&init) == ERROR) {
            RTC_ExitInitMode();
            RTC_WriteProtectionCmd(ENABLE);
            return RTC_CALENDAR_CLOCK_ERROR;
        }
        if(rtc_write_calendar(had_calendar ? &retained : &baseline, RTC_TIME_UNSET) != RTC_CALENDAR_OK)
            return RTC_CALENDAR_WRITE_ERROR;
    } else {
        /* Legacy magic only means initialized; compiler-based timestamps
         * must stay untrusted until a manual or explicit GPS set succeeds. */
        RTC_WriteBackupRegister(RTC_BKP_DRX, RTC_BKP_DATA);
        if(source == RTC_TIME_UNSET) rtc_mark_source(RTC_TIME_UNSET);
    }
    if(!rtc_clock_running() || rtc_read_raw(&current) != RTC_CALENDAR_OK) return RTC_CALENDAR_READ_ERROR;
    rtc_ready = 1U;
    return RTC_CALENDAR_OK;
}

uint8_t RTC_CLK_Config(void) { return RTC_Config(); }

void RTC_TimeAndDate_Set(void)
{
    RtcCalendar current;
    static const RtcCalendar baseline = {2000U,1U,1U,6U,0U,0U,0U};
    if(rtc_ready && rtc_clock_running() && rtc_read_raw(&current) != RTC_CALENDAR_OK)
        (void)rtc_write_calendar(&baseline, RTC_TIME_UNSET);
}

void RTC_TimeAndDate_Show(void)
{
    RtcCalendar current;
    (void)RTC_ReadCalendar(&current);
}

uint8_t RTC_SynchronizeCalendar(uint16_t year, uint8_t month, uint8_t day,
                                uint8_t hour, uint8_t minute, uint8_t second)
{
    RtcCalendar requested, current;
    uint32_t a, b;
    requested.year = year; requested.month = month; requested.day = day;
    requested.weekday = 0U; requested.hour = hour; requested.minute = minute; requested.second = second;
    if(!RTC_CalendarValidate(&requested)) return 0U;
    if(RTC_TimeIsValid() && RTC_ReadCalendar(&current) == RTC_CALENDAR_OK) {
        a = rtc_seconds(&requested); b = rtc_seconds(&current);
        if(a == b || a + 1UL == b || b + 1UL == a) return 0U;
    }
    return RTC_SetCalendar(&requested, RTC_TIME_GPS) == RTC_CALENDAR_OK;
}
