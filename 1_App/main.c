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
#include "multi_button_user.h"
#include "common.h"
#include "nmea/nmea.h"
#include "gt9xx.h"
#include "palette.h"
#include "param.h"
#include "ugui.h"
#include "ugui_config.h"
#include "malloc.h"

#include <stddef.h>

#include "inv_mpu.h"

extern unsigned int Task_Delay[5];

extern volatile uint16_t ADC1_Value[NUM_OF_ADC1CHANNEL];
extern volatile uint16_t ADC3_Value[NUM_OF_ADC3CHANNEL];

extern SD_Error Status;
extern volatile  param_Config param;;
extern param_Config * pt_param;

extern FATFS fs;                   	/* FatFs�ļ�ϵͳ���� */
extern FIL fnew_sdcard;				/* �ļ����� */
extern FRESULT res;                	/* �ļ�������� */
extern unsigned int fnum;			/* �ļ��ɹ���д���� */

uint32_t *p1=0;
uint8_t sramx=1;			//0:�ڲ�SRAM 1:�ⲿSRAM

float pitch,roll,yaw; 		//dmp����ŷ����
short aacx,aacy,aacz;		//���ٶȴ�����ԭʼ����
short gyrox,gyroy,gyroz;	//������ԭʼ����
short temp;					//�¶�
float yaw_new;

