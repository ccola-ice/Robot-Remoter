#include "gui.h"
#include "bsp_fsmc_lcd.h"
#include "bsp_adc1_independent_dual.h"
#include "bsp_adc3_independent_dual.h"
#include "bsp_usart_debug.h"
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h" 
#include "bsp_mpu6050.h"
#include "ugui.h"
#include "ugui_config.h"
#include "nmea/nmea.h"
#include "palette.h"
#include "gt9xx.h"
#include "bsp_i2c_touch.h"

void uGUI_window_1_callback (UG_MESSAGE* msg);
UG_GUI gui;
UG_WINDOW window_1 ;
UG_BUTTON button_1 ;
UG_BUTTON button_2 ;
UG_BUTTON button_3 ;
UG_TEXTBOX textbox_1 ;
UG_OBJECT obj_buff_wnd_1[10] ;	

extern volatile uint16_t ADC1_Value[NUM_OF_ADC1CHANNEL];
extern volatile uint16_t ADC3_Value[NUM_OF_ADC3CHANNEL];

extern nmeaINFO info;          	//GPS解码后得到的信息
extern nmeaTIME beiJingTime;    		//北京时间
extern double deg_lat;//转换成[degree].[degree]格式的纬度
extern double deg_lon;//转换成[degree].[degree]格式的经度


float pitch,roll,yaw; 		//欧拉角
short aacx,aacy,aacz;		//加速度传感器原始数据
short gyrox,gyroy,gyroz;	//陀螺仪原始数据
short temp;					//温度

char displayBuffer[100];

uint8_t display_flag = 0;	//界面切换标志，每次只有第一次切换界面时才清屏（ILI9806G_Clear），其他情况不清屏

void system_basic_information(void)
{	
	if(display_flag == 1)
	{
		display_flag = 0;
		ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);
		I2C_GTP_IRQDisable();
	}
	
	LCD_SetFont(&Font24x48);
	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLACK);
	
	ILI9806G_DispStringLine_EN(LINE(0),"version: Remoter v1.0");
	ILI9806G_DispStringLine_EN(LINE(1),"MCU:STM32F407ZGT6 168MHZ");
	ILI9806G_DispStringLine_EN(LINE(2),"INTER_RAM:192KB INTER_FLASH:1MB");
	ILI9806G_DispStringLine_EN(LINE(3),"EXTER_RAM:1MB   EXTER_FLASH:16MB");
	ILI9806G_DispStringLine_EN(LINE(4),"resolution:480x800px");
	ILI9806G_DispStringLine_EN(LINE(5),"LCD driver:ILI9806G");
	ILI9806G_DispStringLine_EN(LINE(6),"TOUCH driver:GT911");
	ILI9806G_DispStringLine_EN(LINE(7),"About:zb18747639545@163.com");
}

void main_menu(void)
{
	if(display_flag == 1)
	{
		display_flag = 0;
		ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);
		I2C_GTP_IRQDisable();
	}
	
	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(RED);
	LCD_SetFont(&Font24x48);
	ILI9806G_DispStringLine_EN(LINE(0),"Remoter-1.0");
	LCD_SetFont(&Font24x48);
	ILI9806G_DispStringLine_EN(LINE(1),"Welcome to use!");
}

