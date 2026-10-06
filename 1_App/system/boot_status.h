#ifndef BOOT_STATUS_H
#define BOOT_STATUS_H

#include <stdint.h>

/* 每个项目只记录一次最终结果。NOT_TESTED 表示流程结束，不代表测试通过。 */
typedef enum {
    BOOT_CLOCK, BOOT_LCD, BOOT_SRAM, BOOT_FLASH, BOOT_EEPROM,
    BOOT_RTC, BOOT_ADC1, BOOT_ADC3, BOOT_MPU, BOOT_MPU_SAMPLE,
    BOOT_TOUCH, BOOT_SD, BOOT_SD_FS, BOOT_NRF, BOOT_PARAMS,
    BOOT_NRF_CONFIG, BOOT_GPS, BOOT_TIMERS,
    BOOT_KEYS, BOOT_ANALOG, BOOT_OUTPUTS, BOOT_UART,
    BOOT_RADIO, BOOT_FLASH_WRITE, BOOT_EEPROM_WRITE, BOOT_SD_WRITE,
    BOOT_INTERNAL_MEMORY, BOOT_ITEM_COUNT
} BootItem;

/* PENDING 可直接记录终态，也可先进入 RUNNING；终态一旦记录便不再回退。 */
typedef enum {
    BOOT_PENDING, BOOT_RUNNING, BOOT_PASS, BOOT_FAIL, BOOT_NOT_TESTED
} BootState;

/* 汇总区分尚未结束、存在失败、有项目未测试和全部通过，避免把完成率当作成功率。 */
typedef enum {
    BOOT_INCOMPLETE, BOOT_FAILED, BOOT_PARTIAL, BOOT_PASSED
} BootOutcome;

/* 单项结果：elapsed_ms 为该项耗时，detail 为供界面/串口展示的简短说明。 */
typedef struct {
    BootState state;
    uint32_t elapsed_ms;
    char detail[72];
} BootResult;

/* 一轮启动的汇总；completed 包含所有终态，passed/failed/not_tested 为各自数量。
 * elapsed_ms 由启动流程填写总耗时，独立于各检测项的 elapsed_ms。 */
typedef struct {
    BootResult items[BOOT_ITEM_COUNT];
    uint8_t completed;
    uint8_t passed;
    uint8_t failed;
    uint8_t not_tested;
    uint32_t elapsed_ms;
} BootReport;

/* 名称表与 BootItem 枚举顺序一一对应，扩展检测项时须同步维护。 */
extern const char * const boot_item_names[BOOT_ITEM_COUNT];
const char *boot_state_name(BootState state);
/* report 必须有效；清空计数、耗时与详情，使所有项目重新处于 PENDING。 */
void boot_report_reset(BootReport *report);
/* 仅接受 PENDING 项，成功进入 RUNNING 返回 1；非法编号或重复启动返回 0。 */
uint8_t boot_report_start(BootReport *report, BootItem item);
/* 记录 PASS/FAIL/NOT_TESTED 终态及耗时 ms，成功返回 1；非法状态或重复记录返回 0。
 * detail 可为空指针，非空时复制并截断至结果缓冲区容量，不保存调用方指针。 */
uint8_t boot_report_record(BootReport *report, BootItem item,
                           BootState state, const char *detail, uint32_t ms);
/* 按已结束项目数计算 0..100 的整数完成率，NOT_TESTED 也计入进度。 */
uint8_t boot_report_percent(const BootReport *report);
BootOutcome boot_report_outcome(const BootReport *report);
/* 扫描 data 的 size 字节，找到至少一条完整且校验正确的 NMEA 语句返回 1，否则返回 0。
 * data 必须指向有效缓冲区；这里只证明接收正常，不能证明 GPS 已定位。 */
uint8_t boot_nmea_valid(const uint8_t *data, uint16_t size);

#endif
