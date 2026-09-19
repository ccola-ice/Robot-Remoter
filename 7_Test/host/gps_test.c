#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bsp_usart_gps.h"
#include "nmea_decode_test.h"
#include "nmea/tok.h"

#define GPS_USART_DMA_STREAM 0
#define GPS_DMA_IT_HT 1U
#define GPS_DMA_IT_TC 2U
#define DMA_IT_TEIF1 4U
#define DMA_IT_DMEIF1 8U
#define DMA_IT_FEIF1 16U
#define RESET 0
static uint32_t fake_tick, dma_flags, irq_mask;
static uint16_t dma_remaining = 256;
static int rtc_calls;
static nmeaTIME last_rtc;
extern nmeaINFO info;
extern nmeaPARSER parser;
extern nmeaTIME beiJingTime;

static int DMA_GetITStatus(int stream, uint32_t flag)
{ (void)stream; return (dma_flags & flag) != 0; }
static void DMA_ClearITPendingBit(int stream, uint32_t flag)
{ (void)stream; dma_flags &= ~flag; }
static uint16_t DMA_GetCurrDataCounter(int stream)
{ (void)stream; return dma_remaining; }
static uint32_t __get_PRIMASK(void) { return irq_mask; }
static void __disable_irq(void) { irq_mask = 1; }
static void __set_PRIMASK(uint32_t value) { irq_mask = value; }
int get_tick_count(unsigned long *count) { *count = fake_tick; return 0; }
void trace(const char *text, int length) { (void)text; (void)length; }
void error(const char *text, int length) { (void)text; (void)length; }
void gps_info(const char *text, int length) { (void)text; (void)length; }
uint8_t RTC_SynchronizeCalendar(uint16_t year, uint8_t month, uint8_t day,
                               uint8_t hour, uint8_t minute, uint8_t second)
{
    ++rtc_calls;
    last_rtc.year = year - 1900; last_rtc.mon = month; last_rtc.day = day;
    last_rtc.hour = hour; last_rtc.min = minute; last_rtc.sec = second;
    return 1;
}
#include "gps_queue.inc"

static void reset_fixture(void)
{
    fake_tick = 0;
    rtc_calls = 0;
    gps_rx_head = gps_rx_tail = gps_rx_count = gps_rx_lost = 0;
    gps_rx_overruns = 0;
    dma_flags = 0;
    dma_remaining = 256;
    nmea_decode_init();
}

static int sentence(char *out, const char *body)
{
    return sprintf(out, "$%s*%02X\r\n", body, nmea_calc_crc(body, (int)strlen(body)));
}

static void queue_text(const char *text)
{
    size_t len = strlen(text);
    assert(len < HALF_GPS_RBUFF_SIZE);
    memset(gps_rbuff, ' ', HALF_GPS_RBUFF_SIZE);
    memcpy(gps_rbuff, text, len);
    dma_remaining = HALF_GPS_RBUFF_SIZE;
    dma_flags = GPS_DMA_IT_HT;
    GPS_DMA_ReceiveIRQ();
}

static void feed(const char *body)
{
    char out[256];
    sentence(out, body);
    queue_text(out);
    nmea_decode_test();
}

static const char *rmc = "GNRMC,123519.000,A,4807.038,N,01131.000,E,1.0,84.4,190926,,,A";

