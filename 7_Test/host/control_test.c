#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "param.h"
#include "robot_control_protocol.h"
#include "control_safety.h"
#define NUM_OF_ADC1CHANNEL 7U
#define DIGITAL_CHANNEL_COUNT 6U
#define DMA_FLAG_TCIF4 1U
#define DMA_FLAG_TEIF4 2U
#define DMA_FLAG_DMEIF4 4U
#define DMA_FLAG_FEIF4 8U
#define DMA_SxCR_EN 1U
#define DCH1_GPIO_PORT 0U
#define DCH1_GPIO_PIN 0U
#define RESET 0U
#define Bit_RESET 0U
#define TX_DS 0x20U
#define NRF_TX_PENDING 1U
static struct { uint32_t CR; } dma = {1U};
#define DUAL_ADC1_DMA_STREAM (&dma)
static uint32_t mock_now, mock_flags = 1U;
static uint8_t mock_button = 1U, mock_raw = 1U, mock_result = TX_DS, mock_error;
static uint8_t sent[32];
static unsigned sent_count;
static uint32_t mock_generation;
volatile param_Config param;
volatile uint16_t ADC1_Value[7];
static int get_tick_count(unsigned long *t) { *t = mock_now; return 0; }
static uint8_t NRF_GetIoError(void) { return mock_error; }
static uint32_t NRF_GetConfigGeneration(void) { return mock_generation; }
static void NRF_TxCancel(void) {}
static uint8_t NRF_TxPoll(void) { return mock_result; }
static uint8_t NRF_TxStart(uint8_t *p) { memcpy(sent,p,32); sent_count++; return 1U; }
#define DMA_GetFlagStatus(stream, flags) ((mock_flags & (flags)) != 0U)
#define DMA_ClearFlag(stream, flags) (mock_flags &= ~(flags))
static uint8_t digital_channel_get_stable(uint8_t i) { return i ? 1U : mock_button; }
static uint8_t GPIO_ReadInputDataBit(unsigned p, unsigned pin) { (void)p;(void)pin;return mock_raw; }
/* PRODUCTION_SOURCE */

static RobotControlCommand step(uint32_t delta, uint8_t page)
{
    RobotControlCommand decoded;
    mock_now += delta; mock_flags = 1U;
    control_link_service(page);
    assert(robot_packet_decode(sent,&decoded));
    return decoded;
}
static void neutral_ready(void)
{
    unsigned i;
    mock_button = mock_raw = 1U;
    ADC1_Value[1] = ADC1_Value[2] = ADC1_Value[5] = 2000U;
    for(i=0;i<30;i++) assert(!step(20,1).armed);
}
static void arm(void)
{
    neutral_ready(); mock_button = mock_raw = 0U;
    assert(step(20,1).armed);
}
int main(void)
{
    unsigned i;
    uint8_t bytes[32], backup[32];
    RobotControlCommand c = {65535U,-1000,1000,-1800,1000U,1U,1U}, out;
    robot_packet_encode(bytes,&c); assert(robot_packet_decode(bytes,&out));
    assert(out.x==-1000 && out.y==1000 && out.heading==-1800 && out.sequence==65535U);
    memcpy(backup,bytes,32);
    for(i=0;i<32;i++) { bytes[i]^=1U; assert(!robot_packet_decode(bytes,&out)); bytes[i]^=1U; }
    robot_write16(bytes+16,1001U); robot_write16(bytes+30,robot_packet_crc(bytes,30));
    assert(!robot_packet_decode(bytes,&out));
    memcpy(bytes,backup,32); bytes[26]=1U; robot_write16(bytes+30,robot_packet_crc(bytes,30));
    assert(!robot_packet_decode(bytes,&out));
    c.armed=0U; robot_packet_encode(bytes,&c); assert(robot_packet_decode(bytes,&out));
    assert(!out.x && !out.y && !out.heading && !out.limit);
    assert(robot_packet_crc((const uint8_t*)"123456789",9)==0x29b1U);
    memset((void*)&param,0,sizeof(param)); param.NRF_Mode=1U;
    param.warnBatVolt=3.7f; param.batVoltAdjust=1000U;
    for(i=0;i<8;i++) {param.chLower[i]=0U;param.chMiddle[i]=2000U;param.chUpper[i]=4000U;}
    for(i=0;i<7;i++) ADC1_Value[i]=2000U;
    ADC1_Value[6]=2600U;
    control_link_init(1U);
    mock_button=mock_raw=0U;
    for(i=0;i<40;i++) assert(!step(20,1).armed); /* held at boot */
    arm();
    ADC1_Value[2]=4000U; out=step(20,1); assert(out.armed && out.x==-1000);
    param.chReverse[2]=1U; assert(step(20,1).x==1000); param.chReverse[2]=0U;
    mock_raw=1U; assert(!step(20,1).armed); /* raw release, before debounce */
    arm(); assert(!step(20,0).armed); /* leaving control page */
    for(i=0;i<30;i++) assert(!step(20,1).armed); /* held across re-entry */
    arm(); assert(!step(101,1).armed); /* main-loop stall */
    arm(); mock_now+=20;mock_flags=0;control_link_service(1);
    assert(robot_packet_decode(sent,&out) && !out.armed); /* DMA stopped */
    arm(); ADC1_Value[6]=2000U; assert(!step(20,1).armed);ADC1_Value[6]=2600U;
    arm(); mock_result=0U; for(i=0;i<9;i++) out=step(20,1);
    assert(!out.armed); mock_result=TX_DS;
    for(i=0;i<30;i++) assert(!step(20,1).armed); /* held across reconnect */
    arm(); mock_error=1U; step(20,1); assert(!safety.armed); mock_error=0U;
    arm(); mock_result=0U; for(i=0;i<7;i++) step(20,1);
    mock_result=TX_DS; assert(!step(20,1).armed); /* recovered ACK after timeout */
    arm(); param.NRF_Mode=0U; mock_now++; control_link_service(1U);
    param.NRF_Mode=1U; assert(!step(20,1).armed); /* off/on between TX slots */
    arm(); mock_now++; control_link_service(0U); assert(!step(20,1).armed);
    arm(); mock_generation++; assert(!step(20,1).armed); /* radio reconfigured */
    control_link_init(0U); neutral_ready();mock_button=mock_raw=0U;assert(!step(20,1).armed);
    mock_now=UINT32_MAX-300U;control_link_init(1U); arm();assert(safety.armed);
    assert(sent_count>0U);
    puts("control protocol/arming/ACK loss/DMA freeze/battery/stall/rollover: PASS");
    return 0;
}
