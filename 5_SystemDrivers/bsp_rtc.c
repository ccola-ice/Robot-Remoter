#include "bsp_rtc.h"
#include "bsp_SysTick.h"
#include "bsp_usart_debug.h"
#include "bsp_fsmc_lcd.h"

#define RTC_PRINT

static uint8_t RTC_IsLeapYear(uint16_t year)
{
	return (uint8_t)(((year % 4U) == 0U) &&
	                (((year % 100U) != 0U) || ((year % 400U) == 0U)));
}

static uint8_t RTC_DaysInMonth(uint16_t year, uint8_t month)
{
	static const uint8_t days[12] =
	{
		31U, 28U, 31U, 30U, 31U, 30U,
		31U, 31U, 30U, 31U, 30U, 31U
	};

	if ((month == 0U) || (month > 12U))
	{
		return 0U;
	}

	if ((month == 2U) && RTC_IsLeapYear(year))
	{
		return 29U;
	}

	return days[month - 1U];
}

/* STM32 RTC weekday: Monday = 1, ..., Sunday = 7. */
static uint8_t RTC_CalculateWeekday(uint16_t year, uint8_t month, uint8_t day)
{
	static const uint8_t month_offset[12] =
	{
		0U, 3U, 2U, 5U, 0U, 3U, 5U, 1U, 4U, 6U, 2U, 4U
	};
	uint16_t adjusted_year = year;
	uint8_t weekday;

	if (month < 3U)
	{
		adjusted_year--;
	}

	weekday = (uint8_t)((adjusted_year + adjusted_year / 4U - adjusted_year / 100U +
	                     adjusted_year / 400U + month_offset[month - 1U] + day) % 7U);

	return (weekday == 0U) ? 7U : weekday;
}

static uint8_t RTC_CompileMonth(void)
{
	const char *date = __DATE__;

	if ((date[0] == 'J') && (date[1] == 'a')) return 1U;
	if ((date[0] == 'F')) return 2U;
	if ((date[0] == 'M') && (date[2] == 'r')) return 3U;
	if ((date[0] == 'A') && (date[1] == 'p')) return 4U;
	if ((date[0] == 'M') && (date[2] == 'y')) return 5U;
	if ((date[0] == 'J') && (date[2] == 'n')) return 6U;
	if ((date[0] == 'J') && (date[2] == 'l')) return 7U;
	if ((date[0] == 'A') && (date[1] == 'u')) return 8U;
	if ((date[0] == 'S')) return 9U;
	if ((date[0] == 'O')) return 10U;
	if ((date[0] == 'N')) return 11U;
	return 12U;
}

/**
  * @brief  设置时间和日期
  * @param  无
  * @retval 无
  */
void RTC_TimeAndDate_Set(void)
{
	RTC_TimeTypeDef RTC_TimeStructure;
	RTC_DateTypeDef RTC_DateStructure;
	const char *compile_date = __DATE__;
	const char *compile_time = __TIME__;
	uint16_t year = (uint16_t)((compile_date[7] - '0') * 1000 +
	                           (compile_date[8] - '0') * 100 +
	                           (compile_date[9] - '0') * 10 +
	                           (compile_date[10] - '0'));
	uint8_t month = RTC_CompileMonth();
	uint8_t day = (uint8_t)(((compile_date[4] == ' ') ? 0 : (compile_date[4] - '0')) * 10 +
	                        (compile_date[5] - '0'));

	/* 首次启动使用本次固件的编译时间，避免继续使用历史固定日期。 */
	RTC_TimeStructure.RTC_H12 = RTC_H12_AMorPM;
	RTC_TimeStructure.RTC_Hours = (uint8_t)((compile_time[0] - '0') * 10 + (compile_time[1] - '0'));
	RTC_TimeStructure.RTC_Minutes = (uint8_t)((compile_time[3] - '0') * 10 + (compile_time[4] - '0'));
	RTC_TimeStructure.RTC_Seconds = (uint8_t)((compile_time[6] - '0') * 10 + (compile_time[7] - '0'));

	RTC_DateStructure.RTC_WeekDay = RTC_CalculateWeekday(year, month, day);
	RTC_DateStructure.RTC_Date = day;
	RTC_DateStructure.RTC_Month = month;
	RTC_DateStructure.RTC_Year = (uint8_t)(year - 2000U);

	if ((RTC_SetDate(RTC_Format_BINorBCD, &RTC_DateStructure) == SUCCESS) &&
	    (RTC_SetTime(RTC_Format_BINorBCD, &RTC_TimeStructure) == SUCCESS))
	{
		RTC_WriteBackupRegister(RTC_BKP_DRX, RTC_BKP_DATA);
	}
}

