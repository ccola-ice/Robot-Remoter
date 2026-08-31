#include "gui.h"
#include "bsp_fsmc_lcd.h"
#include "bsp_adc1_independent_dual.h"
#include "bsp_adc3_independent_dual.h"
#include "bsp_usart_debug.h"
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h" 
#include "bsp_mpu6050.h"
#include "nmea/nmea.h"
#include "palette.h"
#include "gt9xx.h"
#include "bsp_i2c_touch.h"

extern volatile uint16_t ADC1_Value[NUM_OF_ADC1CHANNEL];
extern volatile uint16_t ADC3_Value[NUM_OF_ADC3CHANNEL];

extern nmeaINFO info;          		//GPS解码后得到的信息
extern nmeaTIME beiJingTime; 		//北京时间
extern double deg_lat;				//转换成[degree].[degree]格式的纬度
extern double deg_lon;				//转换成[degree].[degree]格式的经度

extern float pitch,roll,yaw; 		//欧拉角
extern short aacx,aacy,aacz;		//加速度传感器原始数据
extern short gyrox,gyroy,gyroz;		//陀螺仪原始数据
extern short temp;					//温度

/* ARM scatter-loading symbols: their addresses are the linker-calculated sizes. */
extern uint8_t Image$$ER_IROM1$$Length;
extern uint8_t Image$$RW_IRAM1$$Length;
extern uint8_t Image$$RW_IRAM1$$ZI$$Length;

#define GUI_MCU_FLASH_BYTES (1024UL * 1024UL)
#define GUI_MCU_RAM_BYTES   (128UL * 1024UL)

char displayBuffer[100];

static uint8_t display_flag = 0;	//界面切换标志，每次只有第一次切换界面时才清屏（ILI9806G_Clear），其他情况不清屏

void gui_prepare_page(void)
{
	display_flag = 1;
}

static void gui_draw_progress_bar(uint16_t x, uint16_t y, uint16_t width,
							  uint16_t height, uint32_t percent_x10,
							  uint16_t fill_color)
{
	uint16_t inner_width;
	uint16_t filled_width;

	if(percent_x10 > 1000UL)
	{
		percent_x10 = 1000UL;
	}

	inner_width = (width > 4U) ? (width - 4U) : 0U;
	filled_width = (uint16_t)((inner_width * percent_x10 + 500UL) / 1000UL);

	LCD_SetTextColor(GREY);
	ILI9806G_DrawRectangle(x, y, width, height, 1U);
	if(filled_width > 0U)
	{
		LCD_SetTextColor(fill_color);
		ILI9806G_DrawRectangle(x + 2U, y + 2U, filled_width, height - 4U, 1U);
	}
	LCD_SetTextColor(BLACK);
	ILI9806G_DrawRectangle(x, y, width, height, 0U);
}

static void gui_draw_channel_card(uint16_t x, uint16_t y, uint8_t channel,
							  uint16_t value, uint16_t bar_color)
{
	uint16_t shown_value = (value > 4095U) ? 4095U : value;
	uint16_t bar_x = x + 170U;
	uint16_t bar_width = 206U;

	LCD_SetTextColor(GREY);
	ILI9806G_DrawRectangle(x, y, 392U, 46U, 1U);
	LCD_SetBackColor(GREY);
	LCD_SetTextColor(BLACK);
	sprintf(displayBuffer, "CH%02u  %4u", (uint16_t)(channel + 1U), value);
	ILI9806G_DispString_EN(x + 8U, y + 7U, displayBuffer);

	gui_draw_progress_bar(bar_x, y + 13U, bar_width, 20U,
						  ((uint32_t)shown_value * 1000UL) / 4095UL,
						  bar_color);
	LCD_SetTextColor(RED);
	ILI9806G_DrawLine(bar_x + bar_width / 2U, y + 11U,
					  bar_x + bar_width / 2U, y + 35U);
}

