#ifndef CONTROL_SAFETY_H
#define CONTROL_SAFETY_H
#include <stdint.h>

typedef struct {
    uint32_t neutral_since;
    uint8_t neutral_tracking, ready, armed;
} ControlSafety;

/* Faults, leaving the control page and missing ACKs all require re-arming.
 * Holding the enable button across boot/reconnect never arms the vehicle. */
static __inline uint8_t control_safety_step(ControlSafety *s, uint32_t now,
                                          uint8_t healthy, uint8_t pressed,
                                          uint8_t neutral)
{
    if(!healthy) {
        s->neutral_tracking = s->ready = s->armed = 0U;
        return 0U;
    }
    if(!pressed) {
        s->armed = 0U;
        if(neutral) {
            if(!s->neutral_tracking) { s->neutral_since = now; s->neutral_tracking = 1U; }
            if((uint32_t)(now - s->neutral_since) >= 500U) s->ready = 1U;
        } else {
            s->neutral_tracking = s->ready = 0U;
        }
    } else {
        if(!s->armed && s->ready && neutral) s->armed = 1U;
        s->neutral_tracking = s->ready = 0U;
    }
    return s->armed;
}
#endif
