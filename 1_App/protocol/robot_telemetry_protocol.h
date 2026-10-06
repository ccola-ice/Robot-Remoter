#ifndef ROBOT_TELEMETRY_PROTOCOL_H
#define ROBOT_TELEMETRY_PROTOCOL_H

/* 仅实现 RT v1 编解码，尚未接入接收端发布、NRF 传输和 GUI 数据汇总。
 * 保持 RC v1 控制帧不变。
 * 线上整数采用大端字节序，不直接传输紧凑结构体或浮点数。
 * 会话、时效和有效性规则见 docs/通信协议设计.md。 */
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

/* valid 位掩码只在对应帧类型内解释，不同类型可复用同一位；flags 则是所有类型共用的标志。 */
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

/* 各类遥测独立过期，收到状态帧不能为旧姿态或旧位置续期；编解码函数不会自动计时。 */
#define ROBOT_TELEMETRY_STATUS_TIMEOUT_MS 1000U
#define ROBOT_TELEMETRY_MOTION_TIMEOUT_MS 300U
#define ROBOT_TELEMETRY_POSITION_TIMEOUT_MS 1000U
#define ROBOT_TELEMETRY_GNSS_TIMEOUT_MS 2000U

/* 接收端应用状态：电压用 mV，电量用百分比，控制命令年龄用 ms。
 * 两项计数分别是应用拒绝帧数和接受序号的缺口累计，不能直接解释为无线丢包率。 */
typedef struct {
    uint16_t voltage_mv;
    uint8_t battery_percent, state;
    uint16_t faults, control_age_ms, rejected_frames, missing_commands;
} RobotTelemetryStatus;

/* 车体运动量：速度 mm/s，姿态角 0.1 度，去重力线加速度 mm/s²。
 * 每组三轴数值共用一个有效位，生产者应先完成单位与坐标转换。 */
typedef struct {
    int16_t speed_mm_s;
    int16_t roll_ddeg, pitch_ddeg, yaw_ddeg;
    int16_t acceleration_x_mm_s2, acceleration_y_mm_s2, acceleration_z_mm_s2;
} RobotTelemetryMotion;

/* 本会话建立时的本地固定坐标，位置以 mm 表示；有效时 frame 必须为 FRAME_LOCAL。
 * 重设位置原点需要建立新会话，不能在同一会话内混用不同原点。 */
typedef struct {
    int32_t x_mm, y_mm, z_mm;
    uint8_t frame;
} RobotTelemetryPosition;

/* 全球经纬度使用 1e-7 度整数，海拔使用 mm；与本地位置分开传输。
 * 未定位时仍可报告定位类型和卫星数，经纬度、海拔是否可用分别看有效位。 */
typedef struct {
    int32_t latitude_e7, longitude_e7, altitude_mm;
    uint8_t fix, satellites; /* 定位类型：0=未定位，2=二维定位，3=三维定位。 */
} RobotTelemetryGnss;

/* 解码后的公共头和互斥载荷：type 决定读取哪个 union 成员，valid 指定其中的有效字段。
 * sequence 在每个 type 内独立递增；accepted_control_sequence 为最近接受的 RC 序号，
 * 它是否有效由 flags 决定，数值为零本身不是无效标记。 */
typedef struct {
    uint8_t type, valid, flags;
    uint16_t sequence, accepted_control_sequence;
    uint32_t session; /* 非零、单调递增的启动纪元编号。 */
    union {
        RobotTelemetryStatus status;
        RobotTelemetryMotion motion;
        RobotTelemetryPosition position;
        RobotTelemetryGnss gnss;
    } data;
} RobotTelemetryFrame;

/* 这些基础读写函数只转换字节序，不检查指针或长度；边界由整帧入口统一验证。 */
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
    /* 显式还原补码负数，避免依赖超出有符号范围的强制转换行为。 */
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
/* 与 RC 控制帧使用相同的 CRC 参数，整帧调用时仅覆盖公共头及载荷，不包含末尾 CRC。 */
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

