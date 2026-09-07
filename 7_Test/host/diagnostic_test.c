#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "spi_flash_layout.h"

static uint8_t flash[4096], eeprom[256], file_data[4096];
static unsigned writes, erases, ee_writes, opens, closes, unlinks, existing;
static unsigned flash_fault, occupied_read_fault, program_fault, erase_fault;
static unsigned ee_fault, ee_mismatch, sd_fault, short_io, total_ms;
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
typedef unsigned FRESULT;
typedef struct {unsigned pos, size;} FIL;
#define FR_OK 0U
#define FR_EXIST 1U
#define FR_DISK_ERR 2U
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
    }
    f->pos=0U;
    return FR_OK;
}
static FRESULT f_write(FIL *f, const void *data, UINT size, UINT *done)
{
    assert(f->pos + size <= sizeof(file_data));
    *done=short_io == 1U ? size-1U : size;
    memcpy(file_data + f->pos,data,*done); f->pos+=*done; f->size=f->pos;
    return sd_fault == 2U ? FR_DISK_ERR : FR_OK;
}
static FRESULT f_read(FIL *f, void *data, UINT size, UINT *done)
{
    assert(f->pos + size <= sizeof(file_data));
    *done=short_io == 2U ? size-1U : size;
    memcpy(data,file_data + f->pos,*done); f->pos+=*done;
    if(sd_fault == 6U) ((uint8_t*)data)[0]^=1U;
    return sd_fault == 7U ? FR_DISK_ERR : FR_OK;
}
static FRESULT f_sync(FIL *f) {(void)f; return sd_fault == 3U ? FR_DISK_ERR : FR_OK;}
static FRESULT f_close(FIL *f) {(void)f; closes++; return sd_fault == 4U ? FR_DISK_ERR : FR_OK;}
static FRESULT f_unlink(const char *path)
{
    unsigned name;
    assert(sscanf(path,"0:/D%3u.TMP", &name) == 1 && name >= existing);
    unlinks++; return sd_fault == 8U ? FR_DISK_ERR : FR_OK;
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

#include "hardware_tests.c"

static void reset(void)
{
    memset(flash,255,sizeof(flash)); memset(eeprom,255,sizeof(eeprom));
    writes=erases=ee_writes=opens=closes=unlinks=existing=0U;
    flash_fault=occupied_read_fault=program_fault=erase_fault=0U;
    ee_fault=ee_mismatch=sd_fault=short_io=total_ms=0U;
    uart_no_loopback=uart_mismatch=uart_sent=0U;
    memset(&uart,0,sizeof(uart)); uart.CR1=USART_CR1_RXNEIE | 0x8000U;
    flash_id=0xef4018UL;
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
    }
    reset(); short_io=1U; assert(hardware_sd_write_test() == HW_FAIL && unlinks == 1U);
    reset(); short_io=2U; assert(hardware_sd_write_test() == HW_FAIL && unlinks == 1U);
    reset(); assert(hardware_uart_loopback_test() == HW_PASS && uart_sent == 32U && uart.CR1 == 0x8001U);
    reset(); uart_no_loopback=1U;
    assert(hardware_uart_loopback_test() == HW_FAIL && total_ms <= 102U && uart.CR1 == 0x8001U);
    reset(); uart_mismatch=1U; assert(hardware_uart_loopback_test() == HW_FAIL && uart.CR1 == 0x8001U);
    reset(); uart.SR=USART_SR_ORE; assert(hardware_uart_loopback_test() == HW_FAIL && uart.CR1 == 0x8001U);
    puts("PASS: diagnostic storage guards, readback failures, cleanup, UART timeout/restore, owned RAM");
    return 0;
}
