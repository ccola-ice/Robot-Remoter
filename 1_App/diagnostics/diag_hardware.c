#include "diag_hardware.h"
#include "bsp_spi_flash.h"
#include "spi_flash_layout.h"
#include "bsp_i2c_eeprom.h"
#include "bsp_Systick.h"
#include "ff.h"
#include "diskio.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* 启动及菜单串行调用检测入口，不允许嵌套或并行执行。
 * RAM 检测与存储器读写检测互斥，复用本模块自有空间；RAM 访问保持 volatile。 */
static union {
    volatile uint32_t ram[256];
    struct {
        uint8_t sample[256];
        uint8_t verify[256];
    } io;
} test_buffer;
static FIL test_file;

const char *hardware_result_name(HwResult result)
{
    static const char * const names[] = {"PASS", "FAIL", "NOT TESTED / BLOCKED", "CANCELLED"};
    return result <= HW_CANCELLED ? names[result] : "FAIL";
}

/* ROM 常量验证只读访问，RAM 使用逐位与反码模式；发现首个不一致立即返回失败。 */
HwResult hardware_memory_test(void)
{
    static const uint32_t rom[] = {0x01234567UL, 0x89abcdefUL,
                                   0x55aa55aaUL, 0xaa55aa55UL};
    const volatile uint32_t *probe = rom; /* 将对象保留在 ROM 中，并强制通过总线实际读取。 */
    uint32_t i, bit, pattern;
    if(probe[0] != 0x01234567UL || probe[1] != 0x89abcdefUL ||
       probe[2] != 0x55aa55aaUL || probe[3] != 0xaa55aa55UL) return HW_FAIL;
    /* 仅测试模块自有 RAM；逐位模式叠加索引，覆盖数据位翻转与样本内寻址。 */
    for(bit = 0; bit < 32U; bit++) {
        pattern = 1UL << bit;
        for(i = 0; i < 256U; i++) test_buffer.ram[i] = pattern ^ i;
        for(i = 0; i < 256U; i++) if(test_buffer.ram[i] != (pattern ^ i)) return HW_FAIL;
        for(i = 0; i < 256U; i++) test_buffer.ram[i] = ~(pattern ^ i);
        for(i = 0; i < 256U; i++) if(test_buffer.ram[i] != ~(pattern ^ i)) return HW_FAIL;
    }
    return HW_PASS;
}

/* 先逐块确认整个诊断扇区空白，再按物理页写入和回读；结束时统一擦除并检查清理结果。 */
HwResult hardware_flash_write_test(void)
{
    uint32_t id, offset;
    uint16_t i;
    HwResult result = HW_PASS;
    if(FLASH_BootProbe(&id)) return HW_FAIL;
    /* 不得擦除已经存有资源、参数或文件系统数据的扇区。 */
    for(offset = 0; offset < SPI_FLASH_LAYOUT_SECTOR_SIZE; offset += sizeof(test_buffer.io.sample)) {
        FLASH_Read_Data(test_buffer.io.sample, SPI_FLASH_DIAG_ADDR + offset,
                        sizeof(test_buffer.io.sample));
        if(FLASH_GetIoError()) return HW_FAIL;
        for(i = 0; i < sizeof(test_buffer.io.sample); i++)
            if(test_buffer.io.sample[i] != 0xffU) return HW_BLOCKED;
    }
    for(offset = 0; offset < SPI_FLASH_LAYOUT_SECTOR_SIZE; offset += sizeof(test_buffer.io.sample)) {
        for(i = 0; i < sizeof(test_buffer.io.sample); i++)
            test_buffer.io.sample[i] = (uint8_t)(i ^ (offset >> 8) ^ 0xa5U);
        /* W25Q128 物理页大小为 256 字节，不得使用旧版的 4 KiB 页写入方式。 */
        FLASH_Write_Page_v3(test_buffer.io.sample, SPI_FLASH_DIAG_ADDR + offset,
                            sizeof(test_buffer.io.sample));
        FLASH_Read_Data(test_buffer.io.verify, SPI_FLASH_DIAG_ADDR + offset,
                        sizeof(test_buffer.io.verify));
        if(FLASH_GetIoError() || memcmp(test_buffer.io.sample, test_buffer.io.verify,
                                       sizeof(test_buffer.io.sample))) {
            result = HW_FAIL;
            break;
        }
    }
    /* 只允许擦除本次测试使用、且测试前已确认为空白的专用扇区。 */
    FLASH_Erase_Sectors(SPI_FLASH_DIAG_ADDR);
    for(offset = 0; offset < SPI_FLASH_LAYOUT_SECTOR_SIZE; offset += sizeof(test_buffer.io.sample)) {
        FLASH_Read_Data(test_buffer.io.verify, SPI_FLASH_DIAG_ADDR + offset,
                        sizeof(test_buffer.io.verify));
        if(FLASH_GetIoError()) result = HW_FAIL;
        for(i = 0; i < sizeof(test_buffer.io.verify); i++)
            if(test_buffer.io.verify[i] != 0xffU) result = HW_FAIL;
    }
    return result;
}

