#include "diag_hardware.h"
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
#include "boot_sram.h"
#include "image_viewer.h"
#include "ff.h"
#include "gps_service.h"
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h" 
#include "ui_pages.h"
#include "ui_menu.h"
#include "control_link.h"
#include "multi_button.h"
#include "multi_button_user.h"
#include "common.h"
#include "nmea/nmea.h"
#include "gt9xx.h"
#include "palette.h"
#include "app_config.h"

#include "inv_mpu.h"

#define MPU_DMP_BOOT_ATTEMPTS 3U
#define TOUCH_BOOT_ATTEMPTS   3U

extern unsigned int Task_Delay[5];

extern volatile uint16_t ADC1_Value[NUM_OF_ADC1CHANNEL];
extern volatile uint16_t ADC3_Value[NUM_OF_ADC3CHANNEL];

extern volatile  param_Config param;;

FATFS fs_sdcard;                   	/* SD 卡 FatFs 文件系统对象 */
FATFS fs_flash;                    	/* SPI Flash FatFs 文件系统对象 */

float pitch,roll,yaw; 		// DMP 解算得到的欧拉角
short aacx,aacy,aacz;		// 加速度传感器原始数据
short gyrox,gyroy,gyroz;	// 陀螺仪原始数据
short temp;					// 温度
uint8_t imu_data_valid;
static unsigned long imu_last_sample_ms;
static uint8_t imu_dmp_ready;
float yaw_new;

/* 任务标志由中断置位、主循环按需消费；积压的同类节拍会合并，不逐次补跑。 */
volatile u8 finish_1hz=0,finish_2hz=0,finish_5hz=0,finish_10hz=0,finish_20hz=0,finish_33hz=0,finish_50hz=0,finish_100hz=0;
volatile u8 finish_button_10ms=0;

static BootReport boot_report;
static uint32_t boot_last_cycles, boot_total_ms, boot_cycle_remainder;
static uint32_t boot_item_started_ms;

/* 返回启动阶段累计的毫秒数，在应用节拍尚未启动时使用 DWT 周期计数差值计时。
 * 保留不足一毫秒的周期余数，避免频繁调用产生累计截断误差。 */
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

/* 开始指定自检项：记录起始时间、更新报告，并同步输出串口日志和启动页。 */
static void boot_start(BootItem item)
{
    boot_item_started_ms = boot_now_ms();
    boot_report_start(&boot_report, item);
    boot_report.elapsed_ms = boot_item_started_ms;
    printf("[BOOT] START %s\r\n", boot_item_names[item]);
    gui_boot_update(&boot_report, (uint8_t)item);
}

/* 记录自检项的结果、说明和耗时，刷新日志与进度；需与 boot_start 配对调用。
 * 单项耗时从最近一次 boot_start 起算，因此启动检查按顺序串行执行。 */
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

/* 将因前置检查失败或需要人工确认而跳过的项目记为未测试，并保留原因。 */
static void boot_skip(BootItem item, const char *reason)
{
    boot_start(item);
    boot_done(item, BOOT_NOT_TESTED, reason);
}

/* 在约 100 ms 内检查指定 DMA 流是否完成新一轮 ADC 采样，并验证 count 个样本。
 * tc 为完成标志，te/dme/fe 为错误标志；返回 1 表示通过，超时、错误或超量程返回 0。 */
