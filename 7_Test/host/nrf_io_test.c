#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t u8;
typedef uint32_t u32;
#define __IO volatile
#define SPI3 3U
#define GPIOE 5U
#define GPIO_Pin_4 16U
#define GPIO_Pin_5 32U
#define GPIO_Pin_6 64U
#define NRF_TIMEOUT 16U
#define RESET 0U
#define ERROR 0U
#define SUCCESS 1U
#define SPI_I2S_FLAG_TXE 1U
#define SPI_I2S_FLAG_RXNE 2U
#define CoreDebug_DEMCR_TRCENA_Msk 1U
#define DWT_CTRL_CYCCNTENA_Msk 1U
#include "bsp_spi_nrf.h"

static struct { uint32_t DEMCR; } debug;
static struct { uint32_t CTRL, CYCCNT; } cycles;
#define CoreDebug (&debug)
#define DWT (&cycles)
static uint32_t SystemCoreClock=168000000UL;
static uint8_t registers[32][5], payload[32], command, offset, response;
static unsigned selected, ce, payload_pending, sent, flushes, irq_reads;
static unsigned fail_flag, fault_calls, absent, automatic_status, pulse_us;

static void GPIO_ResetBits(unsigned port,unsigned pin)
{
    (void)port;
    if(pin==NRF_CSN_PIN) {assert(!selected);selected=1U;offset=0U;}
    else if(pin==NRF_CE_PIN) ce=0U;
    else assert(0);
}
static void GPIO_SetBits(unsigned port,unsigned pin)
{
    (void)port;
    if(pin==NRF_CSN_PIN) {assert(selected);selected=0U;}
    else if(pin==NRF_CE_PIN) {
        ce=1U;
        if(payload_pending && (registers[CONFIG][0]&3U)==2U) {
            sent++;
            registers[STATUS][0]=(uint8_t)automatic_status;
            if(automatic_status & TX_DS) payload_pending=0U;
        }
    } else assert(0);
}
static uint8_t GPIO_ReadOutputDataBit(unsigned port,unsigned pin)
{(void)port;(void)pin;return (uint8_t)ce;}
static uint8_t GPIO_ReadInputDataBit(unsigned port,unsigned pin)
{(void)port;(void)pin;irq_reads++;return 1U;}
static unsigned SPI_I2S_GetFlagStatus(unsigned spi,unsigned flag)
{
    (void)spi;
    if(flag==fail_flag) {assert(++fault_calls<=NRF_TIMEOUT+1U);return RESET;}
    return 1U;
}
static void SPI_I2S_SendData(unsigned spi,uint8_t value)
{
    (void)spi;assert(selected);
    if(offset++==0U) {
        command=value;response=absent ? 0xffU : registers[STATUS][0];
        if(command==FLUSH_TX) {payload_pending=0U;flushes++;}
        return;
    }
    if(absent) {response=0xffU;return;}
    if(command<=0x1fU) response=registers[command][offset-2U];
    else if(command<=0x3fU) {
        unsigned reg=command&0x1fU;
        if(reg==STATUS) registers[reg][0]&=(uint8_t)~value;
        else registers[reg][offset-2U]=value;
    } else if(command==WR_TX_PLOAD) {
        payload[offset-2U]=value;
        if(offset==33U) payload_pending=1U;
    }
}
static uint8_t SPI_I2S_ReceiveData(unsigned spi) {(void)spi;return response;}
static void Delay_us(uint32_t us)
{
    if(ce) pulse_us+=us;
    cycles.CYCCNT+=us*(SystemCoreClock/1000000UL);
}

#include "production_nrf.inc"

static void reset(void)
{
    memset(registers,0,sizeof(registers));
    memset(payload,0,sizeof(payload));
    selected=ce=payload_pending=sent=flushes=irq_reads=0U;
    fail_flag=fault_calls=absent=pulse_us=0U;
    automatic_status=TX_DS;
    registers[CONFIG][0]=0x08U; /* Chip reset CRC is only 8-bit. */
    cycles.CYCCNT=0U;
    NRF_ResetState();
}