HwResult hardware_eeprom_write_test(void)
{
    uint8_t value, i, patterns[] = {0x55U, 0xaaU};
    HwResult result = HW_PASS;
    /* 仅占用空白的诊断保留字节；测试写入失败时也尝试写回并验证 0xFF。 */
    if(EEPROM_Random_Read(HW_EEPROM_TEST_BYTE, &value)) return HW_FAIL;
    if(value != 0xffU) return HW_BLOCKED;
    for(i = 0U; i < sizeof(patterns); i++) {
        if(EEPROM_Byte_Write(HW_EEPROM_TEST_BYTE, patterns[i]) ||
           EEPROM_Random_Read(HW_EEPROM_TEST_BYTE, &value) || value != patterns[i]) {
            result = HW_FAIL;
            break;
        }
    }
    if(EEPROM_Byte_Write(HW_EEPROM_TEST_BYTE, 0xffU) ||
       EEPROM_Random_Read(HW_EEPROM_TEST_BYTE, &value) || value != 0xffU) result = HW_FAIL;
    return result;
}

/* 屏幕复用调用方缓冲，串口保留完整信息；只在结果或错误产生时输出。 */
static void sd_test_detail(char *detail, uint16_t capacity, HwResult result,
                           const char *format, ...)
{
    va_list args;
    const DiskSdError *error = disk_sd_get_error();
    if(detail != NULL && capacity != 0U) {
        va_start(args, format);
        vsnprintf(detail, capacity, format, args);
        va_end(args);
        detail[capacity - 1U] = '\0';
        /* 请求拒绝时 SD 错误码为零，必须按操作标记判断快照是否有效。 */
        if(result == HW_FAIL) {
            size_t used = strlen(detail);
            if(error->operation != 0U)
                snprintf(detail + used, capacity - used, " %c%u D%u SD%u L%lu",
                         error->operation, (unsigned)error->phase, (unsigned)error->result,
                         (unsigned)error->code, (unsigned long)error->sector);
            else
                snprintf(detail + used, capacity - used, " IO=none");
            detail[capacity - 1U] = '\0';
        }
    }
    printf("[DIAG] ");
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    if(result == HW_FAIL) {
        if(error->operation != 0U)
            printf(" disk=%c/%u D=%u SD=%u LBA=%lu", error->operation,
                   (unsigned)error->phase, (unsigned)error->result,
                   (unsigned)error->code, (unsigned long)error->sector);
        else
            printf(" IO=none");
    }
    printf("\r\n");
}

/* 创建临时文件后，所有读写错误都转到 cleanup；opened 标记句柄是否还需关闭，
 * close_stage 区分写后关闭和读后关闭，便于串口定位失败阶段。 */
