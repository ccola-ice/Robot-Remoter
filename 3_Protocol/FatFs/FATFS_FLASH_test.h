#ifndef __FATFS_FLASH_TEST_H
#define __FATFS_FLASH_TEST_H
#include "stm32f4xx.h"
#include "ff.h"
#include "bsp_usart_debug.h"
#include "bsp_fsmc_sram.h"
#include "string.h"

void fatfs_flash_test(void);

FRESULT miscellaneous(void);
FRESULT file_check(void);
FRESULT scan_files (char* path);
void fatfs_flash_test2(void);




#endif
