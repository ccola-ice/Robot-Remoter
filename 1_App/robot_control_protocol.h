#ifndef ROBOT_CONTROL_PROTOCOL_H
#define ROBOT_CONTROL_PROTOCOL_H
#include <stdint.h>
#include <string.h>

#define ROBOT_PACKET_SIZE 32U
#define ROBOT_LINK_TIMEOUT_MS 200U
#define ROBOT_FLAG_ARMED 1U

typedef struct {
    uint16_t sequence;
    int16_t x, y, heading; /* 前两项范围为 ±1000；第三项为 ±1800，单位为 0.1 度。 */
    uint16_t limit;       /* 相对接收端配置的轮速上限，取值 0..1000，表示千分比。 */
    uint16_t digital;     /* 低六位对应 DCH1..6，输入为低电平有效。 */
    uint8_t armed;
} RobotControlCommand;

static __inline uint16_t robot_read16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static __inline void robot_write16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8); p[1] = (uint8_t)value;
}
static __inline uint16_t robot_packet_crc(const uint8_t *p, uint8_t count)
{
    uint16_t crc = 0xffffU;
    uint8_t i;
    while(count--) {
        crc ^= (uint16_t)*p++ << 8;
        for(i = 0U; i < 8U; i++)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000U) ? 0x1021U : 0U));
    }
    return crc;
}
static __inline void robot_packet_encode(uint8_t *p, const RobotControlCommand *c)
{
    memset(p, 0, ROBOT_PACKET_SIZE);
    /* 旧协议的模式和速度上限字段保持为零；本帧不作为旧协议控制命令。 */
    p[10] = 'R'; p[11] = 'C'; p[12] = 1U;
    p[13] = c->armed ? ROBOT_FLAG_ARMED : 0U;
    robot_write16(p + 14, c->sequence);
    if(c->armed) {
        robot_write16(p + 16, (uint16_t)c->x);
        robot_write16(p + 18, (uint16_t)c->y);
        robot_write16(p + 20, (uint16_t)c->heading);
        robot_write16(p + 22, c->limit);
    }
    robot_write16(p + 24, c->digital);
    robot_write16(p + 30, robot_packet_crc(p, 30U));
}
static __inline uint8_t robot_packet_decode(const uint8_t *p, RobotControlCommand *c)
{
    uint8_t i;
    RobotControlCommand decoded;
    if(!p || !c || p[10] != 'R' || p[11] != 'C' || p[12] != 1U ||
       (p[13] & ~ROBOT_FLAG_ARMED) != 0U ||
       robot_read16(p + 30) != robot_packet_crc(p, 30U)) return 0U;
    for(i = 0U; i < 10U; i++) if(p[i]) return 0U;
    for(i = 26U; i < 30U; i++) if(p[i]) return 0U;
    decoded.armed = p[13]; decoded.sequence = robot_read16(p + 14);
    decoded.x = (int16_t)robot_read16(p + 16);
    decoded.y = (int16_t)robot_read16(p + 18);
    decoded.heading = (int16_t)robot_read16(p + 20);
    decoded.limit = robot_read16(p + 22);
    decoded.digital = robot_read16(p + 24);
    if(decoded.x < -1000 || decoded.x > 1000 || decoded.y < -1000 ||
       decoded.y > 1000 || decoded.heading < -1800 || decoded.heading > 1800 ||
       decoded.limit > 1000U || decoded.digital > 63U) return 0U;
    if(!decoded.armed && (decoded.x || decoded.y || decoded.heading || decoded.limit))
        return 0U;
    if(decoded.armed && (!(decoded.digital & 1U) || !decoded.limit)) return 0U;
    *c = decoded;
    return 1U;
}
#endif
