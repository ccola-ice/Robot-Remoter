#include "bsp_Systick.h"
#include "bsp_exti.h"
#include "bsp_rtc.h"

#include "bsp_gpio_led.h"
#include "bsp_gpio_button.h"
#include "bsp_gpio_stick.h"

#include "bsp_usart_debug.h"
#include "bsp_usart_extra.h"
#include "bsp_usart_gps.h"

#include "bsp_i2c_eeprom.h"
#include "bsp_i2c_mpu6050.h"
#include "bsp_i2c_touch.h"
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

void setup(void)
{
	u8 dmp_result = MPU_DMP_INIT_ERROR_DEVICE;
	u8 dmp_attempt;
	char dmp_status[48];

	// put your setup code here, to run once:
	SysTick_Init();
	Exti_Init();
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	Debug_USART_Config();
	EXPAND_USART_Config();
	RTC_Config();
	LED_GPIO_Config();
	STICK_GPIO_Config();
	EEPROM_I2C_Init();
	FLASH_SPI_Init();
	NRF_SPI_Init();
	SRAM_FSMC_Config();
	ILI9806G_Init();
	ILI9806G_GramScan(LCD_SCAN_MODE);
	gui_boot_begin();
	gui_boot_update(18U, 0U, "Display controller online", 0U);

	gui_boot_update(22U, 1U, "MPU6050 interface prepared", 0U);
	EXTI_MPU_Config();
	GPS_USART_Config();
	GPS_DMA_Config();
	user_BUTTON_init();
	Independent_Dual_ADC1_Init();
	Independent_Dual_ADC3_Init();
	gui_boot_update(30U, 1U, "Input devices configured", 0U);
	gui_boot_update(34U, 1U, "Starting MPU6050 DMP", 0U);
	for(dmp_attempt = 0U; dmp_attempt < MPU_DMP_BOOT_ATTEMPTS; dmp_attempt++)
	{
		dmp_result = mpu_dmp_init();
		if(dmp_result == MPU_DMP_INIT_OK)
		{
			imu_dmp_ready = 1U;
			break;
		}

		printf("MPU6050 DMP初始化失败：阶段错误码 %u，第%u/%u次。\r\n",
			   (uint16_t)dmp_result, (uint16_t)(dmp_attempt + 1U),
			   (uint16_t)MPU_DMP_BOOT_ATTEMPTS);
		if((dmp_attempt + 1U) < MPU_DMP_BOOT_ATTEMPTS)
		{
			sprintf(dmp_status, "DMP error E%u, retry %u/%u",
					(uint16_t)dmp_result, (uint16_t)(dmp_attempt + 2U),
					(uint16_t)MPU_DMP_BOOT_ATTEMPTS);
			gui_boot_update(34U, 1U, dmp_status, 1U);
			Delay_ms(50U);
		}
	}
	if(imu_dmp_ready != 0U)
	{
		printf("MPU6050 DMP库初始化成功！\r\n");
		gui_boot_update(42U, 1U, "MPU6050 DMP online", 0U);
	}
	else
	{
		sprintf(dmp_status, "MPU6050 offline - DMP error E%u",
				(uint16_t)dmp_result);
		printf("MPU6050已离线，系统继续启动。错误码：%u\r\n",
			   (uint16_t)dmp_result);
		gui_boot_update(42U, 1U, dmp_status, 1U);
	}
	gui_boot_update(48U, 2U, "Starting SD card", 0U);
	while(SD_Init() != SD_OK)
	{    
		gui_boot_update(48U, 2U, "SD card retrying", 1U);
		printf("SD卡初始化失败！\r\n");
	}
	printf("SD卡初始化成功！\r\n");
	gui_boot_update(55U, 2U, "SD card online", 0U);
	GTP_Init_Panel();
	gui_boot_update(62U, 2U, "Touch controller online", 0U);
	res = f_mount(&fs_sdcard,"0:",1);//挂载sd文件系统
	if(res != FR_OK )
	{
		gui_boot_update(62U, 2U, "SD filesystem mount failed", 1U);
		printf("\r\nSD卡文件系统挂载失败，检查SD卡格式！(%d)\r\n",res);
		while(1);
	}
	gui_boot_update(68U, 2U, "SD filesystem mounted", 0U);
	res = f_mount(&fs_flash,"1:",1);//挂载flash文件系统
	if(res!=FR_OK)
	{
		gui_boot_update(68U, 2U, "Flash filesystem failed", 1U);
		printf("\r\n外部Flash文件系统挂载失败！(%d)\r\n",res);
		while(1);
	}
	gui_boot_update(74U, 2U, "Flash filesystem mounted", 0U);
	if(nrf24l01_check() != 0)
	{
		gui_boot_update(80U, 2U, "NRF24L01 not detected", 1U);
		printf("NRF模块未连接，继续启动，可在菜单中重新检测。\r\n");
	}
	else
	{
		gui_boot_update(80U, 2U, "NRF24L01 online", 0U);
	}
	nmea_decode_init();//NMEA解码初始化准备
	write_default_param();
	nrf24l01_apply_settings(param.NRF_Mode, param.NRF_Channel,
						param.NRF_Power, param.NRF_DataRate);
	gui_boot_update(84U, 2U, "Runtime services ready", 0U);

	printf("\r\n*****************************初始化设置完成**********************************\r\n");
}

int hardware_test(void)
{
	eeprom_test();
	gui_boot_update(87U, 2U, "EEPROM test completed", 0U);
	flash_test();
	gui_boot_update(90U, 2U, "SPI Flash test completed", 0U);
	if(sram_read_write_test() == 1)
	{
		printf("sram 测试成功\r\n");
		gui_boot_update(92U, 2U, "External SRAM test passed", 0U);
	}
	else
	{
		gui_boot_update(92U, 2U, "External SRAM test failed", 1U);
	}
	fatfs_flash_test();
	fatfs_flash_test2();
	gui_boot_update(95U, 2U, "Flash filesystem tested", 0U);
	fatfs_sdcard_test();
	gui_boot_update(97U, 2U, "SD filesystem tested", 0U);
	
	//扩展串口4测试
	USART_printf(EXPAND_USART,"THIS IS UART4\r\n");
	USART_printf(EXPAND_USART,"UART4测试正常\r\n");
	gui_boot_update(99U, 2U, "Self-test sequence complete", 0U);


	return 0;
}

int main(void)
{
	setup();

	hardware_test();
	gui_boot_finish();

	LCD_SetFont(&Font16x32);
	LCD_SetColors(GREEN,BLACK);	
	ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);
	menu_init();
	menu_process();

	BASIC_TIM6_Configuration(8400-1, 99); 				//周期：10ms
	GENERAL_TIM2_InitConfiguration(65536-1,128-1);		//周期：100ms
	GENERAL_TIM3_InitConfiguration(65536-1,128-1);		//周期：50ms
	GENERAL_TIM4_InitConfiguration(8400-1, 99);			//周期：10ms
	GENERAL_TIM5_InitConfiguration(8400-1, 9);			//周期：1ms
	//BASIC_TIM7_InitConfiguration(10000-1,168-1);		//周期：1ms

    while(1)
    {
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
			menu_tick_10ms();
		}

		if(finish_100hz == 1)
		{

			finish_100hz = 0;
		}

		menu_process();
	}
}