void system_basic_information(void)
{
	uint32_t code_ro_bytes;
	uint32_t flash_bytes;
	uint32_t ram_bytes;
	uint32_t zi_bytes;
	uint32_t rw_bytes;
	uint32_t flash_percent_x10;
	uint32_t ram_percent_x10;

	if(display_flag == 1)
	{
		display_flag = 0;
		ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);
		I2C_GTP_IRQDisable();
	}
	
	code_ro_bytes = (uint32_t)(uintptr_t)&Image$$ER_IROM1$$Length;
	rw_bytes = (uint32_t)(uintptr_t)&Image$$RW_IRAM1$$Length;
	zi_bytes = (uint32_t)(uintptr_t)&Image$$RW_IRAM1$$ZI$$Length;
	ram_bytes = rw_bytes + zi_bytes;
	flash_bytes = code_ro_bytes + rw_bytes;
	flash_percent_x10 = (flash_bytes * 1000UL + GUI_MCU_FLASH_BYTES / 2UL) /
						GUI_MCU_FLASH_BYTES;
	ram_percent_x10 = (ram_bytes * 1000UL + GUI_MCU_RAM_BYTES / 2UL) /
					GUI_MCU_RAM_BYTES;

	LCD_SetFont(&Font16x32);
	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLUE);
	ILI9806G_DispString_EN(4U, LINE(0), "System / Memory Information");
	LCD_SetTextColor(BLACK);
	ILI9806G_DispString_EN(4U, LINE(1), "MCU: STM32F407ZGT6 @ 168 MHz");

	sprintf(displayBuffer, "Flash linked: %lu.%01lu / 1024.0 KB   %lu.%01lu%% ",
			flash_bytes / 1024UL, ((flash_bytes % 1024UL) * 10UL) / 1024UL,
			flash_percent_x10 / 10UL, flash_percent_x10 % 10UL);
	ILI9806G_DispString_EN(4U, LINE(2), displayBuffer);
	gui_draw_progress_bar(20U, LINE(3) + 5U, 760U, 22U, flash_percent_x10, BLUE);

	sprintf(displayBuffer, "RAM linked: %lu.%01lu / 128.0 KB      %lu.%01lu%% ",
			ram_bytes / 1024UL, ((ram_bytes % 1024UL) * 10UL) / 1024UL,
			ram_percent_x10 / 10UL, ram_percent_x10 % 10UL);
	ILI9806G_DispString_EN(4U, LINE(4), displayBuffer);
	gui_draw_progress_bar(20U, LINE(5) + 5U, 760U, 22U, ram_percent_x10, GREEN);

	sprintf(displayBuffer, "ROM Code + RO: %lu.%01lu KB                  ",
			code_ro_bytes / 1024UL, ((code_ro_bytes % 1024UL) * 10UL) / 1024UL);
	ILI9806G_DispString_EN(4U, LINE(7), displayBuffer);
	sprintf(displayBuffer, "RW initialized: %lu bytes                    ", rw_bytes);
	ILI9806G_DispString_EN(4U, LINE(8), displayBuffer);
	sprintf(displayBuffer, "ZI/BSS + Heap/Stack: %lu.%01lu KB            ",
			zi_bytes / 1024UL, ((zi_bytes % 1024UL) * 10UL) / 1024UL);
	ILI9806G_DispString_EN(4U, LINE(9), displayBuffer);

	sprintf(displayBuffer, "RAM remaining: %lu.%01lu KB                  ",
			(GUI_MCU_RAM_BYTES - ram_bytes) / 1024UL,
			(((GUI_MCU_RAM_BYTES - ram_bytes) % 1024UL) * 10UL) / 1024UL);
	ILI9806G_DispString_EN(4U, LINE(10), displayBuffer);
	sprintf(displayBuffer, "Flash remaining: %lu.%01lu KB                ",
			(GUI_MCU_FLASH_BYTES - flash_bytes) / 1024UL,
			(((GUI_MCU_FLASH_BYTES - flash_bytes) % 1024UL) * 10UL) / 1024UL);
	ILI9806G_DispString_EN(4U, LINE(11), displayBuffer);

	LCD_SetTextColor(BLUE);
	ILI9806G_DispString_EN(4U, LINE(14), "Live values from current linker image");
}

