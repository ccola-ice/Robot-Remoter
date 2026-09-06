#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "boot_status.h"

/* Compile the production SRAM algorithm against owned host memory.
 * The legacy SRAM_test.h happens to use this include guard. */
#define __SDCARD_TEST_H
static uint16_t host_sram[512];
#define SRAM_BASE_ADDR ((uintptr_t)host_sram)
#define IS62WV51216_SIZE (sizeof(host_sram))
#include "../SRAM_test.c"

static void report_regressions(void)
{
    BootReport report;
    unsigned i, failed_at;
    char long_detail[128];
    boot_report_reset(&report);
    assert(boot_report_percent(&report) == 0U);
    assert(boot_report_outcome(&report) == BOOT_INCOMPLETE);
    assert(!boot_report_start(&report, BOOT_ITEM_COUNT));
    assert(!boot_report_record(&report, BOOT_CLOCK, BOOT_RUNNING, "", 0));
    assert(boot_report_start(&report, BOOT_CLOCK));
    assert(!boot_report_start(&report, BOOT_CLOCK));
    assert(boot_report_percent(&report) == 0U); /* retries/running do not advance */
    memset(long_detail, 'x', sizeof(long_detail));
    long_detail[sizeof(long_detail)-1] = 0;
    assert(boot_report_record(&report, BOOT_CLOCK, BOOT_PASS, long_detail, 12));
    assert(report.items[BOOT_CLOCK].detail[71] == 0);
    assert(!boot_report_record(&report, BOOT_CLOCK, BOOT_PASS, "duplicate", 0));
    assert(report.completed == 1);

    /* Failure at ANY stage must survive all later successes and completion. */
    for(failed_at = 0; failed_at < BOOT_ITEM_COUNT; failed_at++) {
        boot_report_reset(&report);
        for(i = 0; i < BOOT_ITEM_COUNT; i++) {
            assert(boot_report_start(&report, (BootItem)i));
            assert(boot_report_record(&report, (BootItem)i,
                   i == failed_at ? BOOT_FAIL : BOOT_PASS, "checked", i));
            assert(boot_report_percent(&report) == (i + 1) * 100 / BOOT_ITEM_COUNT);
        }
        assert(report.failed == 1);
        assert(boot_report_percent(&report) == 100);
        assert(boot_report_outcome(&report) == BOOT_FAILED);
        assert(!boot_report_record(&report, (BootItem)failed_at, BOOT_PASS, "overwrite", 0));
        assert(boot_report_outcome(&report) == BOOT_FAILED);
    }
    boot_report_reset(&report);
    for(i = 0; i < BOOT_ITEM_COUNT; i++)
        boot_report_record(&report, (BootItem)i, BOOT_NOT_TESTED, "preserved", 0);
    assert(report.passed == 0 && report.not_tested == BOOT_ITEM_COUNT);
    assert(boot_report_percent(&report) == 100);
    assert(boot_report_outcome(&report) == BOOT_PARTIAL);
    boot_report_reset(&report);
    for(i = 0; i < BOOT_ITEM_COUNT; i++)
        boot_report_record(&report, (BootItem)i, BOOT_PASS, "checked", 0);
    assert(boot_report_outcome(&report) == BOOT_PASSED);
}

static void gps_regressions(void)
{
    const char *valid = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    uint8_t data[256];
    size_t len = strlen(valid);
    memcpy(data, valid, len);
    assert(boot_nmea_valid(data, (uint16_t)len));
    assert(!boot_nmea_valid(data, (uint16_t)(len-1)));
    data[12] ^= 1;
    assert(!boot_nmea_valid(data, (uint16_t)len));
    memset(data, 0xff, sizeof(data));
    assert(!boot_nmea_valid(data, sizeof(data)));
    memcpy(data+13, valid, len);
    assert(boot_nmea_valid(data, sizeof(data)));
    data[14] = '0';
    assert(!boot_nmea_valid(data, sizeof(data)));
}

static void sram_regressions(void)
{
    uint16_t before[512];
    unsigned i;
    for(i = 0; i < 512; i++) host_sram[i] = (uint16_t)(i * 173U + 29U);
    memcpy(before, host_sram, sizeof(before));
    assert(sram_read_write_test() == 1);
    assert(memcmp(before, host_sram, sizeof(before)) == 0);
    /* Partial final block and untouched guard bytes around the requested area. */
    assert(sram_test_region((volatile uint8_t *)host_sram + 2, 510) == 1);
    assert(memcmp(before, host_sram, sizeof(before)) == 0);
    assert(sram_test_region((volatile uint8_t *)host_sram, 511) == 0);
    assert(memcmp(before, host_sram, sizeof(before)) == 0);
}

int main(void)
{
    report_regressions();
    gps_regressions();
    sram_regressions();
    puts("PASS: boot outcomes/progress, NMEA validation, SRAM bounds/restoration");
    return 0;
}
