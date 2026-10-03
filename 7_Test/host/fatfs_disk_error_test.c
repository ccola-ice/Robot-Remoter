#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ff.h"
#include "diskio.h"
#include "spi_flash_layout.h"

#define __IO volatile
#define __weak
typedef uint8_t u8;
#include "production_sd_types.h"

/* 4 MiB FAT16 卡：引导扇区、两份 FAT、固定根目录和单扇区簇。 */
enum {
    SECTOR_BYTES = 512,
    CARD_SECTORS = 8192,
    FAT_SECTORS = 32,
    FIRST_FAT_SECTOR = 1,
    ROOT_SECTOR = 1 + 2 * FAT_SECTORS,
    DATA_SECTOR = ROOT_SECTOR + 32
};
static BYTE card[CARD_SECTORS][SECTOR_BYTES];
static FATFS filesystem;
static unsigned read_calls, write_calls, abort_calls;
static DWORD fail_read_sector, fail_write_sector;
static DWORD first_write_sector;
static SD_Error read_error, write_error;
SD_CardInfo SDCardInfo;

SD_Error SD_Init(void) { return SD_OK; }
SDTransferState SD_GetStatus(void) { return SD_TRANSFER_OK; }
SD_Error SD_WaitReadOperation(void) { return SD_OK; }
SD_Error SD_WaitWriteOperation(void) { return SD_OK; }
SD_Error SD_WaitReady(void) { return SD_OK; }
void SD_AbortTransfer(void) { abort_calls++; }

SD_Error SD_ReadMultiBlocks(uint8_t *buffer, uint64_t address, uint16_t size, uint32_t count)
{
    DWORD sector = (DWORD)(address / SECTOR_BYTES);
    read_calls++;
    assert(size == SECTOR_BYTES && address % SECTOR_BYTES == 0U);
    assert(sector < CARD_SECTORS && count <= CARD_SECTORS - sector);
    if (sector == fail_read_sector) return read_error;
    memcpy(buffer, card[sector], size * count);
    return SD_OK;
}

SD_Error SD_WriteMultiBlocks(uint8_t *buffer, uint64_t address, uint16_t size, uint32_t count)
{
    DWORD sector = (DWORD)(address / SECTOR_BYTES);
    if (write_calls++ == 0U) first_write_sector = sector;
    assert(size == SECTOR_BYTES && address % SECTOR_BYTES == 0U);
    assert(sector < CARD_SECTORS && count <= CARD_SECTORS - sector);
    if (sector == fail_write_sector) return write_error;
    memcpy(card[sector], buffer, size * count);
    return SD_OK;
}

/* 当前集成测试只挂载 SD；若误用 SPI Flash，应立即失败。 */
#define FLASH_ID 0xef4018U
#define FLASH_SECTOR_SIZE 4096U
static void FLASH_SPI_Init(void) { assert(0); }
static uint32_t FLASH_Read_FlashID(void) { assert(0); return 0U; }
static uint8_t FLASH_GetIoError(void) { assert(0); return 1U; }
static void FLASH_Read_Data(uint8_t *buffer, uint32_t address, uint16_t size)
{
    (void)buffer; (void)address; (void)size; assert(0);
}
static void FLASH_Erase_Sectors(uint32_t address) { (void)address; assert(0); }
static void FLASH_Write_Data(uint8_t *buffer, uint32_t address, uint16_t size)
{
    (void)buffer; (void)address; (void)size; assert(0);
}
#include "production_diskio.inc"

static void store_word(BYTE *target, unsigned value)
{
    target[0] = (BYTE)value;
    target[1] = (BYTE)(value >> 8);
}

static void fresh_card(void)
{
    BYTE *boot = card[0];
    assert(f_mount(NULL, "0:", 0) == FR_OK);
    memset(card, 0, sizeof(card));
    memset(&filesystem, 0, sizeof(filesystem));
    boot[0] = 0xeb; boot[1] = 0x3c; boot[2] = 0x90;
    memcpy(boot + 3, "RAM TEST", 8);
    store_word(boot + 11, SECTOR_BYTES);
    boot[13] = 1;
    store_word(boot + 14, 1);
    boot[16] = 2;
    store_word(boot + 17, 512);
    store_word(boot + 19, CARD_SECTORS);
    boot[21] = 0xf8;
    store_word(boot + 22, FAT_SECTORS);
    store_word(boot + 24, 63);
    store_word(boot + 26, 255);
    boot[36] = 0x80; boot[38] = 0x29;
    memcpy(boot + 43, "TEST CARD  ", 11);
    memcpy(boot + 54, "FAT16   ", 8);
    store_word(boot + 510, 0xaa55);
    store_word(card[FIRST_FAT_SECTOR], 0xfff8);
    store_word(card[FIRST_FAT_SECTOR] + 2, 0xffff);
    memcpy(card[FIRST_FAT_SECTOR + FAT_SECTORS], card[FIRST_FAT_SECTOR], SECTOR_BYTES);
    SDCardInfo.CardCapacity = (uint64_t)CARD_SECTORS * SECTOR_BYTES;
    SDCardInfo.CardBlockSize = SECTOR_BYTES;
    fail_read_sector = fail_write_sector = UINT32_MAX;
    read_error = write_error = SD_OK;
    read_calls = write_calls = abort_calls = 0U;
    first_write_sector = UINT32_MAX;
    disk_sd_clear_error();
    assert(f_mount(&filesystem, "0:", 1) == FR_OK);
    assert(filesystem.fs_type == FS_FAT16);
}

