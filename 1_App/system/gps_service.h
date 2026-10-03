#ifndef GPS_SERVICE_H
#define GPS_SERVICE_H

#include <stdint.h>

#define GPS_DATA_MAX_AGE_MS 3000UL

void gps_service_init(void);
/* 消费 DMA 接收队列并更新定位与时间；返回解析数量，未初始化时返回 -1。 */
int gps_service_poll(void);
/* 定位数据与完整日期时间分别判断有效期。 */
uint8_t gps_data_is_fresh(void);
uint8_t gps_time_is_fresh(void);

#endif
