#include "bsp_Systick.h"
#include "bsp_exti.h"
#include "bsp_rtc.h"

#include "bsp_gpio_led.h"
#include "bsp_gpio_button.h"
#include "bsp_gpio_digital_channel.h"

#include "bsp_usart_debug.h"
#include "bsp_usart_extra.h"
#include "bsp_usart_gps.h"

#include "bsp_i2c_eeprom.h"
#include "bsp_i2c_mpu6050.h"
#include "bsp_i2c_touch.h"
#include "gt9xx.h"
#include "bsp_mpu6050.h"
#include "bsp_mpu6050_exti.h"

#include "bsp_spi_flash.h"
#include "bsp_spi_nrf.h"
#include "bsp_sdio_sd.h"

#include "bsp_adc1_independent_dual.h"
#include "bsp_adc3_independent_dual.h"

#include "bsp_basic_tim6.h"
#include "bsp_basic_tim7.h"
#include "bsp_general_tim2.h"
#include "bsp_general_tim3.h"
#include "bsp_general_tim4.h"
#include "bsp_general_tim5.h"

#include "bsp_fsmc_sram.h"
#include "bsp_fsmc_lcd.h"

#include "bsp_bmp.h"

#include "jpgPort.h"
#include "platform_nrf.h"
#include "FLASH_test.h"
#include "EEPROM_test.h"
#include "SRAM_test.h"
#include "FATFS_FLASH_test.h"
#include "FATFS_SDCARD_test.h"
#include "nmea_decode_test.h"
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h" 
#include "gui.h"
#include "menu.h"
#include "multi_button.h"
#include "multi_button_user.h"
#include "common.h"
#include "nmea/nmea.h"
#include "gt9xx.h"
#include "palette.h"
#include "param.h"

#include "inv_mpu.h"

#define MPU_DMP_BOOT_ATTEMPTS 3U
#define TOUCH_BOOT_ATTEMPTS   3U

extern unsigned int Task_Delay[5];

extern volatile uint16_t ADC1_Value[NUM_OF_ADC1CHANNEL];
extern volatile uint16_t ADC3_Value[NUM_OF_ADC3CHANNEL];

extern volatile  param_Config param;;

FATFS fs_sdcard;                   	/* SD卡 FatFs工作区 */
FATFS fs_flash;                    	/* SPI Flash FatFs工作区 */
extern FIL fnew_sdcard;				/* 文件对象 */
extern FRESULT res;                	/* 文件操作结果 */
extern unsigned int fnum;			/* 文件成功读写数量 */

float pitch,roll,yaw; 		//dmp解算欧拉角
short aacx,aacy,aacz;		//加速度传感器原始数据
short gyrox,gyroy,gyroz;	//陀螺仪原始数据
short temp;					//温度
uint8_t imu_data_valid;
static uint8_t imu_read_failures;
static uint8_t imu_dmp_ready;
float yaw_new;

u8 finish_1hz=0,finish_2hz=0,finish_5hz=0,finish_10hz=0,finish_20hz=0,finish_33hz=0,finish_50hz=0,finish_100hz=0;
volatile u8 finish_button_10ms=0;

static BootReport boot_report;
static uint32_t boot_last_cycles, boot_total_ms, boot_cycle_remainder;
static uint32_t boot_item_started_ms;

/* DWT runs before the application's TIM6/GTP tick is started. */
static uint32_t boot_now_ms(void)
{
    uint32_t now = DWT->CYCCNT;
    uint32_t delta = now - boot_last_cycles;
    uint32_t cycles_per_ms = SystemCoreClock / 1000UL;
    boot_last_cycles = now;
    boot_total_ms += delta / cycles_per_ms;
    boot_cycle_remainder += delta % cycles_per_ms;
    boot_total_ms += boot_cycle_remainder / cycles_per_ms;
    boot_cycle_remainder %= cycles_per_ms;
    return boot_total_ms;
}

static void boot_start(BootItem item)
{
    boot_item_started_ms = boot_now_ms();
    boot_report_start(&boot_report, item);
    boot_report.elapsed_ms = boot_item_started_ms;
    printf("[BOOT] START %s\r\n", boot_item_names[item]);
    gui_boot_update(&boot_report, (uint8_t)item);
}

