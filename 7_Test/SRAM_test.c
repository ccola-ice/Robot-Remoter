#include "SRAM_test.h"

/* External SRAM only. Save/restore each block using compiler-allocated RAM.
 * Never use a literal address in MCU RAM, Flash, EEPROM or an SD/FatFs volume.
 * Run before mounting filesystems or starting application consumers.
 * This is a block R/W check, not a complete address-alias/March memory test. */
static uint8_t sram_test_region(volatile uint8_t *base, uint32_t size)
{
    uint8_t backup[256];
    uint32_t offset, i, count;
    uint8_t failed, pattern8;
    uint16_t pattern16;
    volatile uint16_t *words;

    if((size & 1U) != 0U) return 0U;
    for(offset = 0U; offset < size; offset += sizeof(backup)) {
        count = size - offset;
        if(count > sizeof(backup)) count = sizeof(backup);
        failed = 0U;
        for(i = 0U; i < count; i++) backup[i] = base[offset + i];
        for(i = 0U; i < count; i++)
            base[offset + i] = (uint8_t)(i ^ 0x55U);
        for(i = 0U; i < count; i++) {
            pattern8 = (uint8_t)(i ^ 0x55U);
            if(base[offset + i] != pattern8) failed = 1U;
            base[offset + i] = (uint8_t)~pattern8;
        }
        for(i = 0U; i < count; i++) {
            pattern8 = (uint8_t)~(i ^ 0x55U);
            if(base[offset + i] != pattern8) failed = 1U;
        }
        words = (volatile uint16_t *)(base + offset);
        for(i = 0U; i < count / 2U; i++)
            words[i] = (uint16_t)((offset / 2U + i) ^ 0xa55aU);
        for(i = 0U; i < count / 2U; i++) {
            pattern16 = (uint16_t)((offset / 2U + i) ^ 0xa55aU);
            if(words[i] != pattern16) failed = 1U;
            words[i] = (uint16_t)~pattern16;
        }
        for(i = 0U; i < count / 2U; i++) {
            pattern16 = (uint16_t)~((offset / 2U + i) ^ 0xa55aU);
            if(words[i] != pattern16) failed = 1U;
        }
        for(i = 0U; i < count; i++) base[offset + i] = backup[i];
        for(i = 0U; i < count; i++)
            if(base[offset + i] != backup[i]) failed = 1U;
        if(failed != 0U) {
            printf("[BOOT] SRAM compare/restore failed, offset=0x%08lX\r\n",
                   (unsigned long)offset);
            return 0U;
        }
    }
    return 1U;
}

uint8_t sram_read_write_test(void)
{
    return sram_test_region((volatile uint8_t *)SRAM_BASE_ADDR,
                             IS62WV51216_SIZE);
}
