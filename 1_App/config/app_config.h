#ifndef  APP_CONFIG_H
#define  APP_CONFIG_H
#include "stm32f4xx.h"
#include "spi_flash_layout.h"

#define chNum 8

#define ON  1
#define OFF 0

#define PARAM_FLASH_SAVE_SECTOR SPI_FLASH_PARAM_SECTOR
#define PARAM_FLASH_SAVE_ADDR   SPI_FLASH_PARAM_ADDR

/* 参数结构的布局标记独立于 Flash 记录封装版本，旧标记用于迁移识别。 */
#define FM_FLAG 	0x0003
#define FM_PREVIOUS_FLAG 0x0002
#define FM_VERSION	"V1.0.0"
#define FM_TIME		"2024.07.10"

#pragma pack(1)// 单字节对齐，保持持久化载荷中的字段偏移稳定。
/* version 之前的字段会被保存，存储格式要求 int/float 均为 32 位。
 * 新增或重排持久化字段时，需要同步调整记录版本及迁移逻辑。 */
typedef struct param_Config   // 用户参数设置结构体
{
	u16 writeFlag;// 参数布局标记，取 FM_FLAG 或可识别的旧版值。
	u16 chLower[chNum];// 各通道 ADC 校准下限，须满足下限 < 中点 < 上限。
	u16 chMiddle[chNum];//遥杆的中值
	u16 chUpper[chNum];//遥杆的最大值
	int PWMadjustValue[chNum];// 归一化输入的微调量，范围 -1000..1000。
	u8 chReverse[chNum];// 通道方向：0 为正常，1 为反向。
	u8 PWMadjustUnit;//微调单位
	float warnBatVolt;// 本机低电压报警门槛，单位 V。
	u8 throttlePreference;//左右手油门，1为左手油门
	u16 batVoltAdjust;// 本机电压校准系数，1000 表示 1.000 倍。
	u8 modelType;//模型类型
	u8 NRF_Mode;//是否启动无线发射
	u8 keySound;//是否有按键音效
	u8 onImage;//开机画面0,1
	float RecWarnBatVolt;// 接收机低电压报警设置，单位 V。
	u8 clockMode;//闹钟是否报警
	u8 clockTime;//闹钟时间5min
	u8 clockCheck;//开机是否自检一下油门
	u8 throttleProtect;//油门保护值0%
	u8 PPM_Out;//是否PPM输出
	u8 NRF_Power;// 无线功率配置值，允许 0x09、0x0b、0x0d、0x0f。
	u8 NRF_Channel;//NRF射频频道，范围0~125
	u8 NRF_DataRate;//NRF空中速率：0=250Kbps，1=1Mbps，2=2Mbps
	u8 screenBrightness;//屏幕亮度百分比，10～100
	/* 运行时字符串地址不写入 Flash，加载后重新绑定到本固件的常量。 */
	char *version;
	char *version_time;
}param_Config;
#pragma pack()

extern volatile param_Config param;

/* 恢复全局 RAM 默认参数，不保存；返回值固定为 0。 */
unsigned char set_default_param(void);
/* 启动加载入口：读取有效记录，必要时迁移或首次保存；0 成功，1 存储失败。 */
unsigned char write_default_param(void);
/* 初始化调用方持有的参数对象，config 必须有效；不访问硬件。 */
void param_load_defaults(volatile param_Config *config);
/* 原地修复非法数值并更新版本指针；数值修复返回 1，仅更新指针仍返回 0。 */
uint8_t param_sanitize(volatile param_Config *config);

/* 将全局参数保存到非活动槽并回读验证；0 成功，1 失败，不自动配置无线硬件。 */
uint8_t write_param(void);





#endif // APP_CONFIG_H
