#include "param.h"
#include "bsp_spi_flash.h"
#include <stddef.h>
#include <string.h>

/* 只持久化结构体前部的参数值，不保存运行时字符串指针。
 * 格式版本 1 保留现有 STM32 小端序及 32 位 int/float 的存储布局。 */
#define PARAM_PAYLOAD_SIZE offsetof(param_Config, version)
#define PARAM_FLASH_LEGACY_ADDR 0UL
#define PARAM_FLASH_OVERLAP_ADDR (2560UL * 4096UL)
#define PARAM_RECORD_MAGIC 0x31524d50UL
#define PARAM_RECORD_SCHEMA 1U
#define PARAM_RECORD_COMMITTED 0x434f4d54UL

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint16_t schema;
    uint16_t payload_size;
    uint32_t sequence;
    uint8_t payload[PARAM_PAYLOAD_SIZE];
    uint32_t crc;
    uint32_t committed;
} param_record;
#pragma pack(pop)
typedef char param_layout_requires_32_bit_values[
    (sizeof(int) == 4U && sizeof(float) == 4U) ? 1 : -1];
typedef char param_record_fits_one_sector[
    (sizeof(param_record) <= SPI_FLASH_LAYOUT_SECTOR_SIZE) ? 1 : -1];

volatile param_Config param;

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

/* 对范围比较取反，可同时拒绝 NaN、正无穷和负无穷。 */
uint8_t param_sanitize(volatile param_Config *config)
{
    uint8_t i, changed = 0U;
    param_Config defaults;
    param_load_defaults(&defaults);
#define PARAM_REPAIR(field, invalid) do { \
    if(invalid) { config->field = defaults.field; changed = 1U; } \
} while(0)
    PARAM_REPAIR(writeFlag, config->writeFlag != FM_FLAG);
    for(i = 0U; i < chNum; i++) {
        if(config->chUpper[i] > 4095U ||
           config->chLower[i] >= config->chMiddle[i] ||
           config->chMiddle[i] >= config->chUpper[i]) {
            config->chLower[i] = defaults.chLower[i];
            config->chMiddle[i] = defaults.chMiddle[i];
            config->chUpper[i] = defaults.chUpper[i];
            changed = 1U;
        }
        PARAM_REPAIR(PWMadjustValue[i], config->PWMadjustValue[i] < -1000 ||
                     config->PWMadjustValue[i] > 1000);
        PARAM_REPAIR(chReverse[i], config->chReverse[i] > 1U);
    }
    PARAM_REPAIR(PWMadjustUnit, config->PWMadjustUnit < 1U || config->PWMadjustUnit > 100U);
    PARAM_REPAIR(warnBatVolt, !(config->warnBatVolt >= 2.5f && config->warnBatVolt <= 5.0f));
    PARAM_REPAIR(RecWarnBatVolt, !(config->RecWarnBatVolt >= 3.0f && config->RecWarnBatVolt <= 30.0f));
    PARAM_REPAIR(throttlePreference, config->throttlePreference > 1U);
    PARAM_REPAIR(batVoltAdjust, config->batVoltAdjust < 500U || config->batVoltAdjust > 1500U);
    PARAM_REPAIR(modelType, config->modelType > 2U);
    PARAM_REPAIR(NRF_Mode, config->NRF_Mode > 1U);
    PARAM_REPAIR(keySound, config->keySound > 1U);
    PARAM_REPAIR(onImage, config->onImage > 1U);
    PARAM_REPAIR(clockMode, config->clockMode > 1U);
    PARAM_REPAIR(clockTime, config->clockTime == 0U);
    PARAM_REPAIR(clockCheck, config->clockCheck > 1U);
    PARAM_REPAIR(throttleProtect, config->throttleProtect > 100U);
    PARAM_REPAIR(PPM_Out, config->PPM_Out > 1U);
    PARAM_REPAIR(NRF_Power, config->NRF_Power != 0x09U &&
                 config->NRF_Power != 0x0bU && config->NRF_Power != 0x0dU &&
                 config->NRF_Power != 0x0fU);
    PARAM_REPAIR(NRF_Channel, config->NRF_Channel > 125U);
    PARAM_REPAIR(NRF_DataRate, config->NRF_DataRate > 2U);
#undef PARAM_REPAIR
    config->version = FM_VERSION;
    config->version_time = FM_TIME;
    return changed;
}

static uint32_t param_crc32(const uint8_t *data, uint16_t size)
{
    uint32_t crc = 0xffffffffUL;
    uint8_t bit;
    while(size-- != 0U) {
        crc ^= *data++;
        for(bit = 0U; bit < 8U; bit++) {
            crc = (crc >> 1) ^ ((crc & 1U) ? 0xedb88320UL : 0UL);
        }
    }
    return ~crc;
}

static uint8_t param_read_slot(uint32_t address, param_record *record)
{
    param_Config candidate;
    memset(record, 0xff, sizeof(*record));
    FLASH_Read_Data((uint8_t *)record, address, sizeof(*record));
    if(FLASH_GetIoError() != 0U || record->magic != PARAM_RECORD_MAGIC ||
       record->schema != PARAM_RECORD_SCHEMA || record->payload_size != PARAM_PAYLOAD_SIZE ||
       record->committed != PARAM_RECORD_COMMITTED ||
       record->crc != param_crc32((const uint8_t *)record, offsetof(param_record, crc))) return 0U;
    param_load_defaults(&candidate);
    memcpy(&candidate, record->payload, PARAM_PAYLOAD_SIZE);
    return param_sanitize(&candidate) == 0U ? 1U : 0U;
}

