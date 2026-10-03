#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "spi_flash_layout.h"

static uint8_t flash[4096], eeprom[256], file_data[4096];
static unsigned writes, erases, ee_writes, opens, closes, unlinks, existing;
static unsigned flash_fault, occupied_read_fault, program_fault, erase_fault;
static unsigned ee_fault, ee_mismatch, sd_fault, short_io, total_ms;
static unsigned sd_close_fault, sd_delete_fault, sd_fault_offset;
static unsigned sd_low_level_fault, sd_error_clears;
static uint32_t flash_id;
static uint8_t FLASH_BootProbe(uint32_t *id) {*id=flash_id; return flash_id != 0xef4018UL;}
static uint8_t FLASH_GetIoError(void) {return flash_fault;}
static void FLASH_Read_Data(uint8_t *data, uint32_t addr, uint16_t size)
{
    assert(addr >= SPI_FLASH_DIAG_ADDR && addr + size <= SPI_FLASH_DIAG_ADDR + 4096U);
    memcpy(data, flash + addr - SPI_FLASH_DIAG_ADDR, size);
    if(occupied_read_fault) flash_fault=1U;
}
static void FLASH_Write_Page_v3(uint8_t *data, uint32_t addr, uint16_t size)
{
    assert(addr >= SPI_FLASH_DIAG_ADDR && addr + size <= SPI_FLASH_DIAG_ADDR + 4096U);
    assert((addr & 255U) == 0U && size == 256U);
    writes++;
    if(!program_fault) memcpy(flash + addr - SPI_FLASH_DIAG_ADDR, data, size);
}
static void FLASH_Erase_Sectors(uint32_t addr)
{
    assert(addr == SPI_FLASH_DIAG_ADDR); erases++;
    if(!erase_fault) memset(flash,255,sizeof(flash));
}
static uint8_t EEPROM_Random_Read(uint8_t address, uint8_t *value)
{
    *value = eeprom[address];
    if(ee_mismatch && ee_writes == 1U) *value ^= 1U;
    return ee_fault == 1U;
}
static uint8_t EEPROM_Byte_Write(uint8_t address, uint8_t value)
{
    assert(address == 0xffU); ee_writes++;
    if(ee_fault != 2U) eeprom[address]=value;
    return ee_fault == 2U;
}
typedef unsigned UINT;
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
#include "diskio.h"
static DiskSdError mock_sd_error;
void disk_sd_clear_error(void)
{
    memset(&mock_sd_error, 0, sizeof(mock_sd_error));
    sd_error_clears++;
}
const DiskSdError *disk_sd_get_error(void) { return &mock_sd_error; }
typedef unsigned FRESULT;
/* 保留真实 FatFs 的 4 KiB 扇区缓存，检查文件对象与测试样本确实独立。 */
typedef struct {unsigned pos, size; uint8_t sector[4096];} FIL;
#define FR_OK 0U
#define FR_EXIST 8U
#define FR_DISK_ERR 1U
#define FA_READ 1U
#define FA_WRITE 2U
#define FA_CREATE_NEW 4U
#define f_size(f) ((f)->size)
static FRESULT f_open(FIL *f, const char *path, unsigned mode)
{
    unsigned name;
    assert(sscanf(path,"0:/D%3u.TMP", &name) == 1);
    opens++;
    if(mode & FA_WRITE) {
        assert(mode == (FA_WRITE | FA_CREATE_NEW));
        if(existing == 1000U || name < existing) return FR_EXIST;
        if(sd_fault == 1U) return FR_DISK_ERR;
        memset(file_data,0,sizeof(file_data)); f->size=0U;
    } else {
        assert(mode == FA_READ);
        if(sd_fault == 5U) return FR_DISK_ERR;
        if(sd_fault == 9U) f->size = 4095U;
    }
    f->pos=0U;
    memset(f->sector, 0xA5, sizeof(f->sector));
    return FR_OK;
}
static FRESULT f_write(FIL *f, const void *data, UINT size, UINT *done)
{
    assert(f->pos + size <= sizeof(file_data));
    unsigned fault_here = f->pos == sd_fault_offset;
    if(sd_low_level_fault && fault_here) {
        mock_sd_error.operation = 'W';
        mock_sd_error.phase = sd_low_level_fault == 2U ? DISK_SD_PHASE_CHECK : DISK_SD_PHASE_TRANSFER;
        mock_sd_error.code = sd_low_level_fault == 2U ? 0U : 4U;
        mock_sd_error.result = sd_low_level_fault == 2U ? RES_PARERR : RES_ERROR;
        mock_sd_error.sector = 12345U;
        *done = 0U;
        return FR_DISK_ERR;
    }
    *done=short_io == 1U && fault_here ? size-1U : size;
    memset(f->sector, 0xA5, sizeof(f->sector));
    memcpy(f->sector,data,*done);
    memcpy(file_data + f->pos,f->sector,*done); f->pos+=*done; f->size=f->pos;
    return sd_fault == 2U && fault_here ? FR_DISK_ERR : FR_OK;
}
static FRESULT f_read(FIL *f, void *data, UINT size, UINT *done)
{
    assert(f->pos + size <= sizeof(file_data));
    unsigned fault_here = f->pos == sd_fault_offset;
    *done=short_io == 2U && fault_here ? size-1U : size;
    memset(f->sector, 0xA5, sizeof(f->sector));
    memcpy(f->sector,file_data + f->pos,*done);
    memcpy(data,f->sector,*done); f->pos+=*done;
    if(sd_fault == 6U && fault_here) ((uint8_t*)data)[0]^=1U;
    return sd_fault == 7U && fault_here ? FR_DISK_ERR : FR_OK;
}
static FRESULT f_sync(FIL *f) {(void)f; return sd_fault == 3U ? FR_DISK_ERR : FR_OK;}
static FRESULT f_close(FIL *f)
{
    (void)f; closes++;
    return sd_fault == 4U || (sd_fault == 10U && closes == 2U) ||
           (sd_close_fault & (1U << (closes - 1U))) ? FR_DISK_ERR : FR_OK;
}
static FRESULT f_unlink(const char *path)
{
    unsigned name;
    assert(sscanf(path,"0:/D%3u.TMP", &name) == 1 && name >= existing);
    unlinks++; return sd_fault == 8U || sd_delete_fault ? FR_DISK_ERR : FR_OK;
}

