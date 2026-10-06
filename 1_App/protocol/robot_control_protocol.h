#ifndef ROBOT_CONTROL_PROTOCOL_H
#define ROBOT_CONTROL_PROTOCOL_H
#include <stdint.h>
#include <string.h>

/* RC v1 固定 32 字节：0..9 和 26..29 为零，10..13 是标识/版本/标志，
 * 14..25 是序号及控制字段，30..31 存放前 30 字节的 CRC。 */
#define ROBOT_PACKET_SIZE 32U
/* 接收端控制超时约定，与发射端用于撤销使能的 ACK 超时门槛分别维护。 */
#define ROBOT_LINK_TIMEOUT_MS 200U
#define ROBOT_FLAG_ARMED 1U

/* 内存命令对象由编码器转换为线上布局，不能直接将结构体作为无线载荷发送。
 * sequence 用于区分新旧命令；heading 是相对本次使能时航向的目标偏移，不是转向角速度。 */
typedef struct {
    uint16_t sequence;
    int16_t x, y, heading; /* 前两项范围为 ±1000；第三项为 ±1800，单位为 0.1 度。 */
    uint16_t limit;        /* 相对接收端配置的轮速上限，取值 0..1000，表示千分比。 */
    uint16_t digital;      /* 低六位对应 DCH1..6，输入为低电平有效。 */
    uint8_t armed;
} RobotControlCommand;

/* 线上字段显式按大端读写，避免依赖处理器字节序和结构体对齐方式。 */
static __inline uint16_t robot_read16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static __inline void robot_write16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8); p[1] = (uint8_t)value;
}
/* CRC 初值为 0xffff、多项式为 0x1021；帧末两字节校验前 30 字节。 */
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
/* 编码到至少 32 字节的有效缓冲区；调用方负责保证 p/c 非空、互不重叠且字段合法。
 * 此函数不返回错误，也不裁剪越界值；未使能命令的运动字段会被强制编码为零。 */
static __inline void robot_packet_encode(uint8_t *p, const RobotControlCommand *c)
{
    memset(p, 0, ROBOT_PACKET_SIZE);
    /* 旧协议的模式和速度上限字段保持为零；本帧不作为旧协议控制命令。 */
    p[10] = 'R'; p[11] = 'C'; p[12] = 1U;
    p[13] = c->armed ? ROBOT_FLAG_ARMED : 0U;
    robot_write16(p + 14, c->sequence);
    /* 未使能时运动字段保持为零，序号和数字通道仍正常传送。 */
    if(c->armed) {
        robot_write16(p + 16, (uint16_t)c->x);
        robot_write16(p + 18, (uint16_t)c->y);
        robot_write16(p + 20, (uint16_t)c->heading);
        robot_write16(p + 22, c->limit);
    }
    robot_write16(p + 24, c->digital);
    robot_write16(p + 30, robot_packet_crc(p, 30U));
}
/* 解码完整的 32 字节帧，成功返回 1 并写入 c，失败返回 0 且保留原输出。
 * 接口不接收长度，调用方必须先确认包长；序号新旧与超时处理由上层负责。 */
static __inline uint8_t robot_packet_decode(const uint8_t *p, RobotControlCommand *c)
{
    uint8_t i;
    RobotControlCommand decoded;
    /* 先校验帧格式、保留位和 CRC，再检查控制约束；失败时不覆盖调用方的命令。 */
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
    /* 停止帧必须没有运动量；使能帧同时要求 DCH1 有效和非零速度上限。 */
    if(!decoded.armed && (decoded.x || decoded.y || decoded.heading || decoded.limit))
        return 0U;
    if(decoded.armed && (!(decoded.digital & 1U) || !decoded.limit)) return 0U;
    *c = decoded;
    return 1U;
}
#endif
