#ifndef BOOT_SRAM_H
#define BOOT_SRAM_H

#include <stdint.h>

/* 上电时检查外部 SRAM 的字节和半字读写，并恢复检查前的数据。
 * 必须在文件系统及其他 SRAM 使用者启动前调用；1 表示全部通过，0 表示检测或恢复失败。 */
uint8_t boot_sram_check(void);

#endif
