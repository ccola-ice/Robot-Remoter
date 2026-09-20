#ifndef ROBOT_TELEMETRY_PROTOCOL_H
#define ROBOT_TELEMETRY_PROTOCOL_H

/* RT v1 codec only. Receiver publishing, NRF transport and GUI aggregation
 * are intentionally not connected. RC v1 control frames remain unchanged.
 * Wire integers are big endian; no packed structs or floating point on wire.
 * See docs/通信协议设计.md for session, freshness and validity rules. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ROBOT_TELEMETRY_SIZE 32U
#define ROBOT_TELEMETRY_VERSION 1U
#define ROBOT_TELEMETRY_CONTROL_VALID 0x01U

#define ROBOT_TELEMETRY_STATUS 1U
#define ROBOT_TELEMETRY_MOTION 2U
#define ROBOT_TELEMETRY_POSITION 3U
#define ROBOT_TELEMETRY_GNSS 4U

#define ROBOT_TELEMETRY_VOLTAGE_VALID 0x01U
#define ROBOT_TELEMETRY_PERCENT_VALID 0x02U
#define ROBOT_TELEMETRY_STATE_VALID 0x04U
#define ROBOT_TELEMETRY_CONTROL_AGE_VALID 0x08U
#define ROBOT_TELEMETRY_COUNTERS_VALID 0x10U
#define ROBOT_TELEMETRY_SPEED_VALID 0x01U
#define ROBOT_TELEMETRY_ATTITUDE_VALID 0x02U
#define ROBOT_TELEMETRY_ACCEL_VALID 0x04U
#define ROBOT_TELEMETRY_POSITION_VALID 0x01U
#define ROBOT_TELEMETRY_FIX_VALID 0x01U
#define ROBOT_TELEMETRY_COORD_VALID 0x02U
#define ROBOT_TELEMETRY_ALTITUDE_VALID 0x04U

#define ROBOT_TELEMETRY_BOOT_LOCK 0U
#define ROBOT_TELEMETRY_STOPPED 1U
#define ROBOT_TELEMETRY_READY 2U
#define ROBOT_TELEMETRY_ARMED 3U
#define ROBOT_TELEMETRY_FAILSAFE 4U
#define ROBOT_TELEMETRY_FAULT_LINK 0x01U
#define ROBOT_TELEMETRY_FAULT_IMU 0x02U
#define ROBOT_TELEMETRY_FAULT_BATTERY 0x04U
#define ROBOT_TELEMETRY_FAULT_ESTOP 0x08U
#define ROBOT_TELEMETRY_FAULT_MOTOR 0x10U
#define ROBOT_TELEMETRY_FAULT_BOOT 0x20U
#define ROBOT_TELEMETRY_FRAME_LOCAL 1U

#define ROBOT_TELEMETRY_STATUS_TIMEOUT_MS 1000U
#define ROBOT_TELEMETRY_MOTION_TIMEOUT_MS 300U
#define ROBOT_TELEMETRY_POSITION_TIMEOUT_MS 1000U
#define ROBOT_TELEMETRY_GNSS_TIMEOUT_MS 2000U

typedef struct {
    uint16_t voltage_mv;
    uint8_t battery_percent, state;
    uint16_t faults, control_age_ms, rejected_frames, missing_commands;
} RobotTelemetryStatus;

typedef struct {
    int16_t speed_mm_s;
    int16_t roll_ddeg, pitch_ddeg, yaw_ddeg;
    int16_t acceleration_x_mm_s2, acceleration_y_mm_s2, acceleration_z_mm_s2;
} RobotTelemetryMotion;

typedef struct {
    int32_t x_mm, y_mm, z_mm;
    uint8_t frame;
} RobotTelemetryPosition;

typedef struct {
    int32_t latitude_e7, longitude_e7, altitude_mm;
    uint8_t fix, satellites; /* fix: 0=none, 2=2D, 3=3D */
} RobotTelemetryGnss;

typedef struct {
    uint8_t type, valid, flags;
    uint16_t sequence, accepted_control_sequence;
    uint32_t session; /* Nonzero monotonically increasing boot epoch. */
    union {
        RobotTelemetryStatus status;
        RobotTelemetryMotion motion;
        RobotTelemetryPosition position;
        RobotTelemetryGnss gnss;
    } data;
} RobotTelemetryFrame;

