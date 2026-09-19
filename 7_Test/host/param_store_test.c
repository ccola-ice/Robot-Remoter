#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../1_App/param.c"

static uint8_t storage[SPI_FLASH_LAYOUT_TOTAL_SIZE];
static uint8_t io_error;
static int power_budget = -1;
static unsigned erase_calls, program_calls, read_calls, fail_read;

uint8_t FLASH_GetIoError(void) { return io_error; }

static int consume_power(void)
{
    if(io_error) return 0;
    if(power_budget == 0) { io_error = 1U; return 0; }
    if(power_budget > 0) power_budget--;
    return 1;
}

void FLASH_Read_Data(uint8_t *data, uint32_t address, uint16_t size)
{
    assert(address + size <= sizeof(storage));
    if(++read_calls == fail_read) { io_error = 1U; return; }
    if(io_error) return;
    memcpy(data, storage + address, size);
}

void FLASH_Erase_Sectors(uint32_t address)
{
    unsigned i;
    /* No parameter operation may mutate fonts, diagnostics or FatFs. */
    assert(address == SPI_FLASH_PARAM_SLOT_A_ADDR || address == SPI_FLASH_PARAM_SLOT_B_ADDR);
    erase_calls++;
    for(i = 0; i < SPI_FLASH_LAYOUT_SECTOR_SIZE && consume_power(); i++)
        storage[address + i] = 0xffU;
}

void FLASH_Write_Data(uint8_t *data, uint32_t address, uint16_t size)
{
    unsigned i;
    uint32_t base = address & ~(SPI_FLASH_LAYOUT_SECTOR_SIZE - 1UL);
    assert(base == SPI_FLASH_PARAM_SLOT_A_ADDR || base == SPI_FLASH_PARAM_SLOT_B_ADDR);
    assert(address + size <= base + SPI_FLASH_LAYOUT_SECTOR_SIZE);
    program_calls++;
    for(i = 0; i < size && consume_power(); i++) storage[address + i] &= data[i];
}

static void reset_io(void)
{
    io_error = 0U;
    power_budget = -1;
    erase_calls = program_calls = read_calls = fail_read = 0U;
}

static void erase_fixture(void)
{
    memset(storage, 0xff, sizeof(storage));
    memset((void *)&param, 0, sizeof(param));
    reset_io();
}

static void reboot(void)
{
    reset_io();
    memset((void *)&param, 0, sizeof(param));
    assert(write_default_param() == 0U);
    assert(strcmp(param.version, FM_VERSION) == 0 && strcmp(param.version_time, FM_TIME) == 0);
}

static void test_round_trip(void)
{
    param_Config expected;
    unsigned i;
    erase_fixture();
    assert(write_default_param() == 0U);
    assert(erase_calls == 1U && program_calls == 2U);
    param.warnBatVolt = 4.3f;
    param.RecWarnBatVolt = 24.0f;
    param.batVoltAdjust = 1470U;
    param.clockTime = 255U;
    param.NRF_Channel = 125U;
    param.NRF_DataRate = 0U;
    param.NRF_Power = 0x0fU;
    param.modelType = 2U;
    param.PWMadjustUnit = 100U;
    param.PPM_Out = ON;
    param.throttleProtect = 100U;
    for(i = 0U; i < chNum; i++) {
        param.chLower[i] = (uint16_t)(100U + i);
        param.chMiddle[i] = (uint16_t)(1900U + i);
        param.chUpper[i] = (uint16_t)(4000U + i);
        param.PWMadjustValue[i] = (i & 1U) ? -1000 : 1000;
        param.chReverse[i] = (uint8_t)(i & 1U);
    }
    expected = param;
    assert(write_param() == 0U);
    reboot();
    assert(memcmp((const void *)&param, &expected, PARAM_PAYLOAD_SIZE) == 0);
    assert(erase_calls == 0U && program_calls == 0U);
}

