#include "param.h"
#include "bsp_spi_flash.h"
#include <string.h>

#define   PARAM_DATA_SIZE  sizeof(param)
#define   PARAM_FLASH_LEGACY_ADDR 0UL
/* Firmware builds before the partition fix stored parameters at physical
 * sector 2560, which is inside drive 1:.  Read it once for migration only;
 * never erase or write this address again. */
#define   PARAM_FLASH_OVERLAP_ADDR (2560UL * 4096UL)

volatile  param_Config    param;

void param_load_defaults(volatile param_Config *config)
{
	uint8_t i;

	config->writeFlag = FM_FLAG;
	for(i = 0U; i < chNum; i++)
	{
		config->chLower[i] = 0U;
		config->chMiddle[i] = 2047U;
		config->chUpper[i] = 4095U;
		config->PWMadjustValue[i] = 0;
		config->chReverse[i] = OFF;
	}
	config->PWMadjustUnit = 2U;
	config->warnBatVolt = 3.7f;
	config->throttlePreference = ON;
	config->batVoltAdjust = 1000U;
	config->modelType = 0U;
	config->NRF_Mode = ON;
	config->keySound = ON;
	config->onImage = 0U;
	config->RecWarnBatVolt = 10.9f;
	config->clockMode = OFF;
	config->clockTime = 19U;
	config->clockCheck = OFF;
	config->throttleProtect = 0U;
	config->PPM_Out = OFF;
	config->NRF_Power = 0x09U;
	config->NRF_Channel = 40U;
	config->NRF_DataRate = 2U;
	config->version = FM_VERSION;
	config->version_time = FM_TIME;
}

unsigned char set_default_param(void)
{
	param_load_defaults(&param);
    
	return 0;
}

unsigned char write_default_param(void)
{
	uint8_t migration_required = 0U;

	FLASH_Read_Data((uint8_t *)&param, PARAM_FLASH_SAVE_ADDR, PARAM_DATA_SIZE);			//从FLASH中读取参数结构体
	if((param.writeFlag != FM_FLAG) && (param.writeFlag != FM_PREVIOUS_FLAG))
	{
		/* Recover settings written by the faulty overlapping layout. */
		FLASH_Read_Data((uint8_t *)&param, PARAM_FLASH_OVERLAP_ADDR,
					PARAM_DATA_SIZE);
		if((param.writeFlag == FM_FLAG) || (param.writeFlag == FM_PREVIOUS_FLAG))
		{
			migration_required = 1U;
			printf("recover params from old overlapping sector\r\n");
		}
		else
		{
			/* Very old builds used sector zero.  It is also read-only here. */
			FLASH_Read_Data((uint8_t *)&param, PARAM_FLASH_LEGACY_ADDR,
						PARAM_DATA_SIZE);
			if((param.writeFlag == FM_FLAG) ||
			   (param.writeFlag == FM_PREVIOUS_FLAG))
			{
				migration_required = 1U;
				printf("recover params from legacy sector\r\n");
			}
		}
	}
	
	if(param.writeFlag == FM_PREVIOUS_FLAG)
	{
		/* 仅迁移本版本新增的NRF字段，保留原有通道校准等用户参数。 */
		param.writeFlag = FM_FLAG;
		param.NRF_Channel = 40;
		param.NRF_DataRate = 2;
		param.version = FM_VERSION;
		param.version_time = FM_TIME;
		migration_required = 1U;
	}
	else if(param.writeFlag != FM_FLAG)	//判断是否为最新版本
	{
		set_default_param();		//设置默认参数
		migration_required = 1U;
		printf("update default params\r\n");
	}

	if(migration_required != 0U)
	{
		param.version = FM_VERSION;
		param.version_time = FM_TIME;
		FLASH_Erase_Sectors(PARAM_FLASH_SAVE_ADDR);
		FLASH_Write_Data((uint8_t *)&param, PARAM_FLASH_SAVE_ADDR, PARAM_DATA_SIZE);	//写入FLASH
		printf("params stored in reserved sector %lu\r\n",
			   (unsigned long)PARAM_FLASH_SAVE_SECTOR);
	}

	/* 指针只在运行期使用，不依赖Flash中保存的旧地址。 */
	param.version = FM_VERSION;
	param.version_time = FM_TIME;

	return  0;
}

//向flash中的参数结构体写入新的数据
//由于flash写入数据前必须擦除整个扇区，所以写入参数的操作只能是以写入整个结构体的形式完成。
//使用方法：更新param里的某个元素；调用write_param。
//param.clockTime = 33;
//write_param();
uint8_t write_param(void)
{
	param_Config verify_param;

	param.writeFlag = FM_FLAG;
	param.version = FM_VERSION;
	param.version_time = FM_TIME;
	FLASH_Erase_Sectors(PARAM_FLASH_SAVE_ADDR);
	FLASH_Write_Data((uint8_t *)&param, PARAM_FLASH_SAVE_ADDR, sizeof(param));	//每次都写入整个param结构体
	FLASH_Read_Data((uint8_t *)&verify_param, PARAM_FLASH_SAVE_ADDR,
					sizeof(verify_param));
	return (memcmp((const void *)&param, &verify_param, sizeof(param)) == 0) ?
		   0U : 1U;
}