static void boot_done(BootItem item, BootState state, const char *detail)
{
    uint32_t now = boot_now_ms();
    boot_report_record(&boot_report, item, state, detail, now - boot_item_started_ms);
    boot_report.elapsed_ms = now;
    printf("[BOOT] %u/%u %u%% %-10s %s: %s (%lu ms)\r\n",
           boot_report.completed, (unsigned)BOOT_ITEM_COUNT,
           boot_report_percent(&boot_report), boot_state_name(state),
           boot_item_names[item], detail,
           (unsigned long)boot_report.items[item].elapsed_ms);
    gui_boot_update(&boot_report, (uint8_t)item);
}

static void boot_skip(BootItem item, const char *reason)
{
    boot_start(item);
    boot_done(item, BOOT_NOT_TESTED, reason);
}

static uint8_t boot_adc_check(DMA_Stream_TypeDef *stream, uint32_t tc,
                             uint32_t te, uint32_t dme, uint32_t fe,
                             volatile uint16_t *values,
                             uint8_t count)
{
    uint16_t wait_ms;
    uint8_t i;
    DMA_ClearFlag(stream, tc | te | dme | fe);
    for(wait_ms = 0U; wait_ms < 100U; wait_ms++) {
        if(DMA_GetFlagStatus(stream, te) != RESET ||
           DMA_GetFlagStatus(stream, dme) != RESET ||
           DMA_GetFlagStatus(stream, fe) != RESET) return 0U;
        if(DMA_GetFlagStatus(stream, tc) != RESET) {
            for(i = 0U; i < count; i++) if(values[i] > 4095U) return 0U;
            return 1U;
        }
        Delay_ms(1U);
    }
    return 0U;
}

static uint8_t boot_rtc_tick(void)
{
    uint16_t i;
    uint32_t before;
    before = RTC_GetSubSecond();
    (void)RTC->DR; /* Unlock the shadow registers after SSR. */
    for(i = 0U; i < 100U; i++) {
        Delay_ms(1U);
        if(RTC_GetSubSecond() != before) {
            (void)RTC->DR;
            return 1U;
        }
        (void)RTC->DR;
    }
    return 0U;
}

static uint8_t boot_gps_check(void)
{
    uint8_t snapshot[GPS_RBUFF_SIZE];
    uint16_t next, i, wait_ms;
    for(wait_ms = 0U; wait_ms < 1500U; wait_ms += 10U) {
        next = (GPS_RBUFF_SIZE - DMA_GetCurrDataCounter(GPS_USART_DMA_STREAM))
               % GPS_RBUFF_SIZE;
        for(i = 0U; i < GPS_RBUFF_SIZE; i++)
            snapshot[i] = ((volatile uint8_t *)gps_rbuff)[(next + i) % GPS_RBUFF_SIZE];
        if(boot_nmea_valid(snapshot, sizeof(snapshot))) return 1U;
        Delay_ms(10U);
    }
    return 0U;
}

static uint8_t boot_timers_check(void)
{
    /* TIM4 is an external encoder counter: a stationary encoder is not a fault. */
    TIM_TypeDef *timers[4] = {TIM2, TIM3, TIM5, TIM6};
    uint32_t first[4];
    uint8_t seen = 0U, i;
    uint16_t wait;
    for(i = 0U; i < 4U; i++) first[i] = TIM_GetCounter(timers[i]);
    finish_button_10ms = 0U;
    finish_10hz = 0U;
    for(wait = 0U; wait < 200U; wait++) {
        for(i = 0U; i < 4U; i++) {
            if((timers[i]->CR1 & TIM_CR1_CEN) != 0U &&
               TIM_GetCounter(timers[i]) != first[i]) seen |= (uint8_t)(1U << i);
        }
        if(seen == 0x0fU && finish_button_10ms && finish_10hz) return 1U;
        Delay_ms(1U);
    }
    return 0U;
}

