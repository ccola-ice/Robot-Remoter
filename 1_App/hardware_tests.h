#ifndef HARDWARE_TESTS_H
#define HARDWARE_TESTS_H
#include <stdint.h>
typedef enum { HW_PASS, HW_FAIL, HW_BLOCKED, HW_CANCELLED } HwResult;
HwResult hardware_memory_test(void);
HwResult hardware_flash_write_test(void);
HwResult hardware_eeprom_write_test(void);
HwResult hardware_sd_write_test(void);
HwResult hardware_uart_loopback_test(void);
HwResult hardware_radio_test(uint8_t receive);
/* Both AT24C08 and AT24C256 are fitted: only address 0x50 is accessed. */
#define HW_EEPROM_TEST_BYTE 0xffU
const char *hardware_result_name(HwResult result);
#endif