void main_menu(uint8_t selected_item)
{
	static const char *menu_text[5] =
	{
		"System Information",
		"Channel Monitor",
		"GPS / BDS Information",
		"Touch Draw Board",
		"NRF Wireless Settings"
	};
	static const char *menu_hint[5] =
	{
		"Firmware and memory usage",
		"ADC channels and attitude",
		"Position, time and satellites",
		"Touch drawing tools",
		"Radio setup and live status"
	};
	static const uint16_t card_y[5] = {80U, 144U, 208U, 272U, 336U};
	uint8_t i;
	uint16_t card_color;

	if(display_flag == 1)
	{
		display_flag = 0;
		ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);
		I2C_GTP_IRQDisable();
	}
	
	LCD_SetFont(&Font16x32);
	LCD_SetTextColor(BLUE);
	ILI9806G_DrawRectangle(4U, 0U, 792U, 64U, 1U);
	LCD_SetBackColor(BLUE);
	LCD_SetTextColor(WHITE);
	ILI9806G_DispString_EN(20U, 0U, "REMOTER CONTROL CENTER");
	ILI9806G_DispString_EN(20U, 32U, "LEFT/RIGHT: Select     OK: Enter");

	for(i = 0; i < 5U; i++)
	{
		card_color = (i == selected_item) ? BLUE : GREY;
		LCD_SetTextColor(card_color);
		ILI9806G_DrawRectangle(4U, card_y[i], 792U, 48U, 1U);
		LCD_SetBackColor(card_color);
		LCD_SetTextColor((i == selected_item) ? WHITE : BLACK);
		sprintf(displayBuffer, "%c %u. %-22.22s %-19.19s",
				(i == selected_item) ? '>' : ' ', (uint16_t)(i + 1U),
				menu_text[i], menu_hint[i]);
		ILI9806G_DispString_EN(12U, card_y[i] + 8U, displayBuffer);
	}

	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(WHITE);
	ILI9806G_DrawRectangle(4U, 416U, 792U, 32U, 1U);
	LCD_SetTextColor(BLUE);
	sprintf(displayBuffer, "Selected %u / 5: %s",
			(uint16_t)(selected_item + 1U), menu_text[selected_item]);
	ILI9806G_DispString_EN(4U, 416U, displayBuffer);
}

void nrf_settings_page(uint8_t selected_item, uint8_t editing,
					   uint8_t enabled, uint8_t channel,
					   uint8_t power_index, uint8_t data_rate,
					   const char *status_text, uint8_t runtime_valid,
					   uint8_t runtime_enabled, uint8_t runtime_channel,
					   uint8_t runtime_power_index, uint8_t runtime_data_rate)
{
	static const int8_t power_dbm[4] = {-18, -12, -6, 0};
	static const char *rate_text[3] = {"250 Kbps", "1 Mbps", "2 Mbps"};
	uint8_t i;
	char marker;

	if(display_flag == 1)
	{
		display_flag = 0;
		ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);
		I2C_GTP_IRQDisable();
	}

	if(power_index > 3U)
	{
		power_index = 0U;
	}
	if(data_rate > 2U)
	{
		data_rate = 2U;
	}
	if(runtime_power_index > 3U)
	{
		runtime_power_index = 0U;
	}
	if(runtime_data_rate > 2U)
	{
		runtime_data_rate = 2U;
	}

	LCD_SetFont(&Font16x32);
	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLUE);
	ILI9806G_DispString_EN(4U, LINE(0), "NRF Wireless Settings");
	ILI9806G_DispString_EN(4U, LINE(1), "LEFT/RIGHT: Select   OK: Edit/Run   BACK: Exit");

	for(i = 0; i < 6U; i++)
	{
		marker = (i == selected_item) ? ((editing != 0U) ? '*' : '>') : ' ';
		if(i == selected_item)
		{
			LCD_SetBackColor((editing != 0U) ? RED : BLUE);
			LCD_SetTextColor(WHITE);
		}
		else
		{
			LCD_SetBackColor(WHITE);
			LCD_SetTextColor(BLACK);
		}

		switch(i)
		{
			case 0U:
				sprintf(displayBuffer, "%c Wireless:   %-3s                         ", marker,
						enabled ? "ON" : "OFF");
				break;
			case 1U:
				sprintf(displayBuffer, "%c RF Channel: %3u  (%4u MHz)              ", marker,
						channel, 2400U + channel);
				break;
			case 2U:
				sprintf(displayBuffer, "%c TX Power:   %3d dBm                     ", marker,
						power_dbm[power_index]);
				break;
			case 3U:
				sprintf(displayBuffer, "%c Air Rate:   %-8s                    ", marker,
						rate_text[data_rate]);
				break;
			case 4U:
				sprintf(displayBuffer, "%c Apply & Save                              ", marker);
				break;
			default:
				sprintf(displayBuffer, "%c Check Module Connection                   ", marker);
				break;
		}
		ILI9806G_DispString_EN(4U, LINE(i + 3U), displayBuffer);
	}

	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLUE);
	sprintf(displayBuffer, "Connection: %-35s", runtime_valid ? "OK" : "FAILED");
	ILI9806G_DispString_EN(4U, LINE(9), displayBuffer);
	if(runtime_valid != 0U)
	{
		sprintf(displayBuffer, "Actual: %-3s CH%3u %3d dBm %-8s          ",
				runtime_enabled ? "ON" : "OFF", runtime_channel,
				power_dbm[runtime_power_index], rate_text[runtime_data_rate]);
	}
	else
	{
		sprintf(displayBuffer, "Actual: register readback unavailable          ");
	}
	ILI9806G_DispString_EN(4U, LINE(10), displayBuffer);
	sprintf(displayBuffer, "Result: %-38s", status_text);
	ILI9806G_DispString_EN(4U, LINE(11), displayBuffer);
	LCD_SetTextColor(BLACK);
	ILI9806G_DispString_EN(4U, LINE(13), "Blue=selected  Red=editing  OK=confirm");
}

