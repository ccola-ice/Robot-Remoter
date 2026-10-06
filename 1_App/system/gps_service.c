#include "gps_service.h"
#include "bsp_usart_gps.h"
#include "bsp_SysTick.h"
#include "bsp_rtc.h"
#include "nmea/nmea.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* 对外保留最近解析出的信息；坐标为十进制度，使用前须检查对应有效期。
 * new_parse 只表示本轮解析出了报文，不表示报文已提供有效定位。 */
double deg_lat, deg_lon;
nmeaINFO info;
nmeaPARSER parser;
uint8_t new_parse;
nmeaTIME beiJingTime;

/* 定位与日期时间独立过期；有效期按 DMA 接收时刻计算，避免积压数据被当作新数据。 */
static uint8_t parser_ready, position_valid, time_valid, rtc_time_set;
static uint32_t position_ms, time_ms;
static nmeaTIME last_rtc_time;

static uint32_t gps_now(void)
{
    unsigned long now;
    get_tick_count(&now);
    return (uint32_t)now;
}

/* 使用无符号差值计算时间间隔，使毫秒计数在正常短周期查询中跨回绕仍可比较。 */
uint8_t gps_data_is_fresh(void)
{
    return position_valid && (uint32_t)(gps_now() - position_ms) < GPS_DATA_MAX_AGE_MS;
}

uint8_t gps_time_is_fresh(void)
{
    return time_valid && (uint32_t)(gps_now() - time_ms) < GPS_DATA_MAX_AGE_MS;
}

