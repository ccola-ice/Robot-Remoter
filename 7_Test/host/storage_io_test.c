#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "spi_flash_layout.h"

#define __IO volatile
#define __weak
typedef uint8_t BYTE, u8;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef unsigned int UINT;
#include "production_storage_types.h"

#define ENABLE 1U
#define DISABLE 0U
#define RESET 0U
#define CoreDebug_DEMCR_TRCENA_Msk 1U
#define DWT_CTRL_CYCCNTENA_Msk 1U
#define SD_WAIT_POLL_LIMIT 16U
#define SDIO_CMD0TIMEOUT 16U
#define SDIO_STATIC_FLAGS 0xfffU
#define SDIO_IT_DCRCFAIL 1U
#define SDIO_IT_DTIMEOUT 2U
#define SDIO_IT_DATAEND 4U
#define SDIO_IT_TXFIFOHE 8U
#define SDIO_IT_RXFIFOHF 16U
#define SDIO_IT_TXUNDERR 32U
#define SDIO_IT_RXOVERR 64U
#define SDIO_IT_STBITERR 128U
#define SDIO_FLAG_TXACT 1U
#define SDIO_FLAG_RXACT 2U
#define SDIO_FLAG_CCRCFAIL 4U
#define SDIO_FLAG_CMDREND 8U
#define SDIO_FLAG_CTIMEOUT 16U
#define SD_SDIO_DMA_FLAG_FEIF 1U
#define SD_SDIO_DMA_FLAG_DMEIF 2U
#define SD_SDIO_DMA_FLAG_TEIF 4U
#define SD_SDIO_DMA_FLAG_HTIF 8U
#define SD_SDIO_DMA_FLAG_TCIF 16U
#define SD_SDIO_DMA_STREAM 0U
typedef struct { uint32_t DEMCR; } Debug;
typedef struct { uint32_t CTRL, CYCCNT; } Cycles;
typedef struct { volatile uint32_t STA; uint32_t DCTRL; } Sdio;
typedef struct { uint32_t LISR; } Dma;
static Debug debug;
static Cycles cycles;
static Sdio sdio;
static Dma dma;
#define CoreDebug (&debug)
#define DWT (&cycles)
#define SDIO (&sdio)
#define DMA2 (&dma)
static uint32_t SystemCoreClock = 168000000U;
static volatile uint32_t StopCondition, TransferEnd, DMAEndOfTransfer;
static volatile SD_Error TransferError;
SD_CardInfo SDCardInfo;
static SD_Error init_error, start_error, transfer_error, stop_error;
static SDTransferState ready_status;
static unsigned status_calls, start_calls, stop_calls, dma_enabled, dma_requests;
static unsigned read_complete, data_complete;
static uint64_t last_address;
static uint32_t irq_pending;

static void SDIO_ITConfig(uint32_t flags, unsigned enable) {(void)flags;(void)enable;}
static void SDIO_DMACmd(unsigned enabled) {dma_requests = enabled;}
static void DMA_Cmd(unsigned stream, unsigned enabled) {(void)stream;dma_enabled = enabled;}
static void DMA_ClearFlag(unsigned stream, uint32_t flags) {(void)stream;(void)flags;}
static void SDIO_ClearFlag(uint32_t flags) {sdio.STA &= ~flags;}
static void SDIO_ClearITPendingBit(uint32_t flags) {irq_pending &= ~flags;}
static unsigned SDIO_GetITStatus(uint32_t flags) {return irq_pending & flags;}
SD_Error SD_StopTransfer(void) {stop_calls++;return stop_error;}
SDTransferState SD_GetStatus(void) {status_calls++;return ready_status;}
SD_Error SD_Init(void) {return init_error;}
SD_Error SD_ReadMultiBlocks(uint8_t *buff, uint64_t address, uint16_t size, uint32_t count)
{
    start_calls++;
    last_address = address;
    assert(size == 512U);
    if(start_error != SD_OK) return start_error;
    memset(buff, 0xa5, size * count);
    DMAEndOfTransfer = read_complete;
    TransferEnd = data_complete;
    TransferError = transfer_error;
    StopCondition = 1U;
    dma_enabled = dma_requests = 1U;
    return SD_OK;
}
SD_Error SD_WriteMultiBlocks(uint8_t *buff, uint64_t address, uint16_t size, uint32_t count)
{
    return SD_ReadMultiBlocks(buff,address,size,count);
}