static __inline uint16_t robot_telemetry_read16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static __inline uint32_t robot_telemetry_read32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}
static __inline int16_t robot_telemetry_read_i16(const uint8_t *p)
{
    uint16_t n = robot_telemetry_read16(p);
    return n <= 32767U ? (int16_t)n : (int16_t)(-1 - (int32_t)(65535U - n));
}
static __inline int32_t robot_telemetry_read_i32(const uint8_t *p)
{
    uint32_t n = robot_telemetry_read32(p);
    return n <= 0x7fffffffUL ? (int32_t)n : -1 - (int32_t)(0xffffffffUL - n);
}
static __inline void robot_telemetry_write16(uint8_t *p, uint16_t n)
{
    p[0] = (uint8_t)(n >> 8); p[1] = (uint8_t)n;
}
static __inline void robot_telemetry_write32(uint8_t *p, uint32_t n)
{
    p[0] = (uint8_t)(n >> 24); p[1] = (uint8_t)(n >> 16);
    p[2] = (uint8_t)(n >> 8); p[3] = (uint8_t)n;
}
static __inline uint16_t robot_telemetry_crc(const uint8_t *p, size_t length)
{
    uint16_t crc = 0xffffU;
    uint8_t bit;
    while(length-- != 0U) {
        crc ^= (uint16_t)*p++ << 8;
        for(bit = 0U; bit < 8U; ++bit)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000U) ? 0x1021U : 0U));
    }
    return crc;
}

/* Half-range serial arithmetic: equality and the ambiguous half turn are old.
 * A session change must be accepted before resetting per-type sequence state. */
static __inline uint8_t robot_telemetry_sequence_newer(uint16_t next, uint16_t previous)
{
    uint16_t delta = (uint16_t)(next - previous);
    return delta != 0U && delta < 0x8000U;
}
static __inline uint8_t robot_telemetry_session_newer(uint32_t next, uint32_t previous)
{
    uint32_t delta = next - previous;
    return next != 0U && delta != 0U && delta < 0x80000000UL;
}
/* Caller must separately know that a sample was received. Elapsed intervals
 * must be less than one complete uint32_t clock wrap. */
static __inline uint8_t robot_telemetry_is_fresh(uint32_t now, uint32_t received,
                                               uint32_t timeout_ms)
{
    return timeout_ms != 0U && timeout_ms < 0x80000000UL &&
           (uint32_t)(now - received) < timeout_ms;
}

