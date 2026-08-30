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

/* Initialize the menu after the LCD is ready. */
void menu_init(void);

/* Called by the button callbacks. The event is queued and handled later. */
void menu_post_key(MenuKey key);

/* Called every 10 ms together with button_ticks(). */
void menu_tick_10ms(void);

/* Non-blocking menu state machine. Call it repeatedly from the main loop. */
void menu_process(void);

#endif
