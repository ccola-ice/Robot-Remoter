/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2014        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */
#include "bsp_spi_flash.h"
#include "spi_flash_layout.h"
#include "bsp_sdio_sd.h"
#include "string.h"
#include <stdint.h>

//定义逻辑设备号
#define SD_CARD     0
#define SPI_FLASH   1

#define SD_BLOCKSIZE     512
extern  SD_CardInfo SDCardInfo;
static DSTATUS sd_disk_status = STA_NOINIT;

/* Validate before address arithmetic; FatFs must not reach reserved flash. */
static DRESULT disk_check_request(BYTE pdrv, const void *buff, DWORD sector, UINT count)
{
    uint64_t sectors;
    if (buff == 0 || count == 0U) return RES_PARERR;
    if (pdrv == SD_CARD) {
        if (sd_disk_status & STA_NOINIT) return RES_NOTRDY;
        if (count > 0x01ffffffUL / SD_BLOCKSIZE) return RES_PARERR;
        sectors = SDCardInfo.CardCapacity / SD_BLOCKSIZE;
    } else if (pdrv == SPI_FLASH) {
        if (FLASH_GetIoError() != 0U) return RES_NOTRDY;
        sectors = SPI_FLASH_FATFS_SECTOR_COUNT;
    } else return RES_PARERR;
    if (sector >= sectors || count > sectors - sector) return RES_PARERR;
    return RES_OK;
}