static uint8_t boot_adc_check(DMA_Stream_TypeDef *stream, uint32_t tc,
                             uint32_t te, uint32_t dme, uint32_t fe,
                             volatile uint16_t *values,
                             uint8_t count)
{
    uint16_t wait_ms;
    uint8_t i;
    /* 清除旧标志后等待一次新的 DMA 完成，避免把缓冲区中的历史值当作采样成功。 */
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

/* 在约 100 ms 内观察 RTC 亚秒计数是否变化；返回 1 表示时钟在运行，超时返回 0。
 * 此检查不判断日历时间是否已经校准。 */
static uint8_t boot_rtc_tick(void)
{
    uint16_t i;
    uint32_t before;
    before = RTC_GetSubSecond();
    (void)RTC->DR; /* 读取 SSR 后再读取 DR，解除影子寄存器锁定。 */
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

/* 在约 1.5 秒的观察窗口内，从 GPS 环形接收区寻找校验通过的完整 NMEA 语句。
 * 返回 1 表示接收通信检查通过，超时返回 0；不要求卫星定位成功。 */
static uint8_t boot_gps_check(void)
{
    uint8_t snapshot[GPS_RBUFF_SIZE];
    uint16_t next, i, wait_ms;
    for(wait_ms = 0U; wait_ms < 1500U; wait_ms += 10U) {
        /* 从 DMA 下一写入位置展开环形缓冲区，便于校验跨缓冲区末尾的完整语句。 */
        next = (GPS_RBUFF_SIZE - DMA_GetCurrDataCounter(GPS_USART_DMA_STREAM))
               % GPS_RBUFF_SIZE;
        for(i = 0U; i < GPS_RBUFF_SIZE; i++)
            snapshot[i] = ((volatile uint8_t *)gps_rbuff)[(next + i) % GPS_RBUFF_SIZE];
        if(boot_nmea_valid(snapshot, sizeof(snapshot))) return 1U;
        Delay_ms(10U);
    }
    return 0U;
}

/* 在约 200 ms 内确认 TIM2/3/5/6 计数推进，且按键和 10 Hz 任务标志均被中断置位。
 * 计数与中断活动均满足条件返回 1，否则返回 0。 */
static uint8_t boot_timers_check(void)
{
    /* TIM4 用于外部编码器计数，编码器静止不应判为故障。 */
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

/* 完成上电初始化，并按依赖顺序执行自检，将各项结果统一写入 boot_report。
 * 检查失败仍记录后续可检查项，运动控制是否允许启用由 main 根据关键结果决定。 */
void setup(void)
{
    uint8_t attempt, ok, code;
    uint8_t rf_enabled, rf_channel, rf_power, rf_rate;
    u8 dmp_result = MPU_DMP_INIT_ERROR_DEVICE;
    int32_t touch_result = -1;
    SD_Error sd_result = SD_ERROR;
    HwResult hw_result;
    uint32_t flash_id = 0UL;
    RCC_ClocksTypeDef clocks;
    char detail[72];

    /* 先建立延时、中断和调试串口基础，供后续外设初始化、重试和日志输出使用。 */
    SysTick_Init();
    Exti_Init();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    Debug_USART_Config();
    /* 业务定时器尚未启动，启用 DWT 作为自检计时源，并清空本次启动报告。 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0UL;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    boot_report_reset(&boot_report);
    boot_last_cycles = boot_total_ms = boot_cycle_remainder = 0UL;

    /* 保持板上 GPIO/FSMC 的初始化顺序。仅调用这些配置函数
     * 不能把相应硬件检查判定为通过。 */
    EXPAND_USART_Config();
    LED_GPIO_Config();
    digital_channel_init();
    EEPROM_I2C_Init();
    FLASH_SPI_Init();
    NRF_SPI_Init();
    SRAM_FSMC_Config();

    /* 必须先配置显示屏，才能显示检查列表。
     * 发送显示命令本身不能证明屏幕的实际显示效果正常。 */
    ILI9806G_Init();
    ILI9806G_GramScan(LCD_SCAN_MODE);
    gui_boot_begin();
    /* 根据寄存器推算时钟频率，并检查 HSE/PLL 就绪状态，确认时钟配置与预期一致。 */
    boot_start(BOOT_CLOCK);
    RCC_GetClocksFreq(&clocks);
    ok = (clocks.SYSCLK_Frequency == SystemCoreClock &&
          clocks.HCLK_Frequency == 168000000UL &&
          RCC_GetFlagStatus(RCC_FLAG_HSERDY) == SET &&
          RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == SET);
    boot_done(BOOT_CLOCK, ok ? BOOT_PASS : BOOT_FAIL,
              "RCC HSE/PLL flags + system/bus clock readback");
    boot_skip(BOOT_LCD, "Initialized; visual quality needs operator inspection");

    /* 配置实体按键并注册事件回调，后续由后台服务按 10 ms 节拍推进按键状态机。 */
    user_BUTTON_init();

    /* 在 f_mount 和应用使用存储器之前完成存储器测试。 */
    boot_start(BOOT_SRAM);
    ok = boot_sram_check();
    /* SRAM 检查失败时禁用 LCD 页缓存，并不给图片解码器分配外部工作区。 */
    LCD_PageBuffer_Enable(ok);
    /* 首 750 KiB 保留给 LCD，剩余 274 KiB 用于 PNG/GIF 解码。 */
    file_image_set_workspace(ok ? (void *)(SRAM_BASE_ADDR + 800UL * 480UL * 2UL) : NULL,
                             ok ? IS62WV51216_SIZE - 800UL * 480UL * 2UL : 0UL);
    boot_done(BOOT_SRAM, ok ? BOOT_PASS : BOOT_FAIL,
              "External 1 MiB, 8/16-bit R/W, each block restored");

    /* Flash 先做只读探测，通过后才测试专用扇区的写入、回读和擦除。
     * 专用扇区已被占用时拒绝擦除，并将写测试记为失败。 */
    boot_start(BOOT_FLASH);
    code = FLASH_BootProbe(&flash_id);
    sprintf(detail, "JEDEC=%06lX expected=%06lX; read-only probe E%u",
            (unsigned long)flash_id, (unsigned long)FLASH_ID, code);
    boot_done(BOOT_FLASH, code == 0U ? BOOT_PASS : BOOT_FAIL, detail);
    if(code == 0U) {
        boot_start(BOOT_FLASH_WRITE);
        hw_result = hardware_flash_write_test();
        boot_done(BOOT_FLASH_WRITE, hw_result == HW_PASS ? BOOT_PASS : BOOT_FAIL,
                  hw_result == HW_BLOCKED ? "Dedicated sector 0x5FE000 occupied; no erase performed" :
                  hw_result == HW_PASS ? "Dedicated sector program/readback/erase verified" :
                                         "Dedicated sector program/readback/erase failed");
    } else boot_skip(BOOT_FLASH_WRITE, "Blocked: Flash read-only probe failed");

    /* EEPROM 先检查应答和重复读取，再测试保留字节；写测试负责恢复原来的空白值。 */
    boot_start(BOOT_EEPROM);
    code = EEPROM_BootProbe();
    sprintf(detail, "ACK + repeated read of first 256 bytes; E%u", code);
    boot_done(BOOT_EEPROM, code == 0U ? BOOT_PASS : BOOT_FAIL, detail);
    if(code == 0U) {
        boot_start(BOOT_EEPROM_WRITE);
        hw_result = hardware_eeprom_write_test();
        boot_done(BOOT_EEPROM_WRITE, hw_result == HW_PASS ? BOOT_PASS : BOOT_FAIL,
                  hw_result == HW_BLOCKED ? "Reserved byte 0xFF occupied; no write performed" :
                  hw_result == HW_PASS ? "Reserved byte 0xFF write/readback/restore verified" :
                                         "Reserved byte write/readback/restore failed");
    } else boot_skip(BOOT_EEPROM_WRITE, "Blocked: EEPROM read-only probe failed");

    /* RTC 配置成功后继续验证计数变化，避免仅凭初始化返回值判定时钟正常。 */
    boot_start(BOOT_RTC);
    code = RTC_Config();
    ok = (code == 0U) ? boot_rtc_tick() : 0U;
    sprintf(detail, "Clock startup/init E%u; subsecond counter %s",
            code, ok ? "advancing" : "failed");
    boot_done(BOOT_RTC, ok ? BOOT_PASS : BOOT_FAIL, detail);

    /* 分别启动两组 ADC/DMA，确认新数据到达且处于 12 位范围；输入精度和行程另行检查。 */
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

    /* 提前开启 GPS 串口 DMA 并初始化解析服务，使后续外设初始化期间也能接收数据。 */
    GPS_USART_Config();
    GPS_DMA_Config();
    gps_service_init();
    /* MPU 中断配置完成后尝试加载 DMP，失败时限次重试，避免无限阻塞启动。 */
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
        /* 固件初始化成功后，再单独等待有效姿态样本，区分设备就绪与实际数据可用。 */
        boot_start(BOOT_MPU_SAMPLE);
        ok = 0U;
        for(attempt = 0U; attempt < 20U; attempt++) {
            if(mpu_dmp_get_data(&pitch, &roll, &yaw) == 0U) { ok = 1U; break; }
            Delay_ms(25U);
        }
        imu_data_valid = ok;
        get_tick_count(&imu_last_sample_ms);
        boot_done(BOOT_MPU_SAMPLE, ok ? BOOT_PASS : BOOT_FAIL,
                  "Wait up to 500 ms for a decoded DMP FIFO sample");
    } else boot_skip(BOOT_MPU_SAMPLE, "Blocked: MPU6050 initialization failed");

    /* 触摸控制器通信失败时限次重试；通过只说明版本和配置通信正常。 */
    boot_start(BOOT_TOUCH);
    for(attempt = 0U; attempt < TOUCH_BOOT_ATTEMPTS; attempt++) {
        touch_result = GTP_Init_Panel();
        if(touch_result == 0) break;
        if(attempt + 1U < TOUCH_BOOT_ATTEMPTS) Delay_ms(50U);
    }
    sprintf(detail, "Controller version/config communication; result=%ld",
            (long)touch_result);
    boot_done(BOOT_TOUCH, touch_result == 0 ? BOOT_PASS : BOOT_FAIL, detail);

    /* SD 检查依次经过卡识别、文件系统挂载和文件读写；前一阶段失败则跳过依赖项。 */
    boot_start(BOOT_SD);
    for(attempt = 0U; attempt < 3U; attempt++) {
        sd_result = SD_Init();
        if(sd_result == SD_OK) break;
        if(attempt + 1U < 3U) Delay_ms(50U);
    }
    sprintf(detail, "SD command handshake/card information; result=%u", sd_result);
    boot_done(BOOT_SD, sd_result == SD_OK ? BOOT_PASS : BOOT_FAIL, detail);
    if(sd_result == SD_OK) {
        FRESULT mount_result;
        boot_start(BOOT_SD_FS);
        mount_result = f_mount(&fs_sdcard, "0:", 1);
        sprintf(detail, "Read partition/FAT metadata; mount result=%u", mount_result);
        boot_done(BOOT_SD_FS, mount_result == FR_OK ? BOOT_PASS : BOOT_FAIL, detail);
        if(mount_result == FR_OK) {
            boot_start(BOOT_SD_WRITE);
            hw_result = hardware_sd_write_test_detail(detail, sizeof(detail));
            boot_done(BOOT_SD_WRITE, hw_result == HW_PASS ? BOOT_PASS : BOOT_FAIL,
                      detail);
        } else boot_skip(BOOT_SD_WRITE, "Blocked: SD filesystem mount failed");
    } else {
        boot_skip(BOOT_SD_FS, "Blocked: SD card initialization failed");
        boot_skip(BOOT_SD_WRITE, "Blocked: SD card initialization failed");
    }

    /* 先确认 NRF 的 SPI 寄存器访问正常，待参数加载后再应用频道、功率和速率。 */
    boot_start(BOOT_NRF);
    ok = nrf24l01_check() == 0U;
    boot_done(BOOT_NRF, ok ? BOOT_PASS : BOOT_FAIL,
              "NRF RF_CH complementary-pattern write/read/restore");

    /* Flash 可用时加载或迁移持久化参数；读写失败则退回 RAM 默认值并保留失败状态。
     * Flash 探测失败时直接使用默认值，不再访问持久化存储。 */
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
    LCD_SetBrightness(param.screenBrightness);
    /* 无线参数下发后回读验证；频道和速率按驱动限幅规则比较，功率只比较对应配置位。 */
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

    /* 检查此前开启的 GPS 接收链路，有完整有效语句即可通过，无需等待卫星定位。 */
    boot_start(BOOT_GPS);
    ok = boot_gps_check();
    boot_done(BOOT_GPS, ok ? BOOT_PASS : BOOT_FAIL,
              ok ? "Received complete checksum-valid NMEA; fix not tested" :
                   "No checksum-valid NMEA within receive window");

    /* 启动运行期定时器后同时检查计数和中断活动，确认主循环能够取得任务节拍。 */
    boot_start(BOOT_TIMERS);
    BASIC_TIM6_Configuration(8400-1, 99);
    GENERAL_TIM2_InitConfiguration(65536-1,128-1);
    GENERAL_TIM3_InitConfiguration(65536-1,128-1);
    GENERAL_TIM4_InitConfiguration(8400-1, 99);
    GENERAL_TIM5_InitConfiguration(8400-1, 9);
    ok = boot_timers_check();
    boot_done(BOOT_TIMERS, ok ? BOOT_PASS : BOOT_FAIL,
              "TIM2/3/5/6 counters + TIM5/TIM6 interrupt activity");

    /* 需要实际操作、已知输入或对端设备的项目保留为未测试，留待人工诊断。 */
    boot_skip(BOOT_KEYS, "Hardware Tests: exercise keys and switches");
    boot_skip(BOOT_ANALOG, "Travel/calibration/battery accuracy require known inputs");
    boot_skip(BOOT_OUTPUTS, "Configured; LED/buzzer need physical feedback");
    boot_skip(BOOT_UART, "Configured; no external loopback fixture");
    boot_skip(BOOT_RADIO, "No peer/ACK test; SPI presence is not an RF link test");
    /* 内部存储检查仅覆盖诊断模块自有 RAM 和 ROM 常量，不代表整片存储完整性。 */
    boot_start(BOOT_INTERNAL_MEMORY);
    boot_done(BOOT_INTERNAL_MEMORY, hardware_memory_test() == HW_PASS ? BOOT_PASS : BOOT_FAIL,
              "Owned 1 KiB RAM walking patterns + ROM constants; not whole-chip integrity");
}

/* 显示并打印启动结果；失败或检查未完成时等待用户通过 OK 键确认，
 * 其他结果短暂停留后继续进入菜单。 */
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

    /* 故障结果保持显示，直到完成一次新的、经过消抖的实体 OK 按下并松开。
     * 上电时已按住的按键不能关闭结果页。 */
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
        /* 检查进度完成后短暂停留，便于阅读；此延时不计入检查进度。 */
        Delay_ms(1200U);
    }
}

