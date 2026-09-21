#ifndef __MENU_H_
#define __MENU_H_

#include "stm32f4xx.h"

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
uint8_t menu_control_active(void);

/* 由按键回调调用，仅将事件入队，稍后统一处理。 */
void menu_post_key(MenuKey key);
/* 仅用于方向键长按；输入繁忙时丢弃连发事件，避免排队积压。 */
void menu_post_repeat(MenuKey key);

/* 与 button_ticks() 一起每 10 ms 调用一次。 */
void menu_tick_10ms(void);

/* 非阻塞菜单状态机，由主循环持续调用。 */
void menu_process(void);

/* 后续 NRF 数据包解码可通过此接口发布一帧完整的机器人遥测数据。 */
void menu_robot_telemetry_update(const GuiRobotTelemetry *telemetry);

#endif
