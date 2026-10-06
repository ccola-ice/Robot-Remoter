#ifndef DIAG_HARDWARE_H
#define DIAG_HARDWARE_H
#include <stdint.h>
/* 检测入口在前台串行调用，内部复用静态缓冲区，不可从中断或并发任务重入。
 * BLOCKED 表示前提不满足、未完成检测，CANCELLED 表示用户中止，均不能计为通过。 */
typedef enum { HW_PASS, HW_FAIL, HW_BLOCKED, HW_CANCELLED } HwResult;
/* 抽查 4 个 ROM 常量及模块专用的 1 KiB RAM，不遍历其他模块使用的内存。 */
HwResult hardware_memory_test(void);
/* 专用扇区须全为 0xFF，否则返回 BLOCKED；写读比对后擦除并验证恢复为空白。 */
HwResult hardware_flash_write_test(void);
/* 只测 HW_EEPROM_TEST_BYTE，非空白时返回 BLOCKED；写读后恢复为 0xFF。 */
HwResult hardware_eeprom_write_test(void);
/* 创建独占临时文件，写入/回读核对 4 KiB 后删除；已有同名文件不会被覆盖。 */
HwResult hardware_sd_write_test(void);
/* detail 可为 NULL；capacity 包含字符串结尾，失败时保留最先发生的错误。 */
HwResult hardware_sd_write_test_detail(char *detail, uint16_t capacity);
/* 调用前须将 UART4 的 TX/RX 外接短路并停用外部设备；检测会暂时屏蔽接收中断。 */
HwResult hardware_uart_loopback_test(void);
/* receive 非零为接收端，否则为发送并等 ACK；对端需运行互补角色，双方固定频道 40。
 * 检查 8 个 32 字节报文，BACK 可取消；退出恢复寄存器，恢复失败时保持 CE 为低。 */
HwResult hardware_radio_test(uint8_t receive);
/* 板上同时装有 AT24C08 和 AT24C256；这里只访问地址 0x50。 */
#define HW_EEPROM_TEST_BYTE 0xffU
const char *hardware_result_name(HwResult result);
#endif
