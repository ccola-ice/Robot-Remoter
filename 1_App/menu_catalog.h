#ifndef MENU_CATALOG_H
#define MENU_CATALOG_H
#include <stdint.h>

/* One ordering for navigation and labels. Groups contain 3, 2 and 6 entries. */
#define MENU_ENTRY_LIST(X) \
    X(ROBOT_CONTROL, "Robot Control", "Hold DCH1 to enable / release to stop") \
    X(MONITOR, "Channel Monitor", "Raw inputs / calibrated values / TX frame") \
    X(DIGITAL_CHANNELS, "Digital Inputs", "Six switches / raw and debounced state") \
    X(PARAMETER_SETTINGS, "Parameters", "Calibration / reverse / trim / save") \
    X(NRF, "NRF Wireless", "Channel / power / air rate / module check") \
    X(SYSTEM_INFO, "System Info", "Firmware / memory / boot report") \
    X(IMU, "IMU / MPU6050", "Local attitude / acceleration") \
    X(GPS, "GPS / BDS", "Local position / satellites") \
    X(FILE_BROWSER, "File Browser", "SD card and SPI Flash files") \
    X(DIAGNOSTICS, "Hardware Tests", "Operator tests / hardware diagnostics") \
    X(EEPROM, "EEPROM", "AT24C08 diagnostic window")

#define MENU_ENTRY_ENUM(page, label, hint) MENU_ENTRY_##page,
typedef enum { MENU_ENTRY_LIST(MENU_ENTRY_ENUM) MENU_ENTRY_COUNT } MenuEntry;
#undef MENU_ENTRY_ENUM
#define MENU_GROUP_COUNT 3U

static __inline uint8_t menu_group_first(uint8_t group)
{
    return group == 0U ? 0U : group == 1U ? 3U : 5U;
}
static __inline uint8_t menu_group_count(uint8_t group)
{
    return group == 0U ? 3U : group == 1U ? 2U : 6U;
}
static __inline uint8_t menu_entry_group(uint8_t entry)
{
    return entry < 3U ? 0U : entry < 5U ? 1U : 2U;
}
#endif
