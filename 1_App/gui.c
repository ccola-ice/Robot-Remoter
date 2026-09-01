#include "gui.h"
#include "bsp_fsmc_lcd.h"
#include "bsp_adc1_independent_dual.h"
#include "bsp_adc3_independent_dual.h"
#include "bsp_usart_debug.h"
#include "bsp_Systick.h"
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h" 
#include "bsp_mpu6050.h"
#include "nmea/nmea.h"
#include "palette.h"
#include "gt9xx.h"
#include "bsp_i2c_touch.h"
#include <string.h>

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
extern uint8_t imu_data_valid;

/* ARM scatter-loading symbols: their addresses are the linker-calculated sizes. */
extern uint8_t Image$$ER_IROM1$$Length;
extern uint8_t Image$$RW_IRAM1$$Length;
extern uint8_t Image$$RW_IRAM1$$ZI$$Length;

#define GUI_MCU_FLASH_BYTES (1024UL * 1024UL)
#define GUI_MCU_RAM_BYTES   (128UL * 1024UL)

char displayBuffer[100];

static uint8_t display_flag = 0;	//界面切换标志，每次只有第一次切换界面时才清屏（ILI9806G_Clear），其他情况不清屏
static uint16_t boot_progress_width;
static uint8_t boot_last_stage;
static const uint16_t boot_stage_x[4] = {120U, 260U, 400U, 540U};
static const uint8_t boot_stage_text_x[4] = {44U, 36U, 32U, 40U};
static const char *boot_stage_label[4] = {"CORE", "DEVICE", "SERVICE", "READY"};

void gui_prepare_page(void)
{
	display_flag = 1;
}

void gui_boot_begin(void)
{
	uint8_t i;

	boot_progress_width = 0U;
	boot_last_stage = 0xffU;
	LCD_SetBackColor(BLACK);
	LCD_SetTextColor(BLACK);
	ILI9806G_Clear(0U, 0U, LCD_X_LENGTH, LCD_Y_LENGTH);

	LCD_SetFont(&Font8x16);
	LCD_SetBackColor(BLACK);
	LCD_SetTextColor(BLUE2);
	ILI9806G_DispString_EN(20U, 16U, "REMOTER / EMBEDDED CONTROL PLATFORM");
	ILI9806G_DispString_EN(608U, 16U, "REAL BOOT STATUS");

	LCD_SetTextColor(BLUE2);
	ILI9806G_DrawRectangle(72U, 64U, 656U, 192U, 0U);
	ILI9806G_DrawLine(104U, 232U, 696U, 232U);

	LCD_SetFont(&Font24x48);
	LCD_SetTextColor(WHITE);
	ILI9806G_DispString_EN(316U, 88U, "REMOTER");
	LCD_SetFont(&Font16x32);
	LCD_SetTextColor(BLUE2);
	ILI9806G_DispString_EN(240U, 152U, "ROBOT CONTROL SYSTEM");
	LCD_SetFont(&Font8x16);
	LCD_SetTextColor(GREY);
	ILI9806G_DispString_EN(272U, 208U, "STM32F407 / NRF24L01 / MPU6050");

	LCD_SetTextColor(WHITE);
	ILI9806G_DrawRectangle(120U, 304U, 560U, 24U, 1U);
	LCD_SetTextColor(BLUE2);
	ILI9806G_DrawRectangle(120U, 304U, 560U, 24U, 0U);

	for(i = 0U; i < 4U; i++)
	{
		LCD_SetTextColor(BLUE2);
		ILI9806G_DrawRectangle(boot_stage_x[i], 392U, 120U, 32U, 0U);
		LCD_SetBackColor(BLACK);
		ILI9806G_DispString_EN(boot_stage_x[i] + boot_stage_text_x[i], 400U,
							(char *)boot_stage_label[i]);
	}

	LCD_SetFont(&Font16x32);
	LCD_SetTextColor(WHITE);
	ILI9806G_DispString_EN(616U, 344U, "  0%");
}