static void test_every_interrupted_update(void)
{
    uint8_t slot_a[SPI_FLASH_LAYOUT_SECTOR_SIZE], slot_b[SPI_FLASH_LAYOUT_SECTOR_SIZE];
    unsigned cut, total = SPI_FLASH_LAYOUT_SECTOR_SIZE + sizeof(param_record);
    erase_fixture();
    assert(write_default_param() == 0U); /* B holds defaults. */
    param.clockTime = 41U;
    assert(write_param() == 0U); /* A is active, B is the erase target. */
    memcpy(slot_a, storage + SPI_FLASH_PARAM_SLOT_A_ADDR, sizeof(slot_a));
    memcpy(slot_b, storage + SPI_FLASH_PARAM_SLOT_B_ADDR, sizeof(slot_b));
    for(cut = 0U; cut <= total; cut++) {
        memcpy(storage + SPI_FLASH_PARAM_SLOT_A_ADDR, slot_a, sizeof(slot_a));
        memcpy(storage + SPI_FLASH_PARAM_SLOT_B_ADDR, slot_b, sizeof(slot_b));
        reset_io();
        param.clockTime = 42U;
        power_budget = (int)cut;
        assert(write_param() == (cut < total ? 1U : 0U));
        assert(memcmp(storage + SPI_FLASH_PARAM_SLOT_A_ADDR, slot_a, sizeof(slot_a)) == 0);
        reboot();
        assert(param.clockTime == (cut < total ? 41U : 42U));
        assert(erase_calls == 0U); /* Recovery loads an intact slot without rewriting it. */
    }
    printf("parameter power cuts: %u erase/program boundaries passed\n", total + 1U);
}

static void test_corruption_and_sequence_wrap(void)
{
    param_record good, invalid;
    unsigned i;
    erase_fixture();
    assert(write_default_param() == 0U);
    param.clockTime = 42U;
    assert(write_param() == 0U);
    memcpy(&good, storage + SPI_FLASH_PARAM_SLOT_A_ADDR, sizeof(good));
    for(i = 0U; i < sizeof(good); i++) {
        memcpy(storage + SPI_FLASH_PARAM_SLOT_A_ADDR, &good, sizeof(good));
        storage[SPI_FLASH_PARAM_SLOT_A_ADDR + i] ^= 1U;
        reboot();
        assert(param.clockTime == 19U); /* Every record byte is checked. */
    }
    invalid = good;
    invalid.payload[offsetof(param_Config, NRF_Channel)] = 255U;
    invalid.crc = param_crc32((const uint8_t *)&invalid, offsetof(param_record, crc));
    memcpy(storage + SPI_FLASH_PARAM_SLOT_A_ADDR, &invalid, sizeof(invalid));
    reboot();
    assert(param.clockTime == 19U); /* A valid CRC cannot bypass field validation. */
    good.sequence = 0xffffffffUL;
    good.crc = param_crc32((const uint8_t *)&good, offsetof(param_record, crc));
    memcpy(storage + SPI_FLASH_PARAM_SLOT_A_ADDR, &good, sizeof(good));
    memset(storage + SPI_FLASH_PARAM_SLOT_B_ADDR, 0xff, SPI_FLASH_LAYOUT_SECTOR_SIZE);
    reboot();
    assert(param.clockTime == 42U);
    param.clockTime = 43U;
    assert(write_param() == 0U);
    reboot();
    assert(param.clockTime == 43U); /* Sequence zero is newer than 0xffffffff. */
}