static void create_file(FIL *file)
{
    assert(f_open(file, "0:/D000.TMP", FA_CREATE_NEW | FA_WRITE) == FR_OK);
    /* 创建目录项尚在 FatFs 缓存中，此时根本没有发生物理写入。 */
    assert(write_calls == 0U);
    assert(f_size(file) == 0U);
}

static void assert_error(BYTE operation, BYTE phase, DRESULT result,
                         SD_Error code, DWORD sector)
{
    const DiskSdError *error = disk_sd_get_error();
    assert(error->operation == operation);
    assert(error->phase == phase);
    assert(error->result == result);
    assert(error->code == code);
    assert(error->sector == sector);
}

static void test_write_and_remount(void)
{
    FIL file;
    BYTE payload[256], recovered[256];
    UINT written, received;
    unsigned i;
    fresh_card();
    create_file(&file);
    for (i = 0; i < sizeof(payload); i++) payload[i] = (BYTE)(i ^ 0x5aU);
    assert(f_write(&file, payload, sizeof(payload), &written) == FR_OK);
    assert(written == sizeof(payload));
    /* 第一次 256 字节写入只写回目录扇区，数据仍在文件缓存里。 */
    assert(write_calls == 1U && first_write_sector == ROOT_SECTOR);
    assert(f_sync(&file) == FR_OK);
    assert(f_close(&file) == FR_OK);
    assert(memcmp(card[DATA_SECTOR], payload, sizeof(payload)) == 0);
    assert(disk_sd_get_error()->operation == 0U);
    assert(f_mount(NULL, "0:", 0) == FR_OK);
    memset(&filesystem, 0, sizeof(filesystem));
    assert(f_mount(&filesystem, "0:", 1) == FR_OK);
    assert(f_open(&file, "0:/D000.TMP", FA_READ) == FR_OK);
    assert(f_size(&file) == sizeof(payload));
    assert(f_read(&file, recovered, sizeof(recovered), &received) == FR_OK);
    assert(received == sizeof(recovered) && memcmp(payload, recovered, received) == 0);
    assert(f_close(&file) == FR_OK);
    puts("PASS: real FatFs create/write 256/sync/remount/readback");
}

static void test_directory_write_error(void)
{
    FIL file;
    BYTE payload[256] = {0};
    UINT written = 999;
    fresh_card();
    create_file(&file);
    fail_write_sector = ROOT_SECTOR;
    write_error = SD_TX_UNDERRUN;
    assert(f_write(&file, payload, sizeof(payload), &written) == FR_DISK_ERR);
    assert(written == 0U && first_write_sector == ROOT_SECTOR && abort_calls == 1U);
    assert_error('W', DISK_SD_PHASE_START, RES_ERROR, SD_TX_UNDERRUN, ROOT_SECTOR);
    /* 后续读请求和清理失败不能覆盖首次写入故障。 */
    assert(disk_read(0, card[0], 0, 1) == RES_NOTRDY);
    (void)f_close(&file);
    (void)f_unlink("0:/D000.TMP");
    assert_error('W', DISK_SD_PHASE_START, RES_ERROR, SD_TX_UNDERRUN, ROOT_SECTOR);
    puts("PASS: first f_write returns FR_DISK_ERR/0 bytes on directory write; first cause survives cleanup");
}

static void test_fat_read_error_during_write(void)
{
    FIL file;
    BYTE payload[256] = {0};
    UINT written = 999;
    fresh_card();
    create_file(&file);
    fail_read_sector = FIRST_FAT_SECTOR;
    read_error = SD_DATA_CRC_FAIL;
    assert(f_write(&file, payload, sizeof(payload), &written) == FR_DISK_ERR);
    assert(written == 0U && write_calls == 1U && abort_calls == 1U);
    assert_error('R', DISK_SD_PHASE_START, RES_ERROR, SD_DATA_CRC_FAIL, FIRST_FAT_SECTOR);
    (void)f_close(&file);
    puts("PASS: first f_write records read direction when allocation FAT read fails");
}

static void test_directory_sector_rejected(void)
{
    FIL file;
    BYTE payload[256] = {0};
    UINT written = 999;
    unsigned reads_before;
    fresh_card();
    create_file(&file);
    reads_before = read_calls;
    /* 注入容量信息错误：根目录地址不再通过 diskio 的扇区边界检查。 */
    SDCardInfo.CardCapacity = (uint64_t)ROOT_SECTOR * SECTOR_BYTES;
    assert(f_write(&file, payload, sizeof(payload), &written) == FR_DISK_ERR);
    assert(written == 0U && write_calls == 0U && read_calls == reads_before);
    assert(abort_calls == 0U);
    assert_error('W', DISK_SD_PHASE_CHECK, RES_PARERR, SD_OK, ROOT_SECTOR);
    (void)f_close(&file);
    (void)f_unlink("0:/D000.TMP");
    assert_error('W', DISK_SD_PHASE_CHECK, RES_PARERR, SD_OK, ROOT_SECTOR);
    puts("PASS: bounds rejection records RES_PARERR/phase CHECK without starting SD hardware");
}

int main(void)
{
    assert(sizeof(DWORD) == 4U && sizeof(WORD) == 2U);
    test_write_and_remount();
    test_directory_write_error();
    test_fat_read_error_during_write();
    test_directory_sector_rejected();
    assert(f_mount(NULL, "0:", 0) == FR_OK);
    puts("FatFs/diskio integration tests passed.");
    return 0;
}