void system_data_read_and_set()
{
	if(display_flag == 1)
	{
		display_flag = 0;
		ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);
		I2C_GTP_IRQDisable();
	}
	LCD_SetFont(&Font16x32);
	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLACK);
	
	/* LCD显示 */
	/* 设置前景颜色（字体颜色）*/
	LCD_SetTextColor(BLUE);
	
	//ILI9806G_DispStringLine_EN(LINE(0)," GPS Info:");
	ILI9806G_DispStringLine_EN_CH(LINE(0),"野火4.3寸LCD参数：");
	/* 设置前景颜色（字体颜色）*/
	//LCD_SetTextColor(WHITE);
	
	/* 显示时间日期 */
	sprintf(displayBuffer," Date:%4d/%02d/%02d ", beiJingTime.year+1900, beiJingTime.mon,beiJingTime.day);
	ILI9806G_DispStringLine_EN(LINE(1),displayBuffer);
  
	sprintf(displayBuffer," 时间:%02d:%02d:%02d", beiJingTime.hour,beiJingTime.min,beiJingTime.sec);
	//sprintf(displayBuffer," Time:%02d:%02d:%02d", beiJingTime.hour,beiJingTime.min,beiJingTime.sec);
	ILI9806G_DispStringLine_EN_CH(LINE(2),displayBuffer);
		
	/* 纬度 经度*/
	sprintf(displayBuffer," lat :%.6f ", deg_lat);
	ILI9806G_DispStringLine_EN(LINE(3),displayBuffer);
	
	sprintf(displayBuffer," lon :%.6f",deg_lon);
	ILI9806G_DispStringLine_EN(LINE(4),displayBuffer);

	/* 正在使用的卫星 可见的卫星*/
	sprintf(displayBuffer," GPS Sat in use:%2d ", info.satinfo.inuse);
	ILI9806G_DispStringLine_EN(LINE(5),displayBuffer);    

	sprintf(displayBuffer," GPS Sat in view:%2d", info.satinfo.inview);
	ILI9806G_DispStringLine_EN(LINE(6),displayBuffer);    

	/* 正在使用的卫星 可见的卫星*/
	sprintf(displayBuffer," BDS Sat in use:%2d ", info.BDsatinfo.inuse);
	ILI9806G_DispStringLine_EN(LINE(7),displayBuffer);    
	
	sprintf(displayBuffer," BDS Sat in view:%2d", info.BDsatinfo.inview);
	ILI9806G_DispStringLine_EN(LINE(8),displayBuffer);    
	
	/* 海拔高度 */
	sprintf(displayBuffer," Altitude:%4.2f m", info.elv);
	ILI9806G_DispStringLine_EN(LINE(9),displayBuffer);
	
	/* 速度 */
	sprintf(displayBuffer," speed:%4.2f km/h", info.speed);
	ILI9806G_DispStringLine_EN(LINE(10),displayBuffer);
	
	/* 航向 */
	sprintf(displayBuffer," Angle:%3.2f deg", info.direction);
	ILI9806G_DispStringLine_EN(LINE(11),displayBuffer);

}

void mpu6050_euler_information(void)
{
	if(display_flag == 1)
	{
		display_flag = 0;
		ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);
		I2C_GTP_IRQDisable();
	}
	
	LCD_SetFont(&Font16x32);
	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLACK);
	
	ILI9806G_DispStringLine_EN(LINE(0),"4.3 inch LCD");
	ILI9806G_DispStringLine_EN(LINE(1),"resolution:480x800px");
	ILI9806G_DispStringLine_EN(LINE(2),"LCD driver:ILI9806G");
	ILI9806G_DispStringLine_EN(LINE(3),"Touch driver:GT911");
//	if(mpu_dmp_get_data(&pitch,&roll,&yaw)!=0)
//	{
////		printf("%d\r\n",mpu_dmp_get_data(&pitch,&roll,&yaw));
////		temp = MPU_Get_Temperature();				//得到温度值
////		MPU_Get_Accelerometer(&aacx,&aacy,&aacz);	//得到加速度传感器数据
////		MPU_Get_Gyroscope(&gyrox,&gyroy,&gyroz);	//得到陀螺仪数据
//	}
	sprintf(displayBuffer,"mpu_dmp_get_data error!");
	ILI9806G_DispStringLine_EN(LINE(4),displayBuffer);
	
	sprintf(displayBuffer,"pitch:%2.3f roll:%2.3f yaw:%2.3f temp:%4d",pitch,roll,yaw,temp);
	ILI9806G_DispStringLine_EN(LINE(5),displayBuffer);
	
	LCD_SetTextColor(RED);
	sprintf(displayBuffer,"CH1:%4d CH2:%4d CH3:%4d CH4:%4d CH5:%4d ", ADC1_Value[0],ADC1_Value[1],ADC1_Value[2],ADC1_Value[3],ADC1_Value[4]);
	ILI9806G_DispStringLine_EN(LINE(6),displayBuffer);
	sprintf(displayBuffer,"CH6:%4d CH7:%4d CH8:%4d CH9:%4d CH10:%4d ",ADC1_Value[5],ADC1_Value[6],ADC3_Value[0],ADC3_Value[1],ADC3_Value[2]);
	ILI9806G_DispStringLine_EN(LINE(7),displayBuffer);
	
}

