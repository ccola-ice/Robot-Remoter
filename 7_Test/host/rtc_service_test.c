#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bsp_rtc.h"
#include "nmea/time.h"

RTC_TestRegisters rtc_test_registers;
RCC_TestRegisters rcc_test_registers;
static uint32_t backup[20], elapsed_us, latched_date, old_tr,old_dr;
static uint8_t backup_access, write_protected, date_locked, lse_fails, init_fails, sync_fails, corrupt_on_write;
static unsigned lse_starts, domain_resets, init_calls, init_entries, calendar_writes, time_reads, date_reads;
static uint8_t rollover_once;

/* Actual GPS acceptance and UTC conversion, connected to the actual RTC driver. */
#define GPS_DATA_MAX_AGE_MS 3000UL
static nmeaTIME beiJingTime, last_rtc_time;
static uint8_t time_valid, rtc_time_set;
static uint32_t time_ms, gps_test_ms;
static int get_tick_count(unsigned long *value) { *value=gps_test_ms; return 0; }
#include "rtc_gps_impl.inc"

static uint32_t bcd(unsigned value) { return value/10U*16U+value%10U; }
static void hardware_calendar(unsigned year,unsigned month,unsigned day,unsigned hour,unsigned minute,unsigned second)
{
    RTC->TR=bcd(hour)<<16 | bcd(minute)<<8 | bcd(second);
    RTC->DR=bcd(year-2000U)<<16 | (uint32_t)RTC_CalendarWeekday(year,month,day)<<13 | bcd(month)<<8 | bcd(day);
}
void RCC_APB1PeriphClockCmd(uint32_t peripheral,FunctionalState state)
{ assert(peripheral==RCC_APB1Periph_PWR && state==ENABLE); }
void PWR_BackupAccessCmd(FunctionalState state) { backup_access=(uint8_t)state; }
void RCC_LSEConfig(uint8_t state)
{
    assert(state==RCC_LSE_ON && backup_access); lse_starts++;
    RCC->BDCR &= ~(RCC_BDCR_LSEON|RCC_BDCR_LSERDY|RCC_BDCR_LSEBYP);
    RCC->BDCR |= RCC_BDCR_LSEON;
    if(!lse_fails) RCC->BDCR |= RCC_BDCR_LSERDY;
}
FlagStatus RCC_GetFlagStatus(uint8_t flag)
{ assert(flag==RCC_FLAG_LSERDY); return RCC->BDCR&RCC_BDCR_LSERDY ? SET:RESET; }
void RCC_BackupResetCmd(FunctionalState state)
{
    assert(backup_access);
    if(state==ENABLE) {
        domain_resets++; memset(backup,0,sizeof(backup));
        memset(RTC,0,sizeof(*RTC)); RCC->BDCR=0U;
    }
}
void RCC_RTCCLKConfig(uint32_t source)
{ assert(backup_access && source==RCC_RTCCLKSource_LSE); RCC->BDCR |= source; }
void RCC_RTCCLKCmd(FunctionalState state)
{ assert(backup_access); if(state) RCC->BDCR|=RCC_BDCR_RTCEN; else RCC->BDCR&=~RCC_BDCR_RTCEN; }
uint32_t RTC_ReadBackupRegister(uint32_t index) { assert(index<20U); return backup[index]; }
void RTC_WriteBackupRegister(uint32_t index,uint32_t value)
{ assert(index<20U && backup_access); backup[index]=value; }
void RTC_WriteProtectionCmd(FunctionalState state) { write_protected=(uint8_t)state; }
ErrorStatus RTC_EnterInitMode(void)
{
    assert(!write_protected); init_entries++;
    RTC->ISR|=RTC_ISR_INIT; old_tr=RTC->TR; old_dr=RTC->DR;
    return init_fails ? ERROR:SUCCESS;
}
void RTC_ExitInitMode(void)
{
    if(RTC->ISR&RTC_ISR_INIT) {
        if(RTC->TR!=old_tr || RTC->DR!=old_dr) {
            assert(!write_protected); calendar_writes++;
            if(corrupt_on_write) { RTC->DR=0U; corrupt_on_write=0U; }
        }
        RTC->ISR&=~RTC_ISR_INIT;
    }
}
ErrorStatus RTC_WaitForSynchro(void)
{
    RTC->ISR &= ~RTC_ISR_RSF;
    if(sync_fails) return ERROR;
    RTC->ISR |= RTC_ISR_RSF;
    return SUCCESS;
}
ErrorStatus RTC_Init(RTC_InitTypeDef *init)
{
    init_calls++;
    assert(init->RTC_AsynchPrediv==127U && init->RTC_SynchPrediv==255U && init->RTC_HourFormat==0U);
    if(init_fails) return ERROR;
    RTC->PRER=init->RTC_AsynchPrediv<<16 | init->RTC_SynchPrediv;
    RTC->CR &= ~RTC_CR_FMT;
    RTC->ISR &= ~RTC_ISR_INIT;
    return SUCCESS;
}
void RTC_GetTime(uint32_t format,RTC_TimeTypeDef *value)
{
    uint32_t tr=RTC->TR;
    assert(format==RTC_Format_BCD); time_reads++;
    if(!(RTC->CR&RTC_CR_BYPSHAD)) { latched_date=RTC->DR; date_locked=1U; }
    value->RTC_Hours=(uint8_t)((tr>>16)&0x3fU); value->RTC_Minutes=(uint8_t)((tr>>8)&0x7fU);
    value->RTC_Seconds=(uint8_t)(tr&0x7fU); value->RTC_H12=(uint8_t)((tr>>16)&0x40U);
    if(rollover_once) { hardware_calendar(2028U,3U,1U,0U,0U,0U); rollover_once=0U; }
}
void RTC_GetDate(uint32_t format,RTC_DateTypeDef *value)
{
    uint32_t dr=date_locked ? latched_date:RTC->DR;
    assert(format==RTC_Format_BCD); date_reads++; date_locked=0U;
    value->RTC_Year=(uint8_t)(dr>>16); value->RTC_Month=(uint8_t)((dr>>8)&0x1fU);
    value->RTC_Date=(uint8_t)(dr&0x3fU); value->RTC_WeekDay=(uint8_t)((dr>>13)&7U);
}
void Delay_us(volatile uint32_t microseconds) { elapsed_us+=microseconds; }