/**
  * @brief  使用外部可靠日历时间校准RTC
  * @retval 1: 已校准，0: 输入无效、无需校准或写入失败
  */
uint8_t RTC_SynchronizeCalendar(uint16_t year, uint8_t month, uint8_t day,
                                uint8_t hour, uint8_t minute, uint8_t second)
{
	RTC_TimeTypeDef current_time;
	RTC_DateTypeDef current_date;
	RTC_TimeTypeDef target_time;
	RTC_DateTypeDef target_date;
	int16_t second_error;

	if ((year < 2000U) || (year > 2099U) ||
	    (month == 0U) || (month > 12U) ||
	    (day == 0U) || (day > RTC_DaysInMonth(year, month)) ||
	    (hour > 23U) || (minute > 59U) || (second > 59U))
	{
		return 0U;
	}

	RTC_GetTime(RTC_Format_BIN, &current_time);
	RTC_GetDate(RTC_Format_BIN, &current_date);

	second_error = (int16_t)current_time.RTC_Seconds - (int16_t)second;
	if ((current_date.RTC_Year == (uint8_t)(year - 2000U)) &&
	    (current_date.RTC_Month == month) &&
	    (current_date.RTC_Date == day) &&
	    (current_time.RTC_Hours == hour) &&
	    (current_time.RTC_Minutes == minute) &&
	    (second_error >= -1) && (second_error <= 1))
	{
		return 0U;
	}

	target_date.RTC_WeekDay = RTC_CalculateWeekday(year, month, day);
	target_date.RTC_Date = day;
	target_date.RTC_Month = month;
	target_date.RTC_Year = (uint8_t)(year - 2000U);

	target_time.RTC_H12 = RTC_H12_AM;
	target_time.RTC_Hours = hour;
	target_time.RTC_Minutes = minute;
	target_time.RTC_Seconds = second;

	if ((RTC_SetDate(RTC_Format_BIN, &target_date) == ERROR) ||
	    (RTC_SetTime(RTC_Format_BIN, &target_time) == ERROR))
	{
		return 0U;
	}

	RTC_WriteBackupRegister(RTC_BKP_DRX, RTC_BKP_DATA);
	return 1U;
}

/**
  * @brief  显示时间和日期
  * @param  无
  * @retval 无
  */