void setup(void)
{
    uint8_t attempt, ok, code;
    uint8_t rf_enabled, rf_channel, rf_power, rf_rate;
    u8 dmp_result = MPU_DMP_INIT_ERROR_DEVICE;
    int32_t touch_result = -1;
    SD_Error sd_result = SD_ERROR;
    uint32_t flash_id = 0UL;
    RCC_ClocksTypeDef clocks;
    char detail[72];

    SysTick_Init();
    Exti_Init();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    Debug_USART_Config();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0UL;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    boot_report_reset(&boot_report);
    boot_last_cycles = boot_total_ms = boot_cycle_remainder = 0UL;

    /* Preserve the board's GPIO/FSMC initialization order. These configuration
     * calls alone do not resolve any hardware check as PASS. */
    EXPAND_USART_Config();
    LED_GPIO_Config();
    digital_channel_init();
    EEPROM_I2C_Init();
    FLASH_SPI_Init();
    NRF_SPI_Init();
    SRAM_FSMC_Config();

    /* The display must be configured before it can show the checklist.
     * Sending display commands is not claimed as a visual hardware test. */
    ILI9806G_Init();
    ILI9806G_GramScan(LCD_SCAN_MODE);
    gui_boot_begin();
    boot_start(BOOT_CLOCK);
    RCC_GetClocksFreq(&clocks);
    ok = (clocks.SYSCLK_Frequency == SystemCoreClock &&
          clocks.HCLK_Frequency == 168000000UL &&
          RCC_GetFlagStatus(RCC_FLAG_HSERDY) == SET &&
          RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == SET);
    boot_done(BOOT_CLOCK, ok ? BOOT_PASS : BOOT_FAIL,
              "RCC HSE/PLL flags + system/bus clock readback");
    boot_skip(BOOT_LCD, "Initialized; visual quality needs operator inspection");

    user_BUTTON_init();

    /* Keep the memory test before f_mount and all application memory use. */
    boot_start(BOOT_SRAM);
    ok = sram_read_write_test();
    boot_done(BOOT_SRAM, ok ? BOOT_PASS : BOOT_FAIL,
              "External 1 MiB, 8/16-bit R/W, each block restored");

    boot_start(BOOT_FLASH);
    code = FLASH_BootProbe(&flash_id);
    sprintf(detail, "JEDEC=%06lX expected=%06lX; read-only probe E%u",
            (unsigned long)flash_id, (unsigned long)FLASH_ID, code);
    boot_done(BOOT_FLASH, code == 0U ? BOOT_PASS : BOOT_FAIL, detail);

    boot_start(BOOT_EEPROM);
    code = EEPROM_BootProbe();
    sprintf(detail, "ACK + repeated read of first 256 bytes; E%u", code);
    boot_done(BOOT_EEPROM, code == 0U ? BOOT_PASS : BOOT_FAIL, detail);

    boot_start(BOOT_RTC);
    code = RTC_Config();
    ok = (code == 0U) ? boot_rtc_tick() : 0U;
    sprintf(detail, "Clock startup/init E%u; subsecond counter %s",
            code, ok ? "advancing" : "failed");
    boot_done(BOOT_RTC, ok ? BOOT_PASS : BOOT_FAIL, detail);

    boot_start(BOOT_ADC1);
    Independent_Dual_ADC1_Init();
    ok = boot_adc_check(DUAL_ADC1_DMA_STREAM, DMA_FLAG_TCIF4,
                        DMA_FLAG_TEIF4, DMA_FLAG_DMEIF4, DMA_FLAG_FEIF4,
                        ADC1_Value, NUM_OF_ADC1CHANNEL);
    boot_done(BOOT_ADC1, ok ? BOOT_PASS : BOOT_FAIL,
              "Fresh DMA complete + seven 12-bit samples");
    boot_start(BOOT_ADC3);
    Independent_Dual_ADC3_Init();
    ok = boot_adc_check(DUAL_ADC3_DMA_STREAM, DMA_FLAG_TCIF0,
                        DMA_FLAG_TEIF0, DMA_FLAG_DMEIF0, DMA_FLAG_FEIF0,
                        ADC3_Value, NUM_OF_ADC3CHANNEL);
    boot_done(BOOT_ADC3, ok ? BOOT_PASS : BOOT_FAIL,
              "Fresh DMA complete + three 12-bit samples");

    GPS_USART_Config();
    GPS_DMA_Config();
    nmea_decode_init();
    EXTI_MPU_Config();
    boot_start(BOOT_MPU);
    for(attempt = 0U; attempt < MPU_DMP_BOOT_ATTEMPTS; attempt++) {
        dmp_result = mpu_dmp_init();
        if(dmp_result == MPU_DMP_INIT_OK) { imu_dmp_ready = 1U; break; }
        printf("[BOOT] MPU DMP attempt=%u E%u\r\n", attempt + 1U, dmp_result);
        if(attempt + 1U < MPU_DMP_BOOT_ATTEMPTS) Delay_ms(50U);
    }
    sprintf(detail, "Communication + DMP firmware readback; E%u", dmp_result);
    boot_done(BOOT_MPU, imu_dmp_ready ? BOOT_PASS : BOOT_FAIL, detail);
    if(imu_dmp_ready) {
        boot_start(BOOT_MPU_SAMPLE);
        ok = 0U;
        for(attempt = 0U; attempt < 20U; attempt++) {
            if(mpu_dmp_get_data(&pitch, &roll, &yaw) == 0U) { ok = 1U; break; }
            Delay_ms(25U);
        }
        imu_data_valid = ok;
        boot_done(BOOT_MPU_SAMPLE, ok ? BOOT_PASS : BOOT_FAIL,
                  "Wait up to 500 ms for a decoded DMP FIFO sample");
    } else boot_skip(BOOT_MPU_SAMPLE, "Blocked: MPU6050 initialization failed");

    boot_start(BOOT_TOUCH);
    for(attempt = 0U; attempt < TOUCH_BOOT_ATTEMPTS; attempt++) {
        touch_result = GTP_Init_Panel();
        if(touch_result == 0) break;
        if(attempt + 1U < TOUCH_BOOT_ATTEMPTS) Delay_ms(50U);
    }
    sprintf(detail, "Controller version/config communication; result=%ld",
            (long)touch_result);
    boot_done(BOOT_TOUCH, touch_result == 0 ? BOOT_PASS : BOOT_FAIL, detail);

    boot_start(BOOT_SD);
    for(attempt = 0U; attempt < 3U; attempt++) {
        sd_result = SD_Init();
        if(sd_result == SD_OK) break;
        if(attempt + 1U < 3U) Delay_ms(50U);
    }
    sprintf(detail, "SD command handshake/card information; result=%u", sd_result);
    boot_done(BOOT_SD, sd_result == SD_OK ? BOOT_PASS : BOOT_FAIL, detail);
    if(sd_result == SD_OK) {
        boot_start(BOOT_SD_FS);
        res = f_mount(&fs_sdcard, "0:", 1);
        sprintf(detail, "Read partition/FAT metadata; mount result=%u", res);
        boot_done(BOOT_SD_FS, res == FR_OK ? BOOT_PASS : BOOT_FAIL, detail);
    } else boot_skip(BOOT_SD_FS, "Blocked: SD card initialization failed");

    boot_start(BOOT_NRF);
    ok = nrf24l01_check() == 0U;
    boot_done(BOOT_NRF, ok ? BOOT_PASS : BOOT_FAIL,
              "NRF TX address register write/read comparison");

    if(boot_report.items[BOOT_FLASH].state == BOOT_PASS) {
        boot_start(BOOT_PARAMS);
        code = write_default_param();
        if(code != 0U) set_default_param();
        boot_done(BOOT_PARAMS, code == 0U ? BOOT_PASS : BOOT_FAIL,
                  code == 0U ? "Loaded/defaulted; any migration write verified" :
                               "SPI load/save failed; RAM defaults in use");
    } else {
        set_default_param();
        boot_skip(BOOT_PARAMS, "Flash unavailable; RAM defaults, no persistence access");
    }
    if(boot_report.items[BOOT_NRF].state == BOOT_PASS) {
        boot_start(BOOT_NRF_CONFIG);
        nrf24l01_apply_settings(param.NRF_Mode, param.NRF_Channel,
                                param.NRF_Power, param.NRF_DataRate);
        code = nrf24l01_read_runtime(&rf_enabled, &rf_channel, &rf_power, &rf_rate);
        ok = (code == 0U && rf_enabled == (param.NRF_Mode != 0U) &&
              rf_channel == (param.NRF_Channel > 125U ? 125U : param.NRF_Channel) &&
              (rf_power & 0x06U) == (param.NRF_Power & 0x06U) &&
              rf_rate == (param.NRF_DataRate >= 2U ? 2U : param.NRF_DataRate));
        boot_done(BOOT_NRF_CONFIG, ok ? BOOT_PASS : BOOT_FAIL,
                  "Channel, power, rate and enabled state compared");
    } else boot_skip(BOOT_NRF_CONFIG, "Blocked: NRF register test failed");

    boot_start(BOOT_GPS);
    ok = boot_gps_check();
    boot_done(BOOT_GPS, ok ? BOOT_PASS : BOOT_FAIL,
              ok ? "Received complete checksum-valid NMEA; fix not tested" :
                   "No checksum-valid NMEA within receive window");

    boot_start(BOOT_TIMERS);
    BASIC_TIM6_Configuration(8400-1, 99);
    GENERAL_TIM2_InitConfiguration(65536-1,128-1);
    GENERAL_TIM3_InitConfiguration(65536-1,128-1);
    GENERAL_TIM4_InitConfiguration(8400-1, 99);
    GENERAL_TIM5_InitConfiguration(8400-1, 9);
    ok = boot_timers_check();
    boot_done(BOOT_TIMERS, ok ? BOOT_PASS : BOOT_FAIL,
              "TIM2/3/5/6 counters + TIM5/TIM6 interrupt activity");

    boot_skip(BOOT_KEYS, "Keys/switches/TIM4 encoder require manual movement");
    boot_skip(BOOT_ANALOG, "Travel/calibration/battery accuracy require known inputs");
    boot_skip(BOOT_OUTPUTS, "Configured; LED/buzzer need physical feedback");
    boot_skip(BOOT_UART, "Configured; no external loopback fixture");
    boot_skip(BOOT_RADIO, "No peer/ACK test; SPI presence is not an RF link test");
    boot_skip(BOOT_FLASH_WRITE, "NOT TESTED: preserve stored data; no test erase/program");
    boot_skip(BOOT_EEPROM_WRITE, "NOT TESTED: preserve contents and write endurance");
    boot_skip(BOOT_SD_WRITE, "NOT TESTED: preserve files; no format/write test");
    boot_skip(BOOT_TOUCH_QUALITY, "Manual full-screen accuracy test required");
    boot_skip(BOOT_INTERNAL_MEMORY, "No destructive test of running MCU ROM/RAM");
}