void gui_boot_update(uint8_t percent, uint8_t stage,
					 const char *status_text, uint8_t warning)
{
	uint16_t next_width;

	if(percent > 100U)
	{
		percent = 100U;
	}
	if(stage > 3U)
	{
		stage = 3U;
	}

	next_width = (uint16_t)((556UL * percent) / 100UL);
	if(next_width > boot_progress_width)
	{
		LCD_SetTextColor(BLUE2);
		ILI9806G_DrawRectangle(122U + boot_progress_width, 306U,
							  next_width - boot_progress_width, 20U, 1U);
		boot_progress_width = next_width;
	}

	if(stage != boot_last_stage)
	{
		LCD_SetFont(&Font8x16);
		LCD_SetTextColor(BLUE2);
		ILI9806G_DrawRectangle(boot_stage_x[stage], 392U, 120U, 32U, 1U);
		LCD_SetBackColor(BLUE2);
		LCD_SetTextColor(WHITE);
		ILI9806G_DispString_EN(boot_stage_x[stage] + boot_stage_text_x[stage],
							400U, (char *)boot_stage_label[stage]);
		boot_last_stage = stage;
	}

	LCD_SetFont(&Font16x32);
	LCD_SetBackColor(BLACK);
	LCD_SetTextColor(BLACK);
	ILI9806G_DrawRectangle(120U, 344U, 440U, 32U, 1U);
	LCD_SetTextColor((warning != 0U) ? YELLOW : WHITE);
	ILI9806G_DispString_EN(120U, 344U, (char *)status_text);
	LCD_SetTextColor(WHITE);
	sprintf(displayBuffer, "%3u%%", (uint16_t)percent);
	ILI9806G_DispString_EN(616U, 344U, displayBuffer);

	/* Briefly hold each completed real milestone so it remains readable. */
	Delay_ms(80U);
}

void gui_boot_finish(void)
{
	gui_boot_update(100U, 3U, "System ready", 0U);
	LCD_SetFont(&Font8x16);
	LCD_SetTextColor(GREEN);
	ILI9806G_DrawRectangle(boot_stage_x[3], 392U, 120U, 32U, 1U);
	LCD_SetBackColor(GREEN);
	LCD_SetTextColor(BLACK);
	ILI9806G_DispString_EN(boot_stage_x[3] + boot_stage_text_x[3], 400U,
						"READY");
	LCD_SetBackColor(BLACK);
	LCD_SetTextColor(GREY);
	ILI9806G_DispString_EN(288U, 456U, "CONTROL IS NOW ONLINE");
	Delay_ms(600U);
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

	LCD_SetTextColor(WHITE);
	ILI9806G_DrawRectangle(x, y, width, height, 1U);
	if(filled_width > 0U)
	{
		LCD_SetTextColor(fill_color);
		ILI9806G_DrawRectangle(x + 2U, y + 2U, filled_width, height - 4U, 1U);
	}
	LCD_SetTextColor(BLACK);
	ILI9806G_DrawRectangle(x, y, width, height, 0U);
}