//绘制触摸画板界面
void Draw_Board(void)
{
	if(display_flag == 1)
	{
		display_flag = 0;
		ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);
		I2C_GTP_IRQEnable();
		Palette_Init(LCD_SCAN_MODE);
	}	
}

void uGUI(void)
{	
	if(display_flag == 1)
	{
		display_flag = 0;
		ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);
		I2C_GTP_IRQDisable();
		
		UG_Init(&gui, (void(*)(UG_U16,UG_U16,UG_COLOR))_HW_DrawPoint,800,480);
		UG_DriverRegister(DRIVER_DRAW_LINE,(void *)_HW_DrawLine);
		UG_DriverRegister(DRIVER_FILL_FRAME,(void *)_HW_FillFrame);
		UG_DriverEnable(DRIVER_DRAW_LINE);
		UG_DriverEnable(DRIVER_FILL_FRAME);
	
		UG_FontSelect ( &FONT_16X26 ) ;
		UG_FillScreen ( C_YELLOW_GREEN ) ;
		UG_PutChar('B',0,0,C_BLACK,C_YELLOW_GREEN);
		UG_PutNum(800-32,480-26,99,2,C_WHITE,C_YELLOW_GREEN);

		UG_WindowCreate(&window_1 , obj_buff_wnd_1 , 20, uGUI_window_1_callback);
		UG_WindowSetTitleText ( &window_1 , "Remoter-Settings") ;
		UG_WindowSetTitleTextFont ( &window_1 , &FONT_16X26 ) ;

		UG_ButtonCreate (&window_1 , &button_1 , BTN_ID_0 , 10, 10, 150 , 80 ) ;
//		UG_ButtonCreate (&window_1 , &button_2 , BTN_ID_1 , 10, 80 , 150 , 150 ) ;
//		UG_ButtonCreate (&window_1 , &button_3 , BTN_ID_2 , 10, 150 , 150 , 220 ) ;

		UG_ButtonSetFont(&window_1 , BTN_ID_0, &FONT_12X20 );
		UG_ButtonSetText(&window_1 , BTN_ID_0, "NRF24L01\nA");
		UG_ButtonSetFont(&window_1 , BTN_ID_1, &FONT_12X20 );
		UG_ButtonSetText(&window_1 , BTN_ID_1, "GPS\nB");
		UG_ButtonSetFont(&window_1 , BTN_ID_2, &FONT_12X20 );
		UG_ButtonSetText(&window_1,  BTN_ID_2, "Channels\nC");

		UG_TextboxCreate(&window_1,&textbox_1,TXB_ID_0,120,10,310,200);
		UG_TextboxSetFont(&window_1,TXB_ID_0,&FONT_12X16);
		UG_TextboxSetText(&window_1,TXB_ID_0,"Thisisjust\naverysimple\nwindowto\ndemonstrate\nsomebasic\nfeaturesofuGUI！");
		UG_TextboxSetForeColor(&window_1,TXB_ID_0,C_BLACK);
		UG_TextboxSetAlignment(&window_1,TXB_ID_0,ALIGN_CENTER);
		
	}UG_WindowShow(&window_1);
}

void uGUI_window_1_callback (UG_MESSAGE* msg)
{
	if ( msg->type == MSG_TYPE_OBJECT )
	{
		if ( msg->id == OBJ_TYPE_BUTTON )
		{
			switch ( msg->sub_id )
			{
				case BTN_ID_0:
				{
					printf("BTN_ID_0\n\r");
					break ;
				}
				case BTN_ID_1:
				{
					printf("BTN_ID_1\n\r");
					break ;
				}
				case BTN_ID_2:
				{
					printf("BTN_ID_2\n\r");
					break ;
				}
			}
		}
	}
}

