#ifndef DIAG_HARDWARE_H
#define DIAG_HARDWARE_H
#include <stdint.h>
typedef enum { HW_PASS, HW_FAIL, HW_BLOCKED, HW_CANCELLED } HwResult;
HwResult hardware_memory_test(void);
HwResult hardware_flash_write_test(void);
HwResult hardware_eeprom_write_test(void);
HwResult hardware_sd_write_test(void);
/* detail 可为 NULL；capacity 包含字符串结尾，失败时保留最先发生的错误。 */
HwResult hardware_sd_write_test_detail(char *detail, uint16_t capacity);
HwResult hardware_uart_loopback_test(void);
HwResult hardware_radio_test(uint8_t receive);
/* 板上同时装有 AT24C08 和 AT24C256；这里只访问地址 0x50。 */
#define HW_EEPROM_TEST_BYTE 0xffU
const char *hardware_result_name(HwResult result);
#endif
