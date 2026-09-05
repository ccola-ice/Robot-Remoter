#include "gt9xx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bsp_i2c_touch.h"
#include "bsp_fsmc_lcd.h"
#include "palette.h"

// 4.5寸屏GT5688驱动配置
const uint8_t CTP_CFG_GT5688[] =  {
			0x96,0xE0,0x01,0x56,0x03,0x05,0x35,0x00,0x01,0x00,
			0x00,0x05,0x50,0x3C,0x53,0x11,0x00,0x00,0x22,0x22,
			0x14,0x18,0x1A,0x1D,0x0A,0x04,0x00,0x00,0x00,0x00,
			0x00,0x00,0x53,0x00,0x14,0x00,0x00,0x84,0x00,0x00,
			0x3C,0x19,0x19,0x64,0x1E,0x28,0x88,0x29,0x0A,0x2D,
			0x2F,0x29,0x0C,0x20,0x33,0x60,0x13,0x02,0x24,0x00,
			0x00,0x20,0x3C,0xC0,0x14,0x02,0x00,0x00,0x54,0xAC,
			0x24,0x9C,0x29,0x8C,0x2D,0x80,0x32,0x77,0x37,0x6E,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0x50,0x3C,
			0xFF,0xFF,0x07,0x00,0x00,0x00,0x02,0x14,0x14,0x03,
			0x04,0x00,0x21,0x64,0x0A,0x00,0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
			0x32,0x20,0x50,0x3C,0x3C,0x00,0x00,0x00,0x00,0x00,
			0x0D,0x06,0x0C,0x05,0x0B,0x04,0x0A,0x03,0x09,0x02,
			0xFF,0xFF,0xFF,0xFF,0x00,0x01,0x02,0x03,0x04,0x05,
			0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
			0x10,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,0x3C,0x00,0x05,0x1E,0x00,0x02,
			0x2A,0x1E,0x19,0x14,0x02,0x00,0x03,0x0A,0x05,0x00,
			0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xFF,0xFF,0x86,
			0x22,0x03,0x00,0x00,0x33,0x00,0x0F,0x00,0x00,0x00,
			0x50,0x3C,0x50,0x00,0x00,0x00,0x1A,0x64,0x01

};

//GT9147配置参数表
//第一个字节为版本号(0X60),必须保证新的版本号大于等于GT9147内部
//flash原有版本号,才会更新配置.
const u8 CTP_CFG_GT9147[]=
{
0x99,0xE0,0x01,0x20,0x03,0x05,0x34,0x00,0x02,
0x08,0x1E,0x08,0x50,0x3C,0x0F,0x05,0x00,0x00,
0xFF,0x67,0x02,0x02,0x00,0x18,0x1A,0x1E,0x14,
0x88,0x28,0x0A,0x55,0x57,0xD3,0x07,0x03,0x00,
0x00,0x42,0x32,0x1D,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x32,0x00,0x00,0x2A,0x4B,0x78,0x94,
0xD5,0x02,0x07,0x00,0x00,0x04,0x88,0x4E,0x00,
0x7E,0x56,0x00,0x76,0x5E,0x00,0x6E,0x68,0x00,
0x67,0x72,0x00,0x67,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x0F,0x0F,0x03,0x06,0x10,0x42,0xF8,0x0F,0x14,
0x00,0x00,0x00,0x00,0x1A,0x18,0x16,0x14,0x12,
0x10,0x0E,0x0C,0x0A,0x08,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,
0x04,0x05,0x06,0x08,0x0A,0x0C,0x1D,0x1E,0x1F,
0x20,0x22,0x24,0x28,0x29,0xFF,0xFF,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0x37,0x01
//	0x99,0XE0,0X01,0X20,0X03,0X05,0X35,0X00,0X02,0X08,
//	0X1E,0X08,0X50,0X3C,0X0F,0X05,0X00,0X00,0XFF,0X67,
//	0X50,0X00,0X00,0X18,0X1A,0X1E,0X14,0X89,0X28,0X0A,
//	0X30,0X2E,0XBB,0X0A,0X03,0X00,0X00,0X02,0X33,0X1D,
//	0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X32,0X00,0X00,
//	0X2A,0X1C,0X5A,0X94,0XC5,0X02,0X07,0X00,0X00,0X00,
//	0XB5,0X1F,0X00,0X90,0X28,0X00,0X77,0X32,0X00,0X62,
//	0X3F,0X00,0X52,0X50,0X00,0X52,0X00,0X00,0X00,0X00,
//	0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
//	0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X0F,
//	0X0F,0X03,0X06,0X10,0X42,0XF8,0X0F,0X14,0X00,0X00,
//	0X00,0X00,0X1A,0X18,0X16,0X14,0X12,0X10,0X0E,0X0C,
//	0X0A,0X08,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
//	0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
//	0X00,0X00,0X29,0X28,0X24,0X22,0X20,0X1F,0X1E,0X1D,
//	0X0E,0X0C,0X0A,0X08,0X06,0X05,0X04,0X02,0X00,0XFF,
//	0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
//	0X00,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,
//	0XFF,0XFF,0XFF,0XFF,
};  

