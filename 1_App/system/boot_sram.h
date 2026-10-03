#ifndef BOOT_SRAM_H
#define BOOT_SRAM_H

#include <stdint.h>

/* 上电时检查外部 SRAM 的字节和半字读写，并恢复检查前的数据。 */
uint8_t boot_sram_check(void);

#endif