/* 执行任务前，以原子操作取出并清除合并后的待处理事件；
 * 处理过程中到来的新中断仍保留为待处理状态，留给下一轮循环。
 * 返回原待处理值，并恢复调用前的中断屏蔽状态。 */
static uint8_t take_tick(volatile uint8_t *pending)
{
    uint32_t mask = __get_PRIMASK();
    uint8_t ready;
    __disable_irq();
    ready = *pending;
    *pending = 0U;
    __set_PRIMASK(mask);
    return ready;
}

/* 主循环的协作调度入口：先服务控制链路，再处理按键、触摸、GPS 和周期采样。
 * 文件解码和目录扫描也调用此服务，避免长操作饿死按键、通信和传感器任务。
 * 此处不绘制页面、不进入菜单，也不访问文件系统，防止重入。 */
static void app_background_service(void)
{
    unsigned long now;
    control_link_service(menu_control_active());
    if(take_tick(&finish_button_10ms)) {
        button_ticks();
        digital_channel_update_10ms();
        menu_tick_10ms();
    }
    GTP_Service();
    /* 及时处理已接收完整的 GPS 输入，不受 UI 刷新节奏限制。 */
    gps_service_poll();
    if(take_tick(&finish_1hz) && imu_dmp_ready) {
        temp = MPU_Get_Temperature();
        MPU_Get_Accelerometer(&aacx,&aacy,&aacz);
        MPU_Get_Gyroscope(&gyrox,&gyroy,&gyroz);
    }
    if(take_tick(&finish_100hz)) {
        get_tick_count(&now);
        /* DMP 就绪时，单次取帧失败保留上次姿态，连续一秒无样本才撤销有效标志。 */
        if(imu_dmp_ready && mpu_dmp_get_data(&pitch,&roll,&yaw) == 0U) {
            imu_data_valid = 1U;
            imu_last_sample_ms = now;
        } else if(!imu_dmp_ready || (uint32_t)(now - imu_last_sample_ms) >= 1000U) {
            imu_data_valid = 0U;
        }
    }
    if(take_tick(&finish_10hz)) RTC_TimeAndDate_Show();
}

/* 应用入口：完成启动检查与结果确认，初始化菜单和控制链路的启动准入条件，
 * 随后循环执行后台服务与菜单处理。 */
int main(void)
{
    setup();
    boot_show_result();
    LCD_SetFont(&Font16x32);
    LCD_SetColors(GREEN,BLACK);
    ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);
    menu_init();
    menu_set_background_service(app_background_service);
    /* 菜单可在启动检查失败后继续使用，但运动使能必须通过这些关键项目的检查。 */
    control_link_init(
        boot_report.items[BOOT_CLOCK].state == BOOT_PASS &&
        boot_report.items[BOOT_INTERNAL_MEMORY].state == BOOT_PASS &&
        boot_report.items[BOOT_ADC1].state == BOOT_PASS &&
        boot_report.items[BOOT_PARAMS].state == BOOT_PASS &&
        boot_report.items[BOOT_NRF_CONFIG].state == BOOT_PASS &&
        boot_report.items[BOOT_TIMERS].state == BOOT_PASS);
    menu_process();
    while(1) {
        app_background_service();
        menu_process();
    }
}