const uint8_t CTP_CFG_GT917S[] =  {
  0x84,0x20,0x03,0xE0,0x01,0x05,0x05,0x00,0x00,0x40,
  0x00,0x0F,0x78,0x64,0x53,0x11,0x00,0x00,0x00,0x00,
  0x23,0x17,0x19,0x1D,0x0F,0x04,0x00,0x00,0x00,0x00,
  0x00,0x00,0x04,0x51,0x14,0x00,0x00,0x00,0x00,0x00,
  0x32,0x00,0x00,0x50,0x38,0x28,0x8A,0x20,0x11,0x37,
  0x39,0xA2,0x07,0x38,0x6D,0x28,0x11,0x03,0x24,0x00,
  0x01,0x28,0x50,0xC0,0x94,0x02,0x00,0x00,0x53,0xB8,
  0x2E,0xA2,0x35,0x8F,0x3B,0x80,0x42,0x75,0x49,0x6B,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0x4C,0x3C,
  0xFF,0xFF,0x07,0x14,0x14,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x50,0x73,
  0x50,0x32,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x1F,0x1D,0x1B,0x1A,0x19,0x18,0x17,0x16,0x15,0x09,
  0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0x1C,0x1B,0x1A,0x19,0x18,0x17,0x15,0x14,
  0x13,0x12,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x05,0x00,0x00,0x0F,
  0x00,0x00,0x00,0x80,0x46,0x08,0x96,0x50,0x32,0x0A,
  0x0A,0x64,0x32,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x32,0x03,0x0C,0x08,0x23,0x00,0x14,0x23,0x00,0x28,
  0x46,0x30,0x3C,0xD0,0x07,0x50,0x70,0xB0,0x01
};

const uint8_t CTP_CFG_GT911[] =  {
  0x53,0x20,0x03,0xE0,0x01,0x05,0x0D,0x10,0x01,0x18,
  0x28,0x0F,0x50,0x32,0x03,0x05,0x00,0x00,0x00,0x00,
  0x11,0x11,0x05,0x18,0x1A,0x1E,0x14,0x88,0x29,0x0A,
  0x52,0x50,0x40,0x04,0x00,0x00,0x00,0x1A,0x32,0x1C,
  0x00,0x01,0x00,0x0F,0x00,0x2A,0xFF,0x7F,0x00,0x50,
  0x32,0x3C,0x64,0x94,0xD5,0x02,0x07,0x00,0x00,0x04,
  0x9F,0x3F,0x00,0x90,0x46,0x00,0x84,0x4D,0x00,0x79,
  0x55,0x00,0x6D,0x5F,0x00,0x6D,0x00,0x00,0x00,0x00,
  0xF0,0x4A,0x3A,0xFF,0xFF,0x27,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,
  0x12,0x14,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0x22,0x21,0x20,0x1F,0x1E,0x1D,0x1C,0x18,
  0x16,0x12,0x10,0x0F,0x08,0x06,0x04,0x02,0x00,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0x17,0x01
};

//uint8_t config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
//                = {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};

TOUCH_IC touchIC = GT911;			

const TOUCH_PARAM_TypeDef touch_param[2] = 
{
  /* GT917S,4.3寸屏 */
  {
  .max_width = 800,
  .max_height = 480,
  .config_reg_addr = 0x8050,
  },
  
  /* GT911,4.3寸屏 */
  {
  .max_width = 800,
  .max_height = 480,
  .config_reg_addr = 0x8047,
  },
};
						

static int8_t GTP_I2C_Test(void);

static volatile uint8_t g_gtp_irq_pending = 0U;
static volatile uint32_t g_gtp_irq_count = 0U;
static uint8_t g_gtp_active_mask = 0U;
static uint16_t g_gtp_raw_width = GTP_MAX_WIDTH;
static uint16_t g_gtp_raw_height = GTP_MAX_HEIGHT;
static uint8_t g_gtp_trigger_type = GTP_INT_TRIGGER;
static int16_t pre_x[GTP_MAX_TOUCH] = {-1, -1, -1, -1, -1};
static int16_t pre_y[GTP_MAX_TOUCH] = {-1, -1, -1, -1, -1};
#define GTP_CAL_POINT_COUNT 3U

static const int32_t g_cal_target_x[GTP_CAL_POINT_COUNT] = {80, 720, 80};
static const int32_t g_cal_target_y[GTP_CAL_POINT_COUNT] = {80, 80, 400};
static int32_t g_cal_raw_x[GTP_CAL_POINT_COUNT];
static int32_t g_cal_raw_y[GTP_CAL_POINT_COUNT];
static int64_t g_cal_sum_x;
static int64_t g_cal_sum_y;
static int64_t g_cal_det;
static uint16_t g_cal_samples;
static uint8_t g_cal_point;
static uint8_t g_cal_pressed;
static uint8_t g_cal_active;
static uint8_t g_cal_valid;

static void GTP_CalibrationDrawTarget(void)
{
    char message[32];
    int32_t target_x = g_cal_target_x[g_cal_point];
    int32_t target_y = g_cal_target_y[g_cal_point];

    ILI9806G_Clear(0U, 0U, LCD_X_LENGTH, LCD_Y_LENGTH);
    LCD_SetFont(&Font16x32);
    LCD_SetColors(CL_RED, CL_WHITE);
    sprintf(message, "Touch cross %u/%u", (unsigned int)g_cal_point + 1U,
            (unsigned int)GTP_CAL_POINT_COUNT);
    ILI9806G_DispString_EN(280U, 220U, message);
    ILI9806G_DrawLine((uint16_t)(target_x - 20), (uint16_t)target_y,
                      (uint16_t)(target_x + 20), (uint16_t)target_y);
    ILI9806G_DrawLine((uint16_t)target_x, (uint16_t)(target_y - 20),
                      (uint16_t)target_x, (uint16_t)(target_y + 20));
}

void GTP_CalibrationStart(void)
{
    g_cal_point = 0U;
    g_cal_pressed = 0U;
    g_cal_samples = 0U;
    g_cal_sum_x = 0;
    g_cal_sum_y = 0;
    g_cal_active = 1U;
    g_cal_valid = 0U;
    g_gtp_active_mask = 0U;
    memset(pre_x, 0xFF, sizeof(pre_x));
    memset(pre_y, 0xFF, sizeof(pre_y));
    printf("<<-GTP-INFO->> unified three-point calibration started\r\n");
    GTP_CalibrationDrawTarget();
}

uint8_t GTP_CalibrationIsReady(void)
{
    return g_cal_valid;
}