typedef struct {uint32_t CR1, SR, DR;} Uart;
static Uart uart;
#define UART4 (&uart)
#define USART_CR1_RXNEIE 1U
#define USART_FLAG_TXE 2U
#define USART_FLAG_RXNE 4U
#define USART_FLAG_TC 8U
#define USART_SR_PE 16U
#define USART_SR_FE 32U
#define USART_SR_NE 64U
#define USART_SR_ORE 128U
#define SET 1U
static unsigned uart_no_loopback, uart_mismatch, uart_sent;
static unsigned USART_GetFlagStatus(Uart *u, unsigned flag)
{
    assert(!(u->CR1 & USART_CR1_RXNEIE));
    return flag != USART_FLAG_RXNE || !uart_no_loopback;
}
static void USART_SendData(Uart *u, uint8_t data) {u->DR=data; uart_sent++;}
static uint16_t USART_ReceiveData(Uart *u) {return u->DR ^ uart_mismatch;}
static void Delay_ms(unsigned ms) {total_ms+=ms; assert(total_ms < 10000U);}

/* 捕获串口日志，确认清理失败有记录且不会取代屏幕上的首个错误。 */
static char diag_log[4096];
static unsigned diag_log_size;
static int diag_vprintf(const char *format, va_list args)
{
    int count = vsnprintf(diag_log + diag_log_size, sizeof(diag_log) - diag_log_size, format, args);
    assert(count >= 0 && (unsigned)count < sizeof(diag_log) - diag_log_size);
    diag_log_size += (unsigned)count;
    return count;
}
static int diag_printf(const char *format, ...)
{
    int count;
    va_list args;
    va_start(args, format);
    count = diag_vprintf(format, args);
    va_end(args);
    return count;
}
#define printf diag_printf
#define vprintf diag_vprintf
#include "diag_hardware.c"
#undef printf
#undef vprintf

static void reset(void)
{
    memset(flash,255,sizeof(flash)); memset(eeprom,255,sizeof(eeprom));
    writes=erases=ee_writes=opens=closes=unlinks=existing=0U;
    flash_fault=occupied_read_fault=program_fault=erase_fault=0U;
    ee_fault=ee_mismatch=sd_fault=short_io=total_ms=0U;
    sd_close_fault=sd_delete_fault=sd_fault_offset=0U;
    sd_low_level_fault=sd_error_clears=0U;
    memset(&mock_sd_error, 0, sizeof(mock_sd_error));
    diag_log_size=0U; diag_log[0]='\0';
    uart_no_loopback=uart_mismatch=uart_sent=0U;
    memset(&uart,0,sizeof(uart)); uart.CR1=USART_CR1_RXNEIE | 0x8000U;
    flash_id=0xef4018UL;
}

