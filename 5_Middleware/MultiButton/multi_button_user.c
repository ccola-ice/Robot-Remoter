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

    button_attach(&button_ok, SINGLE_CLICK, button_ok_single_click_Handler);
    button_attach(&button_back, SINGLE_CLICK, button_back_single_click_Handler);
    button_attach(&button_left, SINGLE_CLICK, button_left_single_click_Handler);
    button_attach(&button_right, SINGLE_CLICK, button_right_single_click_Handler);

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

void button_ok_single_click_Handler(void *btn)
{
    (void)btn;
    menu_post_key(MENU_KEY_OK);
}

void button_back_single_click_Handler(void *btn)
{
    (void)btn;
    menu_post_key(MENU_KEY_BACK);
}

void button_right_single_click_Handler(void *btn)
{
    (void)btn;
    menu_post_key(MENU_KEY_RIGHT);
}

void button_left_single_click_Handler(void *btn)
{
    (void)btn;
    menu_post_key(MENU_KEY_LEFT);
}