static void GTP_CalibrationRawDown(uint16_t raw_x, uint16_t raw_y)
{
    if((g_cal_active == 0U) || (g_cal_point >= GTP_CAL_POINT_COUNT))
    {
        return;
    }

    if(g_cal_pressed == 0U)
    {
        g_cal_pressed = 1U;
        g_cal_samples = 0U;
        g_cal_sum_x = 0;
        g_cal_sum_y = 0;
    }

    if(g_cal_samples < 64U)
    {
        g_cal_sum_x += raw_x;
        g_cal_sum_y += raw_y;
        g_cal_samples++;
    }
}

static void GTP_CalibrationRawUp(void)
{
    int32_t dx1;
    int32_t dy1;
    int32_t dx2;
    int32_t dy2;

    if((g_cal_active == 0U) || (g_cal_pressed == 0U) ||
       (g_cal_samples < 3U))
    {
        g_cal_pressed = 0U;
        return;
    }

    g_cal_raw_x[g_cal_point] = (int32_t)(g_cal_sum_x / g_cal_samples);
    g_cal_raw_y[g_cal_point] = (int32_t)(g_cal_sum_y / g_cal_samples);
    printf("<<-GTP-INFO->> cal P%u raw=(%ld,%ld) target=(%ld,%ld), samples=%u\r\n",
           (unsigned int)g_cal_point + 1U,
           g_cal_raw_x[g_cal_point], g_cal_raw_y[g_cal_point],
           g_cal_target_x[g_cal_point], g_cal_target_y[g_cal_point],
           (unsigned int)g_cal_samples);
    g_cal_pressed = 0U;
    g_cal_point++;

    if(g_cal_point < GTP_CAL_POINT_COUNT)
    {
        GTP_CalibrationDrawTarget();
        return;
    }

    dx1 = g_cal_raw_x[1] - g_cal_raw_x[0];
    dy1 = g_cal_raw_y[1] - g_cal_raw_y[0];
    dx2 = g_cal_raw_x[2] - g_cal_raw_x[0];
    dy2 = g_cal_raw_y[2] - g_cal_raw_y[0];
    g_cal_det = (int64_t)dx1 * dy2 - (int64_t)dy1 * dx2;

    if((g_cal_det > -1000) && (g_cal_det < 1000))
    {
        printf("<<-GTP-ERROR->> calibration points invalid, retrying\r\n");
        GTP_CalibrationStart();
        return;
    }

    g_cal_active = 0U;
    g_cal_valid = 1U;
    printf("<<-GTP-INFO->> calibration ready, determinant=%lld\r\n", g_cal_det);
    Palette_Init(LCD_SCAN_MODE);
}

static uint8_t GTP_ApplyCalibration(uint16_t raw_x, uint16_t raw_y,
                                    int32_t *screen_x, int32_t *screen_y)
{
    int32_t dx1 = g_cal_raw_x[1] - g_cal_raw_x[0];
    int32_t dy1 = g_cal_raw_y[1] - g_cal_raw_y[0];
    int32_t dx2 = g_cal_raw_x[2] - g_cal_raw_x[0];
    int32_t dy2 = g_cal_raw_y[2] - g_cal_raw_y[0];
    int32_t offset_x = (int32_t)raw_x - g_cal_raw_x[0];
    int32_t offset_y = (int32_t)raw_y - g_cal_raw_y[0];
    int64_t along_x;
    int64_t along_y;

    if((screen_x == NULL) || (screen_y == NULL) || (g_cal_valid == 0U))
    {
        return 0U;
    }

    along_x = (int64_t)offset_x * dy2 - (int64_t)offset_y * dx2;
    along_y = (int64_t)dx1 * offset_y - (int64_t)dy1 * offset_x;
    *screen_x = g_cal_target_x[0] +
        (int32_t)(((int64_t)(g_cal_target_x[1] - g_cal_target_x[0]) * along_x) /
                  g_cal_det);
    *screen_y = g_cal_target_y[0] +
        (int32_t)(((int64_t)(g_cal_target_y[2] - g_cal_target_y[0]) * along_y) /
                  g_cal_det);

    if(*screen_x < 0) *screen_x = 0;
    if(*screen_y < 0) *screen_y = 0;
    if(*screen_x >= LCD_X_LENGTH) *screen_x = LCD_X_LENGTH - 1;
    if(*screen_y >= LCD_Y_LENGTH) *screen_y = LCD_Y_LENGTH - 1;
    return 1U;
}

//static void Delay(__IO uint32_t nCount)	 //简单的延时函数
//{
//	for(; nCount != 0; nCount--);
//}


/**
  * @brief   使用IIC进行数据传输
  * @param
  *		@arg i2c_msg:数据传输结构体
  *		@arg num:数据传输结构体的个数
  * @retval  正常完成的传输结构个数，若不正常，返回0xff
  */
static int I2C_Transfer( struct i2c_msg *msgs,int num)
{
	int im = 0;
	int ret = 0;

	GTP_DEBUG_FUNC();

	for (im = 0; ret == 0 && im != num; im++)
	{
		if ((msgs[im].flags&I2C_M_RD))																//根据flag判断是读数据还是写数据
		{
			ret = I2C_ReadBytes(msgs[im].addr, msgs[im].buf, msgs[im].len);		//IIC读取数据
		} else
		{
			ret = I2C_WriteBytes(msgs[im].addr,  msgs[im].buf, msgs[im].len);	//IIC写入数据
		}
	}
	if(ret != 0)
		return -1;

	return im;   													//正常完成的传输结构个数
}

/**
  * @brief   从IIC设备中读取数据
  * @param
  *		@arg client_addr:设备地址
  *		@arg  buf[0~1]: 读取数据寄存器的起始地址
  *		@arg buf[2~len-1]: 存储读出来数据的缓冲buffer
  *		@arg len:    GTP_ADDR_LENGTH + read bytes count（寄存器地址长度+读取的数据字节数）
  * @retval  i2c_msgs传输结构体的个数，2为成功，其它为失败
  */
