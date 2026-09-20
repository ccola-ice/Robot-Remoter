#ifndef CONTROL_LINK_H
#define CONTROL_LINK_H
#include <stdint.h>
#include "robot_control_protocol.h"

typedef struct ControlLinkSnapshot {
    uint16_t raw[6];
    int16_t calibrated[6];
    RobotControlCommand transmitted;
    uint32_t tx_started, tx_acked, tx_failed;
    uint16_t sample_age_ms, tx_age_ms, ack_age_ms;
    uint8_t sampled, input_fresh, sent, ack_seen;
} ControlLinkSnapshot;

void control_link_init(uint8_t boot_permitted);
void control_link_service(uint8_t control_page);
void control_link_inhibit(void);
const char *control_link_status(void);
/* Main-loop observer only: never polls/consumes the radio or ADC DMA flags. */
void control_link_get_snapshot(ControlLinkSnapshot *snapshot);
#endif