static void gui_update_progress_bar(uint16_t x, uint16_t y, uint16_t width,
								uint16_t height, uint32_t percent_x10,
								uint16_t fill_color, uint16_t *previous_width,
								uint8_t draw_frame)
{
	uint16_t inner_width;
	uint16_t filled_width;
	uint16_t old_width;
	uint16_t changed_width;

	if(percent_x10 > 1000UL)
	{
		percent_x10 = 1000UL;
	}

	inner_width = (width > 4U) ? (width - 4U) : 0U;
	filled_width = (uint16_t)((inner_width * percent_x10 + 500UL) / 1000UL);
	old_width = (*previous_width <= inner_width) ? *previous_width : 0U;

	if(draw_frame != 0U)
	{
		LCD_SetTextColor(WHITE);
		ILI9806G_DrawRectangle(x, y, width, height, 1U);
		LCD_SetTextColor(BLACK);
		ILI9806G_DrawRectangle(x, y, width, height, 0U);
		old_width = 0U;
	}
	else
	{
		changed_width = (filled_width > old_width) ?
						(filled_width - old_width) : (old_width - filled_width);
		/* Ignore one-pixel ADC/float jitter to avoid continuous LCD writes. */
		if(changed_width < 2U)
		{
			return;
		}
	}

	if(filled_width > old_width)
	{
		LCD_SetTextColor(fill_color);
		ILI9806G_DrawRectangle(x + 2U + old_width, y + 2U,
							  filled_width - old_width, height - 4U, 1U);
	}
	else if(old_width > filled_width)
	{
		LCD_SetTextColor(WHITE);
		ILI9806G_DrawRectangle(x + 2U + filled_width, y + 2U,
							  old_width - filled_width, height - 4U, 1U);
	}

	*previous_width = filled_width;
}

static void gui_draw_channel_card(uint16_t x, uint16_t y, uint8_t channel,
							  uint16_t value, uint16_t bar_color,
							  uint16_t *previous_width, uint8_t draw_frame)
{
	uint16_t shown_value = (value > 4095U) ? 4095U : value;
	uint16_t bar_x = x + 170U;
	uint16_t bar_width = 206U;

	if(draw_frame != 0U)
	{
		LCD_SetTextColor(GREY);
		ILI9806G_DrawRectangle(x, y, 392U, 46U, 1U);
		LCD_SetBackColor(GREY);
		LCD_SetTextColor(BLACK);
		sprintf(displayBuffer, "CH%02u", (uint16_t)(channel + 1U));
		ILI9806G_DispString_EN(x + 8U, y + 7U, displayBuffer);
	}

	LCD_SetBackColor(GREY);
	LCD_SetTextColor(BLACK);
	sprintf(displayBuffer, "%4u", value);
	ILI9806G_DispString_EN(x + 88U, y + 7U, displayBuffer);

	gui_update_progress_bar(bar_x, y + 13U, bar_width, 20U,
							((uint32_t)shown_value * 1000UL) / 4095UL,
							bar_color, previous_width, draw_frame);
	LCD_SetTextColor(RED);
	ILI9806G_DrawLine(bar_x + bar_width / 2U, y + 11U,
					  bar_x + bar_width / 2U, y + 35U);
}

static uint32_t gui_imu_axis_percent(float value, float limit)
{
	if(value < -limit)
	{
		value = -limit;
	}
	else if(value > limit)
	{
		value = limit;
	}

	return (uint32_t)(((value + limit) * 1000.0f) / (2.0f * limit) + 0.5f);
}