static __inline uint8_t robot_telemetry_valid(const RobotTelemetryFrame *f)
{
    if(!f || f->session == 0U || (f->flags & ~ROBOT_TELEMETRY_CONTROL_VALID) != 0U ||
       (!(f->flags & ROBOT_TELEMETRY_CONTROL_VALID) && f->accepted_control_sequence != 0U))
        return 0U;
    switch(f->type) {
    case ROBOT_TELEMETRY_STATUS: {
        const RobotTelemetryStatus *s = &f->data.status;
        if((f->valid & ~0x1fU) != 0U || s->battery_percent > 100U ||
           s->state > ROBOT_TELEMETRY_FAILSAFE || (s->faults & ~0x3fU) != 0U ||
           (!(f->valid & ROBOT_TELEMETRY_VOLTAGE_VALID) && s->voltage_mv != 0U) ||
           (!(f->valid & ROBOT_TELEMETRY_PERCENT_VALID) && s->battery_percent != 0U) ||
           (!(f->valid & ROBOT_TELEMETRY_STATE_VALID) && (s->state != 0U || s->faults != 0U)) ||
           (!(f->valid & ROBOT_TELEMETRY_CONTROL_AGE_VALID) && s->control_age_ms != 0U) ||
           ((f->valid & ROBOT_TELEMETRY_CONTROL_AGE_VALID) &&
            !(f->flags & ROBOT_TELEMETRY_CONTROL_VALID)) ||
           (s->state == ROBOT_TELEMETRY_ARMED && !(f->flags & ROBOT_TELEMETRY_CONTROL_VALID)) ||
           (!(f->valid & ROBOT_TELEMETRY_COUNTERS_VALID) &&
            (s->rejected_frames != 0U || s->missing_commands != 0U))) return 0U;
        break;
    }
    case ROBOT_TELEMETRY_MOTION: {
        const RobotTelemetryMotion *m = &f->data.motion;
        if((f->valid & ~0x07U) != 0U ||
           m->roll_ddeg < -1800 || m->roll_ddeg > 1800 ||
           m->pitch_ddeg < -1800 || m->pitch_ddeg > 1800 ||
           m->yaw_ddeg < -1800 || m->yaw_ddeg > 1800 ||
           (!(f->valid & ROBOT_TELEMETRY_SPEED_VALID) && m->speed_mm_s != 0) ||
           (!(f->valid & ROBOT_TELEMETRY_ATTITUDE_VALID) &&
            (m->roll_ddeg != 0 || m->pitch_ddeg != 0 || m->yaw_ddeg != 0)) ||
           (!(f->valid & ROBOT_TELEMETRY_ACCEL_VALID) &&
            (m->acceleration_x_mm_s2 != 0 || m->acceleration_y_mm_s2 != 0 ||
             m->acceleration_z_mm_s2 != 0))) return 0U;
        break;
    }
    case ROBOT_TELEMETRY_POSITION: {
        const RobotTelemetryPosition *p = &f->data.position;
        if((f->valid & ~ROBOT_TELEMETRY_POSITION_VALID) != 0U) return 0U;
        if(f->valid) {
            if(p->frame != ROBOT_TELEMETRY_FRAME_LOCAL) return 0U;
        } else if(p->x_mm != 0 || p->y_mm != 0 || p->z_mm != 0 || p->frame != 0U)
            return 0U;
        break;
    }
    case ROBOT_TELEMETRY_GNSS: {
        const RobotTelemetryGnss *g = &f->data.gnss;
        if((f->valid & ~0x07U) != 0U || (g->fix != 0U && g->fix != 2U && g->fix != 3U) ||
           g->latitude_e7 < -900000000L || g->latitude_e7 > 900000000L ||
           g->longitude_e7 < -1800000000L || g->longitude_e7 > 1800000000L ||
           (!(f->valid & ROBOT_TELEMETRY_FIX_VALID) && (g->fix != 0U || g->satellites != 0U)) ||
           (!(f->valid & ROBOT_TELEMETRY_COORD_VALID) &&
            (g->latitude_e7 != 0 || g->longitude_e7 != 0)) ||
           ((f->valid & ROBOT_TELEMETRY_COORD_VALID) &&
            (!(f->valid & ROBOT_TELEMETRY_FIX_VALID) || g->fix < 2U)) ||
           (!(f->valid & ROBOT_TELEMETRY_ALTITUDE_VALID) && g->altitude_mm != 0) ||
           ((f->valid & ROBOT_TELEMETRY_ALTITUDE_VALID) &&
            (!(f->valid & ROBOT_TELEMETRY_FIX_VALID) || g->fix != 3U))) return 0U;
        break;
    }
    default: return 0U;
    }
    return 1U;
}

/* Both directions require exactly 32 bytes and leave the destination intact
 * on every failure, including null arguments and unsupported frame types. */
static __inline uint8_t robot_telemetry_encode(uint8_t *out, size_t length,
                                             const RobotTelemetryFrame *f)
{
    uint8_t p[ROBOT_TELEMETRY_SIZE];
    if(!out || length != ROBOT_TELEMETRY_SIZE || !robot_telemetry_valid(f)) return 0U;
    memset(p, 0, sizeof(p));
    p[0] = 'R'; p[1] = 'T'; p[2] = ROBOT_TELEMETRY_VERSION; p[3] = f->type;
    robot_telemetry_write16(p + 4, f->sequence);
    robot_telemetry_write16(p + 6, f->accepted_control_sequence);
    robot_telemetry_write32(p + 8, f->session);
    p[12] = f->valid; p[13] = f->flags;
    switch(f->type) {
    case ROBOT_TELEMETRY_STATUS: {
        const RobotTelemetryStatus *s = &f->data.status;
        robot_telemetry_write16(p + 14, s->voltage_mv);
        p[16] = s->battery_percent; p[17] = s->state;
        robot_telemetry_write16(p + 18, s->faults);
        robot_telemetry_write16(p + 20, s->control_age_ms);
        robot_telemetry_write16(p + 22, s->rejected_frames);
        robot_telemetry_write16(p + 24, s->missing_commands);
        break;
    }
    case ROBOT_TELEMETRY_MOTION: {
        const RobotTelemetryMotion *m = &f->data.motion;
        robot_telemetry_write16(p + 14, (uint16_t)m->speed_mm_s);
        robot_telemetry_write16(p + 16, (uint16_t)m->roll_ddeg);
        robot_telemetry_write16(p + 18, (uint16_t)m->pitch_ddeg);
        robot_telemetry_write16(p + 20, (uint16_t)m->yaw_ddeg);
        robot_telemetry_write16(p + 22, (uint16_t)m->acceleration_x_mm_s2);
        robot_telemetry_write16(p + 24, (uint16_t)m->acceleration_y_mm_s2);
        robot_telemetry_write16(p + 26, (uint16_t)m->acceleration_z_mm_s2);
        break;
    }
    case ROBOT_TELEMETRY_POSITION: {
        const RobotTelemetryPosition *v = &f->data.position;
        robot_telemetry_write32(p + 14, (uint32_t)v->x_mm);
        robot_telemetry_write32(p + 18, (uint32_t)v->y_mm);
        robot_telemetry_write32(p + 22, (uint32_t)v->z_mm);
        p[26] = v->frame;
        break;
    }
    case ROBOT_TELEMETRY_GNSS: {
        const RobotTelemetryGnss *g = &f->data.gnss;
        robot_telemetry_write32(p + 14, (uint32_t)g->latitude_e7);
        robot_telemetry_write32(p + 18, (uint32_t)g->longitude_e7);
        robot_telemetry_write32(p + 22, (uint32_t)g->altitude_mm);
        p[26] = g->fix; p[27] = g->satellites;
        break;
    }
    default: return 0U;
    }
    robot_telemetry_write16(p + 30, robot_telemetry_crc(p, 30U));
    memcpy(out, p, sizeof(p));
    return 1U;
}

