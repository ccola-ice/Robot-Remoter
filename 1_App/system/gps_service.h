#ifndef GPS_SERVICE_H
#define GPS_SERVICE_H

#include <stdint.h>

/* 从接收时刻起允许保留数据的最长时间；位置和完整日期时间分别计时。 */
#define GPS_DATA_MAX_AGE_MS 3000UL

/* 在前台初始化解析服务；可重复调用，重建解析器并使已有位置/时间缓存失效。
 * 不负责配置 GPS 串口和 DMA，底层接收初始化由启动流程完成。 */
void gps_service_init(void);
/* 由前台周期调用，消费 DMA 接收队列并更新定位与时间，不能与 init 并行执行。
 * 返回本轮解析报文数量（不等同于有效定位数）；解析器未就绪返回 -1。
 * 丢块、积压超时或解析器入队失败会使缓存失效；本函数可能分配/释放内存，不在中断中调用。 */
int gps_service_poll(void);
/* 返回值非零才可使用对应缓存；定位有效不要求已有完整日期，时间有效也不要求已定位。 */
uint8_t gps_data_is_fresh(void);
uint8_t gps_time_is_fresh(void);

#endif