static void gui_draw_imu_axis(uint16_t y, const char *name, float value,
						  float limit, uint16_t color,
						  uint16_t *previous_width, uint8_t draw_frame)
{
	if(draw_frame != 0U)
	{
		LCD_SetTextColor(GREY);
		ILI9806G_DrawRectangle(4U, y, 792U, 80U, 1U);
		LCD_SetBackColor(GREY);
		LCD_SetTextColor(BLACK);
		ILI9806G_DispString_EN(20U, y + 8U, (char *)name);
		sprintf(displayBuffer, "Range: +/-%3.0f deg", limit);
		ILI9806G_DispString_EN(476U, y + 8U, displayBuffer);
	}

	LCD_SetBackColor(GREY);
	LCD_SetTextColor(color);
	sprintf(displayBuffer, "%+7.1f deg", value);
	ILI9806G_DispString_EN(156U, y + 8U, displayBuffer);
	gui_update_progress_bar(20U, y + 48U, 760U, 22U,
							gui_imu_axis_percent(value, limit), color,
							previous_width, draw_frame);
	LCD_SetTextColor(RED);
	ILI9806G_DrawLine(400U, y + 45U, 400U, y + 73U);
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
	static uint8_t last_selected_item = 0xffU;
	static const char *menu_text[8] =
	{
		"System Information",
		"Channel Monitor",
		"IMU / MPU6050",
		"GPS / BDS",
		"Touch Draw Board",
		"NRF Wireless",
		"File Browser",
		"Parameter Settings"
	};
	static const char *menu_hint[8] =
	{
		"Memory / firmware",
		"10 analog channels",
		"Live attitude / motion",
		"Position / satellites",
		"Touch drawing tools",
		"Radio setup / status",
		"Browse SD card and SPI Flash",
		"View / edit / save settings"
	};
	uint8_t i;
	uint16_t card_x;
	uint16_t card_y;
	uint16_t card_width;
	uint16_t card_height;
	uint8_t first_draw = 0U;

	if(selected_item >= 8U)
	{
		selected_item = 0U;
	}

	if(display_flag == 1)
	{
		display_flag = 0;
		ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);
		I2C_GTP_IRQDisable();
		first_draw = 1U;
		last_selected_item = 0xffU;
	}
	
	LCD_SetFont(&Font16x32);
	if(first_draw != 0U)
	{
		LCD_SetTextColor(BLUE);
		ILI9806G_DrawRectangle(4U, 0U, 792U, 64U, 1U);
		LCD_SetBackColor(BLUE);
		LCD_SetTextColor(WHITE);
		ILI9806G_DispString_EN(20U, 0U, "REMOTER CONTROL CENTER");
		ILI9806G_DispString_EN(20U, 32U, "LEFT/RIGHT: Select     OK: Enter");
	}

	for(i = 0; i < 8U; i++)
	{
		card_x = ((i & 1U) == 0U) ? 4U : 404U;
		card_y = 72U + (uint16_t)(i / 2U) * 82U;
		card_width = 392U;
		card_height = 70U;

		if(first_draw != 0U)
		{
			LCD_SetTextColor(GREY);
			ILI9806G_DrawRectangle(card_x, card_y, card_width, card_height, 1U);
			LCD_SetBackColor(GREY);
			LCD_SetTextColor(BLACK);
			LCD_SetFont(&Font16x32);
			sprintf(displayBuffer, "%u. %-18.18s",
					(uint16_t)(i + 1U), menu_text[i]);
			ILI9806G_DispString_EN(card_x + 20U, card_y + 4U, displayBuffer);
			LCD_SetFont(&Font8x16);
			sprintf(displayBuffer, "%-40.40s", menu_hint[i]);
			ILI9806G_DispString_EN(card_x + 20U, card_y + 44U, displayBuffer);
		}

		if((first_draw != 0U) || (i == selected_item) ||
		   (i == last_selected_item))
		{
			/* A narrow indicator and outline avoid tearing from full-card fills. */
			LCD_SetTextColor((i == selected_item) ? BLUE : GREY);
			ILI9806G_DrawRectangle(card_x + 4U, card_y + 4U,
							  8U, card_height - 8U, 1U);
			LCD_SetTextColor((i == selected_item) ? BLUE : BLACK);
			ILI9806G_DrawRectangle(card_x, card_y, card_width, card_height, 0U);
		}
	}

	LCD_SetFont(&Font16x32);
	LCD_SetBackColor(WHITE);
	if(first_draw != 0U)
	{
		LCD_SetTextColor(WHITE);
		ILI9806G_DrawRectangle(4U, 416U, 792U, 32U, 1U);
	}
	LCD_SetTextColor(BLUE);
	sprintf(displayBuffer, "Selected: %u / 8 ",
			(uint16_t)(selected_item + 1U));
	ILI9806G_DispString_EN(4U, 416U, displayBuffer);
	last_selected_item = selected_item;
}