static void boot_show_result(void)
{
    uint8_t item;
    uint8_t released = 0U, pressed = 0U;
    gui_boot_finish(&boot_report);
    printf("[BOOT] SUMMARY completed=%u/%u pass=%u fail=%u not_tested=%u ms=%lu\r\n",
           boot_report.completed, (unsigned)BOOT_ITEM_COUNT, boot_report.passed,
           boot_report.failed, boot_report.not_tested, (unsigned long)boot_report.elapsed_ms);
    for(item = 0U; item < BOOT_ITEM_COUNT; item++)
        printf("[BOOT] %-10s %-22s %s\r\n",
               boot_state_name(boot_report.items[item].state), boot_item_names[item],
               boot_report.items[item].detail);

    /* Faults remain on screen until a new, debounced physical OK press.
     * A held key at power-on does not dismiss the result. */
    if(boot_report_outcome(&boot_report) == BOOT_FAILED ||
       boot_report_outcome(&boot_report) == BOOT_INCOMPLETE) {
        while(1) {
            Delay_ms(10U);
            if(read_button_ok_gpio(0U) == BUTTON_OFF) {
                if(pressed >= 3U) break;
                released = 1U;
                pressed = 0U;
            } else if(released != 0U && pressed < 3U) pressed++;
        }
        printf("[BOOT] Faults acknowledged; entering menu with recorded failures\r\n");
    } else {
        /* Readability hold AFTER progress has finished, not simulated work. */
        Delay_ms(1200U);
    }
}