static DRESULT disk_sd_result(SD_Error error)
{
    if (error == SD_OK) return RES_OK;
    SD_AbortTransfer();
    sd_disk_status = STA_NOINIT;
    return RES_ERROR;
}
/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat = STA_NOINIT;
	uint32_t spi_result;

	switch (pdrv) 
	{
		case SD_CARD :
			//SD 卡状态返回分支
			// translate the reslut code here
            if (sd_disk_status == 0U && SD_GetStatus() == SD_TRANSFER_ERROR)
                sd_disk_status = STA_NOINIT;
            stat = sd_disk_status;
			break;

		case SPI_FLASH :
			//SPI FLASH 状态返回分支
			spi_result = FLASH_Read_FlashID();
			if(spi_result == FLASH_ID && FLASH_GetIoError() == 0U)
			{
				stat &= ~STA_NOINIT; //stat最低位为0，表示SPI FLASH为正常状态
			}
			else
			{
				stat |= STA_NOINIT;  //stat最低位为1，表示SPI FLASH为不正常状态
			}

			break;
			
		default:
			stat = STA_NOINIT;		
	}
	
	return stat;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat = STA_NOINIT;

	switch (pdrv) 
	{
		case SD_CARD :
		{
			//SD 初始化分支
			if(SD_Init()==SD_OK)
			{
				stat = 0U;
			}
			else 
			{
				stat = STA_NOINIT;
                SD_AbortTransfer();
			}
            sd_disk_status = stat;
			break;
		}
		
		case SPI_FLASH :
			//SPI_FLASH 初始化分支
			FLASH_SPI_Init();
			stat = disk_status(SPI_FLASH);

			break;
		
		default:
			stat = STA_NOINIT;
	}
	
	return stat;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address in LBA */
	UINT count		/* Number of sectors to read */
)
{
	DRESULT stat = RES_PARERR;
	SD_Error SD_state = SD_OK;
    DRESULT request = disk_check_request(pdrv, buff, sector, count);
    if (request != RES_OK) return request;
	
	switch (pdrv) 
	{
		case SD_CARD :
		{
			//SD 读取分支
			if((uintptr_t)buff & 3)
			{
				DRESULT res = RES_OK;
				DWORD scratch[SD_BLOCKSIZE / 4];

				while (count--) 
				{
					res = disk_read(SD_CARD,(void *)scratch, sector++, 1);

					if (res != RES_OK) 
					{
						break;
					}
					memcpy(buff, scratch, SD_BLOCKSIZE);
					buff += SD_BLOCKSIZE;
				}
				return res;
			}
			
			SD_state=SD_ReadMultiBlocks(buff,(uint64_t)sector*SD_BLOCKSIZE,SD_BLOCKSIZE,count);
			
			if(SD_state==SD_OK)
			{
				/* Check if the Transfer is finished */
				SD_state=SD_WaitReadOperation();
				if (SD_state == SD_OK) SD_state = SD_WaitReady();
			}
			
			stat = disk_sd_result(SD_state);

			break;
		}
		
		case SPI_FLASH :
			//SPI_FLASH 读取分支
			//把要读取的扇区号转换成地址
			//扇区偏移6MB，外部Flash文件系统空间放在SPI Flash后面10MB空间
			sector += SPI_FLASH_FATFS_FIRST_SECTOR;
		
            /* FLASH_Read_Data has a 16-bit byte count: read one 4 KiB sector. */
            while (count-- != 0U) {
                FLASH_Read_Data(buff, sector * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
                if (FLASH_GetIoError() != 0U) return RES_ERROR;
                sector++;
                buff += FLASH_SECTOR_SIZE;
            }

			//默认每次都能正常读取
			stat = RES_OK;
			break;
		
		default:
			stat = RES_PARERR;
	}

	return stat;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)  将数据写入指定扇区空间上                                                     */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address in LBA */
	UINT count			/* Number of sectors to write */
)
{
	DRESULT stat = RES_PARERR;
	SD_Error SD_state = SD_OK;
    DRESULT request = disk_check_request(pdrv, buff, sector, count);
    if (request != RES_OK) return request;
	
	if (!count) 
	{
		return RES_PARERR;		/* Check parameter */
	}
	
	switch (pdrv) 
	{
		case SD_CARD :
		{
			//SD 写入分支
			if((uintptr_t)buff&3)
			{
				DRESULT res = RES_OK;
				DWORD scratch[SD_BLOCKSIZE / 4];

				while (count--) 
				{
					memcpy( scratch,buff,SD_BLOCKSIZE);
					res = disk_write(SD_CARD,(void *)scratch, sector++, 1);
					if (res != RES_OK) 
					{
						break;
					}					
					buff += SD_BLOCKSIZE;
				}
				return res;
			}		
		
			SD_state=SD_WriteMultiBlocks((uint8_t *)buff,(uint64_t)sector*SD_BLOCKSIZE,SD_BLOCKSIZE,count);
			
			if(SD_state==SD_OK)
			{
				/* Check if the Transfer is finished */
				SD_state=SD_WaitWriteOperation();

				/* Wait until end of DMA transfer */
				if (SD_state == SD_OK) SD_state = SD_WaitReady();
			}
			
			stat = disk_sd_result(SD_state);
			
			break;
		}
		
		case SPI_FLASH :
			//SPI_FLASH 写入分支
			//扇区偏移6MB，外部Flash文件系统空间放在SPI Flash后面10MB空间
			sector += SPI_FLASH_FATFS_FIRST_SECTOR;
		
			while(count--)
			{
				//写入前先擦除
				FLASH_Erase_Sectors(sector*FLASH_SECTOR_SIZE);
                if (FLASH_GetIoError() != 0U) return RES_ERROR;
			
				//把要写入的扇区号转换成地址
				FLASH_Write_Data((u8*)buff, sector*FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
                if (FLASH_GetIoError() != 0U) return RES_ERROR;
				
				sector++;
				buff += FLASH_SECTOR_SIZE;
			}
			//默认每次都能正常读取
			stat = RES_OK;
			break;
			
		default:
			stat = RES_PARERR;
	}

	return stat;
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
    if (pdrv != SD_CARD && pdrv != SPI_FLASH) return RES_PARERR;
    if (cmd != CTRL_SYNC && buff == 0) return RES_PARERR;
    if (pdrv == SD_CARD) {
        if (sd_disk_status & STA_NOINIT) return RES_NOTRDY;
        switch (cmd) {
        case GET_SECTOR_SIZE: *(WORD *)buff = SD_BLOCKSIZE; return RES_OK;
        case GET_BLOCK_SIZE: *(DWORD *)buff = 1U; return RES_OK;
        case GET_SECTOR_COUNT:
            *(DWORD *)buff = (DWORD)(SDCardInfo.CardCapacity / SD_BLOCKSIZE);
            return RES_OK;
        case CTRL_SYNC: return disk_sd_result(SD_WaitReady());
        default: return RES_PARERR;
        }
    }
    if (FLASH_GetIoError() != 0U) return RES_ERROR;
    switch (cmd) {
    case GET_SECTOR_COUNT: *(DWORD *)buff = SPI_FLASH_FATFS_SECTOR_COUNT; return RES_OK;
    case GET_SECTOR_SIZE: *(WORD *)buff = FLASH_SECTOR_SIZE; return RES_OK;
    case GET_BLOCK_SIZE: *(DWORD *)buff = 1U; return RES_OK;
    case CTRL_SYNC: return RES_OK; /* Writes complete synchronously. */
    default: return RES_PARERR;
    }
}
#endif


//获取时间
__weak DWORD get_fattime(void)
{
    /* 返回当前时间戳 */
    return    ((DWORD)(2024 - 1980) << 25)  /* Year 2015 */
            | ((DWORD)1 << 21)        /* Month 1 */
            | ((DWORD)1 << 16)        /* Mday 1 */
            | ((DWORD)0 << 11)        /* Hour 0 */
            | ((DWORD)0 << 5)         /* Min 0 */
            | ((DWORD)0 >> 1);        /* Sec 0 */
}

