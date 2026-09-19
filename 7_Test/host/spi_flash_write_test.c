#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "spi_flash_layout.h"

static uint8_t storage[SPI_FLASH_LAYOUT_TOTAL_SIZE], flash_io_error;
static uint8_t selected, opcode, write_enabled, refuse_write_enable;
static uint32_t address, start_address;
static unsigned phase, payload, program_calls, erase_calls;
static int transfer_budget = -1;

static void chip_select(unsigned active)
{
    if(active) {
        assert(!selected);
        selected = 1U;
        phase = payload = 0U;
        address = start_address = 0UL;
        opcode = 0U;
    } else {
        assert(selected);
        if(opcode == 0x06U && !refuse_write_enable) write_enabled = 1U;
        if(opcode == 0x02U) { write_enabled = 0U; program_calls++; }
        if(opcode == 0x20U && phase == 4U && !flash_io_error) {
            assert(write_enabled && address % 4096UL == 0UL);
            memset(storage + address, 0xff, 4096U);
            write_enabled = 0U;
            erase_calls++;
        }
        selected = 0U;
    }
}
#define FLASH_SPI_CS_LOW chip_select(1U)
#define FLASH_SPI_CS_HIGH chip_select(0U)

static uint8_t FLASH_Send_Byte(uint8_t byte)
{
    uint8_t result = 0xffU;
    assert(selected);
    if(flash_io_error) return result;
    if(transfer_budget == 0) { flash_io_error = 2U; return result; }
    if(transfer_budget > 0) transfer_budget--;
    if(phase == 0U) opcode = byte;
    else if(opcode == 0x05U) result = write_enabled ? 2U : 0U;
    else if(phase <= 3U) {
        address = (address << 8) | byte;
        if(phase == 3U) start_address = address;
    } else if(opcode == 0x02U) {
        assert(write_enabled && address < sizeof(storage));
        /* A physical NOR wraps at 256 bytes. Reject crossing here so the
         * test catches corrupting commands even if final data looks close. */
        assert(start_address / 256U == address / 256U);
        storage[address++] &= byte;
        assert(++payload <= 256U);
    } else if(opcode == 0x03U) result = storage[address++];
    phase++;
    return result;
}

static uint8_t FLASH_Receive_Byte(void) { return FLASH_Send_Byte(0xffU); }
static void Delay_ms(uint32_t ms) { (void)ms; assert(0 && "unexpected busy device"); }
static uint8_t FLASH_Error_CallBack(uint8_t code) { flash_io_error = code; return code; }
static uint8_t Flash_Wait_For_Standby(uint32_t);
static void Flash_Write_Enable(void);
void FLASH_Write_Data(uint8_t *, uint32_t, uint16_t);
#include "flash_write_impl.inc"

static void reset(void)
{
    memset(storage, 0xff, sizeof(storage));
    flash_io_error = selected = write_enabled = refuse_write_enable = 0U;
    program_calls = erase_calls = 0U;
    transfer_budget = -1;
}

int main(void)
{
    static const uint32_t offsets[] = {0U, 1U, 127U, 250U, 255U, 256U, 4095U};
    static const uint16_t lengths[] = {0U, 1U, 255U, 256U, 257U, 4096U, 8193U, 65535U};
    static uint8_t data[65535U], result[65535U];
    unsigned i, j, k, expected_calls;
    for(i = 0U; i < sizeof(data); i++) data[i] = (uint8_t)(i * 17U + 53U);
    for(i = 0U; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        for(j = 0U; j < sizeof(lengths) / sizeof(lengths[0]); j++) {
            for(k = 0U; k < 3U; k++) {
                reset();
                if(k == 0U) FLASH_Write_Data(data, offsets[i], lengths[j]);
                else if(k == 1U) FLASH_Write_Page_v2(offsets[i], data, lengths[j]);
                else FLASH_Write_Page_v3(data, offsets[i], lengths[j]);
                assert(flash_io_error == 0U && selected == 0U);
                assert(memcmp(storage + offsets[i], data, lengths[j]) == 0);
                assert(storage[offsets[i] + lengths[j]] == 0xffU);
                if(offsets[i] != 0U) assert(storage[offsets[i] - 1U] == 0xffU);
                expected_calls = lengths[j] == 0U ? 0U :
                    (unsigned)((offsets[i] % 256U + lengths[j] + 255U) / 256U);
                assert(program_calls == expected_calls);
                FLASH_Read_Data(result, offsets[i], lengths[j]);
                assert(memcmp(result, data, lengths[j]) == 0);
            }
        }
    }
    reset();
    FLASH_Write_Data(data, sizeof(storage) - 3U, 3U);
    assert(flash_io_error == 0U);
    reset();
    FLASH_Write_Data(data, sizeof(storage) - 3U, 4U);
    assert(flash_io_error != 0U && program_calls == 0U);
    reset();
    FLASH_Write_Data(data, 0xffffffffUL, 1U);
    assert(flash_io_error != 0U && program_calls == 0U);
    reset();
    FLASH_Write_Page_v1(255U, data, 2U);
    assert(flash_io_error != 0U && program_calls == 0U);
    reset();
    FLASH_Erase_Sectors(1U);
    assert(flash_io_error != 0U && erase_calls == 0U);
    reset();
    FLASH_Erase_Sectors(sizeof(storage));
    assert(flash_io_error != 0U && erase_calls == 0U);
    reset();
    storage[4096] = 0U;
    FLASH_Erase_Sectors(4096U);
    assert(flash_io_error == 0U && erase_calls == 1U && storage[4096] == 0xffU);
    reset();
    refuse_write_enable = 1U;
    FLASH_Write_Data(data, 0U, 512U);
    assert(flash_io_error != 0U && program_calls == 0U && storage[0] == 0xffU);
    reset();
    transfer_budget = 30;
    FLASH_Write_Data(data, 0U, 512U);
    assert(flash_io_error != 0U && program_calls == 1U && storage[256] == 0xffU);
    FLASH_Erase_Sectors(0U);
    assert(erase_calls == 0U); /* A sticky I/O fault cannot trigger further mutations. */
    puts("SPI flash: page boundaries, 16-bit lengths, bounds and write faults passed");
    return 0;
}