void system_data_read_and_set(void)
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
	ILI9806G_DispStringLine_EN_CH(LINE(0),"GPS信息：");

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
	static const uint16_t card_y[5] = {124U, 182U, 240U, 298U, 356U};
	uint16_t channel_value[10];
	uint8_t i;
	uint8_t first_draw = 0U;

	if(display_flag == 1)
	{
		display_flag = 0;
		ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);
		I2C_GTP_IRQDisable();
		first_draw = 1U;
	}

	for(i = 0U; i < 7U; i++)
	{
		channel_value[i] = ADC1_Value[i];
	}
	for(i = 0U; i < 3U; i++)
	{
		channel_value[i + 7U] = ADC3_Value[i];
	}

	LCD_SetFont(&Font16x32);
	if(first_draw != 0U)
	{
		LCD_SetTextColor(BLUE);
		ILI9806G_DrawRectangle(4U, 0U, 792U, 64U, 1U);
		LCD_SetBackColor(BLUE);
		LCD_SetTextColor(WHITE);
		ILI9806G_DispString_EN(20U, 0U, "CHANNEL MONITOR");
		ILI9806G_DispString_EN(20U, 32U, "10 analog inputs + MPU6050 attitude");

		LCD_SetBackColor(WHITE);
		LCD_SetTextColor(BLUE);
		ILI9806G_DispString_EN(4U, 424U, "0..4095  Red=center 2048  BACK=Exit");
	}

	LCD_SetTextColor(GREY);
	ILI9806G_DrawRectangle(4U, 72U, 792U, 40U, 1U);
	LCD_SetBackColor(GREY);
	LCD_SetTextColor(BLUE);
	sprintf(displayBuffer, "P:%6.1f  R:%6.1f  Y:%6.1f  T:%5.1f C",
			pitch, roll, yaw, (float)temp / 100.0f);
	ILI9806G_DispString_EN(12U, 76U, displayBuffer);

	for(i = 0U; i < 5U; i++)
	{
		gui_draw_channel_card(4U, card_y[i], i, channel_value[i], BLUE);
		gui_draw_channel_card(404U, card_y[i], i + 5U,
						  channel_value[i + 5U], GREEN);
	}

	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLACK);
}

//绘制触摸画板界面
void Draw_Board(void)
{
	if(display_flag == 1)
	{
		display_flag = 0;
		I2C_GTP_IRQDisable();
		ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);
		if(GTP_CalibrationIsReady())
		{
			Palette_Init(LCD_SCAN_MODE);
		}
		else
		{
			GTP_CalibrationStart();
		}
		I2C_GTP_IRQEnable();
	}	
}

