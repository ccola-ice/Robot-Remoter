#ifndef BOOT_STATUS_H
#define BOOT_STATUS_H

#include <stdint.h>

/* Every entry is resolved once. NOT_TESTED is completion, never a pass. */
typedef enum {
    BOOT_CLOCK, BOOT_LCD, BOOT_SRAM, BOOT_FLASH, BOOT_EEPROM,
    BOOT_RTC, BOOT_ADC1, BOOT_ADC3, BOOT_MPU, BOOT_MPU_SAMPLE,
    BOOT_TOUCH, BOOT_SD, BOOT_SD_FS, BOOT_NRF, BOOT_PARAMS,
    BOOT_NRF_CONFIG, BOOT_GPS, BOOT_TIMERS,
    BOOT_KEYS, BOOT_ANALOG, BOOT_OUTPUTS, BOOT_UART,
    BOOT_RADIO, BOOT_FLASH_WRITE, BOOT_EEPROM_WRITE, BOOT_SD_WRITE,
    BOOT_TOUCH_QUALITY, BOOT_INTERNAL_MEMORY, BOOT_ITEM_COUNT
} BootItem;

typedef enum {
    BOOT_PENDING, BOOT_RUNNING, BOOT_PASS, BOOT_FAIL, BOOT_NOT_TESTED
} BootState;

typedef enum {
    BOOT_INCOMPLETE, BOOT_FAILED, BOOT_PARTIAL, BOOT_PASSED
} BootOutcome;

typedef struct {
    BootState state;
    uint32_t elapsed_ms;
    char detail[72];
} BootResult;

typedef struct {
    BootResult items[BOOT_ITEM_COUNT];
    uint8_t completed;
    uint8_t passed;
    uint8_t failed;
    uint8_t not_tested;
    uint32_t elapsed_ms;
} BootReport;

extern const char * const boot_item_names[BOOT_ITEM_COUNT];
const char *boot_state_name(BootState state);
void boot_report_reset(BootReport *report);
uint8_t boot_report_start(BootReport *report, BootItem item);
uint8_t boot_report_record(BootReport *report, BootItem item,
                           BootState state, const char *detail, uint32_t ms);
uint8_t boot_report_percent(const BootReport *report);
BootOutcome boot_report_outcome(const BootReport *report);
/* A checksum-valid complete NMEA sentence proves reception, not a GPS fix. */
uint8_t boot_nmea_valid(const uint8_t *data, uint16_t size);

#endif
