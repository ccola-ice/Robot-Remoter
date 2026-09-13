#include "multi_button_user.h"
#include "multi_button.h"

#include "stm32f4xx.h"
#include "bsp_gpio_button.h"
#include "menu.h"

static struct Button button_ok;
static struct Button button_back;
static struct Button button_left;
static struct Button button_right;

void user_BUTTON_init(void)
{
    BUTTON_GPIO_Config();

    button_init(&button_ok, read_button_ok_gpio, 0, 0);
    button_init(&button_back, read_button_back_gpio, 0, 1);
    button_init(&button_left, read_button_left_gpio, 0, 2);
    button_init(&button_right, read_button_right_gpio, 0, 3);

    /* Navigation acts on the debounced down edge, without release/click timeout. */
    button_attach(&button_ok, PRESS_DOWN, button_ok_press_down_Handler);
    button_attach(&button_back, PRESS_DOWN, button_back_press_down_Handler);
    button_attach(&button_left, PRESS_DOWN, button_left_press_down_Handler);
    button_attach(&button_right, PRESS_DOWN, button_right_press_down_Handler);

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
    menu_post_key(MENU_KEY_RIGHT);
}

void button_left_press_down_Handler(void *btn)
{
    (void)btn;
    menu_post_key(MENU_KEY_LEFT);
}

/* Blocking service screens poll GPIO themselves. On return, consume keys
 * still held there without generating a new navigation press in the menu. */
void user_BUTTON_resume(void)
{
    struct Button *buttons[4] = {&button_ok, &button_back, &button_left, &button_right};
    uint8_t i;
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