static void sd_detail_tests(void)
{
    static const char * const errors[] = {
        "SD create FR=1: 0:/D000.TMP IO=none",
        "SD write FR=1 offset=768 bytes=256/256 IO=none",
        "SD sync FR=1 IO=none",
        "SD close-write FR=1 IO=none",
        "SD reopen FR=1 IO=none",
        "SD verify offset=768 byte=58/59 IO=none",
        "SD read FR=1 offset=768 bytes=256/256 IO=none",
        "SD delete FR=1: 0:/D000.TMP IO=none",
        "SD size bytes=4095/4096 IO=none",
        "SD close-read FR=1: 0:/D000.TMP IO=none"
    };
    char detail[96];
    unsigned i;
    for(i = 0U; i < sizeof(errors) / sizeof(errors[0]); i++) {
        reset(); sd_fault = i + 1U; sd_fault_offset = 768U;
        assert(hardware_sd_write_test_detail(detail, sizeof(detail)) == HW_FAIL);
        assert(strcmp(detail, errors[i]) == 0);
        assert(strstr(diag_log, errors[i]) != NULL);
        assert(unlinks == (i == 0U ? 0U : 1U));
    }
    reset(); short_io=1U; sd_fault_offset=768U;
    assert(hardware_sd_write_test_detail(detail, sizeof(detail)) == HW_FAIL);
    assert(strcmp(detail, "SD write FR=0 offset=768 bytes=255/256 IO=none") == 0);
    reset(); short_io=2U; sd_fault_offset=768U;
    assert(hardware_sd_write_test_detail(detail, sizeof(detail)) == HW_FAIL);
    assert(strcmp(detail, "SD read FR=0 offset=768 bytes=255/256 IO=none") == 0);

    reset(); sd_fault=2U; sd_close_fault=1U; sd_delete_fault=1U;
    assert(hardware_sd_write_test_detail(detail, sizeof(detail)) == HW_FAIL);
    assert(strcmp(detail, "SD write FR=1 offset=0 bytes=256/256 IO=none") == 0);
    assert(strstr(diag_log, "SD close-write FR=1: 0:/D000.TMP") != NULL);
    assert(strstr(diag_log, "SD delete FR=1: 0:/D000.TMP") != NULL);
    reset(); sd_fault=6U; sd_close_fault=2U; sd_delete_fault=1U;
    assert(hardware_sd_write_test_detail(detail, sizeof(detail)) == HW_FAIL);
    assert(strcmp(detail, "SD verify offset=0 byte=5B/5A IO=none") == 0);
    assert(strstr(diag_log, "SD close-read FR=1: 0:/D000.TMP") != NULL);
    assert(strstr(diag_log, "SD delete FR=1: 0:/D000.TMP") != NULL);
    reset(); sd_close_fault=2U; sd_delete_fault=1U;
    assert(hardware_sd_write_test_detail(detail, sizeof(detail)) == HW_FAIL);
    assert(strcmp(detail, "SD close-read FR=1: 0:/D000.TMP IO=none") == 0);
    assert(strstr(diag_log, "SD delete FR=1: 0:/D000.TMP") != NULL);

    /* 首次扇区错误和 LBA 追加到屏幕；清理不能替换主错误。 */
    reset(); sd_low_level_fault=1U; sd_close_fault=1U; sd_delete_fault=1U;
    assert(hardware_sd_write_test_detail(detail, 72U) == HW_FAIL);
    assert(strcmp(detail, "SD write FR=1 offset=0 bytes=0/256 W2 D1 SD4 L12345") == 0);
    assert(strstr(diag_log, "disk=W/2 D=1 SD=4 LBA=12345") != NULL);
    assert(sd_error_clears == 1U);
    sd_low_level_fault=sd_close_fault=sd_delete_fault=0U;
    assert(hardware_sd_write_test_detail(detail, 72U) == HW_PASS);
    assert(strcmp(detail, "SD 4KiB verified and deleted") == 0);
    assert(sd_error_clears == 2U && mock_sd_error.code == 0U);
    reset(); sd_low_level_fault=2U;
    assert(hardware_sd_write_test_detail(detail, 72U) == HW_FAIL);
    assert(strcmp(detail, "SD write FR=1 offset=0 bytes=0/256 W0 D4 SD0 L12345") == 0);
    for(i = 1U; i <= 48U; i++) {
        char guarded[50];
        reset(); sd_low_level_fault=1U;
        memset(guarded, '?', sizeof(guarded));
        assert(hardware_sd_write_test_detail(guarded + 1, (uint16_t)i) == HW_FAIL);
        assert(guarded[0] == '?' && guarded[i + 1U] == '?');
        assert(memchr(guarded + 1, '\0', i) != NULL);
    }

    reset(); existing=3U;
    assert(hardware_sd_write_test_detail(detail, sizeof(detail)) == HW_PASS);
    assert(strcmp(detail, "SD 4KiB verified and deleted") == 0);
    assert(opens == 5U && closes == 2U && unlinks == 1U);
    reset(); existing=1000U;
    assert(hardware_sd_write_test_detail(detail, sizeof(detail)) == HW_BLOCKED);
    assert(strcmp(detail, "SD create blocked: D000-D999.TMP all exist") == 0);
    assert(opens == 1000U && !closes && !unlinks);

    /* 空指针、零容量和截断都不能破坏缓冲边界或改变测试结论。 */
    for(i = 0U; i < 2U; i++) {
        char guarded[] = {'L', '?', '?', '?', '?', '?', '?', '?', '?', 'R'};
        HwResult expected = i ? HW_FAIL : HW_PASS;
        reset(); sd_fault=i;
        assert(hardware_sd_write_test_detail(NULL, 96U) == expected);
        reset(); sd_fault=i;
        assert(hardware_sd_write_test_detail(guarded + 1, 0U) == expected);
        assert(guarded[1] == '?');
        reset(); sd_fault=i;
        assert(hardware_sd_write_test_detail(guarded + 1, 1U) == expected);
        assert(guarded[0] == 'L' && guarded[1] == '\0' && guarded[2] == '?');
        reset(); sd_fault=i;
        assert(hardware_sd_write_test_detail(guarded + 1, 8U) == expected);
        assert(guarded[0] == 'L' && guarded[8] == '\0' && guarded[9] == 'R');
    }
}
int main(void)
{
    unsigned i;
    assert(hardware_memory_test() == HW_PASS);
    reset(); assert(hardware_flash_write_test() == HW_PASS && writes == 16U && erases == 1U);
    for(i=0;i<sizeof(flash);i++) assert(flash[i] == 255U);
    reset(); flash[4095]=0x79U;
    assert(hardware_flash_write_test() == HW_BLOCKED && !writes && !erases && flash[4095] == 0x79U);
    reset(); occupied_read_fault=1U;
    assert(hardware_flash_write_test() == HW_FAIL && !writes && !erases);
    reset(); flash_id=0U; assert(hardware_flash_write_test() == HW_FAIL && !erases);
    reset(); program_fault=1U; assert(hardware_flash_write_test() == HW_FAIL && erases == 1U);
    reset(); erase_fault=1U; assert(hardware_flash_write_test() == HW_FAIL);
    reset(); assert(hardware_eeprom_write_test() == HW_PASS && ee_writes == 3U);
    for(i=0;i<sizeof(eeprom);i++) assert(eeprom[i] == 255U);
    reset(); eeprom[255]=0x79U;
    assert(hardware_eeprom_write_test() == HW_BLOCKED && !ee_writes && eeprom[255] == 0x79U);
    reset(); ee_fault=1U; assert(hardware_eeprom_write_test() == HW_FAIL && !ee_writes);
    reset(); ee_fault=2U; assert(hardware_eeprom_write_test() == HW_FAIL);
    reset(); ee_mismatch=1U; assert(hardware_eeprom_write_test() == HW_FAIL && eeprom[255] == 255U);
    reset(); existing=3U; assert(hardware_sd_write_test() == HW_PASS && unlinks == 1U && closes == 2U);
    reset(); existing=1000U; assert(hardware_sd_write_test() == HW_BLOCKED && !unlinks);
    for(i=1;i<=8;i++) {
        reset(); sd_fault=i;
        assert(hardware_sd_write_test() == HW_FAIL);
        assert(unlinks == (i == 1U ? 0U : 1U));
        assert(hardware_memory_test() == HW_PASS);
        reset();
        assert(hardware_sd_write_test() == HW_PASS && closes == 2U && unlinks == 1U);
    }
    reset(); short_io=1U; assert(hardware_sd_write_test() == HW_FAIL && unlinks == 1U);
    reset(); short_io=2U; assert(hardware_sd_write_test() == HW_FAIL && unlinks == 1U);
    reset(); assert(hardware_uart_loopback_test() == HW_PASS && uart_sent == 32U && uart.CR1 == 0x8001U);
    reset(); uart_no_loopback=1U;
    assert(hardware_uart_loopback_test() == HW_FAIL && total_ms <= 102U && uart.CR1 == 0x8001U);
    reset(); uart_mismatch=1U; assert(hardware_uart_loopback_test() == HW_FAIL && uart.CR1 == 0x8001U);
    reset(); uart.SR=USART_SR_ORE; assert(hardware_uart_loopback_test() == HW_FAIL && uart.CR1 == 0x8001U);
    /* 连续切换检测类型，上一种检测留下的数据不应影响后续检测。 */
    for(i = 0U; i < 3U; i++) {
        reset();
        assert(hardware_memory_test() == HW_PASS);
        assert(hardware_flash_write_test() == HW_PASS && writes == 16U && erases == 1U);
        assert(hardware_memory_test() == HW_PASS);
        assert(hardware_sd_write_test() == HW_PASS && closes == 2U && unlinks == 1U);
        assert(hardware_memory_test() == HW_PASS);
    }
    sd_detail_tests();
    puts("PASS: diagnostic storage guards, readback failures, cleanup, UART timeout/restore, owned RAM");
    return 0;
}