/* 校验解析库的日历字段：year 从 1900 起算，mon 为 1..12；当前支持 2000..2099。 */
static int gps_date_valid(const nmeaTIME *utc)
{
    static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int year = utc->year + 1900, limit;
    if(year < 2000 || year > 2099 || utc->mon < 1 || utc->mon > 12 ||
       utc->hour < 0 || utc->hour > 23 || utc->min < 0 || utc->min > 59 ||
       utc->sec < 0 || utc->sec > 59)
        return 0;
    limit = days[utc->mon - 1];
    if(utc->mon == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
        ++limit;
    return utc->day >= 1 && utc->day <= limit;
}

static int gps_position_valid(double lat, char ns, double lon, char ew)
{
    /* NMEA 坐标使用度分格式；范围比较同时排除 NaN。 */
    return (ns == 'N' || ns == 'S') && (ew == 'E' || ew == 'W') &&
        lat >= 0 && lat <= 9000 && lon >= 0 && lon <= 18000 &&
        fmod(lat, 100.0) < 60.0 && fmod(lon, 100.0) < 60.0;
}

/* 兼容省略模式字段的旧报文，仅接纳自主、差分、浮点或固定解模式。 */
static int gps_mode_valid(char mode)
{
    return mode == 0 || mode == 'A' || mode == 'D' || mode == 'F' || mode == 'R';
}

/* 只有合法且未过期的 UTC 才能刷新本地时间；RTC 同步失败不阻止 GPS 时间缓存更新。 */
static void gps_accept_time(const nmeaTIME *utc, uint32_t received_ms)
{
    nmeaTIME local;
    if(!gps_date_valid(utc) || (uint32_t)(gps_now() - received_ms) >= GPS_DATA_MAX_AGE_MS)
        return;
    GMTconvert((nmeaTIME *)utc, &local, 8, 1);
    if(!gps_date_valid(&local))
        return;
    beiJingTime = local;
    time_valid = 1;
    time_ms = received_ms;
    /* 转成北京时间后按秒去重；同步失败时不更新记录，后续有效语句仍可重试。 */
    if(!rtc_time_set || local.year != last_rtc_time.year || local.mon != last_rtc_time.mon ||
       local.day != last_rtc_time.day || local.hour != last_rtc_time.hour ||
       local.min != last_rtc_time.min || local.sec != last_rtc_time.sec)
    {
        if(RTC_SynchronizeCalendar((uint16_t)(local.year + 1900), (uint8_t)local.mon,
            (uint8_t)local.day, (uint8_t)local.hour, (uint8_t)local.min, (uint8_t)local.sec))
        {
            last_rtc_time = local;
            rtc_time_set = 1;
        }
    }
}

/* 有效定位才更新坐标和时间戳；失效时保留数值，但清除信号/定位等级供上层识别。 */
static void gps_accept_position(int valid, uint32_t received_ms)
{
    position_valid = (uint8_t)(valid != 0);
    if(position_valid)
    {
        position_ms = received_ms;
        deg_lat = nmea_ndeg2degree(info.lat);
        deg_lon = nmea_ndeg2degree(info.lon);
    }
    else
    {
        info.sig = NMEA_SIG_BAD;
        info.fix = NMEA_FIX_BAD;
    }
}

/* 按语句能力更新状态：RMC 校验定位和日期时间，GGA/GLL 更新定位有效性，
 * ZDA 可独立校时；GSA 报告无有效定位时立即使旧位置失效。 */
static void gps_process_packet(int type, void *packet, uint32_t received_ms)
{
    int valid;
    switch(type)
    {
    case GPRMC:
    {
        nmeaGPRMC *p = (nmeaGPRMC *)packet;
        valid = p->status == 'A' && gps_mode_valid(p->mode) &&
            gps_position_valid(p->lat, p->ns, p->lon, p->ew) && gps_date_valid(&p->utc);
        nmea_GPRMC2info(p, &info);
        gps_accept_position(valid, received_ms);
        if(valid)
            gps_accept_time(&p->utc, received_ms);
        break;
    }
    case GNRMC:
    {
        nmeaGNRMC *p = (nmeaGNRMC *)packet;
        valid = p->status == 'A' && gps_mode_valid(p->mode) &&
            gps_position_valid(p->Lat, p->uLat, p->Lon, p->uLon) && gps_date_valid(&p->utc);
        nmea_GNRMC2info(p, &info);
        gps_accept_position(valid, received_ms);
        if(valid)
            gps_accept_time(&p->utc, received_ms);
        break;
    }
    case GPGGA:
    {
        nmeaGPGGA *p = (nmeaGPGGA *)packet;
        valid = p->sig >= 1 && p->sig <= 5 && gps_position_valid(p->lat, p->ns, p->lon, p->ew);
        nmea_GPGGA2info(p, &info);
        gps_accept_position(valid, received_ms);
        break;
    }
    case GNGGA:
    {
        nmeaGNGGA *p = (nmeaGNGGA *)packet;
        valid = p->FS >= 1 && p->FS <= 5 && gps_position_valid(p->Lat, p->uLat, p->Lon, p->uLon);
        nmea_GNGGA2info(p, &info);
        gps_accept_position(valid, received_ms);
        break;
    }
    case GNGLL:
    {
        nmeaGNGLL *p = (nmeaGNGLL *)packet;
        valid = p->Value == 'A' && gps_mode_valid(p->mode) &&
            gps_position_valid(p->Lat, p->uLat, p->Lon, p->uLon);
        nmea_GNGLL2info(p, &info);
        gps_accept_position(valid, received_ms);
        break;
    }
    case GPGSA:
        nmea_GPGSA2info((nmeaGPGSA *)packet, &info);
        if(info.fix != NMEA_FIX_2D && info.fix != NMEA_FIX_3D)
            gps_accept_position(0, received_ms);
        break;
    case BDGSA:
        nmea_BDGSA2info((nmeaBDGSA *)packet, &info);
        if(info.fix != NMEA_FIX_2D && info.fix != NMEA_FIX_3D)
            gps_accept_position(0, received_ms);
        break;
    case GPGSV: nmea_GPGSV2info((nmeaGPGSV *)packet, &info); break;
    case BDGSV: nmea_BDGSV2info((nmeaBDGSV *)packet, &info); break;
    case GPVTG: nmea_GPVTG2info((nmeaGPVTG *)packet, &info); break;
    case GNVTG: nmea_GNVTG2info((nmeaGNVTG *)packet, &info); break;
    case GNZDA:
        if(gps_date_valid(&((nmeaGNZDA *)packet)->utc))
        {
            nmea_GNZDA2info((nmeaGNZDA *)packet, &info);
            gps_accept_time(&((nmeaGNZDA *)packet)->utc, received_ms);
        }
        break;
    case GPTXT: nmea_GPTXT2info((nmeaGPTXT *)packet, &info); break;
    }
}

/* 重复初始化先释放旧解析器持有的资源，再重置缓存；初始化失败由 poll 返回 -1 暴露。 */
void gps_service_init(void)
{
    if(parser_ready)
        nmea_parser_destroy(&parser);
    nmea_property()->trace_func = &trace;
    nmea_property()->error_func = &error;
    nmea_property()->info_func = &gps_info;
    nmea_zero_INFO(&info);
    memset(&beiJingTime, 0, sizeof(beiJingTime));
    deg_lat = deg_lon = 0;
    new_parse = position_valid = time_valid = rtc_time_set = 0;
    position_ms = time_ms = 0;
    parser_ready = (uint8_t)nmea_parser_init(&parser);
}

/* 将底层分块字节流交给持续存在的解析器，允许一条 NMEA 语句跨多个 DMA 块。
 * 每次调用排空当前接收队列，再统一检查位置及时间是否已过期。 */
int gps_service_poll(void)
{
    uint8_t block[HALF_GPS_RBUFF_SIZE];
    uint32_t received_ms;
    int length, type, count = 0;
    void *packet;
    if(!parser_ready)
        return -1;
    new_parse = 0;
    /* DMA 中断负责缓存接收块；前台顺序解析，完整报文出队后由此处释放。 */
    while((length = GPS_DMA_ReadBlock(block, &received_ms)) != 0)
    {
        /* 丢块或积压超时会打断字节流，清除残帧以免与后续数据拼成错误报文。 */
        if(length < 0 || (uint32_t)(gps_now() - received_ms) >= GPS_DATA_MAX_AGE_MS)
        {
            nmea_parser_buff_clear(&parser);
            nmea_parser_queue_clear(&parser);
            position_valid = time_valid = 0;
            continue;
        }
        /* 解析器入队失败后丢弃残留状态；下一块重新同步语句边界，避免继续沿用旧有效标志。 */
        if(nmea_parser_push(&parser, (const char *)block, length) < 0)
        {
            nmea_parser_buff_clear(&parser);
            nmea_parser_queue_clear(&parser);
            position_valid = time_valid = 0;
            continue;
        }
        while((type = nmea_parser_pop(&parser, &packet)) != GPNON)
        {
            gps_process_packet(type, packet, received_ms);
            free(packet);
            ++count;
        }
    }
    /* 即使本轮没有新报文，也要让超时数据失效，避免断开 GPS 后沿用旧状态。 */
    if(!gps_data_is_fresh())
    {
        position_valid = 0;
        info.sig = NMEA_SIG_BAD;
        info.fix = NMEA_FIX_BAD;
    }
    if(!gps_time_is_fresh())
        time_valid = 0;
    new_parse = count != 0;
    return count;
}