/* 使用半范围序号运算：序号相等或相差半个计数范围时，均视为旧帧。
 * 必须先接受会话切换，再重置各帧类型的序号状态。 */
static __inline uint8_t robot_telemetry_sequence_newer(uint16_t next, uint16_t previous)
{
    uint16_t delta = (uint16_t)(next - previous);
    return delta != 0U && delta < 0x8000U;
}
/* 判断非零候选会话是否在前一会话之后；相等、倒退或恰好相隔半圈均返回 0。
 * 只提供数值比较，上层仍需决定哪些帧允许建立或切换会话。 */
static __inline uint8_t robot_telemetry_session_newer(uint32_t next, uint32_t previous)
{
    uint32_t delta = next - previous;
    return next != 0U && delta != 0U && delta < 0x80000000UL;
}
/* now/received/timeout_ms 均以毫秒计，年龄严格小于有效超时门槛才返回 1。
 * 调用方必须另行确认已收到过样本；计算的经过时间必须小于
 * uint32_t 时钟完成一次回绕的时间，超时门槛为零或超出半范围时返回 0。 */
static __inline uint8_t robot_telemetry_is_fresh(uint32_t now, uint32_t received,
                                               uint32_t timeout_ms)
{
    return timeout_ms != 0U && timeout_ms < 0x80000000UL &&
           (uint32_t)(now - received) < timeout_ms;
}

/* 检查内存帧的字段范围与关联条件，合法返回 1，空指针或非法字段返回 0。
 * 未声明有效的字段须清零，定位维度和控制状态还需满足依赖条件；不检查序号新旧或时效。 */
static __inline uint8_t robot_telemetry_valid(const RobotTelemetryFrame *f)
{
    if(!f || f->session == 0U || (f->flags & ~ROBOT_TELEMETRY_CONTROL_VALID) != 0U ||
       (!(f->flags & ROBOT_TELEMETRY_CONTROL_VALID) && f->accepted_control_sequence != 0U))
        return 0U;
    switch(f->type) {
    case ROBOT_TELEMETRY_STATUS: {
        /* 控制年龄和 ARMED 都要求存在有效的已接受控制序号；状态与故障位作为一组。 */
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
        /* 姿态角受 ±180 度约束，未声明有效的速度或三轴分组不能携带残留值。 */
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
        /* 坐标有效时必须说明本地参考系；整组无效时连参考系编号也清零。 */
        const RobotTelemetryPosition *p = &f->data.position;
        if((f->valid & ~ROBOT_TELEMETRY_POSITION_VALID) != 0U) return 0U;
        if(f->valid) {
            if(p->frame != ROBOT_TELEMETRY_FRAME_LOCAL) return 0U;
        } else if(p->x_mm != 0 || p->y_mm != 0 || p->z_mm != 0 || p->frame != 0U)
            return 0U;
        break;
    }
    case ROBOT_TELEMETRY_GNSS: {
        /* 经纬度依赖二维及以上定位，海拔依赖三维定位；数值合法与定位有效需同时成立。 */
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

/* 将合法内存帧编码到 out，成功返回 1；编码和解码均要求 length 恰好为 32 字节。
 * 任何失败都返回 0 且不修改目标缓冲区，包括空指针或不支持的帧类型。 */
static __inline uint8_t robot_telemetry_encode(uint8_t *out, size_t length,
                                             const RobotTelemetryFrame *f)
{
    uint8_t p[ROBOT_TELEMETRY_SIZE];
    if(!out || length != ROBOT_TELEMETRY_SIZE || !robot_telemetry_valid(f)) return 0U;
    /* 先在临时帧清零保留区；0..13 为公共头，14..29 为按类型解释的载荷。 */
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

/* 先检查完整帧格式与 CRC，再恢复字段并做语义校验；仅全部通过才写入 out 并返回 1。
 * 失败返回 0 且保留 out 原值；解码成功也仍须由上层检查会话、序号与接收时效。 */
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
    /* 各帧类型占用的载荷长度不同，未使用字节必须为零，不能把保留区解释为数据。 */
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
