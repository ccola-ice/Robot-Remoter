#include "nmea_decode_test.h"
#include "bsp_usart_gps.h"
#include "bsp_SysTick.h"
#include "bsp_rtc.h"
#include "nmea/nmea.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

double deg_lat, deg_lon;
nmeaINFO info;
nmeaPARSER parser;
uint8_t new_parse;
nmeaTIME beiJingTime;

static uint8_t parser_ready, position_valid, time_valid, rtc_time_set;
static uint32_t position_ms, time_ms;
static nmeaTIME last_rtc_time;

static uint32_t gps_now(void)
{
    unsigned long now;
    get_tick_count(&now);
    return (uint32_t)now;
}

uint8_t gps_data_is_fresh(void)
{
    return position_valid && (uint32_t)(gps_now() - position_ms) < GPS_DATA_MAX_AGE_MS;
}

uint8_t gps_time_is_fresh(void)
{
    return time_valid && (uint32_t)(gps_now() - time_ms) < GPS_DATA_MAX_AGE_MS;
}

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
    /* Comparisons also reject NaN; NMEA coordinates are degrees/minutes. */
    return (ns == 'N' || ns == 'S') && (ew == 'E' || ew == 'W') &&
        lat >= 0 && lat <= 9000 && lon >= 0 && lon <= 18000 &&
        fmod(lat, 100.0) < 60.0 && fmod(lon, 100.0) < 60.0;
}

static int gps_mode_valid(char mode)
{
    return mode == 0 || mode == 'A' || mode == 'D' || mode == 'F' || mode == 'R';
}

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

void nmea_decode_init(void)
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

int nmea_decode_test(void)
{
    uint8_t block[HALF_GPS_RBUFF_SIZE];
    uint32_t received_ms;
    int length, type, count = 0;
    void *packet;
    if(!parser_ready)
        return -1;
    new_parse = 0;
    while((length = GPS_DMA_ReadBlock(block, &received_ms)) != 0)
    {
        if(length < 0 || (uint32_t)(gps_now() - received_ms) >= GPS_DATA_MAX_AGE_MS)
        {
            nmea_parser_buff_clear(&parser);
            nmea_parser_queue_clear(&parser);
            position_valid = time_valid = 0;
            continue;
        }
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