int main(void)
{
    uint8_t frame[32]={0x34U,0x43U}, enabled, channel, power, rate;
    unsigned prior_flushes;
    uint32_t generation;
    reset();
    assert(NRF_TxStart(frame)==0U && !sent);
    generation=NRF_GetConfigGeneration();
    nrf24l01_apply_settings(0U,72U,0x0fU,0U);
    assert(NRF_GetConfigGeneration()!=generation);
    assert(registers[CONFIG][0]==0x0cU && registers[RF_CH][0]==72U);
    assert(registers[RF_SETUP][0]==0x27U && !ce);
    assert(nrf24l01_read_runtime(&enabled,&channel,&power,&rate)==0U);
    assert(enabled==0U && channel==72U && power==0x0fU && rate==0U);
    assert(NRF_TxStart(frame)==0U);
    generation=NRF_GetConfigGeneration();
    nrf24l01_apply_settings(1U,40U,0x09U,2U);
    assert(NRF_GetConfigGeneration()!=generation);
    assert(registers[CONFIG][0]==0x0eU && !ce);
    assert(nrf24l01_read_runtime(&enabled,&channel,&power,&rate)==0U);
    assert(enabled==1U && channel==40U && power==0x09U && rate==2U);
    generation=NRF_GetConfigGeneration();
    assert(NRF_TxStart(frame)==1U && pulse_us==20U && !ce);
    assert(NRF_TxStart(frame)==0U); /* Never overwrite an in-flight command. */
    assert(NRF_TxPoll()==TX_DS && sent==1U && irq_reads==0U);
    assert(NRF_GetConfigGeneration()==generation); /* Traffic is not reconfiguration. */
    assert(!payload_pending && !ce && !selected);
    automatic_status=MAX_RT;
    assert(NRF_TxStart(frame)==1U && NRF_TxPoll()==MAX_RT);
    assert(!payload_pending && NRF_GetIoError()==0U);
    automatic_status=0U;
    assert(NRF_TxStart(frame)==1U && NRF_TxPoll()==NRF_TX_PENDING);
    cycles.CYCCNT=nrf_tx_started+(SystemCoreClock/1000U)*30U;
    assert(NRF_TxPoll()==ERROR && !payload_pending && !ce);
    assert(NRF_TxPoll()==ERROR);

    cycles.CYCCNT=0xfffffff0UL;
    assert(NRF_TxStart(frame)==1U);
    cycles.CYCCNT=nrf_tx_started+(SystemCoreClock/1000U)*30U;
    assert(NRF_TxPoll()==ERROR); /* Counter wrap. */
    assert(NRF_TxStart(frame)==1U);
    prior_flushes=flushes;
    nrf24l01_apply_settings(0U,80U,0x0bU,1U);
    assert(!payload_pending && !ce && flushes>prior_flushes);
    assert(NRF_TxPoll()==ERROR && NRF_TxStart(frame)==0U);
    nrf24l01_apply_settings(1U,40U,0x0fU,2U);
    assert(NRF_Tx_Dat(frame)==ERROR && !payload_pending && !irq_reads);
    automatic_status=TX_DS;
    assert(NRF_Tx_Dat(frame)==TX_DS);

    reset();nrf24l01_apply_settings(1U,40U,0x0fU,2U);
    fail_flag=SPI_I2S_FLAG_TXE;
    assert(NRF_TxStart(frame)==0U && NRF_GetIoError()!=0U && !ce && !selected);
    assert(fault_calls==NRF_TIMEOUT+1U);
    fail_flag=0U;
    generation=NRF_GetConfigGeneration();
    nrf24l01_apply_settings(1U,41U,0x0fU,2U);
    assert(NRF_GetConfigGeneration()!=generation); /* Failed apply also invalidates arming. */
    assert(NRF_GetIoError()!=0U && NRF_TxStart(frame)==0U && !ce);
    assert(nrf24l01_read_runtime(&enabled,&channel,&power,&rate)==1U);
    NRF_RX_Mode();assert(!ce);

    reset();nrf24l01_apply_settings(1U,40U,0x0fU,2U);
    automatic_status=0U;
    assert(NRF_TxStart(frame)==1U);
    fail_flag=SPI_I2S_FLAG_RXNE;
    assert(NRF_TxPoll()==ERROR && NRF_GetIoError()!=0U && !ce && !selected);
    assert(fault_calls==NRF_TIMEOUT+1U);

    reset();nrf24l01_apply_settings(1U,40U,0x0fU,2U);absent=1U;
    assert(NRF_TxStart(frame)==0U && NRF_GetIoError()!=0U && !ce);
    reset();
    assert(nrf24l01_read_runtime(0,&channel,&power,&rate)==1U);
    assert(NRF_TxStart(0)==0U);
    nrf24l01_apply_settings(1U,40U,0x0fU,2U);
    NRF_RX_Mode();assert(ce);
    assert(nrf24l01_read_runtime(&enabled,&channel,&power,&rate)==0U && ce);
    fail_flag=SPI_I2S_FLAG_TXE;
    assert(nrf24l01_read_runtime(&enabled,&channel,&power,&rate)==1U && !ce);
    reset();
    generation=NRF_GetConfigGeneration();
    NRF_SetRFConfig(40U,0x0fU);
    assert(NRF_GetConfigGeneration()!=generation);
    generation=NRF_GetConfigGeneration();
    NRF_TX_Mode();
    assert(NRF_GetConfigGeneration()!=generation);
    generation=NRF_GetConfigGeneration();
    NRF_RX_Mode();
    assert(NRF_GetConfigGeneration()!=generation);
    generation=NRF_GetConfigGeneration();
    NRF_PowerDown();
    assert(NRF_GetConfigGeneration()!=generation);
    generation=NRF_GetConfigGeneration();
    NRF_TxCancel();
    assert(NRF_GetConfigGeneration()==generation);
    nrf_config_generation=UINT32_MAX;
    NRF_TX_Mode();
    assert(NRF_GetConfigGeneration()==0U); /* A wrapping generation still changes. */
    puts("NRF I/O and asynchronous transmit regression: PASS");
    return 0;
}