static void test_parser_bounds(void)
{
    nmeaGPGGA gga;
    nmeaGPTXT txt;
    nmeaBDGSA gsa;
    nmeaINFO data;
    nmeaPARSER p;
    char out[2048], body[400], fields[256];
    int len;
    char empty[2] = {'X','X'};
    assert(nmea_scanf(",", 1, "%1s,", empty) == 1 && empty[0] == 0);
    assert(nmea_scanf("ab,", 3, "%1s,", empty) == -1);
    assert(nmea_scanf("a,", 2, "%s,", empty) == -1);
    memset(fields, 'A', 255); fields[255] = 0;
    sprintf(body, "GPTXT,01,01,02,%s", fields);
    len = sentence(out, body);
    assert(nmea_parse_GPTXT(out, len, &txt) == 1 && txt.xx == 1 && txt.zz == 2);
    strcat(body, "A");
    len = sentence(out, body);
    assert(nmea_parse_GPTXT(out, len, &txt) == 0);
    len = sentence(out, "GPGGA,123519.0000,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
    assert(nmea_parse_GPGGA(out, len, &gga) == 0);
    len = sentence(out, "GPGGA,,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
    assert(nmea_parse_GPGGA(out, len, &gga) == 0);
    len = sentence(out, "GPGGA,246000,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
    assert(nmea_parse_GPGGA(out, len, &gga) == 0);
    len = sentence(out, "GPGGA,123519.12,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
    assert(nmea_parse_GPGGA(out, len, &gga) == 1 && gga.utc.hsec == 12);
    len = sentence(out, "BDGSA,A,3,01,02,03,04,,,,,,,,,1.2,0.8,0.9");
    assert(nmea_parse_BDGSA(out, len, &gsa) == 1);
    nmea_zero_INFO(&data);
    data.BDsatinfo.inview = NMEA_MAXSAT + 1;
    data.BDsatinfo.sat[0].id = 1;
    nmea_BDGSA2info(&gsa, &data);
    assert(data.fix == NMEA_FIX_3D && data.BDsatinfo.sat[0].in_use == 1);
    memset(gsa.SVID, 0, sizeof(gsa.SVID));
    nmea_BDGSA2info(&gsa, &data);
    assert(data.BDsatinfo.sat[0].in_use == 0);

    assert(nmea_parser_init(&p));
    len = sentence(out, rmc);
    assert(nmea_parse(&p, out, 17, &data) == 0);
    assert(nmea_parse(&p, out + 17, len - 17, &data) == 1);
    memset(out, ' ', 1100);
    len = sentence(out + 1100, rmc);
    assert(nmea_parse(&p, out, 1100 + len, &data) == 1);
    nmea_parser_destroy(&p);
}

static void test_freshness_and_clock(void)
{
    char out[256];
    reset_fixture();
    assert(!gps_data_is_fresh() && !gps_time_is_fresh());
    feed(rmc);
    assert(gps_data_is_fresh() && gps_time_is_fresh() && rtc_calls == 1);
    assert(beiJingTime.hour == 20 && last_rtc.mon == 9);
    feed(rmc);
    assert(rtc_calls == 1);
    fake_tick = 2999;
    feed("GPGSV,1,1,01,01,40,083,41");
    assert(gps_data_is_fresh());
    fake_tick = 3000;
    assert(!gps_data_is_fresh() && !gps_time_is_fresh());
    nmea_decode_test();
    assert(info.fix == NMEA_FIX_BAD && info.sig == NMEA_SIG_BAD);
    fake_tick = 4000;
    sentence(out, rmc); out[12] ^= 1;
    queue_text(out); nmea_decode_test();
    assert(!gps_data_is_fresh() && rtc_calls == 1);
    feed("GNRMC,123520.000,V,4807.038,N,01131.000,E,1.0,84.4,190926,,,N");
    assert(!gps_data_is_fresh() && rtc_calls == 1);
    feed("GNRMC,123520.000,A,4807.038,N,01131.000,E,1.0,84.4,310226,,,A");
    assert(!gps_data_is_fresh() && rtc_calls == 1);
    feed("GNRMC,123520.000,A,4861.000,N,01131.000,E,1.0,84.4,190926,,,A");
    assert(!gps_data_is_fresh() && rtc_calls == 1);
    feed("GNZDA,235959.00,31,12,2026,00,00");
    assert(gps_time_is_fresh() && !gps_data_is_fresh() && rtc_calls == 2);
    assert(last_rtc.year == 127 && last_rtc.mon == 1 && last_rtc.day == 1 && last_rtc.hour == 7);
    fake_tick = UINT32_MAX - 1000U;
    feed(rmc);
    fake_tick += 2999U;
    assert(gps_data_is_fresh());
    ++fake_tick;
    assert(!gps_data_is_fresh());
}

static void test_queue_loss(void)
{
    int i;
    char out[256];
    uint8_t block[256];
    uint32_t tick;
    reset_fixture();
    feed(rmc);
    for(i = 0; i < 5; ++i)
        queue_text(" ");
    assert(gps_rx_overrun_count() == 1);
    nmea_decode_test();
    assert(!gps_data_is_fresh() && !gps_time_is_fresh());
    feed(rmc);
    assert(gps_data_is_fresh());
    sentence(out, rmc);
    queue_text(out);
    fake_tick += 3000;
    nmea_decode_test();
    assert(!gps_data_is_fresh());
    dma_flags = GPS_DMA_IT_HT | GPS_DMA_IT_TC;
    dma_remaining = 512;
    memset(gps_rbuff + 256, 'B', 256);
    GPS_DMA_ReceiveIRQ();
    assert(GPS_DMA_ReadBlock(block, &tick) == -1);
    assert(GPS_DMA_ReadBlock(block, &tick) == 256 && block[0] == 'B');
    dma_flags = DMA_IT_TEIF1;
    GPS_DMA_ReceiveIRQ();
    assert(GPS_DMA_ReadBlock(block, &tick) == -1);
    assert(GPS_DMA_ReadBlock(block, &tick) == 0);
}

static void test_timezone(void)
{
    nmeaTIME utc = {100, 2, 28, 20, 0, 0, 0}, local;
    GMTconvert(&utc, &local, 8, 1);
    assert(local.year == 100 && local.mon == 2 && local.day == 29);
    utc.mon = 3; utc.day = 1; utc.hour = 1;
    GMTconvert(&utc, &local, 8, 0);
    assert(local.mon == 2 && local.day == 29 && local.hour == 17);
    GMTconvert(&utc, &local, 0, 0);
    assert(memcmp(&utc, &local, sizeof(utc)) == 0);
}

int main(void)
{
    test_parser_bounds();
    test_freshness_and_clock();
    test_queue_loss();
    test_timezone();
    nmea_parser_destroy(&parser);
    puts("GPS tests passed: parser boundaries, fix status, clock/freshness, DMA queue/loss, tick wrap, calendar.");
    return 0;
}
