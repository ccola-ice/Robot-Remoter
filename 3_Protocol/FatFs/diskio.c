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
#include "bsp_sdio_sd.h"
#include "string.h"

//定义逻辑设备号
#define SD_CARD     0
#define SPI_FLASH   1

#define SD_BLOCKSIZE     512
extern  SD_CardInfo SDCardInfo;
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
			stat &= ~STA_NOINIT;
			break;

		case SPI_FLASH :
			//SPI FLASH 状态返回分支
			spi_result = FLASH_Read_FlashID();
			if(spi_result == FLASH_ID)
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
	DSTATUS stat;

	switch (pdrv) 
	{
		case SD_CARD :
		{
			//SD 初始化分支
			if(SD_Init()==SD_OK)
			{
				stat &= ~STA_NOINIT;
			}
			else 
			{
				stat = STA_NOINIT;
			}
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
	
	switch (pdrv) 
	{
		case SD_CARD :
		{
			//SD 读取分支
			if((DWORD)buff & 3)
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
			
			SD_state=SD_ReadMultiBlocks(buff,sector*SD_BLOCKSIZE,SD_BLOCKSIZE,count);
			
			if(SD_state==SD_OK)
			{
				/* Check if the Transfer is finished */
				SD_state=SD_WaitReadOperation();
				while(SD_GetStatus() != SD_TRANSFER_OK);
			}
			
			if(SD_state!=SD_OK)
				stat = RES_PARERR;
			else
				stat = RES_OK;	

			break;
		}
		
		case SPI_FLASH :
			//SPI_FLASH 读取分支
			//把要读取的扇区号转换成地址
			//扇区偏移6MB，外部Flash文件系统空间放在SPI Flash后面10MB空间
			sector+=1536;
		
			FLASH_Read_Data(buff,sector*FLASH_SECTOR_SIZE, count*FLASH_SECTOR_SIZE);

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
	
	if (!count) 
	{
		return RES_PARERR;		/* Check parameter */
	}
	
	switch (pdrv) 
	{
		case SD_CARD :
		{
			//SD 写入分支
			if((DWORD)buff&3)
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
		
			SD_state=SD_WriteMultiBlocks((uint8_t *)buff,sector*SD_BLOCKSIZE,SD_BLOCKSIZE,count);
			
			if(SD_state==SD_OK)
			{
				/* Check if the Transfer is finished */
				SD_state=SD_WaitWriteOperation();

				/* Wait until end of DMA transfer */
				while(SD_GetStatus() != SD_TRANSFER_OK);			
			}
			
			if(SD_state!=SD_OK)
				stat = RES_PARERR;
			else
				stat = RES_OK;
			
			break;
		}
		
		case SPI_FLASH :
			//SPI_FLASH 写入分支
			//扇区偏移6MB，外部Flash文件系统空间放在SPI Flash后面10MB空间
			sector+=1536;
		
			while(count--)
			{
				//写入前先擦除
				FLASH_Erase_Sectors(sector*FLASH_SECTOR_SIZE);
			
				//把要写入的扇区号转换成地址
				FLASH_Write_Data((u8*)buff, sector*FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
				
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
	DRESULT stat = RES_PARERR;

	switch(pdrv)
	{
		case SD_CARD :
		//SD 分支
		{
			switch (cmd) 
			{
				// Get R/W sector size (WORD) 
				case GET_SECTOR_SIZE :    
					*(WORD * )buff = SD_BLOCKSIZE;
					break;
				
				// Get erase block size in unit of sector (DWORD)
				case GET_BLOCK_SIZE :      
					*(DWORD * )buff = 1;//SDCardInfo.CardBlockSize;
					break;

				case GET_SECTOR_COUNT:
					*(DWORD * )buff = SDCardInfo.CardCapacity/SDCardInfo.CardBlockSize;
					break;
				
				case CTRL_SYNC :
					break;
			}
			stat = RES_OK;
			
			break;
		}
		
		case SPI_FLASH :
			//SPI_FLASH 分支
			switch(cmd)
			{
				//存储介质有多少个sector，文件系统通过该值获得存储介质的容量
				case GET_SECTOR_COUNT:
					*(DWORD *)buff = (16*1024*1024/FLASH_SECTOR_SIZE)-1536;//4096-1536
					stat = RES_OK;
					break;
				
				//每个扇区的个数
				case GET_SECTOR_SIZE:
					*(WORD  *)buff = FLASH_SECTOR_SIZE;
					stat = RES_OK;
					break;
				
				//获取擦除的最小个数， 以sector为单位
				case GET_BLOCK_SIZE:
					*(DWORD  *)buff = 1;
					stat = RES_OK;
					break;
				
				//写入同步在disk_write函数已经完成，这里默认返回ok
				case CTRL_SYNC:
					stat = RES_OK;
					break;
				
				default:
					stat = RES_PARERR;	
					break;
			}
			break;
		
			default:
					stat = RES_PARERR;
	}

	return stat;
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