void RTC_TimeAndDate_Show(void)
{
	static uint8_t Rtctmp = 0xffU;
	RTC_TimeTypeDef RTC_TimeStructure;
	RTC_DateTypeDef RTC_DateStructure;
	
    // 获取日历
    RTC_GetTime(RTC_Format_BIN, &RTC_TimeStructure);
    RTC_GetDate(RTC_Format_BIN, &RTC_DateStructure);
    
    // 每秒打印一次
    if(Rtctmp != RTC_TimeStructure.RTC_Seconds)
    {
		    #ifdef RTC_PRINT		
        // 打印日期
        //printf("The Date :  Y:20%0.2d - M:%0.2d - D:%0.2d - W:%0.2d\r\n", 
        // RTC_DateStructure.RTC_Year,
        // RTC_DateStructure.RTC_Month,
        // RTC_DateStructure.RTC_Date,
        // RTC_DateStructure.RTC_WeekDay);
		    #endif

        #ifdef RTC_PRINT
        // 打印时间
        //printf("The Time :  %0.2d:%0.2d:%0.2d \r\n\r\n", 
        // RTC_TimeStructure.RTC_Hours,
        // RTC_TimeStructure.RTC_Minutes,
        // RTC_TimeStructure.RTC_Seconds);
        #endif

        // //液晶显示日期
        // //先把要显示的数据用sprintf函数转换为字符串，然后才能用液晶显示函数显示
        // sprintf(LCDTemp,"The Date:Y:20%0.2d-M:%0.2d-D:%0.2d-W:%0.2d", 
        // RTC_DateStructure.RTC_Year,
        // RTC_DateStructure.RTC_Month, 
        // RTC_DateStructure.RTC_Date,
        // RTC_DateStructure.RTC_WeekDay);
		    // ILI9806G_DispStringLine_EN(4,LCDTemp);
        
        // //液晶显示时间
        // sprintf(LCDTemp,"The Time :  %0.2d:%0.2d:%0.2d", 
        // RTC_TimeStructure.RTC_Hours, 
        // RTC_TimeStructure.RTC_Minutes, 
        // RTC_TimeStructure.RTC_Seconds);
		    // ILI9806G_DispStringLine_EN(5,LCDTemp);
        
        (void)RTC->DR;
    }
    Rtctmp = RTC_TimeStructure.RTC_Seconds;	
}

/**
  * @brief  RTC配置：选择RTC时钟源，设置RTC_CLK的分频系数
  * @param  无
  * @retval 无
  */
uint8_t RTC_CLK_Config(void)
{  
	  RTC_InitTypeDef RTC_InitStructure;
    uint16_t startup_ms = 0U;
	
	  /*使能 PWR 时钟*/
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    /* PWR_CR:DBF置1，使能RTC、RTC备份寄存器和备份SRAM的访问 */
    PWR_BackupAccessCmd(ENABLE);

#if defined (RTC_CLOCK_SOURCE_LSI) 
  /* 使用LSI作为RTC时钟源会有误差 
	 * 默认选择LSE作为RTC的时钟源
	 */
  /* 使能LSI */ 
  RCC_LSICmd(ENABLE);
  /* 等待LSI稳定 */  
  while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
  {
    if(startup_ms++ >= 100U) return 1U;
    Delay_ms(1U);
  }
  /* 选择LSI做为RTC的时钟源 */
  RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);

#elif defined (RTC_CLOCK_SOURCE_LSE)

  /* 使能LSE */ 
  RCC_LSEConfig(RCC_LSE_ON);
   /* 等待LSE稳定 */   
  while(RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
  {
    if(startup_ms++ >= 3000U) return 1U;
    Delay_ms(1U);
  }
  /* 选择LSE做为RTC的时钟源 */
  RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);    

#endif /* RTC_CLOCK_SOURCE_LSI */

  /* 使能RTC时钟 */
  RCC_RTCCLKCmd(ENABLE);

  /* 等待 RTC APB 寄存器同步 */
  if(RTC_WaitForSynchro() == ERROR) return 2U;
   
/*=====================初始化同步/异步预分频器的值======================*/
	/* 驱动日历的时钟ck_spare = LSE/[(255+1)*(127+1)] = 1HZ */
	
	/* 设置异步预分频器的值 */
	RTC_InitStructure.RTC_AsynchPrediv = ASYNCHPREDIV;
	/* 设置同步预分频器的值 */
	RTC_InitStructure.RTC_SynchPrediv = SYNCHPREDIV;	
	RTC_InitStructure.RTC_HourFormat = RTC_HourFormat_24; 
	/* 用RTC_InitStructure的内容初始化RTC寄存器 */
	if (RTC_Init(&RTC_InitStructure) == ERROR) return 3U;
    return 0U;
}

/**
  * @brief  RTC配置：选择RTC时钟源，设置RTC_CLK的分频系数
  * @param  无
  * @retval 无
  */
