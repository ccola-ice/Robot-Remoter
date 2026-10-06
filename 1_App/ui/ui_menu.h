#ifndef UI_MENU_H
#define UI_MENU_H

#include "stm32f4xx.h"

/* 逻辑按键事件；两个方向键在不同页面用于移动焦点、翻页或调整数值。 */
typedef enum
{
    MENU_KEY_LEFT = 0,
    MENU_KEY_RIGHT,
    MENU_KEY_OK,
    MENU_KEY_BACK
} MenuKey;

typedef struct GuiRobotTelemetry GuiRobotTelemetry;

/* LCD 就绪后初始化菜单。 */
void menu_init(void);
/* 文件读取期间执行后台任务；回调不得重新进入菜单或文件系统。 */
void menu_set_background_service(void (*service)(void));
/* 返回是否位于机器人控制页；它只是运动许可条件之一，不等同于已使能运动。 */
uint8_t menu_control_active(void);

/* 由按键回调调用，仅将事件入队，稍后统一处理。 */
void menu_post_key(MenuKey key);
/* 仅用于方向键长按；输入繁忙时丢弃连发事件，避免排队积压。 */
void menu_post_repeat(MenuKey key);

/* 与 button_ticks() 一起每 10 ms 调用一次。 */
void menu_tick_10ms(void);

/* 由主循环持续推进菜单状态机；独占式诊断页面会阻塞当前调用。 */
/* 事件处理和绘制在主循环执行；耗时文件读取通过后台回调协作服务，不应从中断调用。 */
void menu_process(void);

/* 后续 NRF 数据包解码可通过此接口发布一帧完整的机器人遥测数据。 */
/* 接口复制整个结构并记录接收时刻，调用后无需保留传入对象；空指针保持现状。 */
void menu_robot_telemetry_update(const GuiRobotTelemetry *telemetry);

#endif
