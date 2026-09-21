#include "multi_button_user.h"
#include "multi_button.h"

#include "stm32f4xx.h"
#include "bsp_gpio_button.h"
#include "bsp_SysTick.h"
#include "menu.h"

static struct Button button_ok;
static struct Button button_back;
static struct Button button_left;
static struct Button button_right;

typedef struct {
    uint32_t pressed_ms, repeated_ms;
    uint8_t active, started;
} NavigationRepeat;

static NavigationRepeat navigation_repeat[2];

static void user_button_repeat_begin(uint8_t direction)
{
    unsigned long now;
    get_tick_count(&now);
    navigation_repeat[direction].pressed_ms = (uint32_t)now;
    navigation_repeat[direction].repeated_ms = (uint32_t)now;
    navigation_repeat[direction].active = 1U;
    navigation_repeat[direction].started = 0U;
}

uint8_t user_BUTTON_repeat_held(uint8_t key)
{
    struct Button *button;
    uint8_t direction;
    if(key == MENU_KEY_LEFT) { direction = 0U; button = &button_left; }
    else if(key == MENU_KEY_RIGHT) { direction = 1U; button = &button_right; }
    else return 0U;
    return navigation_repeat[direction].active &&
        button->button_level == button->active_level &&
        button->hal_button_Level(button->button_id) == button->active_level;
}

static void user_button_repeat_hold(void *btn)
{
    struct Button *button = (struct Button *)btn;
    uint8_t direction = button == &button_left ? 0U : 1U;
    MenuKey key = direction == 0U ? MENU_KEY_LEFT : MENU_KEY_RIGHT;
    NavigationRepeat *repeat = &navigation_repeat[direction];
    unsigned long ticks;
    uint32_t now, elapsed, interval;
    if(!user_BUTTON_repeat_held((uint8_t)key) || button->debounce_cnt != 0U) return;
    get_tick_count(&ticks); now = (uint32_t)ticks;
    elapsed = now - repeat->pressed_ms;
    if(elapsed < 500U) return;
    interval = elapsed >= 1500U ? 60U : 120U;
    if(repeat->started && (uint32_t)(now - repeat->repeated_ms) < interval) return;
    repeat->started = 1U;
    repeat->repeated_ms = now;
    /* At most one attempt per callback, even after a long foreground delay.
     * The menu may drop it while busy; do not accumulate a catch-up backlog. */
    menu_post_repeat(key);
}

static void user_button_repeat_release(void *btn)
{
    navigation_repeat[btn == &button_left ? 0U : 1U].active = 0U;
}

void user_BUTTON_cancel_repeat(void)
{
    memset(navigation_repeat,0,sizeof(navigation_repeat));
}

void user_BUTTON_init(void)
{
    BUTTON_GPIO_Config();
    user_BUTTON_cancel_repeat();

    button_init(&button_ok, read_button_ok_gpio, 0, 0);
    button_init(&button_back, read_button_back_gpio, 0, 1);
    button_init(&button_left, read_button_left_gpio, 0, 2);
    button_init(&button_right, read_button_right_gpio, 0, 3);

    /* Navigation acts on the debounced down edge, without release/click timeout. */
    button_attach(&button_ok, PRESS_DOWN, button_ok_press_down_Handler);
    button_attach(&button_back, PRESS_DOWN, button_back_press_down_Handler);
    button_attach(&button_left, PRESS_DOWN, button_left_press_down_Handler);
    button_attach(&button_right, PRESS_DOWN, button_right_press_down_Handler);
    /* Only ADD/SUB navigation repeats. OK, BACK and the separate DCH inputs
     * retain their existing edge-only behavior and hardware assignments. */
    button_attach(&button_left, LONG_PRESS_START, user_button_repeat_hold);
    button_attach(&button_left, LONG_PRESS_HOLD, user_button_repeat_hold);
    button_attach(&button_left, PRESS_UP, user_button_repeat_release);
    button_attach(&button_right, LONG_PRESS_START, user_button_repeat_hold);
    button_attach(&button_right, LONG_PRESS_HOLD, user_button_repeat_hold);
    button_attach(&button_right, PRESS_UP, user_button_repeat_release);

    button_start(&button_ok);
    button_start(&button_back);
    button_start(&button_left);
    button_start(&button_right);
}

uint8_t read_button_ok_gpio(uint8_t button_id)
{
    (void)button_id;
    return GPIO_ReadInputDataBit(BUTTON_OK_GPIO_PORT, BUTTON_OK_PIN);
}

uint8_t read_button_back_gpio(uint8_t button_id)
{
    (void)button_id;
    return GPIO_ReadInputDataBit(BUTTON_BACK_GPIO_PORT, BUTTON_BACK_PIN);
}

uint8_t read_button_left_gpio(uint8_t button_id)
{
    (void)button_id;
    return GPIO_ReadInputDataBit(BUTTON_LEFT_GPIO_PORT, BUTTON_LEFT_PIN);
}

uint8_t read_button_right_gpio(uint8_t button_id)
{
    (void)button_id;
    return GPIO_ReadInputDataBit(BUTTON_RIGHT_GPIO_PORT, BUTTON_RIGHT_PIN);
}

void button_ok_press_down_Handler(void *btn)
{
    (void)btn;
    menu_post_key(MENU_KEY_OK);
}

void button_back_press_down_Handler(void *btn)
{
    (void)btn;
    menu_post_key(MENU_KEY_BACK);
}

void button_right_press_down_Handler(void *btn)
{
    (void)btn;
    user_button_repeat_begin(1U);
    menu_post_key(MENU_KEY_RIGHT);
}

void button_left_press_down_Handler(void *btn)
{
    (void)btn;
    user_button_repeat_begin(0U);
    menu_post_key(MENU_KEY_LEFT);
}

/* Blocking service screens poll GPIO themselves. On return, consume keys
 * still held there without generating a new navigation press in the menu. */
void user_BUTTON_resume(void)
{
    struct Button *buttons[4] = {&button_ok, &button_back, &button_left, &button_right};
    uint8_t i;
    user_BUTTON_cancel_repeat();
    for(i = 0U; i < 4U; i++)
    {
        struct Button *button = buttons[i];
        button->ticks = 0U;
        button->debounce_cnt = 0U;
        button->repeat = 0U;
        button->event = NONE_PRESS;
        button->button_level = button->hal_button_Level(button->button_id);
        button->state = button->button_level == button->active_level ? 5U : 0U;
    }
}
