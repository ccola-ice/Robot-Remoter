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
#include "platform_mpu.h"
#include "FLASH_test.h"
#include "EEPROM_test.h"
#include "SDCARD_test.h"
#include "SRAM_test.h"
#include "FATFS_FLASH_test.h"
#include "FATFS_SDCARD_test.h"
#include "LCD_test.h"
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
#include "malloc.h"

#include <stddef.h>

#include "inv_mpu.h"

#define MALLOC_TEST 0

extern unsigned int Task_Delay[5];

extern volatile uint16_t ADC1_Value[NUM_OF_ADC1CHANNEL];
extern volatile uint16_t ADC3_Value[NUM_OF_ADC3CHANNEL];

extern SD_Error Status;
extern volatile  param_Config param;;
extern param_Config * pt_param;

FATFS fs_sdcard;                   	/* SD卡 FatFs工作区 */
FATFS fs_flash;                    	/* SPI Flash FatFs工作区 */
extern FIL fnew_sdcard;				/* 文件对象 */
extern FRESULT res;                	/* 文件操作结果 */
extern unsigned int fnum;			/* 文件成功读写数量 */

uint32_t *p1=0;
uint8_t sramx=1;			//0:内部SRAM 1:外部SRAM

float pitch,roll,yaw; 		//dmp解算欧拉角
short aacx,aacy,aacz;		//加速度传感器原始数据
short gyrox,gyroy,gyroz;	//陀螺仪原始数据
short temp;					//温度
uint8_t imu_data_valid;
static uint8_t imu_read_failures;
float yaw_new;

u8 finish_1hz=0,finish_2hz=0,finish_5hz=0,finish_10hz=0,finish_20hz=0,finish_33hz=0,finish_50hz=0,finish_100hz=0;
volatile u8 finish_button_10ms=0;

void setup(void)
{
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

	if(MPU_Init() != 0U)	//使用的是软件I2C
	{
		gui_boot_update(22U, 1U, "MPU6050 base init failed", 1U);
	}
	EXTI_MPU_Config();
	GPS_USART_Config();
	GPS_DMA_Config();
	user_BUTTON_init();
	Independent_Dual_ADC1_Init();
	Independent_Dual_ADC3_Init();
	gui_boot_update(30U, 1U, "Input devices configured", 0U);
	gui_boot_update(34U, 1U, "Starting MPU6050 DMP", 0U);
	while(mpu_dmp_init() != 0)
	{
		gui_boot_update(34U, 1U, "MPU6050 DMP retrying", 1U);
		printf("MPU6050 DMP 初始化失败！\n\r");
	}
	printf("MPU6050 DMP库 初始化成功！\n\r");
	gui_boot_update(42U, 1U, "MPU6050 DMP online", 0U);
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
		// my_mem_init(SRAMIN);		//初始化内部内存池
		// my_mem_init(SRAMEX);		//初始化外部内存池
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

#if MALLOC_TEST
	printf("\n\r=================malloc================\n\r");
	printf ( "SRAMIN USED:%d%%\r\n", my_mem_perused(SRAMIN) );//显示内部内存使用率
	printf ( "SRAMEX USED:%d%%\r\n", my_mem_perused(SRAMEX) );//显示外部内存使用率
	p1 = mymalloc ( sramx, 1024 * 16 );//申请2K字节
	if(p1 == NULL){
	printf("mymalloc error!,p1返回失败！\r\n");
	}
	else{
		*(p1+0) = 548;
		*(p1+1) = 1048;
		*(p1+2) = 2048;
		*(p1+3) = 3048;
		*(p1+4) = 4048;
		printf(" *(p1+0) = %d\n\r *(p1+1) = %d\n\r *(p1+2) = %d\n\r *(p1+3) = %d\n\r *(p1+4) = %d\n\r",*(p1+0),*(p1+1),*(p1+2),*(p1+3),*(p1+4));
		printf ( "SRAMEX USED:%d%%\r\n", my_mem_perused(SRAMEX) );//显示外部内存使用率
		myfree(sramx,p1);											//释放内存
		printf ( "SRAMEX USED:%d%%\r\n", my_mem_perused(SRAMEX) );//显示外部内存使用率
	}
	p1=0;														//指向空地
#endif

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
	
	read_param(param.RecWarnBatVolt, PARAM_FLASH_SAVE_ADDR + offsetof(param_Config, RecWarnBatVolt));
	read_param(param.chMiddle[1],    PARAM_FLASH_SAVE_ADDR + offsetof(param_Config, chMiddle[1]));
	read_param(param.clockTime,      PARAM_FLASH_SAVE_ADDR + offsetof(param_Config, clockTime));
	printf("RecWarnBatVolt: %f\r\n", param.RecWarnBatVolt);
	printf("chMiddle[1]:%d\r\n", param.chMiddle[1]);
	printf("clockTime:%d\r\n", param.clockTime);

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
			temp = MPU_Get_Temperature();
			MPU_Get_Accelerometer(&aacx,&aacy,&aacz);
			MPU_Get_Gyroscope(&gyrox,&gyroy,&gyroz);
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
			if(mpu_dmp_get_data(&pitch,&roll,&yaw) == 0)
			{
				imu_data_valid = 1U;
				imu_read_failures = 0U;
			}
			else
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
			RTC_TimeAndDate_Show(); // 显示时间和日期
			finish_10hz = 0;
		}

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