static __inline uint8_t robot_telemetry_decode(const uint8_t *p, size_t length,
                                             RobotTelemetryFrame *out)
{
    RobotTelemetryFrame f;
    if(!p || !out || length != ROBOT_TELEMETRY_SIZE || p[0] != 'R' || p[1] != 'T' ||
       p[2] != ROBOT_TELEMETRY_VERSION ||
       robot_telemetry_read16(p + 30) != robot_telemetry_crc(p, 30U)) return 0U;
    memset(&f, 0, sizeof(f));
    f.type = p[3]; f.sequence = robot_telemetry_read16(p + 4);
    f.accepted_control_sequence = robot_telemetry_read16(p + 6);
    f.session = robot_telemetry_read32(p + 8); f.valid = p[12]; f.flags = p[13];
    switch(f.type) {
    case ROBOT_TELEMETRY_STATUS: {
        RobotTelemetryStatus *s = &f.data.status;
        if(p[26] || p[27] || p[28] || p[29]) return 0U;
        s->voltage_mv = robot_telemetry_read16(p + 14);
        s->battery_percent = p[16]; s->state = p[17];
        s->faults = robot_telemetry_read16(p + 18);
        s->control_age_ms = robot_telemetry_read16(p + 20);
        s->rejected_frames = robot_telemetry_read16(p + 22);
        s->missing_commands = robot_telemetry_read16(p + 24);
        break;
    }
    case ROBOT_TELEMETRY_MOTION: {
        RobotTelemetryMotion *m = &f.data.motion;
        if(p[28] || p[29]) return 0U;
        m->speed_mm_s = robot_telemetry_read_i16(p + 14);
        m->roll_ddeg = robot_telemetry_read_i16(p + 16);
        m->pitch_ddeg = robot_telemetry_read_i16(p + 18);
        m->yaw_ddeg = robot_telemetry_read_i16(p + 20);
        m->acceleration_x_mm_s2 = robot_telemetry_read_i16(p + 22);
        m->acceleration_y_mm_s2 = robot_telemetry_read_i16(p + 24);
        m->acceleration_z_mm_s2 = robot_telemetry_read_i16(p + 26);
        break;
    }
    case ROBOT_TELEMETRY_POSITION: {
        RobotTelemetryPosition *v = &f.data.position;
        if(p[27] || p[28] || p[29]) return 0U;
        v->x_mm = robot_telemetry_read_i32(p + 14);
        v->y_mm = robot_telemetry_read_i32(p + 18);
        v->z_mm = robot_telemetry_read_i32(p + 22); v->frame = p[26];
        break;
    }
    case ROBOT_TELEMETRY_GNSS: {
        RobotTelemetryGnss *g = &f.data.gnss;
        if(p[28] || p[29]) return 0U;
        g->latitude_e7 = robot_telemetry_read_i32(p + 14);
        g->longitude_e7 = robot_telemetry_read_i32(p + 18);
        g->altitude_mm = robot_telemetry_read_i32(p + 22);
        g->fix = p[26]; g->satellites = p[27];
        break;
    }
    default: return 0U;
    }
    if(!robot_telemetry_valid(&f)) return 0U;
    *out = f;
    return 1U;
}
#endif