int main(void)
{
	setup();

	boot_show_result();

	LCD_SetFont(&Font16x32);
	LCD_SetColors(GREEN,BLACK);	
	ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);
	menu_init();
	menu_process();

    while(1)
    {
        GTP_Service();
		if(finish_1hz == 1)
		{
			if(imu_dmp_ready != 0U)
			{
				temp = MPU_Get_Temperature();
				MPU_Get_Accelerometer(&aacx,&aacy,&aacz);
				MPU_Get_Gyroscope(&gyrox,&gyroy,&gyroz);
			}
			finish_1hz = 0;
		}
		
		if(finish_2hz == 1)
		{
			nmea_decode_test();
			finish_2hz = 0;
		}

		if(finish_5hz == 1)
		{
			//printf("finish_5hz = %d\n\r",finish_5hz);
			finish_5hz = 0;
		}

		if(finish_10hz == 1)
		{
			if((imu_dmp_ready != 0U) &&
			   (mpu_dmp_get_data(&pitch,&roll,&yaw) == 0U))
			{
				imu_data_valid = 1U;
				imu_read_failures = 0U;
			}
			else if(imu_dmp_ready != 0U)
			{
				if(imu_read_failures < 10U)
				{
					imu_read_failures++;
				}
				if(imu_read_failures >= 10U)
				{
					imu_data_valid = 0U;
				}
			}
			else
			{
				imu_data_valid = 0U;
			}
			RTC_TimeAndDate_Show(); // 显示时间和日期
			finish_10hz = 0;
		}

		finish_10hz = 0;

		if(finish_50hz == 1)
		{
			finish_50hz = 0;
		}

		if(finish_button_10ms == 1)
		{
			finish_button_10ms = 0;
			button_ticks();
			digital_channel_update_10ms();
			menu_tick_10ms();
		}

		if(finish_100hz == 1)
		{

			finish_100hz = 0;
		}

		menu_process();
	}
}