#define FLASH_ID 0xef4018U
#define FLASH_SECTOR_SIZE 4096U
static uint8_t flash_error;
static unsigned flash_read_calls, flash_erase_calls, flash_write_calls;
static unsigned fail_read, fail_erase, fail_write;
static void FLASH_SPI_Init(void) {}
static uint32_t FLASH_Read_FlashID(void) {return FLASH_ID;}
static uint8_t FLASH_GetIoError(void) {return flash_error;}
static void FLASH_Read_Data(uint8_t *buff,uint32_t address,uint16_t size)
{
    assert(size == 4096U);
    assert(address == (SPI_FLASH_FATFS_FIRST_SECTOR + flash_read_calls) * 4096U);
    flash_read_calls++;
    memset(buff,0x5a,size);
    if(fail_read == flash_read_calls) flash_error = 1U;
}
static void FLASH_Erase_Sectors(uint32_t address)
{
    assert(address >= SPI_FLASH_FATFS_FIRST_SECTOR * 4096U);
    flash_erase_calls++;
    if(fail_erase == flash_erase_calls) flash_error = 2U;
}
static void FLASH_Write_Data(uint8_t *buff,uint32_t address,uint16_t size)
{
    (void)buff;(void)address;assert(size == 4096U);
    flash_write_calls++;
    if(fail_write == flash_write_calls) flash_error = 3U;
}

#include "production_storage.inc"

static void reset(void)
{
    init_error = start_error = transfer_error = stop_error = SD_OK;
    ready_status = SD_TRANSFER_OK;
    status_calls = start_calls = stop_calls = 0U;
    read_complete = data_complete = 1U;
    StopCondition = TransferEnd = DMAEndOfTransfer = 0U;
    TransferError = SD_OK;
    SDCardInfo.CardCapacity = 32ULL * 1024ULL * 1024ULL * 1024ULL;
    SDCardInfo.CardBlockSize = 512U;
    sd_disk_status = STA_NOINIT;
    sdio.STA = sdio.DCTRL = 0U;
    dma_enabled = dma_requests = 0U;
    irq_pending = 0U;
    dma.LISR = 0U;
    flash_error = 0U;
    flash_read_calls = flash_erase_calls = flash_write_calls = 0U;
    fail_read = fail_erase = fail_write = 0U;
    memset(&cycles,0,sizeof(cycles));
}