int main(void)
{
	static uint8_t snipaste_name_count = 0;
	char snipaste_name[40];
	
	SysTick_Init();
	Exti_Init();
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	Debug_USART_Config();
	RTC_Config();
	EXPAND_USART_Config();
	LED_GPIO_Config();
	STICK_GPIO_Config();
	EEPROM_I2C_Init();
	FLASH_SPI_Init();
	NRF_SPI_Init();
	SRAM_FSMC_Config();
	ILI9806G_Init();
	MPU_Init();
	EXTI_MPU_Config();
	GPS_USART_Config();
	GPS_DMA_Config();
	user_BUTTON_init();
	Independent_Dual_ADC1_Init();
	Independent_Dual_ADC3_Init();
	while(mpu_dmp_init())
	{
		printf("MPU6050 DMP ��ʼ��ʧ�ܣ�\n\r");
		Delay_ms(200);
	}
	printf("MPU6050 DMP ��ʼ���ɹ���\n\r");
	
	if((Status = SD_Init()) != SD_OK)
	{    
		printf("SD����ʼ��ʧ�ܣ�\r\n");
	}
	else
	{
		printf("SD����ʼ���ɹ���\r\n");		 
	}
	GTP_Init_Panel();
	printf("\r\n******************************��ʼ�����***********************************\r\n");

	eeprom_test();
	flash_test();
	if(sram_read_write_test() == 1)
	{
		printf("sram ���Գɹ�\r\n");
		// my_mem_init(SRAMIN);		//��ʼ���ڲ��ڴ��
		// my_mem_init(SRAMEX);		//��ʼ���ⲿ�ڴ��
	}
	fatfs_flash_test();
	fatfs_flash_test2();
	fatfs_sdcard_test();
	nrf24l01_check();
	
	/*����sd�ļ�ϵͳ*/
	res = f_mount(&fs,"0:",1);
	if(res != FR_OK)
	{
		printf("\r\n�������������Ѹ�ʽ����fat��ʽ��SD����\r\n");
	}
	LCD_Show_BMP(100,100,"0:Pictures/football.bmp"); //srcdata/Picture/football.bmp
	Delay_ms(1500);
	jpgDisplay("0:Pictures/musicplayer.jpg");
	Delay_ms(1500);
	
	//NMEA����������ʾ��ʼ��׼��
	nmea_decode_init();
	LCD_SetFont(&Font16x32);
	LCD_SetColors(GREEN,BLACK);	
	ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);
	
	//��ʾָ����С�ַ��Ա�
	ILI9806G_DispStringLine_EN_CH(LINE(0),"Remoter");
	ILI9806G_DisplayStringEx(0,1*48,48,48,(uint8_t *)"Remoter",0);
	ILI9806G_DisplayStringEx(0,2*56,56,56,(uint8_t *)"Remoter",0);
	Delay_ms(1000);
	
	USART_printf(EXPAND_USART,"THIS IS UART4\r\n");
	USART_printf(EXPAND_USART,"UART4��������\r\n");
	
	write_default_param();

	read_param(param.RecWarnBatVolt,  PARAM_FLASH_SAVE_ADDR + offsetof(param_Config, RecWarnBatVolt));
	read_param(param.chMiddle[1],     PARAM_FLASH_SAVE_ADDR + offsetof(param_Config, chMiddle[1]));
	read_param(param.clockTime,       PARAM_FLASH_SAVE_ADDR + offsetof(param_Config, clockTime));
	printf("RecWarnBatVolt: %f\r\n", param.RecWarnBatVolt);
	printf("chMiddle[1]:%d\r\n",param.chMiddle[1]);
	printf("clockTime:%d\r\n",param.clockTime);

	//malloc
	// printf("\n\r=================malloc================\n\r");
	// printf ( "SRAMIN USED:%d%%\r\n", my_mem_perused(SRAMIN) );//��ʾ�ڲ��ڴ�ʹ����
	// printf ( "SRAMEX USED:%d%%\r\n", my_mem_perused(SRAMEX) );//��ʾ�ⲿ�ڴ�ʹ����
	// p1 = mymalloc ( sramx, 1024 * 16 );//����2K�ֽ�
	// if(p1 == NULL)
	// {
	// 	printf("mymalloc error!,p1����ʧ�ܣ�\r\n");
	// }
	// else{
	// 	*(p1+0) = 548;
	// 	*(p1+1) = 1048;
	// 	*(p1+2) = 2048;
	// 	*(p1+3) = 3048;
	// 	*(p1+4) = 4048;
	// 	printf(" *(p1+0) = %d\n\r *(p1+1) = %d\n\r *(p1+2) = %d\n\r *(p1+3) = %d\n\r *(p1+4) = %d\n\r",*(p1+0),*(p1+1),*(p1+2),*(p1+3),*(p1+4));
	// 	printf ( "SRAMEX USED:%d%%\r\n", my_mem_perused(SRAMEX) );//��ʾ�ⲿ�ڴ�ʹ����
	// 	myfree(sramx,p1);											//�ͷ��ڴ�
	// 	printf ( "SRAMEX USED:%d%%\r\n", my_mem_perused(SRAMEX) );//��ʾ�ⲿ�ڴ�ʹ����
	// }
	// p1=0;														//ָ��յ�ַ

	//��ͼ��غ�������ͼʱ����� ,������Сjpg��С
	//�������ý�ͼ���֣���ֹ�ظ���ʵ��Ӧ���п���ʹ��ϵͳʱ����������
	snipaste_name_count++; 
	sprintf(snipaste_name,"0:screen_shot_%d.bmp",snipaste_name_count);

	printf("\r\n���ڽ�ͼ...");	
	/*��ͼ�������ú�Һ����ʾ����ͽ�ͼ����*/
	ILI9806G_GramScan(LCD_SCAN_MODE);			
	
	if(Screen_Shot(0,0,LCD_X_LENGTH,LCD_Y_LENGTH,snipaste_name) == 0)
	{
		printf("\r\n��ͼ�ɹ���");
	}
	else
	{
		printf("\r\n��ͼʧ�ܣ�");
	}
	f_mount(NULL,"0:",1);
	
	BASIC_TIM6_Configuration(8400-1, 99); 			//���ڣ�1ms
	GENERAL_TIM2_InitConfiguration(65536-1,128-1);	//���ڣ�100ms
	GENERAL_TIM3_InitConfiguration(65536-1,128-1);	//���ڣ�50ms
	GENERAL_TIM4_InitConfiguration(8400-1, 99);		//���ڣ�1ms
	GENERAL_TIM5_InitConfiguration(8400-1, 999);	//���ڣ�10ms
	//BASIC_TIM7_InitConfiguration(10000-1,168-1); 		//���ڣ�1ms
	printf("\r\n***************************��ʱ����ʼ�����***********************************\r\n");
	
    while(1)
    {
		if(Task_Delay[0] == 0)
		{
			if(mpu_dmp_get_data(&pitch,&roll,&yaw)==0)
			{ 
				temp = MPU_Get_Temperature();				//�õ��¶�ֵ
				// MPU_Get_Accelerometer(&aacx,&aacy,&aacz);	//�õ����ٶȴ���������
				// MPU_Get_Gyroscope(&gyrox,&gyroy,&gyroz);	//�õ�����������
			}
			
			Task_Delay[0]=100;
		}
		
		if(Task_Delay[1] == 0)
		{
			Task_Delay[1]=200;
		}
		
		if(Task_Delay[2] == 0)
		{
			
			Task_Delay[2]=300;
		}
		
		if(Task_Delay[3] == 0)
		{
			Task_Delay[3]=400;	
		}
	}
}