static void gui_file_display_text(char *destination, uint16_t destination_size,
								  const char *source, uint16_t maximum_width)
{
	uint16_t source_index = 0U;
	uint16_t destination_index = 0U;
	uint16_t used_width = 0U;
	uint16_t character_width;
	uint8_t first_byte;
	uint8_t second_byte;
	uint8_t source_advance;
	uint8_t output_bytes;

	while(source[source_index] != '\0')
	{
		first_byte = (uint8_t)source[source_index];
		second_byte = (uint8_t)source[source_index + 1U];
		source_advance = 1U;
		output_bytes = 1U;
		character_width = 16U;

		if((first_byte >= 32U) && (first_byte <= 126U))
		{
			destination[destination_index] = (char)first_byte;
		}
		else if((first_byte >= 0xa1U) && (first_byte <= 0xf7U) &&
				(second_byte >= 0xa1U) && (second_byte <= 0xfeU))
		{
			/* The external 32x32 font contains the GB2312 subset of CP936. */
			destination[destination_index] = (char)first_byte;
			destination[destination_index + 1U] = (char)second_byte;
			source_advance = 2U;
			output_bytes = 2U;
			character_width = 32U;
		}
		else
		{
			/* Skip a complete unsupported GBK pair and show one replacement. */
			destination[destination_index] = '?';
			if((first_byte >= 0x81U) && (first_byte <= 0xfeU) &&
			   (second_byte >= 0x40U) && (second_byte <= 0xfeU) &&
			   (second_byte != 0x7fU))
			{
				source_advance = 2U;
			}
		}

		if((used_width + character_width) > maximum_width)
		{
			break;
		}
		if((destination_index + output_bytes + 1U) > destination_size)
		{
			break;
		}
		destination_index += output_bytes;
		source_index += source_advance;
		used_width += character_width;
	}
	destination[destination_index] = '\0';
}

static void gui_file_size_text(uint32_t size, char *text)
{
	if(size < 1024UL)
	{
		sprintf(text, "%lu B", size);
	}
	else if(size < (1024UL * 1024UL))
	{
		sprintf(text, "%lu KB", size / 1024UL);
	}
	else
	{
		sprintf(text, "%lu.%01lu MB", size / (1024UL * 1024UL),
				(size % (1024UL * 1024UL)) * 10UL / (1024UL * 1024UL));
	}
}