HwResult hardware_sd_write_test_detail(char *detail, uint16_t capacity)
{
    char path[24];
    const char *close_stage = "close-write";
    FRESULT code;
    UINT transferred;
    uint16_t name, block, i;
    uint8_t opened = 0U;
    HwResult result = HW_FAIL;
    if(detail != NULL && capacity != 0U) detail[0] = '\0';
    /* 只在新测试开始时清除，后续关闭/删除失败不得覆盖首次底层错误。 */
    disk_sd_clear_error();
    /* 必须使用 CREATE_NEW，绝不能截断已有文件。 */
    for(name = 0; name < 1000U; name++) {
        sprintf(path, "0:/D%03u.TMP", name);
        code = f_open(&test_file, path, FA_WRITE | FA_CREATE_NEW);
        if(code == FR_OK) break;
        if(code != FR_EXIST) {
            sd_test_detail(detail, capacity, HW_FAIL, "SD create FR=%u: %s", (unsigned)code, path);
            return HW_FAIL;
        }
    }
    if(name == 1000U) {
        sd_test_detail(detail, capacity, HW_BLOCKED, "SD create blocked: D000-D999.TMP all exist");
        return HW_BLOCKED;
    }
    opened = 1U;
    for(block = 0; block < 16U; block++) {
        for(i = 0; i < sizeof(test_buffer.io.sample); i++)
            test_buffer.io.sample[i] = (uint8_t)(i ^ block ^ 0x5aU);
        transferred = 0U;
        code = f_write(&test_file, test_buffer.io.sample, sizeof(test_buffer.io.sample), &transferred);
        if(code != FR_OK || transferred != sizeof(test_buffer.io.sample)) {
            sd_test_detail(detail, capacity, HW_FAIL, "SD write FR=%u offset=%lu bytes=%u/%u",
                           (unsigned)code, (unsigned long)(block * sizeof(test_buffer.io.sample)),
                           (unsigned)transferred, (unsigned)sizeof(test_buffer.io.sample));
            goto cleanup;
        }
    }
    /* 写入完成后同步并关闭，再重新打开逐块核对，覆盖文件写入与读取流程。 */
    code = f_sync(&test_file);
    if(code != FR_OK) {
        sd_test_detail(detail, capacity, HW_FAIL, "SD sync FR=%u", (unsigned)code);
        goto cleanup;
    }
    code = f_close(&test_file);
    opened = 0U;
    if(code != FR_OK) {
        sd_test_detail(detail, capacity, HW_FAIL, "SD close-write FR=%u", (unsigned)code);
        goto cleanup;
    }
    code = f_open(&test_file, path, FA_READ);
    if(code != FR_OK) {
        sd_test_detail(detail, capacity, HW_FAIL, "SD reopen FR=%u", (unsigned)code);
        goto cleanup;
    }
    opened = 1U;
    close_stage = "close-read";
    if(f_size(&test_file) != 4096U) {
        sd_test_detail(detail, capacity, HW_FAIL, "SD size bytes=%lu/4096", (unsigned long)f_size(&test_file));
        goto cleanup;
    }
    for(block = 0; block < 16U; block++) {
        transferred = 0U;
        code = f_read(&test_file, test_buffer.io.verify, sizeof(test_buffer.io.verify), &transferred);
        if(code != FR_OK || transferred != sizeof(test_buffer.io.verify)) {
            sd_test_detail(detail, capacity, HW_FAIL, "SD read FR=%u offset=%lu bytes=%u/%u",
                           (unsigned)code, (unsigned long)(block * sizeof(test_buffer.io.verify)),
                           (unsigned)transferred, (unsigned)sizeof(test_buffer.io.verify));
            goto cleanup;
        }
        for(i = 0; i < sizeof(test_buffer.io.verify); i++) {
            uint8_t expected = (uint8_t)(i ^ block ^ 0x5aU);
            if(test_buffer.io.verify[i] != expected) {
                sd_test_detail(detail, capacity, HW_FAIL, "SD verify offset=%lu byte=%02X/%02X",
                               (unsigned long)(block * sizeof(test_buffer.io.verify) + i),
                               (unsigned)test_buffer.io.verify[i], (unsigned)expected);
                goto cleanup;
            }
        }
    }
    result = HW_PASS;
cleanup:
    /* 清理失败必须记录；已经发生的主错误优先保留在屏幕详情中。 */
    if(opened) {
        code = f_close(&test_file);
        if(code != FR_OK) {
            sd_test_detail(result == HW_PASS ? detail : NULL, capacity, HW_FAIL,
                           "SD %s FR=%u: %s", close_stage, (unsigned)code, path);
            result = HW_FAIL;
        }
    }
    /* 只有 CREATE_NEW 成功后，path 才属于本次测试创建的文件。 */
    code = f_unlink(path);
    if(code != FR_OK) {
        sd_test_detail(result == HW_PASS ? detail : NULL, capacity, HW_FAIL,
                       "SD delete FR=%u: %s", (unsigned)code, path);
        result = HW_FAIL;
    }
    if(result == HW_PASS)
        sd_test_detail(detail, capacity, HW_PASS, "SD 4KiB verified and deleted");
    return result;
}

HwResult hardware_sd_write_test(void)
{
    return hardware_sd_write_test_detail(NULL, 0U);
}

/* 轮询指定 UART4 标志，每次等待最多约 100 ms；断线或硬件异常不会永久阻塞菜单。 */
static uint8_t uart_wait(uint16_t flag)
{
    uint16_t ms;
    for(ms = 0; ms < 100U; ms++) {
        if(USART_GetFlagStatus(UART4, flag) == SET) return 1U;
        Delay_ms(1U);
    }
    return 0U;
}

/* 发送 32 个变化的字节并逐个核对回环结果，同时将奇偶/帧/噪声/溢出错误判为失败。 */
HwResult hardware_uart_loopback_test(void)
{
    uint32_t saved = UART4->CR1 & USART_CR1_RXNEIE;
    uint16_t i;
    volatile uint32_t discard;
    HwResult result = HW_FAIL;
    UART4->CR1 &= ~USART_CR1_RXNEIE; /* 暂停正常的中断回显。 */
    discard = UART4->SR; discard = UART4->DR;
    for(i = 0; i < 32U; i++) {
        uint8_t expected = (uint8_t)(0x5aU ^ (i * 13U));
        if(!uart_wait(USART_FLAG_TXE)) goto cleanup;
        USART_SendData(UART4, expected);
        if(!uart_wait(USART_FLAG_RXNE)) goto cleanup;
        if(UART4->SR & (USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE)) goto cleanup;
        if((uint8_t)USART_ReceiveData(UART4) != expected) goto cleanup;
    }
    result = HW_PASS;
cleanup:
    /* 所有退出路径都等发送结束、清除残留接收状态，再恢复原来的接收中断设置。 */
    if(!uart_wait(USART_FLAG_TC)) result = HW_FAIL;
    Delay_ms(2U);
    discard = UART4->SR; discard = UART4->DR; (void)discard;
    UART4->CR1 = (UART4->CR1 & ~USART_CR1_RXNEIE) | saved;
    return result;
}