static int32_t GTP_I2C_Read(uint8_t client_addr, uint8_t *buf, int32_t len)
{
    struct i2c_msg msgs[2];
    int32_t ret=-1;
    int32_t retries = 0;

    GTP_DEBUG_FUNC();
    /*一个读数据的过程可以分为两个传输过程:
     * 1. IIC  写入 要读取的寄存器地址
     * 2. IIC  读取  数据
     * */

    msgs[0].flags = !I2C_M_RD;					//写入
    msgs[0].addr  = client_addr;					//IIC设备地址
    msgs[0].len   = GTP_ADDR_LENGTH;	//寄存器地址为2字节(即写入两字节的数据)
    msgs[0].buf   = &buf[0];						//buf[0~1]存储的是要读取的寄存器地址
    
    msgs[1].flags = I2C_M_RD;					//读取
    msgs[1].addr  = client_addr;					//IIC设备地址
    msgs[1].len   = len - GTP_ADDR_LENGTH;	//要读取的数据长度
    msgs[1].buf   = &buf[GTP_ADDR_LENGTH];	//buf[GTP_ADDR_LENGTH]之后的缓冲区存储读出的数据

    while(retries < 5)
    {
        ret = I2C_Transfer( msgs, 2);					//调用IIC数据传输过程函数，有2个传输过程
        if(ret == 2)break;
        retries++;
    }
    if((retries >= 5))
    {
        GTP_ERROR("I2C Read: 0x%04X, %d bytes failed, errcode: %d! Process reset.", (((uint16_t)(buf[0] << 8)) | buf[1]), len-2, ret);
    }
    return ret;
}



/**
  * @brief   向IIC设备写入数据
  * @param
  *		@arg client_addr:设备地址
  *		@arg  buf[0~1]: 要写入的数据寄存器的起始地址
  *		@arg buf[2~len-1]: 要写入的数据
  *		@arg len:    GTP_ADDR_LENGTH + write bytes count（寄存器地址长度+写入的数据字节数）
  * @retval  i2c_msgs传输结构体的个数，1为成功，其它为失败
  */
static int32_t GTP_I2C_Write(uint8_t client_addr,uint8_t *buf,int32_t len)
{
    struct i2c_msg msg;
    int32_t ret = -1;
    int32_t retries = 0;

    GTP_DEBUG_FUNC();
    /*一个写数据的过程只需要一个传输过程:
     * 1. IIC连续 写入 数据寄存器地址及数据
     * */
    msg.flags = !I2C_M_RD;			//写入
    msg.addr  = client_addr;			//从设备地址
    msg.len   = len;							//长度直接等于(寄存器地址长度+写入的数据字节数)
    msg.buf   = buf;						//直接连续写入缓冲区中的数据(包括了寄存器地址)

    while(retries < 5)
    {
        ret = I2C_Transfer(&msg, 1);	//调用IIC数据传输过程函数，1个传输过程
        if (ret == 1)break;
        retries++;
    }
    if((retries >= 5))
    {

        GTP_ERROR("I2C Write: 0x%04X, %d bytes failed, errcode: %d! Process reset.", (((uint16_t)(buf[0] << 8)) | buf[1]), len-2, ret);

    }
    return ret;
}



/**
  * @brief   使用IIC读取再次数据，检验是否正常
  * @param
  *		@arg client:设备地址
  *		@arg  addr: 寄存器地址
  *		@arg rxbuf: 存储读出的数据
  *		@arg len:    读取的字节数
  * @retval
  * 	@arg FAIL
  * 	@arg SUCCESS
  */
 int32_t GTP_I2C_Read_dbl_check(uint8_t client_addr, uint16_t addr, uint8_t *rxbuf, int len)
{
    uint8_t buf[16] = {0};
    uint8_t confirm_buf[16] = {0};
    uint8_t retry = 0;
    
    GTP_DEBUG_FUNC();

    while (retry++ < 3)
    {
        memset(buf, 0xAA, 16);
        buf[0] = (uint8_t)(addr >> 8);
        buf[1] = (uint8_t)(addr & 0xFF);
        GTP_I2C_Read(client_addr, buf, len + 2);
        
        memset(confirm_buf, 0xAB, 16);
        confirm_buf[0] = (uint8_t)(addr >> 8);
        confirm_buf[1] = (uint8_t)(addr & 0xFF);
        GTP_I2C_Read(client_addr, confirm_buf, len + 2);

      
        if (!memcmp(buf, confirm_buf, len+2))
        {
            memcpy(rxbuf, confirm_buf+2, len);
            return SUCCESS;
        }
    }    
    GTP_ERROR("I2C read 0x%04X, %d bytes, double check failed!", addr, len);
    return FAIL;
}


/**
  * @brief   关闭GT91xx中断
  * @param 无
  * @retval 无
  */
void GTP_IRQ_Disable(void)
{
    g_gtp_irq_pending = 0U;
    g_gtp_active_mask = 0U;
    memset(pre_x, 0xFF, sizeof(pre_x));
    memset(pre_y, 0xFF, sizeof(pre_y));
    I2C_GTP_IRQDisable();
}

/**
  * @brief   使能GT91xx中断
  * @param 无
  * @retval 无
  */
void GTP_IRQ_Enable(void)
{
    g_gtp_irq_pending = 0U;
    g_gtp_active_mask = 0U;
    memset(pre_x, 0xFF, sizeof(pre_x));
    memset(pre_y, 0xFF, sizeof(pre_y));
    I2C_GTP_IRQEnable();
}


/**
  * @brief   用于处理或报告触屏检测到按下
  * @param
  *    @arg     id: 触摸顺序trackID
  *    @arg     x:  触摸的 x 坐标
  *    @arg     y:  触摸的 y 坐标
  *    @arg     w:  触摸的 大小
  * @retval 无
  */
/*用于记录连续触摸时(长按)的上一次触摸位置，负数值表示上一次无触摸按下*/

