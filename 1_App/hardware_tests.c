#include "hardware_tests.h"
#include "bsp_spi_flash.h"
#include "spi_flash_layout.h"
#include "bsp_i2c_eeprom.h"
#include "bsp_Systick.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

static uint8_t sample[256], verify[256];
static FIL test_file;

const char *hardware_result_name(HwResult result)
{
    static const char * const names[] = {"PASS", "FAIL", "NOT TESTED / BLOCKED", "CANCELLED"};
    return result <= HW_CANCELLED ? names[result] : "FAIL";
}

HwResult hardware_memory_test(void)
{
    static volatile uint32_t ram[256]; /* 仅测试本模块分配的内存，不使用固定地址。 */
    static const uint32_t rom[] = {0x01234567UL, 0x89abcdefUL,
                                   0x55aa55aaUL, 0xaa55aa55UL};
    const volatile uint32_t *probe = rom; /* 将对象保留在 ROM 中，并强制通过总线实际读取。 */
    uint32_t i, bit, pattern;
    if(probe[0] != 0x01234567UL || probe[1] != 0x89abcdefUL ||
       probe[2] != 0x55aa55aaUL || probe[3] != 0xaa55aa55UL) return HW_FAIL;
    for(bit = 0; bit < 32U; bit++) {
        pattern = 1UL << bit;
        for(i = 0; i < 256U; i++) ram[i] = pattern ^ i;
        for(i = 0; i < 256U; i++) if(ram[i] != (pattern ^ i)) return HW_FAIL;
        for(i = 0; i < 256U; i++) ram[i] = ~(pattern ^ i);
        for(i = 0; i < 256U; i++) if(ram[i] != ~(pattern ^ i)) return HW_FAIL;
    }
    return HW_PASS;
}

HwResult hardware_flash_write_test(void)
{
    uint32_t id, offset;
    uint16_t i;
    HwResult result = HW_PASS;
    if(FLASH_BootProbe(&id)) return HW_FAIL;
    /* 不得擦除已经存有资源、参数或文件系统数据的扇区。 */
    for(offset = 0; offset < SPI_FLASH_LAYOUT_SECTOR_SIZE; offset += sizeof(sample)) {
        FLASH_Read_Data(sample, SPI_FLASH_DIAG_ADDR + offset, sizeof(sample));
        if(FLASH_GetIoError()) return HW_FAIL;
        for(i = 0; i < sizeof(sample); i++) if(sample[i] != 0xffU) return HW_BLOCKED;
    }
    for(offset = 0; offset < SPI_FLASH_LAYOUT_SECTOR_SIZE; offset += sizeof(sample)) {
        for(i = 0; i < sizeof(sample); i++) sample[i] = (uint8_t)(i ^ (offset >> 8) ^ 0xa5U);
        /* W25Q128 物理页大小为 256 字节，不得使用旧版的 4 KiB 页写入方式。 */
        FLASH_Write_Page_v3(sample, SPI_FLASH_DIAG_ADDR + offset, sizeof(sample));
        FLASH_Read_Data(verify, SPI_FLASH_DIAG_ADDR + offset, sizeof(verify));
        if(FLASH_GetIoError() || memcmp(sample, verify, sizeof(sample))) {
            result = HW_FAIL;
            break;
        }
    }
    /* 只允许擦除本次测试使用、且测试前已确认为空白的专用扇区。 */
    FLASH_Erase_Sectors(SPI_FLASH_DIAG_ADDR);
    for(offset = 0; offset < SPI_FLASH_LAYOUT_SECTOR_SIZE; offset += sizeof(sample)) {
        FLASH_Read_Data(verify, SPI_FLASH_DIAG_ADDR + offset, sizeof(verify));
        if(FLASH_GetIoError()) result = HW_FAIL;
        for(i = 0; i < sizeof(verify); i++) if(verify[i] != 0xffU) result = HW_FAIL;
    }
    return result;
}

HwResult hardware_eeprom_write_test(void)
{
    uint8_t value, i, patterns[] = {0x55U, 0xaaU};
    HwResult result = HW_PASS;
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

HwResult hardware_sd_write_test(void)
{
    char path[24];
    FRESULT code;
    UINT transferred;
    uint16_t name, block, i;
    uint8_t opened = 0U;
    HwResult result = HW_FAIL;
    /* 必须使用 CREATE_NEW，绝不能截断已有文件。 */
    for(name = 0; name < 1000U; name++) {
        sprintf(path, "0:/D%03u.TMP", name);
        code = f_open(&test_file, path, FA_WRITE | FA_CREATE_NEW);
        if(code == FR_OK) break;
        if(code != FR_EXIST) return HW_FAIL;
    }
    if(name == 1000U) return HW_BLOCKED;
    opened = 1U;
    for(block = 0; block < 16U; block++) {
        for(i = 0; i < sizeof(sample); i++) sample[i] = (uint8_t)(i ^ block ^ 0x5aU);
        if(f_write(&test_file, sample, sizeof(sample), &transferred) != FR_OK ||
           transferred != sizeof(sample)) goto cleanup;
    }
    if(f_sync(&test_file) != FR_OK) goto cleanup;
    code = f_close(&test_file);
    opened = 0U;
    if(code != FR_OK) goto cleanup;
    if(f_open(&test_file, path, FA_READ) != FR_OK) goto cleanup;
    opened = 1U;
    if(f_size(&test_file) != 4096U) goto cleanup;
    for(block = 0; block < 16U; block++) {
        if(f_read(&test_file, verify, sizeof(verify), &transferred) != FR_OK ||
           transferred != sizeof(verify)) goto cleanup;
        for(i = 0; i < sizeof(verify); i++)
            if(verify[i] != (uint8_t)(i ^ block ^ 0x5aU)) goto cleanup;
    }
    result = HW_PASS;
cleanup:
    if(opened && f_close(&test_file) != FR_OK) result = HW_FAIL;
    /* 只有 CREATE_NEW 成功后，path 才属于本次测试创建的文件。 */
    if(f_unlink(path) != FR_OK) {
        printf("[DIAG] SD cleanup failed, temporary file: %s\r\n", path);
        result = HW_FAIL;
    }
    return result;
}

static uint8_t uart_wait(uint16_t flag)
{
    uint16_t ms;
    for(ms = 0; ms < 100U; ms++) {
        if(USART_GetFlagStatus(UART4, flag) == SET) return 1U;
        Delay_ms(1U);
    }
    return 0U;
}

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
    if(!uart_wait(USART_FLAG_TC)) result = HW_FAIL;
    Delay_ms(2U);
    discard = UART4->SR; discard = UART4->DR; (void)discard;
    UART4->CR1 = (UART4->CR1 & ~USART_CR1_RXNEIE) | saved;
    return result;
}
