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
static uint8_t mock_start = 1U;
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
static uint8_t NRF_TxStart(uint8_t *p) { if(!mock_start) return 0U; memcpy(sent,p,32); sent_count++; return 1U; }
#define DMA_GetFlagStatus(stream, flags) ((mock_flags & (flags)) != 0U)
#define DMA_ClearFlag(stream, flags) (mock_flags &= ~(flags))
static uint8_t digital_channel_get_stable(uint8_t i) { return i ? 1U : mock_button; }
static uint8_t GPIO_ReadInputDataBit(unsigned p, unsigned pin) { (void)p;(void)pin;return mock_raw; }
/* PRODUCTION_SOURCE */

static RobotControlCommand step(uint32_t delta, uint8_t page)
{
    RobotControlCommand decoded;
    ControlLinkSnapshot snapshot;
    uint8_t observed[ROBOT_PACKET_SIZE];
    mock_now += delta; mock_flags = 1U;
    control_link_service(page);
    assert(robot_packet_decode(sent,&decoded));
    control_link_get_snapshot(&snapshot);
    assert(snapshot.sent);
    robot_packet_encode(observed, &snapshot.transmitted);
    assert(memcmp(observed, sent, sizeof(observed)) == 0);
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

static void test_monitor_snapshot(void)
{
    ControlLinkSnapshot snapshot;
    unsigned i, sends;
    uint32_t flags;
    mock_now = 100U;
    mock_error = 0U;
    mock_result = TX_DS;
    mock_button = mock_raw = 1U;
    param.NRF_Mode = 1U;
    for(i = 0U; i < 8U; i++) {
        param.chLower[i] = 0U; param.chMiddle[i] = 2000U; param.chUpper[i] = 4000U;
        param.PWMadjustValue[i] = 0; param.chReverse[i] = 0U;
    }
    ADC1_Value[0] = 1000U;
    ADC1_Value[1] = 2010U; /* Inside the control deadband. */
    ADC1_Value[2] = 4000U;
    ADC1_Value[3] = 0U;
    ADC1_Value[4] = 4095U; /* Clamp beyond calibrated endpoint. */
    ADC1_Value[5] = 2500U;
    ADC1_Value[6] = 2600U; /* Battery is excluded from calibrated axes. */
    param.PWMadjustValue[2] = -100;
    param.chReverse[2] = 1U;
    param.chMiddle[5] = 1000U; /* Asymmetric travel. */
    control_link_init(1U);
    control_link_get_snapshot(&snapshot);
    assert(!snapshot.sampled && !snapshot.sent && !snapshot.ack_seen);
    assert(snapshot.sample_age_ms == 65535U && snapshot.tx_age_ms == 65535U && snapshot.ack_age_ms == 65535U);
    mock_start = 0U;
    mock_now += 20U; mock_flags = 1U; control_link_service(0U);
    control_link_get_snapshot(&snapshot);
    assert(!snapshot.sent && snapshot.tx_started == 0U && snapshot.tx_age_ms == 65535U);
    mock_start = 1U;
    step(20U, 0U); /* Monitor is never a control page. */
    control_link_get_snapshot(&snapshot);
    assert(snapshot.sampled && snapshot.input_fresh && snapshot.sent && !snapshot.ack_seen);
    assert(snapshot.calibrated[0] == -500 && snapshot.calibrated[1] == 0);
    assert(snapshot.calibrated[2] == -900 && snapshot.calibrated[3] == -1000);
    assert(snapshot.calibrated[4] == 1000 && snapshot.calibrated[5] == 500);
    assert(snapshot.raw[5] == 2500U && snapshot.tx_started == 1U);
    assert(!snapshot.transmitted.armed && !snapshot.transmitted.x && !snapshot.transmitted.heading);
    sends = sent_count;
    flags = mock_flags;
    mock_now += 7U;
    for(i = 0U; i < 10U; i++) control_link_get_snapshot(&snapshot);
    control_link_get_snapshot(0);
    assert(sent_count == sends && mock_flags == flags && !safety.armed);
    assert(snapshot.sample_age_ms == 7U && snapshot.tx_age_ms == 7U && snapshot.tx_acked == 0U);
    step(20U, 0U);
    control_link_get_snapshot(&snapshot);
    assert(snapshot.ack_seen && snapshot.ack_age_ms == 0U && snapshot.tx_acked == 1U && snapshot.tx_started == 2U);
    mock_result = 0U;
    step(20U, 0U);
    control_link_get_snapshot(&snapshot);
    assert(snapshot.tx_failed == 1U && snapshot.tx_acked == 1U);
    mock_start = 0U;
    step(20U, 0U);
    control_link_get_snapshot(&snapshot);
    assert(snapshot.tx_started == 3U && snapshot.tx_age_ms == 20U);
    mock_start = 1U;
    mock_now += 20U; mock_flags = 0U; control_link_service(0U);
    control_link_get_snapshot(&snapshot);
    assert(!snapshot.input_fresh && snapshot.sample_age_ms == 0U);
    param.NRF_Mode = 0U;
    mock_now++; control_link_service(0U);
    control_link_get_snapshot(&snapshot);
    assert(!snapshot.ack_seen && snapshot.ack_age_ms == 65535U);
    mock_now += 70000U;
    control_link_get_snapshot(&snapshot);
    assert(snapshot.sample_age_ms == 65535U && snapshot.tx_age_ms == 65535U);
    mock_now = UINT32_MAX - 10U;
    param.NRF_Mode = 1U;
    control_link_init(1U);
    step(20U, 0U);
    mock_now += 25U;
    control_link_get_snapshot(&snapshot);
    assert(snapshot.sample_age_ms == 25U && snapshot.tx_age_ms == 25U);
    /* Invalid calibration never appears as a valid deflection. */
    assert(channel_input_normalize(65535U, 0U, 2000U, 4000U, 0, 0U) == 0);
    assert(channel_input_normalize(2000U, 2000U, 2000U, 4000U, 0, 0U) == 0);
    assert(channel_input_normalize(2000U, 0U, 2000U, 4000U, INT32_MAX, 0U) == 0);
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
    test_monitor_snapshot();
    puts("control protocol/arming/ACK loss/DMA freeze/battery/stall/rollover: PASS");
    puts("monitor snapshot: exact transmitted bytes, normalization, read-only access, stale input, ACKs and rollover: PASS");
    return 0;
}