void file_browser_page(const char *path, const GuiFileEntry *entries,
					   uint8_t item_count, uint8_t selected_item,
					   uint8_t first_visible, uint16_t revision,
					   const char *status_text)
{
	static uint16_t last_revision = 0xffffU;
	static uint8_t last_selected_item = 0xffU;
	static uint8_t last_first_visible = 0xffU;
	char safe_text[96];
	char size_text[16];
	uint8_t row;
	uint8_t item_index;
	uint16_t row_y;
	uint8_t first_draw = 0U;
	uint8_t content_changed;

	if(display_flag == 1)
	{
		display_flag = 0;
		ILI9806G_Clear(0U, 0U, LCD_X_LENGTH, LCD_Y_LENGTH);
		I2C_GTP_IRQDisable();
		first_draw = 1U;
		last_revision = 0xffffU;
		last_selected_item = 0xffU;
		last_first_visible = 0xffU;
	}

	content_changed = ((first_draw != 0U) || (revision != last_revision) ||
					   (first_visible != last_first_visible)) ? 1U : 0U;
	LCD_SetFont(&Font16x32);
	if(first_draw != 0U)
	{
		LCD_SetTextColor(BLUE);
		ILI9806G_DrawRectangle(4U, 0U, 792U, 64U, 1U);
		LCD_SetBackColor(BLUE);
		LCD_SetTextColor(WHITE);
		ILI9806G_DispString_EN(20U, 0U, "FILE BROWSER");
		ILI9806G_DispString_EN(20U, 32U, "SD Card / SPI Flash  Read-only mode");
	}

	if(content_changed != 0U)
	{
		LCD_SetTextColor(GREY);
		ILI9806G_DrawRectangle(4U, 72U, 792U, 32U, 1U);
		LCD_SetBackColor(GREY);
		LCD_SetTextColor(BLACK);
		gui_file_display_text(safe_text, sizeof(safe_text), path, 672U);
		strcpy(displayBuffer, "Path: ");
		strcat(displayBuffer, safe_text);
		ILI9806G_DispString_EN_CH(12U, 72U, displayBuffer);

		LCD_SetTextColor(WHITE);
		ILI9806G_DrawRectangle(4U, 112U, 792U, 288U, 1U);
		if(item_count == 0U)
		{
			LCD_SetBackColor(WHITE);
			LCD_SetTextColor(GREY);
			ILI9806G_DispString_EN(260U, 224U, "<EMPTY DIRECTORY>");
		}

		for(row = 0U; row < 6U; row++)
		{
			item_index = (uint8_t)(first_visible + row);
			if(item_index >= item_count)
			{
				break;
			}

			row_y = 112U + (uint16_t)row * 48U;
			LCD_SetTextColor(GREY);
			ILI9806G_DrawRectangle(4U, row_y, 792U, 40U, 1U);
			LCD_SetBackColor(GREY);
			LCD_SetTextColor(BLACK);
			gui_file_display_text(safe_text, sizeof(safe_text),
							  entries[item_index].name,
							  (entries[item_index].is_directory != 0U) ? 672U : 496U);
			if(entries[item_index].is_directory != 0U)
			{
				strcpy(displayBuffer, "DIR  ");
				strcat(displayBuffer, safe_text);
			}
			else
			{
				gui_file_size_text(entries[item_index].size, size_text);
				strcpy(displayBuffer, "FILE ");
				strcat(displayBuffer, safe_text);
			}
			ILI9806G_DispString_EN_CH(20U, row_y + 4U, displayBuffer);
			if(entries[item_index].is_directory == 0U)
			{
				ILI9806G_DispString_EN(620U, row_y + 4U, size_text);
			}
		}
	}

	for(row = 0U; row < 6U; row++)
	{
		item_index = (uint8_t)(first_visible + row);
		if(item_index >= item_count)
		{
			break;
		}
		if((content_changed == 0U) && (item_index != selected_item) &&
		   (item_index != last_selected_item))
		{
			continue;
		}

		row_y = 112U + (uint16_t)row * 48U;
		LCD_SetTextColor((item_index == selected_item) ? BLUE : GREY);
		ILI9806G_DrawRectangle(8U, row_y + 4U, 8U, 32U, 1U);
		LCD_SetTextColor((item_index == selected_item) ? BLUE : BLACK);
		ILI9806G_DrawRectangle(4U, row_y, 792U, 40U, 0U);
	}

	LCD_SetFont(&Font8x16);
	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLUE);
	ILI9806G_DispString_EN(12U, 408U,
						"LEFT/RIGHT Select   OK Open   BACK Parent/Exit");
	LCD_SetFont(&Font16x32);
	LCD_SetTextColor(WHITE);
	ILI9806G_DrawRectangle(4U, 432U, 792U, 32U, 1U);
	LCD_SetTextColor(BLACK);
	gui_file_display_text(safe_text, sizeof(safe_text), status_text, 640U);
	strcpy(displayBuffer, "Status: ");
	strcat(displayBuffer, safe_text);
	ILI9806G_DispString_EN_CH(12U, 432U, displayBuffer);

	last_revision = revision;
	last_selected_item = selected_item;
	last_first_visible = first_visible;
}

