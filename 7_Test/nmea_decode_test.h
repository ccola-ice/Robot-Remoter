#ifndef __NMEA_DECODE_TEST_H
#define __NMEA_DECODE_TEST_H
#include "stm32f4xx.h"

#define GPS_DATA_MAX_AGE_MS 3000UL
void nmea_decode_init(void);
int nmea_decode_test(void);
/* Position and full calendar freshness are tracked independently. */
uint8_t gps_data_is_fresh(void);
uint8_t gps_time_is_fresh(void);

#endif
