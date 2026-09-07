#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bsp_spi_nrf.h"
#include "hardware_tests.h"

static uint8_t regs[32][5], before[32][5], payload[32];
static uint8_t command, offset, selected, response, ce, pending;
static unsigned fail_spi, absent, nack, bad_payload, cancel, busy, misconfigure;
static unsigned packets, elapsed, restored;
static unsigned quiet;
#define SPI_I2S_FLAG_TXE 1U
#define SPI_I2S_FLAG_RXNE 2U
#define RESET 0U
static uint8_t GPIO_ReadOutputDataBit(unsigned port, unsigned pin) {(void)port;(void)pin; return ce;}
static void GPIO_ResetBits(unsigned port, unsigned pin)
{
    (void)port;
    if(pin == NRF_CSN_PIN) { assert(!selected); selected=1U; offset=0U; }
    else if(pin == NRF_CE_PIN) ce=0U;
    else assert(0);
}
static void GPIO_SetBits(unsigned port, unsigned pin)
{
    (void)port;
    if(pin == NRF_CSN_PIN) {assert(selected); selected=0U;}
    else if(pin == NRF_CE_PIN) {
        ce=1U;
        if(pending && !(regs[CONFIG][0]&1U) && !quiet) {
            regs[STATUS][0]=nack ? MAX_RT : TX_DS;
            if(!nack) { pending=0U; packets++; }
        }
    } else assert(0);
}
static unsigned SPI_I2S_GetFlagStatus(unsigned spi, unsigned flag) {(void)spi;(void)flag; return !fail_spi;}
static void SPI_I2S_SendData(unsigned spi, uint8_t data)
{
    (void)spi;
    assert(selected);
    if(offset++ == 0U) {
        command=data; response=regs[STATUS][0];
        if(command == FLUSH_TX || command == FLUSH_RX) { restored=1U; pending=0U; }
        return;
    }
    if(absent) {response=0xffU; return;}
    if(command <= 0x1fU) {
        if(command == STATUS && ce && (regs[CONFIG][0]&1U) && !restored && !quiet)
            regs[STATUS][0] = RX_DR;
        response=regs[command][offset-2U];
    } else if(command <= 0x3fU) {
        uint8_t reg=command&0x1fU;
        if(reg == STATUS) regs[reg][0] &= ~data;
        else if(!(misconfigure && reg == RF_CH)) regs[reg][offset-2U]=data;
    } else if(command == WR_TX_PLOAD) {
        payload[offset-2U]=data;
        if(offset == 33U) pending=1U;
    } else if(command == RD_RX_PLOAD) {
        response=(uint8_t)(0x6dU ^ ((offset-2U)*17U));
        if(bad_payload) response^=1U;
        if(offset == 33U) packets++;
    }
}
static uint8_t SPI_I2S_ReceiveData(unsigned spi) {(void)spi; return response;}
static void Delay_us(unsigned us) {(void)us;}
static void Delay_ms(unsigned ms) {elapsed+=ms; assert(elapsed < 16000U);}
static uint8_t read_button_back_gpio(uint8_t id) {(void)id;return !cancel;}
const char *hardware_result_name(HwResult r)
{
    static const char * const names[]={"PASS","FAIL","BLOCKED","CANCELLED"};
    return names[r];
}

#include "radio_diagnostic.c"

static void reset(void)
{
    unsigned i,j;
    memset(regs,0,sizeof(regs));
    for(i=0;i<32;i++) for(j=0;j<5;j++) regs[i][j]=(uint8_t)(i+j);
    regs[CONFIG][0]=0x0fU; regs[STATUS][0]=0U; regs[FIFO_STATUS][0]=0x11U;
    regs[SETUP_AW][0]=3U;
    memcpy(before,regs,sizeof(regs));
    command=offset=selected=response=pending=0U; ce=1U;
    fail_spi=absent=nack=bad_payload=cancel=busy=misconfigure=packets=elapsed=restored=0U;
    quiet=0U;
}
static void check_restored(void)
{
    assert(ce == 1U && !selected);
    assert(memcmp(before,regs,sizeof(regs)) == 0);
}
int main(void)
{
    unsigned i;
    reset(); assert(hardware_radio_test(0U) == HW_PASS && packets == 8U);
    for(i=0;i<32;i++) assert(payload[i] == (uint8_t)(0x6dU ^ (i*17U)));
    check_restored();
    reset(); assert(hardware_radio_test(1U) == HW_PASS && packets == 8U); check_restored();
    reset(); nack=1U; assert(hardware_radio_test(0U) == HW_FAIL); check_restored();
    reset(); bad_payload=1U; assert(hardware_radio_test(1U) == HW_FAIL); check_restored();
    reset(); cancel=1U; assert(hardware_radio_test(1U) == HW_CANCELLED); check_restored();
    reset(); misconfigure=1U; assert(hardware_radio_test(0U) == HW_FAIL); check_restored();
    reset(); absent=1U; assert(hardware_radio_test(0U) == HW_FAIL && !restored); check_restored();
    reset(); fail_spi=1U; assert(hardware_radio_test(0U) == HW_FAIL && !restored); check_restored();
    reset(); quiet=1U; assert(hardware_radio_test(0U) == HW_FAIL && elapsed < 2100U); check_restored();
    reset(); quiet=1U; assert(hardware_radio_test(1U) == HW_FAIL && elapsed < 15100U); check_restored();
    reset(); regs[FIFO_STATUS][0]=0U;
    assert(hardware_radio_test(0U) == HW_BLOCKED && !restored && !packets);
    assert(regs[FIFO_STATUS][0]==0U && ce == 1U);
    puts("PASS: radio ACK/RX checks, absent device, NACK, corruption, cancel, configuration restore");
    return 0;
}