static void test_legacy_migration(void)
{
    static const uint32_t sources[] = {
        SPI_FLASH_PARAM_SLOT_A_ADDR, PARAM_FLASH_OVERLAP_ADDR, PARAM_FLASH_LEGACY_ADDR
    };
    param_Config legacy;
    unsigned source, old_schema;
    for(source = 0U; source < sizeof(sources) / sizeof(sources[0]); source++) {
        for(old_schema = 0U; old_schema < 2U; old_schema++) {
            erase_fixture();
            param_load_defaults(&legacy);
            legacy.writeFlag = old_schema ? FM_PREVIOUS_FLAG : FM_FLAG;
            legacy.clockTime = 33U;
            legacy.chMiddle[2] = 1876U;
            legacy.NRF_Channel = old_schema ? 0xffU : 63U;
            legacy.NRF_DataRate = old_schema ? 0xffU : 1U;
            memcpy(storage + sources[source], &legacy, PARAM_PAYLOAD_SIZE);
            reboot();
            assert(param.clockTime == 33U && param.chMiddle[2] == 1876U);
            assert(param.NRF_Channel == (old_schema ? 40U : 63U));
            assert(param.NRF_DataRate == (old_schema ? 2U : 1U));
            assert(memcmp(storage + sources[source], &legacy, PARAM_PAYLOAD_SIZE) == 0);
            assert(param_read_slot(SPI_FLASH_PARAM_SLOT_B_ADDR, &(param_record){0}) != 0U);
            reboot();
            assert(param.clockTime == 33U && erase_calls == 0U);
        }
    }
    /* Interrupted first migration must preserve the old single-sector image. */
    erase_fixture();
    param_load_defaults(&legacy);
    legacy.clockTime = 71U;
    memcpy(storage + SPI_FLASH_PARAM_SLOT_A_ADDR, &legacy, PARAM_PAYLOAD_SIZE);
    power_budget = SPI_FLASH_LAYOUT_SECTOR_SIZE + 12;
    assert(write_default_param() != 0U);
    assert(memcmp(storage + SPI_FLASH_PARAM_SLOT_A_ADDR, &legacy, PARAM_PAYLOAD_SIZE) == 0);
    reboot();
    assert(param.clockTime == 71U);
}

static void test_field_validation_and_io_failure(void)
{
    param_Config bad, defaults;
    uint32_t nan_bits = 0x7fc00000UL, infinity_bits = 0x7f800000UL;
    unsigned i;
    param_load_defaults(&defaults);
    bad = defaults;
    memset(bad.chReverse, 0xff, sizeof(bad.chReverse));
    for(i = 0U; i < chNum; i++) {
        bad.chLower[i] = 3000U; bad.chMiddle[i] = 2000U;
        bad.chUpper[i] = 65535U; bad.PWMadjustValue[i] = 2147483647;
    }
    memcpy(&bad.warnBatVolt, &nan_bits, 4);
    memcpy(&bad.RecWarnBatVolt, &infinity_bits, 4);
    bad.PWMadjustUnit = 0U; bad.batVoltAdjust = 0U; bad.modelType = 255U;
    bad.throttlePreference = bad.NRF_Mode = bad.keySound = bad.onImage = 255U;
    bad.clockMode = bad.clockCheck = bad.throttleProtect = bad.PPM_Out = 255U;
    bad.clockTime = 0U; bad.NRF_Power = bad.NRF_Channel = bad.NRF_DataRate = 255U;
    erase_fixture();
    memcpy(storage + SPI_FLASH_PARAM_SLOT_A_ADDR, &bad, PARAM_PAYLOAD_SIZE);
    reboot();
    defaults.NRF_Mode = OFF;
    assert(memcmp((const void *)&param, &defaults, PARAM_PAYLOAD_SIZE) == 0);
    assert(param_sanitize(&param) == 0U);
    reset_io();
    fail_read = 1U;
    assert(write_param() != 0U && erase_calls == 0U && program_calls == 0U);
    reset_io();
    fail_read = 1U;
    assert(write_default_param() != 0U && erase_calls == 0U && program_calls == 0U);
}

int main(void)
{
    test_round_trip();
    test_every_interrupted_update();
    test_corruption_and_sequence_wrap();
    test_legacy_migration();
    test_field_validation_and_io_failure();
    puts("parameter storage: all tests passed");
    return 0;
}