int main(void)
{
    static uint32_t aligned[20U * 4096U / 4U + 1U];
    BYTE *data = (BYTE *)aligned;
    DWORD sectors;
    SD_WaitDeadline deadline;
    reset();
    assert(disk_status(0) == STA_NOINIT && status_calls == 0U);
    assert(disk_read(0,data,0,1) == RES_NOTRDY);
    init_error = SD_ERROR;
    assert(disk_initialize(0) == STA_NOINIT);
    init_error = SD_OK;
    assert(disk_initialize(0) == 0U && disk_status(0) == 0U);
    assert(disk_read(0,data,0,1) == RES_OK && data[0] == 0xa5U);
    assert(stop_calls == 1U && dma_enabled == 0U && dma_requests == 0U);
    assert(disk_write(0,data,8388608UL,1) == RES_OK);
    assert(last_address == 0x100000000ULL); /* No 32-bit byte-address overflow. */
    assert(disk_ioctl(0,GET_SECTOR_COUNT,&sectors) == RES_OK && sectors == 67108864UL);
    assert(disk_ioctl(0,99,&sectors) == RES_PARERR);
    assert(disk_ioctl(0,GET_SECTOR_SIZE,0) == RES_PARERR);
    assert(disk_read(0,data,0,0) == RES_PARERR);
    assert(disk_write(0,data,67108863UL,2) == RES_PARERR);
    assert(disk_read(0,data + 1,1,2) == RES_OK); /* Aligned bounce path. */

    reset();assert(disk_initialize(0) == 0U);
    start_error = SD_CMD_RSP_TIMEOUT;
    assert(disk_read(0,data,0,1) == RES_ERROR && status_calls == 0U);
    assert(disk_status(0) == STA_NOINIT);
    assert(disk_read(0,data,0,1) == RES_NOTRDY);

    reset();assert(disk_initialize(0) == 0U);
    transfer_error = SD_DATA_CRC_FAIL;
    assert(disk_write(0,data,0,1) == RES_ERROR && status_calls == 0U);
    assert(dma_enabled == 0U && stop_calls == 0U);

    reset();assert(disk_initialize(0) == 0U);
    read_complete = data_complete = 0U;
    assert(disk_read(0,data,0,1) == RES_ERROR && status_calls == 0U);
    assert(disk_status(0) == STA_NOINIT && dma_enabled == 0U);
    reset();assert(disk_initialize(0) == 0U);
    read_complete = 0U; /* SDIO DATAEND alone is not a completed DMA read. */
    assert(disk_read(0,data,0,1) == RES_ERROR);
    reset();assert(disk_initialize(0) == 0U);
    sdio.STA = SDIO_FLAG_RXACT; /* Persistent peripheral activity. */
    assert(disk_read(0,data,0,1) == RES_ERROR && dma_requests == 0U);

    reset();assert(disk_initialize(0) == 0U);
    ready_status = SD_TRANSFER_BUSY;
    assert(disk_write(0,data,0,1) == RES_ERROR);
    assert(status_calls == SD_WAIT_POLL_LIMIT + 1U);
    reset();assert(disk_initialize(0) == 0U);
    ready_status = SD_TRANSFER_ERROR;
    assert(disk_ioctl(0,CTRL_SYNC,0) == RES_ERROR && status_calls == 1U);
    reset();assert(disk_initialize(0) == 0U);
    ready_status = SD_TRANSFER_ERROR;
    assert(disk_status(0) == STA_NOINIT);

    reset();
    cycles.CYCCNT = 0xfffffff0UL;
    SD_BeginWait(&deadline);
    cycles.CYCCNT += (SystemCoreClock / 1000UL) * 2000UL;
    assert(SD_WaitExpired(&deadline)); /* Unsigned wraparound. */
    assert(CmdResp3Error() == SD_CMD_RSP_TIMEOUT); /* No hardware flags. */
    sdio.STA = SDIO_FLAG_CMDREND;
    assert(CmdResp3Error() == SD_OK);
    irq_pending = SDIO_IT_DATAEND | SDIO_IT_DCRCFAIL;
    assert(SD_ProcessIRQSrc() == SD_DATA_CRC_FAIL && TransferEnd == 0U);
    dma.LISR = SD_SDIO_DMA_FLAG_TCIF | SD_SDIO_DMA_FLAG_TEIF;
    SD_ProcessDMAIRQ();
    assert(TransferError == SD_ERROR && DMAEndOfTransfer == 0U);
    reset();dma.LISR = SD_SDIO_DMA_FLAG_TCIF;
    SD_ProcessDMAIRQ();
    assert(DMAEndOfTransfer == 1U && TransferError == SD_OK);

    reset();
    assert(disk_initialize(1) == 0U);
    assert(disk_read(1,data,0,20) == RES_OK && flash_read_calls == 20U);
    assert(data[20U * 4096U - 1U] == 0x5aU);
    assert(disk_read(1,data,SPI_FLASH_FATFS_SECTOR_COUNT-1U,2) == RES_PARERR);
    assert(disk_write(1,data,0xffffffffUL,2) == RES_PARERR);
    reset();fail_read = 2U;
    assert(disk_read(1,data,0,3) == RES_ERROR && flash_read_calls == 2U);
    assert(disk_ioctl(1,CTRL_SYNC,0) == RES_ERROR);
    assert(disk_status(1) == STA_NOINIT);
    reset();fail_erase = 1U;
    assert(disk_write(1,data,0,2) == RES_ERROR);
    assert(flash_erase_calls == 1U && flash_write_calls == 0U);
    reset();fail_write = 1U;
    assert(disk_write(1,data,0,2) == RES_ERROR);
    assert(flash_erase_calls == 1U && flash_write_calls == 1U);
    reset();assert(disk_write(1,data,0,2) == RES_OK);
    assert(flash_erase_calls == 2U && flash_write_calls == 2U);
    puts("storage I/O fault regression: PASS");
    return 0;
}