static void reset_hardware(void)
{
    memset(RTC,0,sizeof(*RTC)); memset(RCC,0,sizeof(*RCC)); memset(backup,0,sizeof(backup));
    backup_access=0U; write_protected=1U; date_locked=lse_fails=init_fails=sync_fails=corrupt_on_write=0U;
    elapsed_us=lse_starts=domain_resets=init_calls=init_entries=calendar_writes=time_reads=date_reads=0U;
    rollover_once=0U;
}
static void assert_calendar(unsigned year,unsigned month,unsigned day,unsigned hour,unsigned minute,unsigned second)
{
    RtcCalendar value;
    assert(RTC_ReadCalendar(&value)==RTC_CALENDAR_OK);
    assert(value.year==year && value.month==month && value.day==day);
    assert(value.hour==hour && value.minute==minute && value.second==second);
    assert(value.weekday==RTC_CalendarWeekday(year,month,day));
    assert(time_reads==date_reads && !date_locked);
}
static void date_math(void)
{
    RtcCalendar value={2000U,1U,1U,0U,23U,59U,59U};
    unsigned year,month,day,weekday=6U,count=0U;
    for(year=2000U;year<2100U;year++) for(month=1U;month<=12U;month++)
    for(day=1U;day<=RTC_CalendarDaysInMonth(year,month);day++) {
        value.year=(uint16_t)year; value.month=(uint8_t)month; value.day=(uint8_t)day;
        assert(RTC_CalendarValidate(&value));
        assert(RTC_CalendarWeekday(year,month,day)==weekday);
        weekday=weekday%7U+1U; count++;
    }
    assert(count==36525U);
    assert(RTC_CalendarDaysInMonth(2000U,2U)==29U && RTC_CalendarDaysInMonth(2099U,2U)==28U);
    assert(!RTC_CalendarDaysInMonth(2100U,2U) && !RTC_CalendarDaysInMonth(1999U,1U));
    assert(!RTC_CalendarWeekday(2026U,0U,1U) && !RTC_CalendarWeekday(2026U,2U,29U));
    value.year=2026U; value.month=2U; value.day=29U; assert(!RTC_CalendarValidate(&value));
    value.day=28U; value.hour=24U; assert(!RTC_CalendarValidate(&value));
    value.hour=23U; value.minute=60U; assert(!RTC_CalendarValidate(&value));
    value.minute=59U; value.second=60U; assert(!RTC_CalendarValidate(&value));
    assert(!RTC_CalendarValidate(NULL));
    puts("calendar math: all 36525 days (2000..2099), weekdays, leap years and invalid fields passed");
}
static void lifecycle(void)
{
    RtcCalendar set={2028U,2U,29U,0U,23U,59U,58U};
    unsigned starts,init,writes;
    reset_hardware(); assert(RTC_Config()==0U);
    assert_calendar(2000U,1U,1U,0U,0U,0U); assert(!RTC_TimeIsValid());
    assert(RTC_TimeSource()==RTC_TIME_UNSET && init_calls==1U);
    assert(RTC_SetCalendar(&set,RTC_TIME_MANUAL)==0U && RTC_TimeIsValid());
    assert_calendar(2028U,2U,29U,23U,59U,58U); assert(RTC_TimeSource()==RTC_TIME_MANUAL);
    starts=lse_starts; init=init_calls; writes=calendar_writes;
    assert(RTC_Config()==0U); assert_calendar(2028U,2U,29U,23U,59U,58U);
    assert(lse_starts==starts && init_calls==init && calendar_writes==writes && RTC_TimeIsValid());
    RTC_TimeAndDate_Set(); assert(calendar_writes==writes);
    RTC_TimeAndDate_Show(); assert(calendar_writes==writes);
    set.year=2000U; set.month=2U; set.day=29U;
    assert(RTC_SetCalendar(&set,RTC_TIME_GPS)==0U); writes=calendar_writes;
    assert(RTC_Config()==0U && calendar_writes==writes && RTC_TimeSource()==RTC_TIME_GPS);
    assert_calendar(2000U,2U,29U,23U,59U,58U);
    backup[2]^=1U; assert(!RTC_TimeIsValid());
    assert(RTC_Config()==0U); assert_calendar(2000U,2U,29U,23U,59U,58U);
    assert(!RTC_TimeIsValid());
    puts("RTC lifecycle: unset baseline, manual/GPS source, reset preservation, year-2000 retention, source checksum passed");
}
static void repairs(void)
{
    RtcCalendar set={2026U,9U,21U,0U,12U,34U,56U};
    unsigned i;
    reset_hardware(); RCC->BDCR=RCC_BDCR_LSEON|RCC_BDCR_LSERDY|RCC_BDCR_RTCEN|RCC_RTCCLKSource_LSE;
    RTC->PRER=0x7f00ffU; hardware_calendar(2026U,9U,20U,8U,9U,10U); backup[0]=RTC_BKP_DATA;
    assert(RTC_Config()==0U && !RTC_TimeIsValid() && !lse_starts && !init_calls && !calendar_writes);
    assert_calendar(2026U,9U,20U,8U,9U,10U);
    assert(RTC_SetCalendar(&set,RTC_TIME_MANUAL)==0U);
    RTC->PRER=0x7f01ffU; assert(RTC_Config()==0U); assert(!RTC_TimeIsValid());
    assert_calendar(2026U,9U,21U,12U,34U,56U);
    assert(RTC_SetCalendar(&set,RTC_TIME_MANUAL)==0U);
    RCC->BDCR=(RCC->BDCR&~RCC_BDCR_RTCSEL)|RCC_RTCCLKSource_LSI;
    for(i=3U;i<20U;i++) backup[i]=0x12340000U+i;
    assert(RTC_Config()==0U && domain_resets==1U && !RTC_TimeIsValid());
    assert((RCC->BDCR&RCC_BDCR_RTCSEL)==RCC_RTCCLKSource_LSE);
    assert_calendar(2026U,9U,21U,12U,34U,56U);
    for(i=3U;i<20U;i++) assert(backup[i]==0x12340000U+i);
    RTC->CR|=RTC_CR_FMT; RTC->TR=0x00523456U; /* 12:34:56 PM in 12-hour BCD. */
    assert(RTC_Config()==0U); assert_calendar(2026U,9U,21U,12U,34U,56U);
    RCC->BDCR &= ~(RCC_BDCR_LSEON|RCC_BDCR_LSERDY);
    assert(RTC_Config()==0U); assert_calendar(2026U,9U,21U,12U,34U,56U);
    puts("RTC migration: legacy compiler time remains unset; wrong prescaler/source/12h mode repaired with civil date preserved");
}
static void failures_and_reads(void)
{
    RtcCalendar set={2026U,9U,21U,0U,16U,0U,0U},read;
    uint32_t tr,dr;
    unsigned writes;
    reset_hardware(); lse_fails=1U;
    assert(RTC_Config()!=0U && elapsed_us==3000000U && !domain_resets && !RTC_TimeIsValid());
    assert(RTC_ReadCalendar(&read)!=0U);
    lse_fails=0U; RCC->BDCR=0U; assert(RTC_Config()==0U);
    assert(RTC_SetCalendar(&set,RTC_TIME_MANUAL)==0U); tr=RTC->TR; dr=RTC->DR;
    set.day=31U; assert(RTC_SetCalendar(&set,RTC_TIME_MANUAL)==RTC_CALENDAR_ARGUMENT_ERROR);
    assert(RTC->TR==tr && RTC->DR==dr && RTC_TimeIsValid()); set.day=21U;
    assert(RTC_SetCalendar(NULL,RTC_TIME_MANUAL)!=0U && RTC_SetCalendar(&set,RTC_TIME_UNSET)!=0U);
    assert(RTC_ReadCalendar(NULL)!=0U);
    init_fails=1U; set.hour=17U; assert(RTC_SetCalendar(&set,RTC_TIME_GPS)==RTC_CALENDAR_WRITE_ERROR);
    assert(RTC->TR==tr && RTC->DR==dr && RTC_TimeIsValid() && write_protected);
    init_fails=0U; sync_fails=1U; assert(RTC_SetCalendar(&set,RTC_TIME_GPS)==RTC_CALENDAR_WRITE_ERROR);
    assert(!RTC_TimeIsValid() && RTC_ReadCalendar(&read)==RTC_CALENDAR_READ_ERROR); sync_fails=0U;
    corrupt_on_write=1U; set.hour=18U; assert(RTC_SetCalendar(&set,RTC_TIME_GPS)==RTC_CALENDAR_WRITE_ERROR);
    assert(!RTC_TimeIsValid()); assert(RTC_SetCalendar(&set,RTC_TIME_GPS)==0U);
    hardware_calendar(2028U,2U,29U,23U,59U,59U); rollover_once=1U;
    assert_calendar(2028U,3U,1U,0U,0U,0U);
    RTC->CR|=RTC_CR_BYPSHAD; hardware_calendar(2028U,2U,29U,23U,59U,59U); rollover_once=1U;
    assert_calendar(2028U,3U,1U,0U,0U,0U);
    RTC->TR=0x00121a00U; assert(RTC_ReadCalendar(&read)==RTC_CALENDAR_READ_ERROR); /* Bad BCD, despite decimal 20 range. */
    hardware_calendar(2028U,3U,1U,0U,0U,0U); writes=calendar_writes;
    assert(!RTC_SynchronizeCalendar(2028U,2U,29U,23U,59U,59U)); assert(calendar_writes==writes);
    assert(RTC_SynchronizeCalendar(2028U,3U,1U,0U,5U,0U)); assert(RTC_TimeSource()==RTC_TIME_GPS);
    RCC->BDCR &= ~RCC_BDCR_LSERDY; assert(RTC_ReadCalendar(&read)==RTC_CALENDAR_CLOCK_ERROR);
    RCC->BDCR |= RCC_BDCR_LSERDY; assert(RTC_ReadCalendar(&read)==0U && !RTC_TimeIsValid());
    assert(RTC_SetCalendar(&set,RTC_TIME_GPS)==0U);
    puts("RTC errors: bounded LSE timeout, invalid inputs, init/sync/readback failures, BCD validation, coherent midnight reads passed");
}
static void reset_gps_state(void)
{
    memset(&beiJingTime,0,sizeof(beiJingTime)); memset(&last_rtc_time,0,sizeof(last_rtc_time));
    time_valid=rtc_time_set=0U; time_ms=0U; gps_test_ms=10000U;
}
static void gps_automatic_sync(void)
{
    nmeaTIME utc={126,12,31,16,5,30,0}, invalid;
    RtcCalendar set={2027U,1U,1U,0U,0U,5U,30U};
    unsigned entries,writes,i,j,source;
    int delta;

    reset_hardware(); reset_gps_state(); assert(RTC_Config()==0U);
    entries=init_entries;
    gps_accept_time(&utc,gps_test_ms);
    assert(init_entries==entries+1U && RTC_TimeSource()==RTC_TIME_GPS && RTC_TimeIsValid());
    assert_calendar(2027U,1U,1U,0U,5U,30U);
    assert(beiJingTime.year==127 && beiJingTime.mon==1 && beiJingTime.day==1 && beiJingTime.hour==0);
    assert(gps_time_is_fresh() && rtc_time_set);
    entries=init_entries; writes=calendar_writes;
    for(i=0U;i<100U;i++) gps_accept_time(&utc,gps_test_ms);
    assert(init_entries==entries && calendar_writes==writes);

    /* Even an equal UNSET civil time must establish trusted GPS provenance. */
    reset_hardware(); reset_gps_state(); assert(RTC_Config()==0U);
    hardware_calendar(2027U,1U,1U,0U,5U,30U);
    entries=init_entries; writes=calendar_writes;
    assert(RTC_TimeSource()==RTC_TIME_UNSET);
    gps_accept_time(&utc,gps_test_ms);
    assert(init_entries==entries+1U && calendar_writes==writes);
    assert(RTC_TimeSource()==RTC_TIME_GPS && RTC_TimeIsValid() && rtc_time_set);

    /* Model a running RTC and consecutive GPS seconds, with both trusted sources. */
    for(source=RTC_TIME_MANUAL;source<=RTC_TIME_GPS;source++) for(delta=-1;delta<=1;delta++) {
        reset_hardware(); reset_gps_state(); assert(RTC_Config()==0U);
        assert(RTC_SetCalendar(&set,(RtcTimeSource)source)==0U);
        entries=init_entries; writes=calendar_writes;
        for(i=0U;i<20U;i++) {
            utc.sec=30+(int)i;
            hardware_calendar(2027U,1U,1U,0U,5U,(unsigned)(utc.sec+delta));
            gps_test_ms+=1000U;
            for(j=0U;j<10U;j++) gps_accept_time(&utc,gps_test_ms);
            assert(RTC_TimeIsValid() && RTC_TimeSource()==(RtcTimeSource)source);
            assert(init_entries==entries && calendar_writes==writes && gps_time_is_fresh());
        }
        /* A two-second discrepancy corrects the RTC and changes MANUAL to GPS. */
        utc.sec=55; hardware_calendar(2027U,1U,1U,0U,5U,53U);
        gps_accept_time(&utc,gps_test_ms);
        assert(init_entries==entries+1U && calendar_writes==writes+1U);
        assert(RTC_TimeSource()==RTC_TIME_GPS && rtc_time_set);
        assert_calendar(2027U,1U,1U,0U,5U,55U);
        entries=init_entries;
        for(i=0U;i<10U;i++) gps_accept_time(&utc,gps_test_ms);
        assert(init_entries==entries);
    }

    /* The tolerance is civil-time based, including a year boundary. */
    reset_gps_state(); set.year=2026U; set.month=12U; set.day=31U;
    set.hour=23U; set.minute=59U; set.second=59U;
    assert(RTC_SetCalendar(&set,RTC_TIME_MANUAL)==0U);
    entries=init_entries; utc.min=0; utc.sec=0;
    gps_accept_time(&utc,gps_test_ms);
    assert(init_entries==entries && RTC_TimeSource()==RTC_TIME_MANUAL);
    assert_calendar(2026U,12U,31U,23U,59U,59U);
    utc.sec=1; gps_accept_time(&utc,gps_test_ms);
    assert(init_entries==entries+1U && RTC_TimeSource()==RTC_TIME_GPS);
    assert_calendar(2027U,1U,1U,0U,0U,1U);

    /* Invalid or stale GPS remains rejected before the RTC write path. */
    reset_gps_state(); entries=init_entries;
    invalid=utc; invalid.mon=2; invalid.day=29; gps_accept_time(&invalid,gps_test_ms);
    invalid=utc; invalid.sec=60; gps_accept_time(&invalid,gps_test_ms);
    invalid=utc; invalid.year=199; gps_accept_time(&invalid,gps_test_ms); /* UTC+8 crosses into 2100. */
    gps_accept_time(&utc,gps_test_ms-GPS_DATA_MAX_AGE_MS);
    assert(init_entries==entries && !gps_time_is_fresh());
    gps_test_ms=100U; gps_accept_time(&utc,UINT32_MAX-2899U); /* Age is exactly 3000 ms after wrap. */
    assert(init_entries==entries && !gps_time_is_fresh());
    gps_accept_time(&utc,UINT32_MAX-2898U);
    assert(init_entries==entries && gps_time_is_fresh()); /* 2999 ms remains fresh; time already matches. */
    puts("GPS/RTC integration: actual UTC+8 acceptance, UNSET auto-sync, 1200 tolerated samples retaining MANUAL/GPS, >1s correction, stale/invalid rejection passed");
}
int main(void)
{
    date_math(); lifecycle(); repairs(); failures_and_reads(); gps_automatic_sync();
    puts("RTC host regression passed (actual service driver + mocked STM32 registers)");
    return 0;
}
