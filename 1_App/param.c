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
		param.chLower[i] 	= 0;	//ң�˵���Сֵ
		param.chMiddle[i]   = 2047;	//ң�˵���ֵ
		param.chUpper[i] 	= 4095;	//ң�˵����ֵ
		param.PWMadjustValue[i]=0;//΢��ֵ
		param.chReverse[i] = OFF;	//ͨ����������1Ϊ������0Ϊ��ת
	}
	param.PWMadjustUnit = 2;//΢����λ
	param.warnBatVolt = 3.7;//������ѹ
	param.throttlePreference = ON;//��������
	param.batVoltAdjust = 1000;//��ص�ѹУ׼ֵ
	param.modelType = 0;//ģ�����ͣ���0����1����2
	param.NRF_Mode = ON;//���߷��䣬Ĭ������
	param.keySound = ON;//����������Ĭ������
	param.onImage = 0;//�������棬0����,1������Ĭ�Ϸ���
	param.RecWarnBatVolt = 10.9;//���ջ��ı�����ѹ
	param.clockMode = OFF;//�����Ƿ񱨾���Ĭ�Ϲر�
	param.clockTime = 19;//����ʱ�䣬Ĭ��5min
	param.clockCheck = OFF;//�����Ƿ��Լ�һ�����ţ�Ĭ�Ϲر�
	param.throttleProtect = 0;//���ű���ֵ��Ĭ��0%
	param.PPM_Out = OFF;//PPM�����Ĭ�Ϲر�
	param.NRF_Power = 0x09;//0x0f=0dBm;0x0d=-6dBm;0xb=-12dBm;0x09=-18dBm;����Խ��dBmԽ��
	param.version   = FM_VERSION;
	param.version_time = FM_TIME;
    
	return 0;
}

//pt_param->clockTime = 44;
//write_param();

unsigned char write_default_param(void)
{
	FLASH_Read_Data((uint8_t *)&param, PARAM_FLASH_SAVE_ADDR, PARAM_DATA_SIZE);			//��FLASH�ж�ȡ�����ṹ��
	
	if(param.writeFlag!=FM_FLAG)	//�ж��Ƿ�Ϊ���°汾
	{
		param.writeFlag = FM_FLAG;
		set_default_param();		//����Ĭ�ϲ���
		FLASH_Erase_Sectors(PARAM_FLASH_SAVE_ADDR + 0*4096);
		FLASH_Erase_Sectors(PARAM_FLASH_SAVE_ADDR + 1*4096);
		FLASH_Erase_Sectors(PARAM_FLASH_SAVE_ADDR + 2*4096);
		FLASH_Write_Data((uint8_t *)&param, PARAM_FLASH_SAVE_ADDR, PARAM_DATA_SIZE);	//д��FLASH
		printf("update default params\r\n");
	}

	return  0;
}

#if 0
//ʹ�ýṹ��ָ�����ʽ���е��ѿ��ӷ�ƨ
void write_param(param_Config * pt_param, uint32_t data, uint32_t WriteAddr, uint8_t size)
{
	pt_param->writeFlag  =  data;
	FLASH_Write_Data((uint8_t *)&(pt_param->writeFlag), WriteAddr, size);
}
#endif

//��flash�еĲ����ṹ��д���µ�����
//����flashд������ǰ���������������������д������Ĳ���ֻ������д�������ṹ�����ʽ��ɡ�
//ʹ�÷���������param���ĳ��Ԫ�أ�����write_param��
//param.clockTime = 33;
//write_param();
void write_param(void)
{
	FLASH_Erase_Sectors(PARAM_FLASH_SAVE_ADDR);
	FLASH_Write_Data((uint8_t *)&param, PARAM_FLASH_SAVE_ADDR, sizeof(param));	//ÿ�ζ�д������param�ṹ��
}

//��flash��������,���浽��Ӧ�Ľṹ�������
//��ַ���� PARAM_FLASH_SAVE_ADDR + offsetof(param_Config, clockTime) ����ʽ
void read_param(uint8_t data, uint32_t WriteAddr)
{
	FLASH_Read_Data((uint8_t *)&data, WriteAddr, sizeof(data));
}
