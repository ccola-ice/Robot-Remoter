#ifndef MENU_CATALOG_H
#define MENU_CATALOG_H
#include <stdint.h>

/* One ordering for navigation and labels. Groups contain 3, 2 and 6 entries. */
#define MENU_ENTRY_LIST(X) \
    X(ROBOT_CONTROL, "\273\372\306\367\310\313\277\330\326\306", "Hold DCH1 to enable / release to stop") \
    X(MONITOR, "\315\250\265\300\274\340\312\323", "Raw inputs / calibrated values / TX frame") \
    X(DIGITAL_CHANNELS, "\312\375\327\326\312\344\310\353", "Six switches / raw and debounced state") \
    X(PARAMETER_SETTINGS, "\262\316\312\375\311\350\326\303", "Calibration / reverse / trim / save") \
    X(NRF, "NRF \316\336\317\337", "Channel / power / air rate / module check") \
    X(SYSTEM_INFO, "\317\265\315\263\320\305\317\242", "Firmware / memory / boot report") \
    X(IMU, "\327\313\314\254\264\253\270\320\306\367", "Local attitude / acceleration") \
    X(GPS, "\316\300\320\307\266\250\316\273", "Local position / satellites") \
    X(FILE_BROWSER, "\316\304\274\376\344\257\300\300", "SD card and SPI Flash files") \
    X(DIAGNOSTICS, "\323\262\274\376\262\342\312\324", "Operator tests / hardware diagnostics") \
    X(EEPROM, "EEPROM \262\342\312\324", "AT24C08 diagnostic window")

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
