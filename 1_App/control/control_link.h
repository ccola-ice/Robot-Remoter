#ifndef CONTROL_LINK_H
#define CONTROL_LINK_H
#include <stdint.h>
#include "robot_control_protocol.h"

/* 供界面读取的本机链路快照；raw/calibrated 是输入视图，transmitted 是最近发起的线上帧。
 * sampled/sent 只说明发生过采样或发包，须另看 input_fresh、ack_seen 和事件年龄。 */
typedef struct ControlLinkSnapshot {
    uint16_t raw[6];
    int16_t calibrated[6];
    RobotControlCommand transmitted;
    /* ACK 来自无线硬件，不表示接收端应用已接受命令或执行动作。 */
    uint32_t tx_started, tx_acked, tx_failed;
    /* 单位为毫秒；65535 表示无历史事件，或年龄已超出可表示范围。 */
    uint16_t sample_age_ms, tx_age_ms, ack_age_ms;
    uint8_t sampled, input_fresh, sent, ack_seen;
} ControlLinkSnapshot;

/* 启动后调用一次；boot_permitted 为零时保持启动锁定。 */
void control_link_init(uint8_t boot_permitted);
/* 在前台循环中持续调用；control_page 非零才允许进入运动使能流程。 */
void control_link_service(uint8_t control_page);
/* 立即撤销本机使能、清除 ACK 历史并取消在途发送；不会同步发送停止帧。 */
void control_link_inhibit(void);
/* 返回模块持有的只读状态字符串，由后续服务周期更新。 */
const char *control_link_status(void);
/* 仅供主循环读取状态，不得轮询或清除无线模块及 ADC DMA 的事件标志。 */
void control_link_get_snapshot(ControlLinkSnapshot *snapshot);
#endif
