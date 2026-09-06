#include "boot_status.h"
#include <string.h>

const char * const boot_item_names[BOOT_ITEM_COUNT] = {
    "MCU clocks", "LCD visual quality", "External SRAM R/W",
    "Flash ID + read", "EEPROM ACK + read", "RTC clock tick",
    "ADC1 DMA samples", "ADC3 DMA samples", "MPU6050 DMP init",
    "MPU6050 FIFO data", "Touch controller", "SD card commands",
    "SD filesystem read", "NRF SPI register", "Parameter load",
    "NRF config readback", "GPS NMEA reception", "Runtime timers",
    "Keys/switches/encoder", "Analog travel / Vbat", "LED / buzzer output",
    "UART external loop", "Radio peer link", "Flash erase/program",
    "EEPROM write", "SD / FatFs write", "Touch accuracy",
    "MCU full ROM/RAM"
};

const char *boot_state_name(BootState state)
{
    switch(state) {
    case BOOT_PENDING: return "PENDING";
    case BOOT_RUNNING: return "RUNNING";
    case BOOT_PASS: return "PASS";
    case BOOT_FAIL: return "FAIL";
    case BOOT_NOT_TESTED: return "NOT TESTED";
    default: return "INVALID";
    }
}

void boot_report_reset(BootReport *report)
{
    memset(report, 0, sizeof(*report));
}

uint8_t boot_report_start(BootReport *report, BootItem item)
{
    if((unsigned)item >= BOOT_ITEM_COUNT ||
       report->items[item].state != BOOT_PENDING) return 0U;
    report->items[item].state = BOOT_RUNNING;
    return 1U;
}

uint8_t boot_report_record(BootReport *report, BootItem item,
                           BootState state, const char *detail, uint32_t ms)
{
    BootResult *result;
    if((unsigned)item >= BOOT_ITEM_COUNT ||
       (state != BOOT_PASS && state != BOOT_FAIL && state != BOOT_NOT_TESTED))
        return 0U;
    result = &report->items[item];
    if(result->state != BOOT_PENDING && result->state != BOOT_RUNNING)
        return 0U; /* A later stage cannot erase a recorded failure. */
    result->state = state;
    result->elapsed_ms = ms;
    if(detail != 0) {
        strncpy(result->detail, detail, sizeof(result->detail) - 1U);
        result->detail[sizeof(result->detail) - 1U] = '\0';
    }
    report->completed++;
    if(state == BOOT_PASS) report->passed++;
    else if(state == BOOT_FAIL) report->failed++;
    else report->not_tested++;
    return 1U;
}

uint8_t boot_report_percent(const BootReport *report)
{
    return (uint8_t)((100UL * report->completed) / BOOT_ITEM_COUNT);
}

BootOutcome boot_report_outcome(const BootReport *report)
{
    if(report->failed != 0U) return BOOT_FAILED;
    if(report->completed != BOOT_ITEM_COUNT) return BOOT_INCOMPLETE;
    if(report->not_tested != 0U) return BOOT_PARTIAL;
    return BOOT_PASSED;
}

static int boot_hex(uint8_t c)
{
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

uint8_t boot_nmea_valid(const uint8_t *data, uint16_t size)
{
    uint16_t i, j;
    uint8_t checksum;
    int hi, lo;
    for(i = 0U; i + 10U < size; i++) {
        if(data[i] != '$' || data[i + 6U] != ',') continue;
        for(j = 1U; j <= 5U; j++) {
            if(data[i + j] < 'A' || data[i + j] > 'Z') break;
        }
        if(j != 6U) continue;
        checksum = 0U;
        for(j = i + 1U; j + 4U < size && (uint16_t)(j - i) < 82U; j++) {
            if(data[j] == '*') {
                hi = boot_hex(data[j + 1U]);
                lo = boot_hex(data[j + 2U]);
                if(hi >= 0 && lo >= 0 && checksum == (uint8_t)(hi * 16 + lo) &&
                   data[j + 3U] == '\r' && data[j + 4U] == '\n') return 1U;
                break;
            }
            if(data[j] < 0x20U || data[j] > 0x7eU || data[j] == '$') break;
            checksum ^= data[j];
        }
    }
    return 0U;
}