static void GTP_Touch_Down(int32_t id,int32_t x,int32_t y,int32_t w)
{
  
	GTP_DEBUG_FUNC();

	/*取x、y初始值大于屏幕像素值*/
    GTP_DEBUG("ID:%d, X:%d, Y:%d, W:%d", id, x, y, w);

	
    /* 处理触摸按钮，用于触摸画板 */
    Touch_Button_Down(x,y); 
	

    /*处理描绘轨迹，用于触摸画板 */
    Draw_Trail(pre_x[id],pre_y[id],x,y,&brush);
	
		/************************************/
		/*在此处添加自己的触摸点按下时处理过程即可*/
		/* (x,y) 即为最新的触摸点 *************/
		/************************************/
	
		/*prex,prey数组存储上一次触摸的位置，id为轨迹编号(多点触控时有多轨迹)*/
    pre_x[id] = x; pre_y[id] =y;
	
}


/**
  * @brief   用于处理或报告触屏释放
  * @param 释放点的id号
  * @retval 无
  */
static void GTP_Touch_Up( int32_t id)
{
	

    /*处理触摸释放,用于触摸画板*/
    Touch_Button_Up(pre_x[id],pre_y[id]);

		/*****************************************/
		/*在此处添加自己的触摸点释放时的处理过程即可*/
		/* pre_x[id],pre_y[id] 即为最新的释放点 ****/
		/*******************************************/	
		/***id为轨迹编号(多点触控时有多轨迹)********/
	
	
    /*触笔释放，把pre xy 重置为负*/
	  pre_x[id] = -1;
	  pre_y[id] = -1;		
  
    GTP_DEBUG("Touch id[%2d] release!", id);

}

extern int32_t x = 0;
extern int32_t y = 0;	
/**
  * @brief  触屏处理函数，轮询或者在触摸中断调用
  * @param  无
  * @retval 无
  */
static uint8_t GTP_MapCoordinates(uint16_t raw_x, uint16_t raw_y,
                                  int32_t *screen_x, int32_t *screen_y)
{
    int32_t x_pos = raw_x;
    int32_t y_pos = raw_y;

    if((screen_x == NULL) || (screen_y == NULL) ||
       (raw_x > 4095U) || (raw_y > 4095U))
    {
        return 0U;
    }

    /* Calibration consumes the controller's real installed geometry.  Do not
     * reject points using the nominal config range before applying it. */
    if(g_cal_valid != 0U)
    {
        return GTP_ApplyCalibration(raw_x, raw_y, screen_x, screen_y);
    }

    /* Fallback used only before calibration. */
    switch(LCD_SCAN_MODE)
    {
        case 0U:
            x_pos = (int32_t)LCD_X_LENGTH - 1 - raw_y;
            y_pos = raw_x;
            break;
        case 1U:
            x_pos = raw_x;
            y_pos = (int32_t)LCD_Y_LENGTH - 1 - raw_y;
            break;
        case 2U:
            x_pos = raw_y;
            y_pos = raw_x;
            break;
        case 3U:
            break;
        case 4U:
            x_pos = (int32_t)LCD_X_LENGTH - 1 - raw_y;
            y_pos = (int32_t)LCD_Y_LENGTH - 1 - raw_x;
            break;
        case 5U:
            /* The vendor GT911 profile is native to LCD scan mode 3.
             * Mode 5 is mode 3 rotated by 180 degrees. */
            x_pos = (int32_t)LCD_X_LENGTH - 1 - raw_x;
            y_pos = (int32_t)LCD_Y_LENGTH - 1 - raw_y;
            /* This NT35510/GT911 assembly exposes the horizontal touch origin
             * at the panel centre.  Rotate the two 400-pixel halves into the
             * LCD coordinate order; this is a cyclic half-width shift, not a
             * mirror operation. */
            x_pos += (int32_t)LCD_X_LENGTH / 2;
            if(x_pos >= (int32_t)LCD_X_LENGTH)
            {
                x_pos -= (int32_t)LCD_X_LENGTH;
            }
            break;
        case 6U:
            x_pos = raw_y;
            y_pos = (int32_t)LCD_Y_LENGTH - 1 - raw_x;
            break;
        case 7U:
            x_pos = (int32_t)LCD_X_LENGTH - 1 - raw_x;
            y_pos = raw_y;
            break;
        default:
            return 0U;
    }

    if((x_pos < 0) || (y_pos < 0) ||
       (x_pos >= LCD_X_LENGTH) || (y_pos >= LCD_Y_LENGTH))
    {
        return 0U;
    }

    *screen_x = x_pos;
    *screen_y = y_pos;
    return 1U;
}

