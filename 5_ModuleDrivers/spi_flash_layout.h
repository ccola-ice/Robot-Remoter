#ifndef __SPI_FLASH_LAYOUT_H
#define __SPI_FLASH_LAYOUT_H

/* W25Q128 physical layout, in 4 KiB erase sectors. */
#define SPI_FLASH_LAYOUT_SECTOR_SIZE       4096UL
#define SPI_FLASH_LAYOUT_TOTAL_SIZE        (16UL * 1024UL * 1024UL)
#define SPI_FLASH_LAYOUT_TOTAL_SECTORS     \
    (SPI_FLASH_LAYOUT_TOTAL_SIZE / SPI_FLASH_LAYOUT_SECTOR_SIZE)

/* Resources/configuration occupy the first 6 MiB. */
#define SPI_FLASH_FATFS_FIRST_SECTOR       1536UL
#define SPI_FLASH_FATFS_SECTOR_COUNT       \
    (SPI_FLASH_LAYOUT_TOTAL_SECTORS - SPI_FLASH_FATFS_FIRST_SECTOR)

/* The final reserved sector is dedicated to the parameter image. */
#define SPI_FLASH_PARAM_SECTOR             \
    (SPI_FLASH_FATFS_FIRST_SECTOR - 1UL)
#define SPI_FLASH_PARAM_ADDR               \
    (SPI_FLASH_PARAM_SECTOR * SPI_FLASH_LAYOUT_SECTOR_SIZE)

#if SPI_FLASH_PARAM_SECTOR >= SPI_FLASH_FATFS_FIRST_SECTOR
#error "Parameter storage overlaps the SPI Flash FatFs partition"
#endif

#endif
