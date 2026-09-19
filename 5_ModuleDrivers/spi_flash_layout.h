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

/* Two independent erase sectors for atomic parameter updates. Keep the old
 * raw parameter address as slot A, so migration can first commit to slot B. */
#define SPI_FLASH_PARAM_SECTOR             \
    (SPI_FLASH_FATFS_FIRST_SECTOR - 1UL)
#define SPI_FLASH_PARAM_ADDR               \
    (SPI_FLASH_PARAM_SECTOR * SPI_FLASH_LAYOUT_SECTOR_SIZE)
#define SPI_FLASH_PARAM_SLOT_A_ADDR        SPI_FLASH_PARAM_ADDR
#define SPI_FLASH_PARAM_SLOT_B_ADDR        0x005fd000UL

/* Diagnostic erase sector: between parameter slots B and A.
 * An occupied sector is BLOCKED, never erased by the diagnostic. */
#define SPI_FLASH_DIAG_ADDR                0x005fe000UL
#define SPI_FLASH_FONT_END                 0x005e5900UL
#if SPI_FLASH_DIAG_ADDR < SPI_FLASH_FONT_END || \
    SPI_FLASH_DIAG_ADDR + SPI_FLASH_LAYOUT_SECTOR_SIZE > SPI_FLASH_PARAM_ADDR
#error "Diagnostic sector overlaps stored resources or parameters"
#endif

#if SPI_FLASH_PARAM_SLOT_B_ADDR < SPI_FLASH_FONT_END || \
    SPI_FLASH_PARAM_SLOT_B_ADDR + SPI_FLASH_LAYOUT_SECTOR_SIZE > SPI_FLASH_DIAG_ADDR || \
    SPI_FLASH_PARAM_SLOT_B_ADDR % SPI_FLASH_LAYOUT_SECTOR_SIZE != 0
#error "Parameter slot B overlaps resources/diagnostics or is not sector aligned"
#endif

#if SPI_FLASH_PARAM_SECTOR >= SPI_FLASH_FATFS_FIRST_SECTOR
#error "Parameter storage overlaps the SPI Flash FatFs partition"
#endif

#endif