#define LSE_STARTUP_TIMEOUT     ((uint16_t)0x05000)
void RTC_CLK_Config_Backup(void)
{  
    __IO uint16_t StartUpCounter = 0;
	FlagStatus LSEStatus = RESET;	
	RTC_InitTypeDef RTC_InitStructure;
	
	/* 使能 PWR 时钟 */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
  /* PWR_CR:DBF置1，使能RTC、RTC备份寄存器和备份SRAM的访问 */
  PWR_BackupAccessCmd(ENABLE);
	
/*=========================选择RTC时钟源==============================*/
    /* 默认使用LSE，如果LSE出故障则使用LSI */
  /* 使能LSE */
  RCC_LSEConfig(RCC_LSE_ON);	
	
	/* 等待LSE启动稳定，如果超时则退出 */
  do
  {
    LSEStatus = RCC_GetFlagStatus(RCC_FLAG_LSERDY);
    StartUpCounter++;
  }while((LSEStatus == RESET) && (StartUpCounter != LSE_STARTUP_TIMEOUT));
	
	
	if(LSEStatus == SET )
  {
		printf("\n\r LSE 启动成功 \r\n");
		/* 选择LSE作为RTC的时钟源 */
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
  }
	else
	{
		printf("\n\r LSE 故障，转为使用LSI \r\n");
		
		/* 使能LSI */	
		RCC_LSICmd(ENABLE);
		/* 等待LSI稳定 */ 
		while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
		{			
		}
		
		printf("\n\r LSI 启动成功 \r\n");
		/* 选择LSI作为RTC的时钟源 */
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
	}
	
  /* 使能 RTC 时钟 */
  RCC_RTCCLKCmd(ENABLE);
  /* 等待 RTC APB 寄存器同步 */
  RTC_WaitForSynchro();

/*=====================初始化同步/异步预分频器的值======================*/
	/* 驱动日历的时钟ck_spare = LSE/[(255+1)*(127+1)] = 1HZ */
	
	/* 设置异步预分频器的值为127 */
	RTC_InitStructure.RTC_AsynchPrediv = 0x7F;
	/* 设置同步预分频器的值为255 */
	RTC_InitStructure.RTC_SynchPrediv = 0xFF;	
	RTC_InitStructure.RTC_HourFormat = RTC_HourFormat_24; 
	/* 用RTC_InitStructure的内容初始化RTC寄存器 */
	if (RTC_Init(&RTC_InitStructure) == ERROR)
	{
		printf("\n\r RTC 时钟初始化失败 \r\n");
	}	
}

//RTC功能
uint8_t RTC_Config(void)         
{
    /*
	 * 当我们配置过RTC时间之后就往备份寄存器0写入一个数据做标记
	 * 所以每次程序重新运行的时候就通过检测备份寄存器0的值来判断
	 * RTC 是否已经配置过，如果配置过那就继续运行，如果没有配置过
	 * 就初始化RTC，配置RTC的时间。
	 */
   
    /* RTC配置：选择时钟源，设置RTC_CLK的分频系数 */
    if(RTC_CLK_Config() != 0U) return 1U;

    if (RTC_ReadBackupRegister(RTC_BKP_DRX) != RTC_BKP_DATA)
    {
        /* 设置时间和日期 */
		  RTC_TimeAndDate_Set();
    }
    else
    {
        /* 检查是否电源复位 */
        if (RCC_GetFlagStatus(RCC_FLAG_PORRST) != RESET)
        {
            printf("\r\n 发生电源复位....\r\n");
        }
        /* 检查是否外部复位 */
        else if (RCC_GetFlagStatus(RCC_FLAG_PINRST) != RESET)
        {
            printf("\r\n 发生外部复位....\r\n");
        }

        printf("\r\n 不需要重新配置RTC....\r\n");
    
        /* 使能 PWR 时钟 */
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
        /* PWR_CR:DBF置1，使能RTC、RTC备份寄存器和备份SRAM的访问 */
        PWR_BackupAccessCmd(ENABLE);
        /* 等待 RTC APB 寄存器同步 */
        if(RTC_WaitForSynchro() == ERROR) return 2U;   
    } 
    return RTC_ReadBackupRegister(RTC_BKP_DRX) == RTC_BKP_DATA ? 0U : 3U;
}



/**********************************END OF FILE*************************************/
