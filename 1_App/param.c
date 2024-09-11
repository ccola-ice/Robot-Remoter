#include "param.h"
#include "bsp_i2c_eeprom.h"
#include "bsp_spi_flash.h"
#include <stddef.h>

#define   PARAM_DATA_SIZE  sizeof(param)

volatile  param_Config    param;
struct param_Config * pt_param  = &param;

unsigned char set_default_param(void)
{
    for(int i=0;i<chNum;i++)
	{
		param.chLower[i] 	= 0;	//遥杆的最小值
		param.chMiddle[i]   = 2047;	//遥杆的中值
		param.chUpper[i] 	= 4095;	//遥杆的最大值
		param.PWMadjustValue[i]=0;//微调值
		param.chReverse[i] = OFF;	//通道的正反，1为正常，0为反转
	}
	param.PWMadjustUnit = 2;//微调单位
	param.warnBatVolt = 3.7;//报警电压
	param.throttlePreference = ON;//左手油门
	param.batVoltAdjust = 1000;//电池电压校准值
	param.modelType = 0;//模型类型：翼0，车1，船2
	param.NRF_Mode = ON;//无线发射，默认启动
	param.keySound = ON;//按键声音，默认启动
	param.onImage = 0;//开机画面，0反白,1正常，默认反白
	param.RecWarnBatVolt = 10.9;//接收机的报警电压
	param.clockMode = OFF;//闹钟是否报警，默认关闭
	param.clockTime = 19;//闹钟时间，默认5min
	param.clockCheck = OFF;//开机是否自检一下油门，默认关闭
	param.throttleProtect = 0;//油门保护值，默认0%
	param.PPM_Out = OFF;//PPM输出，默认关闭
	param.NRF_Power = 0x09;//0x0f=0dBm;0x0d=-6dBm;0xb=-12dBm;0x09=-18dBm;功率越大，dBm越大
	param.version   = FM_VERSION;
	param.version_time = FM_TIME;
    
	return 0;
}

//pt_param->clockTime = 44;
//write_param();

unsigned char write_default_param(void)
{
	FLASH_Read_Data((uint8_t *)&param, PARAM_FLASH_SAVE_ADDR, PARAM_DATA_SIZE);			//从FLASH中读取参数结构体
	
	if(param.writeFlag!=FM_FLAG)	//判断是否为最新版本
	{
		param.writeFlag = FM_FLAG;
		set_default_param();		//设置默认参数
		FLASH_Erase_Sectors(PARAM_FLASH_SAVE_ADDR + 0*4096);
		FLASH_Erase_Sectors(PARAM_FLASH_SAVE_ADDR + 1*4096);
		FLASH_Erase_Sectors(PARAM_FLASH_SAVE_ADDR + 2*4096);
		FLASH_Write_Data((uint8_t *)&param, PARAM_FLASH_SAVE_ADDR, PARAM_DATA_SIZE);	//写入FLASH
		printf("update default params\r\n");
	}

	return  0;
}

#if 0
//使用结构体指针的形式，有点脱裤子放屁
void write_param(param_Config * pt_param, uint32_t data, uint32_t WriteAddr, uint8_t size)
{
	pt_param->writeFlag  =  data;
	FLASH_Write_Data((uint8_t *)&(pt_param->writeFlag), WriteAddr, size);
}
#endif

//向flash中的参数结构体写入新的数据
//由于flash写入数据前必须擦除整个扇区，所以写入参数的操作只能是以写入整个结构体的形式完成。
//使用方法：更新param里的某个元素；调用write_param。
//param.clockTime = 33;
//write_param();
void write_param(void)
{
	FLASH_Erase_Sectors(PARAM_FLASH_SAVE_ADDR);
	FLASH_Write_Data((uint8_t *)&param, PARAM_FLASH_SAVE_ADDR, sizeof(param));	//每次都写入整个param结构体
}

//从flash读出参数,保存到对应的结构体变量中
//地址采用 PARAM_FLASH_SAVE_ADDR + offsetof(param_Config, clockTime) 的形式
void read_param(uint8_t data, uint32_t WriteAddr)
{
	FLASH_Read_Data((uint8_t *)&data, WriteAddr, sizeof(data));
}