void parameter_settings_page(const GuiParamRow *rows, uint8_t visible_count,
							 uint8_t selected_row, uint8_t first_visible,
							 uint8_t total_items, uint8_t editing,
							 uint8_t dirty, uint16_t revision,
							 const char *status_text)
{
	static uint16_t last_revision = 0xffffU;
	static uint8_t last_selected_item = 0xffU;
	static uint8_t last_first_visible = 0xffU;
	uint8_t row;
	uint8_t item_index;
	uint8_t selected_item;
	uint8_t first_draw = 0U;
	uint8_t content_changed;
	uint16_t row_y;
	uint16_t scroll_height;
	uint16_t scroll_y;
	const char *state_text;

	if(display_flag == 1)
	{
		display_flag = 0;
		ILI9806G_Clear(0U, 0U, LCD_X_LENGTH, LCD_Y_LENGTH);
		I2C_GTP_IRQDisable();
		first_draw = 1U;
		last_revision = 0xffffU;
		last_selected_item = 0xffU;
		last_first_visible = 0xffU;
	}

	if(visible_count == 0U)
	{
		return;
	}
	if(selected_row >= visible_count)
	{
		selected_row = 0U;
	}
	selected_item = (uint8_t)(first_visible + selected_row);
	content_changed = ((first_draw != 0U) || (revision != last_revision) ||
					   (first_visible != last_first_visible)) ? 1U : 0U;

	if(first_draw != 0U)
	{
		LCD_SetTextColor(BLUE);
		ILI9806G_DrawRectangle(4U, 0U, 792U, 64U, 1U);
		LCD_SetBackColor(BLUE);
		LCD_SetTextColor(WHITE);
		LCD_SetFont(&Font16x32);
		ILI9806G_DispString_EN(20U, 0U, "PARAMETER SETTINGS");
		ILI9806G_DispString_EN(20U, 32U, "Scrollable configuration / explicit Flash save");
	}

	if(content_changed != 0U)
	{
		state_text = (editing != 0U) ? "EDITING" :
					 ((dirty != 0U) ? "UNSAVED" : "SAVED");
		LCD_SetTextColor(GREY);
		ILI9806G_DrawRectangle(4U, 72U, 792U, 32U, 1U);
		LCD_SetBackColor(GREY);
		LCD_SetTextColor(BLACK);
		LCD_SetFont(&Font8x16);
		sprintf(displayBuffer, "ITEMS %u-%u / %u     STATE: %s",
				(uint16_t)(first_visible + 1U),
				(uint16_t)(first_visible + visible_count),
				(uint16_t)total_items, state_text);
		ILI9806G_DispString_EN(20U, 80U, displayBuffer);

		LCD_SetTextColor(WHITE);
		ILI9806G_DrawRectangle(4U, 112U, 792U, 288U, 1U);
	}

	for(row = 0U; row < visible_count; row++)
	{
		item_index = (uint8_t)(first_visible + row);
		if((content_changed == 0U) && (item_index != selected_item) &&
		   (item_index != last_selected_item))
		{
			continue;
		}

		row_y = 112U + (uint16_t)row * 48U;
		if(item_index == selected_item)
		{
			LCD_SetTextColor((editing != 0U) ? RED : BLUE);
			LCD_SetBackColor((editing != 0U) ? RED : BLUE);
		}
		else
		{
			LCD_SetTextColor(GREY);
			LCD_SetBackColor(GREY);
		}
		ILI9806G_DrawRectangle(4U, row_y, 768U, 40U, 1U);
		LCD_SetTextColor((item_index == selected_item) ? WHITE : BLACK);
		LCD_SetFont(&Font16x32);
		sprintf(displayBuffer, "%-23.23s %-20.20s",
				rows[row].label, rows[row].value);
		ILI9806G_DispString_EN(20U, row_y + 4U, displayBuffer);
	}

	/* A compact scrollbar shows where the six-row window is in the list. */
	LCD_SetTextColor(GREY);
	ILI9806G_DrawRectangle(780U, 116U, 8U, 280U, 1U);
	scroll_height = (uint16_t)(280UL * visible_count / total_items);
	if(scroll_height < 24U)
	{
		scroll_height = 24U;
	}
	if(total_items > visible_count)
	{
		scroll_y = (uint16_t)(116U +
			(256UL * first_visible / (total_items - visible_count)));
	}
	else
	{
		scroll_y = 116U;
	}
	LCD_SetTextColor(BLUE);
	ILI9806G_DrawRectangle(780U, scroll_y, 8U, scroll_height, 1U);

	if(content_changed != 0U)
	{
		LCD_SetBackColor(WHITE);
		LCD_SetTextColor(BLUE);
		LCD_SetFont(&Font8x16);
		ILI9806G_DispString_EN(12U, 408U,
			"LEFT/RIGHT Select/Change   OK Edit/Confirm   BACK Cancel/Exit");
		LCD_SetTextColor(WHITE);
		ILI9806G_DrawRectangle(4U, 432U, 792U, 32U, 1U);
		LCD_SetBackColor(WHITE);
		LCD_SetTextColor((dirty != 0U) ? RED : BLACK);
		LCD_SetFont(&Font16x32);
		sprintf(displayBuffer, "Status: %-39.39s", status_text);
		ILI9806G_DispString_EN(12U, 432U, displayBuffer);
	}

	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLACK);
	last_revision = revision;
	last_selected_item = selected_item;
	last_first_visible = first_visible;
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