/* 返回最新已提交记录所在的槽位；两个槽位均无效时返回 2。
 * 模运算比较也能处理序号超过 0xffffffff 后的回绕。 */
static uint8_t param_find_latest(param_record *latest)
{
    param_record other;
    uint8_t a = param_read_slot(SPI_FLASH_PARAM_SLOT_A_ADDR, latest);
    uint8_t b = param_read_slot(SPI_FLASH_PARAM_SLOT_B_ADDR, &other);
    uint32_t distance = other.sequence - latest->sequence;
    if(b != 0U && (a == 0U || (distance != 0UL && distance < 0x80000000UL))) {
        *latest = other;
        return 1U;
    }
    return a != 0U ? 0U : 2U;
}

static uint8_t param_read_legacy(uint32_t address, param_Config *candidate)
{
    param_load_defaults(candidate);
    FLASH_Read_Data((uint8_t *)candidate, address, PARAM_PAYLOAD_SIZE);
    if(FLASH_GetIoError() != 0U ||
       (candidate->writeFlag != FM_FLAG && candidate->writeFlag != FM_PREVIOUS_FLAG)) return 0U;
    if(candidate->writeFlag == FM_PREVIOUS_FLAG) {
        candidate->writeFlag = FM_FLAG;
        candidate->NRF_Channel = 40U;
        candidate->NRF_DataRate = 2U;
    }
    /* 旧格式记录没有校验和。保留可用设置，但只要存储值需要修复，
     * 就不得自动开启无线发射。 */
    if(param_sanitize(candidate) != 0U) candidate->NRF_Mode = OFF;
    return 1U;
}

unsigned char write_default_param(void)
{
    param_record latest;
    param_Config candidate;
    uint8_t selected;
    param_load_defaults(&param);
    selected = param_find_latest(&latest);
    if(FLASH_GetIoError() != 0U) return 1U;
    if(selected != 2U) {
        memcpy((void *)&param, latest.payload, PARAM_PAYLOAD_SIZE);
        param.version = FM_VERSION;
        param.version_time = FM_TIME;
        return 0U;
    }
    /* 迁移源只读，不得擦除旧版的 0 号扇区或与 FatFs 重叠的扇区。
     * 第一条新格式记录写入槽位 B，直到替代记录成功提交前，
     * 始终保留槽位 A 中的原始数据。 */
    if(param_read_legacy(SPI_FLASH_PARAM_SLOT_A_ADDR, &candidate) != 0U ||
       param_read_legacy(PARAM_FLASH_OVERLAP_ADDR, &candidate) != 0U ||
       param_read_legacy(PARAM_FLASH_LEGACY_ADDR, &candidate) != 0U) {
        param = candidate;
    }
    if(FLASH_GetIoError() != 0U) return 1U;
    return write_param();
}

/* 先写入并校验非活动扇区，最后再写入提交标记。
 * 擦除或编程中断时，上一条已提交记录所在的槽位仍保持完整。 */
uint8_t write_param(void)
{
    param_record record, verify;
    param_Config candidate;
    uint32_t address, sequence;
    uint8_t selected;
    if(FLASH_GetIoError() != 0U) return 1U;
    selected = param_find_latest(&record);
    if(FLASH_GetIoError() != 0U) return 1U;
    sequence = selected == 2U ? 0UL : record.sequence + 1UL;
    address = selected == 1U ? SPI_FLASH_PARAM_SLOT_A_ADDR : SPI_FLASH_PARAM_SLOT_B_ADDR;
    candidate = param;
    if(param_sanitize(&candidate) != 0U) candidate.NRF_Mode = OFF;
    memset(&record, 0xff, sizeof(record));
    record.magic = PARAM_RECORD_MAGIC;
    record.schema = PARAM_RECORD_SCHEMA;
    record.payload_size = PARAM_PAYLOAD_SIZE;
    record.sequence = sequence;
    memcpy(record.payload, &candidate, PARAM_PAYLOAD_SIZE);
    record.crc = param_crc32((const uint8_t *)&record, offsetof(param_record, crc));
    FLASH_Erase_Sectors(address);
    if(FLASH_GetIoError() != 0U) return 1U;
    FLASH_Write_Data((uint8_t *)&record, address, offsetof(param_record, committed));
    if(FLASH_GetIoError() != 0U) return 1U;
    FLASH_Read_Data((uint8_t *)&verify, address, sizeof(verify));
    if(FLASH_GetIoError() != 0U || memcmp(&record, &verify, sizeof(record)) != 0) return 1U;
    record.committed = PARAM_RECORD_COMMITTED;
    FLASH_Write_Data((uint8_t *)&record.committed, address + offsetof(param_record, committed),
                     sizeof(record.committed));
    if(FLASH_GetIoError() != 0U || param_read_slot(address, &verify) == 0U ||
       memcmp(&record, &verify, sizeof(record)) != 0) return 1U;
    param = candidate;
    return 0U;
}
