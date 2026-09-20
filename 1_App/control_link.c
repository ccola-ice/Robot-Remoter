#include "control_link.h"
#include "control_safety.h"
#include "channel_input.h"
#include "robot_control_protocol.h"
#include "param.h"
#include "bsp_spi_nrf.h"
#include "bsp_SysTick.h"
#include "bsp_gpio_digital_channel.h"
#include "bsp_adc1_independent_dual.h"

extern volatile uint16_t ADC1_Value[NUM_OF_ADC1CHANNEL];
static ControlSafety safety;
static uint32_t last_send, last_service, last_ack;
static uint32_t radio_generation;
static uint16_t sequence;
static uint8_t boot_ok, busy, ack_seen;
static const char *status_text = "BOOT LOCK";
static ControlLinkSnapshot monitor;
static uint32_t monitor_sample_ms, monitor_tx_ms;

static int16_t control_axis(uint16_t raw, uint8_t channel)
{
    return channel_input_normalize(raw, param.chLower[channel],
        param.chMiddle[channel], param.chUpper[channel],
        param.PWMadjustValue[channel], param.chReverse[channel]);
}

void control_link_init(uint8_t boot_permitted)
{
    unsigned long now;
    get_tick_count(&now);
    memset(&safety, 0, sizeof(safety));
    memset(&monitor, 0, sizeof(monitor));
    status_text = "BOOT LOCK";
    last_send = last_service = (uint32_t)now;
    sequence = 0U; busy = ack_seen = 0U; boot_ok = boot_permitted;
    radio_generation = NRF_GetConfigGeneration();
}

const char *control_link_status(void) { return status_text; }

static uint16_t control_monitor_age(uint32_t now, uint32_t then, uint8_t valid)
{
    uint32_t age = now - then;
    return !valid || age > 65535UL ? 65535U : (uint16_t)age;
}

void control_link_get_snapshot(ControlLinkSnapshot *snapshot)
{
    unsigned long now;
    if(!snapshot) return;
    get_tick_count(&now);
    *snapshot = monitor;
    snapshot->sample_age_ms = control_monitor_age((uint32_t)now, monitor_sample_ms, monitor.sampled);
    snapshot->tx_age_ms = control_monitor_age((uint32_t)now, monitor_tx_ms, monitor.sent);
    snapshot->ack_age_ms = control_monitor_age((uint32_t)now, last_ack, ack_seen);
    snapshot->ack_seen = ack_seen;
}

void control_link_inhibit(void)
{
    memset(&safety, 0, sizeof(safety));
    ack_seen = 0U;
    if(busy) NRF_TxCancel();
    busy = 0U;
}

void control_link_service(uint8_t control_page)
{
    unsigned long tick;
    uint32_t now, gap;
    uint8_t result, channel, fresh, healthy, pressed, neutral, link_ok;
    uint16_t sample[NUM_OF_ADC1CHANNEL];
    float voltage;
    uint8_t packet[ROBOT_PACKET_SIZE];
    RobotControlCommand command;
    get_tick_count(&tick); now = (uint32_t)tick;
    /* Record loss BEFORE polling a newly recovered ACK. A late success may
     * restore the link but must not preserve the previous armed session. */
    if(ack_seen && (uint32_t)(now - last_ack) >= 150U)
        control_link_inhibit();
    if(radio_generation != NRF_GetConfigGeneration()) {
        control_link_inhibit();
        radio_generation = NRF_GetConfigGeneration();
    }
    if(!param.NRF_Mode) control_link_inhibit();
    if(!control_page || (uint32_t)(now - last_service) > 100U)
        memset(&safety, 0, sizeof(safety));
    if(busy) {
        result = NRF_TxPoll();
        if(result != NRF_TX_PENDING) {
            busy = 0U;
            if(result == TX_DS) { last_ack = now; ack_seen = 1U; monitor.tx_acked++; }
            else monitor.tx_failed++;
        }
    }
    if((uint32_t)(now - last_send) < 20U) return;
    gap = now - last_service; last_service = last_send = now;
    fresh = DMA_GetFlagStatus(DUAL_ADC1_DMA_STREAM, DMA_FLAG_TCIF4) != RESET;
    if(DMA_GetFlagStatus(DUAL_ADC1_DMA_STREAM,
       DMA_FLAG_TEIF4 | DMA_FLAG_DMEIF4 | DMA_FLAG_FEIF4) != RESET ||
       !(DUAL_ADC1_DMA_STREAM->CR & DMA_SxCR_EN)) fresh = 0U;
    DMA_ClearFlag(DUAL_ADC1_DMA_STREAM, DMA_FLAG_TCIF4);
    for(channel = 0U; channel < NUM_OF_ADC1CHANNEL; channel++) {
        sample[channel] = ADC1_Value[channel];
        if(sample[channel] > 4095U) fresh = 0U;
    }
    monitor.sampled = 1U;
    monitor.input_fresh = fresh;
    monitor_sample_ms = now;
    for(channel = 0U; channel < 6U; channel++) {
        monitor.raw[channel] = sample[channel];
        monitor.calibrated[channel] = control_axis(sample[channel], channel);
    }
    memset(&command, 0, sizeof(command));
    command.x = -control_axis(sample[2], 2U);
    command.y = control_axis(sample[1], 1U);
    command.heading = (int16_t)(-control_axis(sample[5], 5U) * 18L / 10L);
    command.limit = 1000U;
    for(channel = 0U; channel < DIGITAL_CHANNEL_COUNT; channel++)
        if(digital_channel_get_stable(channel) == 0U)
            command.digital |= (uint16_t)(1U << channel);
    /* Release acts on the physical input immediately; pressing is debounced. */
    pressed = ((command.digital & 1U) != 0U &&
               GPIO_ReadInputDataBit(DCH1_GPIO_PORT, DCH1_GPIO_PIN) == Bit_RESET);
    if(!pressed) command.digital &= (uint16_t)~1U;
    neutral = !command.x && !command.y && !command.heading;
    voltage = (float)sample[6] * (6.6f / 4095.0f) * (float)param.batVoltAdjust / 1000.0f;
    link_ok = ack_seen && (uint32_t)(now - last_ack) < 150U;
    healthy = boot_ok && fresh && control_page && param.NRF_Mode &&
              gap <= 100U && link_ok && voltage >= param.warnBatVolt &&
              !NRF_GetIoError();
    command.armed = control_safety_step(&safety, now, healthy, pressed, neutral);
    if(!boot_ok) status_text = "BOOT LOCK";
    else if(!param.NRF_Mode) status_text = "RADIO OFF";
    else if(!fresh || gap > 100U) status_text = "INPUT STALE";
    else if(voltage < param.warnBatVolt) status_text = "LOW TX BATTERY";
    else if(!control_page) status_text = "STOP / OPEN CONTROL PAGE";
    else if(!link_ok || NRF_GetIoError()) status_text = "NO RADIO ACK";
    else if(command.armed) status_text = "ENABLED / HOLD DCH1";
    else if(safety.ready) status_text = "READY / PRESS DCH1";
    else status_text = "RELEASE DCH1 / CENTER STICKS";
    if(param.NRF_Mode && !busy && !NRF_GetIoError()) {
        command.sequence = sequence++;
        robot_packet_encode(packet, &command);
        busy = NRF_TxStart(packet);
        if(busy) {
            /* Decode the encoded packet: disarmed wire values are all zero. */
            (void)robot_packet_decode(packet, &monitor.transmitted);
            monitor.sent = 1U;
            monitor_tx_ms = now;
            monitor.tx_started++;
        }
    }
}