void channel_monitor_page(void)
{
	static const uint16_t card_y[5] = {80U, 144U, 208U, 272U, 336U};
	static uint16_t previous_bar_width[10];
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
		ILI9806G_DispString_EN(20U, 32U, "10 analog inputs / optimized live view");

		LCD_SetBackColor(WHITE);
		LCD_SetTextColor(BLUE);
		ILI9806G_DispString_EN(4U, 424U, "0..4095  Red=center 2048  BACK=Exit");
	}

	for(i = 0U; i < 5U; i++)
	{
		gui_draw_channel_card(4U, card_y[i], i, channel_value[i], BLUE,
						  &previous_bar_width[i], first_draw);
		gui_draw_channel_card(404U, card_y[i], i + 5U,
						  channel_value[i + 5U], GREEN,
						  &previous_bar_width[i + 5U], first_draw);
	}

	LCD_SetBackColor(WHITE);
	LCD_SetTextColor(BLACK);
}

void imu6050_information(void)
{
	static uint16_t previous_bar_width[3];
	uint8_t first_draw = 0U;

	if(display_flag == 1)
	{
		display_flag = 0;
		ILI9806G_Clear(0,0,LCD_X_LENGTH,LCD_Y_LENGTH);
		I2C_GTP_IRQDisable();
		first_draw = 1U;
	}

	LCD_SetFont(&Font16x32);
	if(first_draw != 0U)
	{
		LCD_SetTextColor(BLUE);
		ILI9806G_DrawRectangle(4U, 0U, 792U, 64U, 1U);
		LCD_SetBackColor(BLUE);
		LCD_SetTextColor(WHITE);
		ILI9806G_DispString_EN(20U, 0U, "IMU / MPU6050");
		ILI9806G_DispString_EN(20U, 32U, "DMP attitude / 100 Hz sensor / 20 FPS UI");

		LCD_SetTextColor(GREY);
		ILI9806G_DrawRectangle(4U, 396U, 792U, 80U, 1U);
	}

	gui_draw_imu_axis(76U, "PITCH", pitch, 90.0f, BLUE,
					   &previous_bar_width[0], first_draw);
	gui_draw_imu_axis(172U, "ROLL", roll, 180.0f, GREEN,
					   &previous_bar_width[1], first_draw);
	gui_draw_imu_axis(268U, "YAW", yaw, 180.0f, RED,
					   &previous_bar_width[2], first_draw);

	LCD_SetBackColor(GREY);
	LCD_SetTextColor(imu_data_valid ? GREEN : RED);
	sprintf(displayBuffer, "%-7s TEMP:%5.1f C  ACC:%6d %6d %6d ",
			imu_data_valid ? "ONLINE" : "WAITING", (float)temp / 100.0f,
			aacx, aacy, aacz);
	ILI9806G_DispString_EN(12U, 400U, displayBuffer);
	LCD_SetTextColor(BLACK);
	sprintf(displayBuffer, "GYRO:%6d %6d %6d             BACK=Exit ",
			gyrox, gyroy, gyroz);
	ILI9806G_DispString_EN(12U, 436U, displayBuffer);

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