static void Goodix_TS_Work_Func(void)
{
    uint8_t end_cmd[3] = {GTP_READ_COOR_ADDR >> 8,
                          GTP_READ_COOR_ADDR & 0xFF, 0U};
    uint8_t point_data[2 + 1 + 8 * GTP_MAX_TOUCH + 1] =
                         {GTP_READ_COOR_ADDR >> 8,
                          GTP_READ_COOR_ADDR & 0xFF};
    uint8_t finger;
    uint8_t touch_num;
    uint8_t current_mask = 0U;
    uint8_t i;
    int32_t ret;

    ret = GTP_I2C_Read(GTP_ADDRESS, point_data, 12);
    if(ret != 2)
    {
        GTP_ERROR("coordinate read failed: %d", ret);
        return;
    }

    finger = point_data[GTP_ADDR_LENGTH];
    if(finger == 0U)
    {
        return;
    }
    if((finger & 0x80U) == 0U)
    {
        goto clear_status;
    }

    touch_num = finger & 0x0FU;
    if(touch_num > GTP_MAX_TOUCH)
    {
        GTP_ERROR("invalid touch count: %u", touch_num);
        goto clear_status;
    }

    if(touch_num > 1U)
    {
        uint8_t extra[8 * GTP_MAX_TOUCH] =
        {
            (GTP_READ_COOR_ADDR + 10U) >> 8,
            (GTP_READ_COOR_ADDR + 10U) & 0xFF
        };

        ret = GTP_I2C_Read(GTP_ADDRESS, extra,
                           2 + 8 * (touch_num - 1U));
        if(ret != 2)
        {
            GTP_ERROR("extra coordinate read failed: %d", ret);
            goto clear_status;
        }
        memcpy(&point_data[12], &extra[2], 8 * (touch_num - 1U));
    }

    for(i = 0U; i < touch_num; i++)
    {
        uint8_t *coor_data = &point_data[i * 8U + 3U];
        uint8_t id = coor_data[0] & 0x0FU;
        uint16_t raw_x;
        uint16_t raw_y;
        uint16_t touch_size;
        int32_t screen_x;
        int32_t screen_y;

        if(id >= GTP_MAX_TOUCH)
        {
            GTP_ERROR("invalid track id: %u", id);
            continue;
        }

        raw_x = (uint16_t)coor_data[1] | ((uint16_t)coor_data[2] << 8);
        raw_y = (uint16_t)coor_data[3] | ((uint16_t)coor_data[4] << 8);
        touch_size = (uint16_t)coor_data[5] | ((uint16_t)coor_data[6] << 8);
        current_mask |= (uint8_t)(1U << id);

        if(g_cal_active != 0U)
        {
            if(i == 0U)
            {
                GTP_CalibrationRawDown(raw_x, raw_y);
            }
            continue;
        }

        if(GTP_MapCoordinates(raw_x, raw_y, &screen_x, &screen_y) == 0U)
        {
            GTP_ERROR("coordinate invalid: raw=(%u,%u)", raw_x, raw_y);
            continue;
        }

        if(g_gtp_active_mask == 0U)
        {
            GTP_INFO("touch raw=(%u,%u) mapped=(%ld,%ld)",
                     raw_x, raw_y, screen_x, screen_y);
        }
        GTP_Touch_Down(id, screen_x, screen_y, touch_size);
    }

    if(g_cal_active != 0U)
    {
        if((g_gtp_active_mask != 0U) && (current_mask == 0U))
        {
            GTP_CalibrationRawUp();
        }
        g_gtp_active_mask = current_mask;
        goto clear_status;
    }

    for(i = 0U; i < GTP_MAX_TOUCH; i++)
    {
        uint8_t id_mask = (uint8_t)(1U << i);
        if(((g_gtp_active_mask & id_mask) != 0U) &&
           ((current_mask & id_mask) == 0U))
        {
            GTP_Touch_Up(i);
        }
    }
    g_gtp_active_mask = current_mask;

clear_status:
    ret = GTP_I2C_Write(GTP_ADDRESS, end_cmd, sizeof(end_cmd));
    if(ret != 1)
    {
        GTP_ERROR("coordinate status clear failed: %d", ret);
    }
}
/**
  * @brief   给触屏芯片重新复位
  * @param 无
  * @retval 无
  */
 int8_t GTP_Reset_Guitar(void)
{
    GTP_DEBUG_FUNC();
#if 1
    I2C_ResetChip();
    return 0;
#else 		//软件复位
    int8_t ret = -1;
    int8_t retry = 0;
    uint8_t reset_command[3]={(uint8_t)GTP_REG_COMMAND>>8,(uint8_t)GTP_REG_COMMAND&0xFF,2};

    //写入复位命令
    while(retry++ < 5)
    {
        ret = GTP_I2C_Write(GTP_ADDRESS, reset_command, 3);
        if (ret > 0)
        {
            GTP_INFO("GTP enter sleep!");

            return ret;
        }

    }
    GTP_ERROR("GTP send sleep cmd failed.");
    return ret;
#endif

}



 /**
   * @brief   进入睡眠模式
   * @param 无
   * @retval 1为成功，其它为失败
   */
//int8_t GTP_Enter_Sleep(void)
//{
//    int8_t ret = -1;
//    int8_t retry = 0;
//    uint8_t reset_comment[3] = {(uint8_t)(GTP_REG_COMMENT >> 8), (uint8_t)GTP_REG_COMMENT&0xFF, 5};//5
//
//    GTP_DEBUG_FUNC();
//
//    while(retry++ < 5)
//    {
//        ret = GTP_I2C_Write(GTP_ADDRESS, reset_comment, 3);
//        if (ret > 0)
//        {
//            GTP_INFO("GTP enter sleep!");
//
//            return ret;
//        }
//
//    }
//    GTP_ERROR("GTP send sleep cmd failed.");
//    return ret;
//}


int8_t GTP_Send_Command(uint8_t command)
{
    int8_t ret = -1;
    int8_t retry = 0;
    uint8_t command_buf[3] = {(uint8_t)(GTP_REG_COMMAND >> 8), (uint8_t)GTP_REG_COMMAND&0xFF, GTP_COMMAND_READSTATUS};

    GTP_DEBUG_FUNC();

    while(retry++ < 5)
    {
        ret = GTP_I2C_Write(GTP_ADDRESS, command_buf, 3);
        if (ret > 0)
        {
            GTP_INFO("send command success!");

            return ret;
        }

    }
    GTP_ERROR("send command fail!");
    return ret;
}

/**
  * @brief   唤醒触摸屏
  * @param 无
  * @retval 0为成功，其它为失败
  */
int8_t GTP_WakeUp_Sleep(void)
{
    uint8_t retry = 0;
    int8_t ret = -1;

    GTP_DEBUG_FUNC();

    while(retry++ < 10)
    {
        ret = GTP_I2C_Test();
        if (ret > 0)
        {
            GTP_INFO("GTP wakeup sleep.");
            return ret;
        }
        GTP_Reset_Guitar();
    }

    GTP_ERROR("GTP wakeup sleep failed.");
    return ret;
}

static int32_t GTP_Get_Info(void)
{
    uint8_t opr_buf[10] = {0U};
    uint16_t width;
    uint16_t height;
    uint8_t module_switch;
    int32_t ret;

    opr_buf[0] = (uint8_t)((GTP_REG_CONFIG_DATA + 1U) >> 8);
    opr_buf[1] = (uint8_t)((GTP_REG_CONFIG_DATA + 1U) & 0xFFU);
    ret = GTP_I2C_Read(GTP_ADDRESS, opr_buf, sizeof(opr_buf));
    if(ret != 2)
    {
        return FAIL;
    }

    width = (uint16_t)opr_buf[2] | ((uint16_t)opr_buf[3] << 8);
    height = (uint16_t)opr_buf[4] | ((uint16_t)opr_buf[5] << 8);
    if((width == 0U) || (height == 0U) || (width > 4096U) || (height > 4096U))
    {
        GTP_ERROR("invalid factory range: %ux%u", width, height);
        return FAIL;
    }

    opr_buf[0] = (uint8_t)((GTP_REG_CONFIG_DATA + 6U) >> 8);
    opr_buf[1] = (uint8_t)((GTP_REG_CONFIG_DATA + 6U) & 0xFFU);
    ret = GTP_I2C_Read(GTP_ADDRESS, opr_buf, 3);
    if(ret != 2)
    {
        return FAIL;
    }

    g_gtp_raw_width = width;
    g_gtp_raw_height = height;
    module_switch = opr_buf[2];
    g_gtp_trigger_type = module_switch & 0x03U;
    I2C_GTP_SetInterruptTrigger(g_gtp_trigger_type);

    GTP_INFO("active range=%ux%u, module=%02X, X2Y=%u, trigger=%u, LCD=%ux%u, scan=%u",
             g_gtp_raw_width, g_gtp_raw_height, module_switch,
             (module_switch & X2Y_LOC) ? 1U : 0U, g_gtp_trigger_type,
             LCD_X_LENGTH, LCD_Y_LENGTH, LCD_SCAN_MODE);
    return SUCCESS;
}
/*******************************************************
Function:
    Initialize gtp.
Input:
    ts: goodix private data
Output:
    Executive outcomes.
        0: succeed, otherwise: failed
*******************************************************/
 int32_t GTP_Init_Panel(void)
{
    int32_t ret = -1;

#if UPDATE_CONFIG
    int32_t i = 0;
    uint16_t check_sum = 0;
    int32_t retry = 0;

    const uint8_t* cfg_info;
    uint8_t cfg_info_len  ;
	uint8_t* config;

    uint8_t cfg_num =0 ;		//需要配置的寄存器个数
#endif

    GTP_DEBUG_FUNC();
	
//uint8_t config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
//                = {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};

		
	
    I2C_Touch_Init();

    ret = GTP_I2C_Test();
    if (ret < 0)
    {
        GTP_ERROR("I2C communication ERROR!");
				return ret;
    } 
		
		//获取触摸IC的型号
    ret = GTP_Read_Version();
    if(ret != 2)
    {
        GTP_ERROR("GT911 version read failed: %d", ret);
        return -1;
    }
    
#if UPDATE_CONFIG
    
    config = (uint8_t *)malloc (GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);

		config[0] = GTP_REG_CONFIG_DATA >> 8;
		config[1] =  GTP_REG_CONFIG_DATA & 0xff;
		
		//根据IC的型号指向不同的配置
		if(touchIC == GT5688)
		{
			cfg_info =  CTP_CFG_GT5688; //指向寄存器配置
			cfg_info_len = CFG_GROUP_LEN(CTP_CFG_GT5688);//计算配置表的大小
		}
		else if(touchIC == GT917S)
		{
			cfg_info =  CTP_CFG_GT917S; //指向寄存器配置
			cfg_info_len = CFG_GROUP_LEN(CTP_CFG_GT917S);//计算配置表的大小
		}
        else if(touchIC == GT911)
		{
			cfg_info =  CTP_CFG_GT911; //指向寄存器配置
			cfg_info_len = CFG_GROUP_LEN(CTP_CFG_GT911);//计算配置表的大小
		}
		else
		{//默认配置为GT917S
			cfg_info =  CTP_CFG_GT917S; //指向寄存器配置
			cfg_info_len = CFG_GROUP_LEN(CTP_CFG_GT917S);//计算配置表的大小
		}
		
    memset(&config[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
    memcpy(&config[GTP_ADDR_LENGTH], cfg_info, cfg_info_len);
		

		cfg_num = cfg_info_len;
		
		GTP_DEBUG("cfg_info_len = %d ",cfg_info_len);
		GTP_DEBUG("cfg_num = %d ",cfg_num);
		GTP_DEBUG_ARRAY(config,6);
		
		/*根据LCD的扫描方向设置分辨率*/
		config[GTP_ADDR_LENGTH+1] = LCD_X_LENGTH & 0xFF;
		config[GTP_ADDR_LENGTH+2] = LCD_X_LENGTH >> 8;
		config[GTP_ADDR_LENGTH+3] = LCD_Y_LENGTH & 0xFF;
		config[GTP_ADDR_LENGTH+4] = LCD_Y_LENGTH >> 8;
		
		/*根据扫描模式设置X2Y交换*/
        if(touchIC == GT917S)
        {
            switch(LCD_SCAN_MODE)
            {
                case 0:case 2:case 4: case 6:
                    config[GTP_ADDR_LENGTH+6] |= (X2Y_LOC);
                    break;
                
                case 1:case 3:case 5: case 7:
                    config[GTP_ADDR_LENGTH+6] &= ~(X2Y_LOC);
                    break;		
            }
        }
        
        //计算要写入checksum寄存器的值
        check_sum = 0;

        /* 计算check sum校验值 */
        if(touchIC == GT911)
        {
            for (i = GTP_ADDR_LENGTH; i < cfg_num; i++)
            {
                check_sum += (config[i] & 0xFF);
            }
            config[ cfg_num] = (~(check_sum & 0xFF)) + 1; 	//checksum
            config[ cfg_num+1] =  1; 						//refresh 配置更新标志
        }
        else if(touchIC == GT9157)
        {
            for (i = GTP_ADDR_LENGTH; i < cfg_num+GTP_ADDR_LENGTH; i++)
            {
                check_sum += (config[i] & 0xFF);
            }
            config[ cfg_num+GTP_ADDR_LENGTH] = (~(check_sum & 0xFF)) + 1; 	//checksum
            config[ cfg_num+GTP_ADDR_LENGTH+1] =  1; 						//refresh 配置更新标志
        }
        else if(touchIC == GT5688 || touchIC == GT917S) 
        {
            for (i = GTP_ADDR_LENGTH; i < (cfg_num+GTP_ADDR_LENGTH -3); i += 2) 
            {
            check_sum += (config[i] << 8) + config[i + 1];
            }

            check_sum = 0 - check_sum;
            GTP_DEBUG("Config checksum: 0x%04X", check_sum);
            //更新checksum
            config[(cfg_num+GTP_ADDR_LENGTH -3)] = (check_sum >> 8) & 0xFF;
            config[(cfg_num+GTP_ADDR_LENGTH -2)] = check_sum & 0xFF;
            config[(cfg_num+GTP_ADDR_LENGTH -1)] = 0x01;
        }
        

    //写入配置信息
    for (retry = 0; retry < 5; retry++)
    {
        ret = GTP_I2C_Write(GTP_ADDRESS, config , cfg_num + GTP_ADDR_LENGTH+2);
        if (ret > 0)
        {
            break;
        }
    }
    Delay(0xfffff);				//延迟等待芯片更新
		

		
#if 1	//读出写入的数据，检查是否正常写入
    //检验读出的数据与写入的是否相同
	{
    	    uint16_t i;
    	    uint8_t buf[300];
    	     buf[0] = config[0];
    	     buf[1] =config[1];    //寄存器地址

    	    GTP_DEBUG_FUNC();

    	    ret = GTP_I2C_Read(GTP_ADDRESS, buf, sizeof(buf));
			   
					GTP_DEBUG("read ");

					GTP_DEBUG_ARRAY(buf,cfg_num);
		
			    GTP_DEBUG("write ");

					GTP_DEBUG_ARRAY(config,cfg_num);

					//不对比版本号
    	    for(i=1;i<cfg_num+GTP_ADDR_LENGTH-3;i++)
    	    {

    	    	if(config[i] != buf[i])
    	    	{
    	    		GTP_ERROR("Config fail ! i = %d ",i);
							free(config);
    	    		return -1;
    	    	}
    	    }
    	    if(i==cfg_num+GTP_ADDR_LENGTH-3)
	    		GTP_DEBUG("Config success ! i = %d ",i);
	}
#endif
	free(config);
#endif

    /* Keep the panel-specific factory sensor/driver channel map.  The generic
     * table is not interchangeable between different GT911 glass layouts. */
	 /* Read back the active geometry and interrupt mode. */
    if(GTP_Get_Info() != SUCCESS)
    {
        GTP_ERROR("GT911 configuration read failed");
        return -1;
    }

    g_gtp_irq_pending = 0U;
    g_gtp_active_mask = 0U;
    memset(pre_x, 0xFF, sizeof(pre_x));
    memset(pre_y, 0xFF, sizeof(pre_y));

    /* Keep EXTI disabled until the drawing page has initialized its buttons. */
    I2C_GTP_IRQDisable();
    return 0;
}


/*******************************************************
Function:
    Read chip version.
Input:
    client:  i2c device
    version: buffer to keep ic firmware version
Output:
    read operation return.
        2: succeed, otherwise: failed
*******************************************************/
int32_t GTP_Read_Version(void)
{
    uint8_t buf[8] = {GTP_REG_VERSION >> 8, GTP_REG_VERSION & 0xFFU};
    int32_t ret = GTP_I2C_Read(GTP_ADDRESS, buf, sizeof(buf));

    if(ret != 2)
    {
        GTP_ERROR("GT911 version register read failed: %d", ret);
        return -1;
    }

    if((buf[2] != '9') || (buf[3] != '1') || (buf[4] != '1'))
    {
        GTP_ERROR("unexpected touch IC id: %02X %02X %02X %02X",
                  buf[2], buf[3], buf[4], buf[5]);
        return -1;
    }

    touchIC = GT911;
    GTP_INFO("GT911 Version: %c%c%c_%02x%02x",
             buf[2], buf[3], buf[4], buf[7], buf[6]);
    return ret;
}
/*******************************************************
Function:
    I2c test Function.
Input:
    client:i2c client.
Output:
    Executive outcomes.
        2: succeed, otherwise failed.
*******************************************************/
static int8_t GTP_I2C_Test( void)
{
    uint8_t test[3] = {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};
    uint8_t retry = 0;
    int8_t ret = -1;

    GTP_DEBUG_FUNC();
  
    while(retry++ < 5)
    {
        ret = GTP_I2C_Read(GTP_ADDRESS, test, 3);
        if (ret > 0)
        {
            return ret;
        }
        GTP_ERROR("GTP i2c test failed time %d.",retry);
    }
    return ret;
}

void GTP_NotifyInterrupt(void)
{
    g_gtp_irq_count++;
    g_gtp_irq_pending = 1U;
}

void GTP_Service(void)
{
    if(g_gtp_irq_pending != 0U)
    {
        g_gtp_irq_pending = 0U;
        if(g_gtp_irq_count <= 8U)
        {
            printf("<<-GTP-IRQ->> count=%lu INT=%u\r\n",
                   g_gtp_irq_count,
                   (unsigned int)GPIO_ReadInputDataBit(GTP_INT_GPIO_PORT,
                                                       GTP_INT_GPIO_PIN));
        }
        GTP_TouchProcess();
    }
}

//检测到触摸中断时调用，
void GTP_TouchProcess(void)
{
  //GTP_DEBUG_FUNC();
  Goodix_TS_Work_Func();

}


//MODULE_DESCRIPTION("GTP Series Driver");
//MODULE_LICENSE("GPL");
