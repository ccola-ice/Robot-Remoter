#include "diagnostics.h"
#include "hardware_tests.h"
#include "menu.h"
#include "bsp_fsmc_lcd.h"
#include "bsp_Systick.h"
#include "multi_button_user.h"
#include "bsp_gpio_digital_channel.h"
#include "bsp_adc1_independent_dual.h"
#include "bsp_adc3_independent_dual.h"
#include "bsp_gpio_led.h"
#include "gt9xx.h"
#include "param.h"
#include <stdio.h>
#include <string.h>

extern volatile uint16_t ADC1_Value[NUM_OF_ADC1CHANNEL];
extern volatile uint16_t ADC3_Value[NUM_OF_ADC3CHANNEL];
extern volatile param_Config param;

#define DIAG_LINE_CACHE_COUNT 14U
typedef struct {
    uint16_t y;
    uint8_t valid;
    char text[96];
} DiagLineCache;
static DiagLineCache diag_line_cache[DIAG_LINE_CACHE_COUNT];

static uint8_t key_down(uint8_t key)
{
    switch(key) {
        case MENU_KEY_LEFT: return !read_button_left_gpio(0U);
        case MENU_KEY_RIGHT: return !read_button_right_gpio(0U);
        case MENU_KEY_OK: return !read_button_ok_gpio(0U);
        default: return !read_button_back_gpio(0U);
    }
}

void diag_release(void)
{
    uint8_t stable = 0U;
    while(stable < 3U) {
        if(key_down(0U) || key_down(1U) || key_down(2U) || key_down(3U)) stable = 0U;
        else stable++;
        Delay_ms(10U);
    }
}

int diag_key(void)
{
    uint8_t key;
    for(key = 0; key < 4U; key++) {
        if(key_down(key)) {
            Delay_ms(20U);
            if(key_down(key)) { diag_release(); return key; }
        }
    }
    return -1;
}

static uint16_t diag_lcd_color(uint8_t color)
{
    switch(color) {
        case DIAG_UI_WHITE: return WHITE;
        case DIAG_UI_BLUE: return BLUE;
        case DIAG_UI_GREY: return GREY;
        case DIAG_UI_RED: return RED;
        case DIAG_UI_GREEN: return GREEN;
        case DIAG_UI_YELLOW: return YELLOW;
        default: return BLACK;
    }
}

void diag_ui_fill(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                  uint8_t color)
{
    LCD_SetTextColor(diag_lcd_color(color));
    ILI9806G_DrawRectangle(x, y, width, height, 1U);
}

void diag_ui_frame(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                   uint8_t color)
{
    LCD_SetTextColor(diag_lcd_color(color));
    ILI9806G_DrawRectangle(x, y, width, height, 0U);
}

void diag_ui_text(uint16_t x, uint16_t y, uint16_t size,
                  uint8_t foreground, uint8_t background, const char *text)
{
    LCD_SetTextColor(diag_lcd_color(foreground));
    LCD_SetBackColor(diag_lcd_color(background));
    ILI9806G_DisplayStringEx(x, y, size, size, (uint8_t *)text, 0U);
}

void diag_ui_ascii(uint16_t x, uint16_t y, uint8_t foreground,
                   uint8_t background, const char *text)
{
    LCD_SetFont(&Font8x16);
    LCD_SetTextColor(diag_lcd_color(foreground));
    LCD_SetBackColor(diag_lcd_color(background));
    ILI9806G_DispString_EN(x, y, (char *)text);
}

void diag_screen(const char *title)
{
    memset(diag_line_cache, 0, sizeof(diag_line_cache));
    LCD_SetBackColor(WHITE);
    LCD_SetTextColor(WHITE);
    ILI9806G_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH);
    diag_ui_fill(4U, 0U, 792U, 64U, DIAG_UI_BLUE);
    /* Native 32-pixel glyphs match the established submenu header weight. */
    diag_ui_text(20U, 4U, 32U, DIAG_UI_WHITE, DIAG_UI_BLUE, title);
}

void diag_line(uint16_t y, const char *text)
{
    const uint8_t *scan = (const uint8_t *)text;
    uint16_t width = 0U;
    uint8_t index, slot = 0U;

    for(index = 0U; index < DIAG_LINE_CACHE_COUNT; index++) {
        if(diag_line_cache[index].valid && diag_line_cache[index].y == y) {
            if(strcmp(diag_line_cache[index].text, text) == 0) return;
            slot = index;
            break;
        }
        if(!diag_line_cache[index].valid) slot = index;
    }

    /* Native glyphs retain their strokes. Each glyph paints its own background,
       so only erase the old line's unused tail after drawing. */
    while(*scan != 0U) {
        if(*scan > 0x80U && scan[1] != 0U) { width += 32U; scan += 2; }
        else { width += 16U; scan++; }
    }
    diag_ui_text(16U, y, 32U, DIAG_UI_BLACK, DIAG_UI_WHITE, text);
    if(width < 772U)
        diag_ui_fill(16U + width, y, 772U - width, 32U, DIAG_UI_WHITE);
    diag_line_cache[slot].y = y;
    diag_line_cache[slot].valid = 1U;
    strncpy(diag_line_cache[slot].text, text,
            sizeof(diag_line_cache[slot].text) - 1U);
    diag_line_cache[slot].text[sizeof(diag_line_cache[slot].text) - 1U] = '\0';
}

void diag_ascii_line(uint16_t y, const char *text)
{
    diag_ui_fill(12U, y, 776U, 18U, DIAG_UI_WHITE);
    diag_ui_ascii(16U, y, DIAG_UI_BLACK, DIAG_UI_WHITE, text);
}

static uint8_t confirm(const char *message)
{
    int key;
    diag_line(406U, message);
    diag_line(438U, "OK：是/继续       BACK：否/取消");
    diag_release();
    for(;;) {
        key = diag_key();
        if(key == MENU_KEY_OK) return 1U;
        if(key == MENU_KEY_BACK) return 0U;
        Delay_ms(10U);
    }
}

static HwResult lcd_test(void)
{
    static const uint16_t colors[] = {RED, GREEN, BLUE, WHITE, BLACK};
    uint8_t i;
    uint16_t x, y;
    for(i = 0; i < 6U; i++) {
        LCD_SetBackColor(i < 5U ? colors[i] : WHITE);
        ILI9806G_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH);
        if(i == 5U) {
            LCD_SetTextColor(BLACK);
            for(x = 0; x < 800U; x += 40U) ILI9806G_DrawLine(x, 0, x, 479U);
            for(y = 0; y < 480U; y += 40U) ILI9806G_DrawLine(0, y, 799U, y);
        }
        /* Let the operator inspect every pixel before putting the prompt on it. */
        Delay_ms(1200U);
        if(!confirm("颜色均匀、网格对齐，且无缺口或错位？")) return HW_FAIL;
    }
    return HW_PASS;
}

static HwResult keys_test(void)
{
    uint8_t seen[10] = {0}, previous[4] = {0}, stable[4] = {0};
    uint16_t tick, back = 0U;
    uint8_t i, value, complete;
    char text[96];
    diag_line(64U, "依次按下/松开菜单键；切换全部 DCH 开关。");
    diag_line(96U, "长按 BACK 1秒取消；限时60秒。");
    diag_release();
    for(tick = 0; tick < 6000U; tick++) {
        complete = 0U;
        digital_channel_update_10ms();
        for(i = 0; i < 10U; i++) {
            if(i < 4U) {
                value = key_down(i);
                if(previous[i] != value) { previous[i] = value; stable[i] = 0U; }
                if(stable[i] < 3U) stable[i]++;
                if(stable[i] >= 3U) seen[i] |= (1U << value);
            } else {
                value = digital_channel_get_stable(i - 4U);
                if(tick >= 3U) seen[i] |= (1U << value);
            }
            if(seen[i] == 3U) complete++;
        }
        if(tick % 20U == 0U) {
            sprintf(text, "L:%u  R:%u  OK:%u  BACK:%u",
                    seen[0], seen[1], seen[2], seen[3]);
            diag_line(160U, text);
            sprintf(text, "DCH 1:%u 2:%u 3:%u 4:%u 5:%u 6:%u",
                    seen[4], seen[5], seen[6], seen[7], seen[8], seen[9]);
            diag_line(192U, text);
            diag_line(224U, "0=未检测，1/2=一种状态，3=两种状态");
        }
        if(complete == 10U) return HW_PASS;
        back = key_down(MENU_KEY_BACK) ? back + 1U : 0U;
        if(back >= 100U) { diag_release(); return HW_CANCELLED; }
        Delay_ms(10U);
    }
    return HW_FAIL;
}

static HwResult analog_test(void)
{
    uint16_t minimum[9], maximum[9] = {0}, tick, value;
    uint8_t low[9] = {0}, high[9] = {0}, seen[9] = {0};
    uint8_t i, complete;
    char text[96];
    for(i = 0; i < 9U; i++) minimum[i] = 4095U;
    diag_line(64U, "9个模拟量均移到两端；须达到 <=410 / >=3685。");
    diag_line(96U, "BACK：取消  限时90秒  仅检测行程");
    for(tick = 0; tick < 9000U; tick++) {
        complete = 0U;
        for(i = 0; i < 9U; i++) {
            value = i < 6U ? ADC1_Value[i] : ADC3_Value[i - 6U];
            if(value < minimum[i]) minimum[i] = value;
            if(value > maximum[i]) maximum[i] = value;
            low[i] = value <= 410U ? (low[i] < 3U ? low[i] + 1U : 3U) : 0U;
            high[i] = value >= 3685U ? (high[i] < 3U ? high[i] + 1U : 3U) : 0U;
            if(low[i] >= 3U) seen[i] |= 1U;
            if(high[i] >= 3U) seen[i] |= 2U;
            if(seen[i] == 3U) complete++;
            if(tick % 20U == 0U) {
                sprintf(text, "通道%u：当前%4u  最小%4u  最大%4u  %s", i + 1U,
                        value, minimum[i], maximum[i],
                        seen[i] == 3U ? "通过" : "请移动");
                diag_line(128U + i * 32U, text);
            }
        }
        if(complete == 9U) return HW_PASS;
        if(diag_key() == MENU_KEY_BACK) return HW_CANCELLED;
        Delay_ms(10U);
    }
    return HW_FAIL;
}

static HwResult battery_test(void)
{
    uint32_t sum = 0U, measured, reference = 3700U;
    int key;
    uint8_t i;
    char text[96];
    diag_line(64U, "用万用表测量电池电压，然后设置下方参考值。");
    diag_line(96U, "LEFT/RIGHT：-/+10 mV  OK：对比  BACK：取消");
    for(;;) {
        sprintf(text, "万用表参考值：%lu mV（范围 2500..4500 mV）", (unsigned long)reference);
        diag_line(150U, text);
        key = diag_key();
        if(key == MENU_KEY_BACK) return HW_CANCELLED;
        if(key == MENU_KEY_LEFT && reference > 2500U) reference -= 10U;
        if(key == MENU_KEY_RIGHT && reference < 4500U) reference += 10U;
        if(key == MENU_KEY_OK) break;
        Delay_ms(10U);
    }
    for(i = 0U; i < 32U; i++) { sum += ADC1_Value[6]; Delay_ms(10U); }
    /* R74=R77=4.7k, ADC nominal 3.3V. Include the existing user correction. */
    measured = (sum / 32U) * 6600UL / 4095UL;
    measured = measured * param.batVoltAdjust / 1000UL;
    sprintf(text, "ADC %lu mV | 万用表 %lu mV | 误差限 +/-150 mV",
            (unsigned long)measured, (unsigned long)reference);
    diag_line(210U, text);
    printf("[DIAG] %s\r\n", text);
    Delay_ms(1200U);
    return measured + 150U >= reference && measured <= reference + 150U ? HW_PASS : HW_FAIL;
}

static void buzzer_tone(void)
{
    GPIO_InitTypeDef config;
    uint32_t mode = GPIOA->MODER, type = GPIOA->OTYPER, speed = GPIOA->OSPEEDR;
    uint32_t pull = GPIOA->PUPDR, old = GPIOA->ODR & GPIO_Pin_15;
    uint16_t i;
    GPIO_ResetBits(GPIOA, GPIO_Pin_15);
    GPIO_StructInit(&config);
    config.GPIO_Pin = GPIO_Pin_15;
    config.GPIO_Mode = GPIO_Mode_OUT;
    config.GPIO_Speed = GPIO_Speed_25MHz;
    config.GPIO_OType = GPIO_OType_PP;
    config.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &config);
    for(i = 0; i < 500U; i++) {
        GPIO_SetBits(GPIOA, GPIO_Pin_15); Delay_us(250U);
        GPIO_ResetBits(GPIOA, GPIO_Pin_15); Delay_us(250U);
    }
    GPIO_WriteBit(GPIOA, GPIO_Pin_15, old ? Bit_SET : Bit_RESET);
    GPIOA->OTYPER = (GPIOA->OTYPER & ~GPIO_Pin_15) | (type & GPIO_Pin_15);
    GPIOA->OSPEEDR = (GPIOA->OSPEEDR & ~(3UL << 30)) | (speed & (3UL << 30));
    GPIOA->PUPDR = (GPIOA->PUPDR & ~(3UL << 30)) | (pull & (3UL << 30));
    GPIOA->MODER = (GPIOA->MODER & ~(3UL << 30)) | (mode & (3UL << 30));
}

static HwResult outputs_test(void)
{
    uint32_t led1 = LED1_GPIO_PORT->ODR & LED1_PIN, led2 = LED2_GPIO_PORT->ODR & LED2_PIN;
    HwResult result = HW_FAIL;
    LED1_ON; LED2_OFF;
    if(!confirm("LED1 是否点亮，且 LED2 熄灭？")) goto cleanup;
    LED1_OFF; LED2_ON;
    if(!confirm("LED2 是否点亮，且 LED1 熄灭？")) goto cleanup;
    LED1_OFF; LED2_OFF;
    buzzer_tone();
    if(!confirm("是否听到蜂鸣器，且两个 LED 均已熄灭？")) goto cleanup;
    result = HW_PASS;
cleanup:
    GPIO_WriteBit(LED1_GPIO_PORT, LED1_PIN, led1 ? Bit_SET : Bit_RESET);
    GPIO_WriteBit(LED2_GPIO_PORT, LED2_PIN, led2 ? Bit_SET : Bit_RESET);
    return result;
}

#define TEST_COUNT 9U
static const char *result_text(HwResult result)
{
    static const char * const names[] = {"通过", "失败", "条件不足", "已取消"};
    return result <= HW_CANCELLED ? names[result] : "失败";
}

static uint8_t diagnostics_state_color(uint8_t state)
{
    if(state == (uint8_t)HW_PASS + 1U) return DIAG_UI_GREEN;
    if(state == (uint8_t)HW_FAIL + 1U) return DIAG_UI_RED;
    if(state == (uint8_t)HW_BLOCKED + 1U) return DIAG_UI_YELLOW;
    return DIAG_UI_BLACK;
}

static void diagnostics_menu_draw_row(const char * const *names,
                                      const uint8_t *states, uint8_t item,
                                      uint8_t selected, uint8_t first_visible)
{
    uint16_t y = 112U + (uint16_t)(item - first_visible) * 48U;
    char line[48];

    diag_ui_fill(4U, y, 792U, 40U, DIAG_UI_GREY);
    diag_ui_fill(8U, y + 4U, 8U, 32U,
                 item == selected ? DIAG_UI_BLUE : DIAG_UI_GREY);
    diag_ui_frame(4U, y, 792U, 40U,
                  item == selected ? DIAG_UI_BLUE : DIAG_UI_BLACK);
    sprintf(line, "%2u  %s", item + 1U, names[item]);
    /* Keep native glyph resolution: downscaling made the strokes too thin. */
    diag_ui_text(28U, y + 4U, 32U, DIAG_UI_BLACK, DIAG_UI_GREY, line);
    diag_ui_text(640U, y + 4U, 32U, diagnostics_state_color(states[item]),
                 DIAG_UI_GREY,
                 states[item] ? result_text((HwResult)(states[item] - 1U)) : "未测试");
}

static void diagnostics_menu_mark_row(uint8_t item, uint8_t first_visible,
                                      uint8_t selected)
{
    uint16_t y = 112U + (uint16_t)(item - first_visible) * 48U;
    diag_ui_fill(8U, y + 4U, 8U, 32U,
                 selected ? DIAG_UI_BLUE : DIAG_UI_GREY);
    diag_ui_frame(4U, y, 792U, 40U,
                  selected ? DIAG_UI_BLUE : DIAG_UI_BLACK);
}

static void diagnostics_menu_draw_window(const char * const *names,
                                         const uint8_t *states,
                                         uint8_t selected,
                                         uint8_t first_visible)
{
    uint8_t row;
    char line[32];

    diag_ui_fill(4U, 72U, 792U, 32U, DIAG_UI_GREY);
    sprintf(line, "项目 %u-%u / %u",
            first_visible + 1U, first_visible + 6U, TEST_COUNT);
    diag_ui_text(20U, 78U, 20U, DIAG_UI_BLACK, DIAG_UI_GREY, line);
    for(row = 0U; row < 6U; row++) {
        diagnostics_menu_draw_row(names, states, first_visible + row,
                                  selected, first_visible);
    }
}

static void diagnostics_menu_draw(const char * const *names,
                                  const uint8_t *states, uint8_t selected,
                                  uint8_t first_visible)
{
    diag_screen("硬件测试");
    diag_ui_ascii(20U, 42U, DIAG_UI_WHITE, DIAG_UI_BLUE,
                  "HARDWARE DIAGNOSTICS / MANUAL ITEMS");
    diagnostics_menu_draw_window(names, states, selected, first_visible);

    diag_ui_text(12U, 406U, 16U, DIAG_UI_BLUE, DIAG_UI_WHITE,
                 "LEFT/RIGHT：选择   OK：运行   BACK：返回");
    diag_ui_fill(4U, 432U, 792U, 32U, DIAG_UI_WHITE);
    diag_ui_text(12U, 440U, 16U, DIAG_UI_BLACK, DIAG_UI_WHITE,
                 "EEPROM / Flash / SD 卡：开机自动检测");
}

void diagnostics_menu(void)
{
    static const char * const names[TEST_COUNT] = {"LCD 颜色/网格", "按键/DCH开关",
        "模拟量行程", "电池电压对比", "LED/蜂鸣器", "UART4 线缆回环",
        "NRF 发送+ACK（需另一台）", "NRF 接收（需另一台）",
        "MCU 内存抽检"};
    static uint8_t states[TEST_COUNT]; /* 0=untested; else HwResult+1, this power cycle. */
    uint8_t selected = 0U, first_visible = 0U, redraw = 1U;
    uint8_t old_selected, old_first_visible;
    int key;
    HwResult result;
    GTP_IRQ_Disable();
    diag_release();
    for(;;) {
        if(redraw) {
            diagnostics_menu_draw(names, states, selected, first_visible);
            redraw = 0U;
        }
        key = diag_key();
        if(key == MENU_KEY_BACK) break;
        if(key == MENU_KEY_LEFT) {
            old_selected = selected;
            old_first_visible = first_visible;
            selected = selected ? selected - 1U : TEST_COUNT - 1U;
            if(selected < first_visible) first_visible = selected;
            if(selected >= first_visible + 6U) first_visible = selected - 5U;
            if(first_visible != old_first_visible)
                diagnostics_menu_draw_window(names, states, selected, first_visible);
            else {
                diagnostics_menu_mark_row(old_selected, first_visible, 0U);
                diagnostics_menu_mark_row(selected, first_visible, 1U);
            }
        }
        if(key == MENU_KEY_RIGHT) {
            old_selected = selected;
            old_first_visible = first_visible;
            selected = (selected + 1U) % TEST_COUNT;
            if(selected < first_visible) first_visible = selected;
            if(selected >= first_visible + 6U) first_visible = selected - 5U;
            if(first_visible != old_first_visible)
                diagnostics_menu_draw_window(names, states, selected, first_visible);
            else {
                diagnostics_menu_mark_row(old_selected, first_visible, 0U);
                diagnostics_menu_mark_row(selected, first_visible, 1U);
            }
        }
        if(key == MENU_KEY_OK) {
            diag_screen(names[selected]);
            result = HW_CANCELLED;
            switch(selected) {
                case 0: result = lcd_test(); break;
                case 1: result = keys_test(); break;
                case 2: result = analog_test(); break;
                case 3: result = battery_test(); break;
                case 4: result = outputs_test(); break;
                case 5:
                    if(confirm("断开 UART4；短接 PA0(TX)-PA1(RX)。已就绪？"))
                        result = hardware_uart_loopback_test();
                    break;
                case 6: case 7:
                    if(confirm(selected == 6U ? "先启动另一台 NRF 接收；频道40、1 Mbps。就绪？" :
                               "启动后20秒内运行另一台的 NRF 发送。已就绪？"))
                        result = hardware_radio_test(selected == 7U);
                    break;
                case 8: result = hardware_memory_test(); break;
                default: break;
            }
            /* Cancellation never hides a recorded failure from an earlier attempt. */
            if(result != HW_CANCELLED) states[selected] = (uint8_t)result + 1U;
            printf("[DIAG] %s: %s\r\n", names[selected], hardware_result_name(result));
            diag_screen(names[selected]);
            diag_line(100U, result_text(result));
            if(result == HW_BLOCKED) diag_line(160U, "前置条件不足，本次不记录为通过。");
            if(selected == 8U) diag_line(160U, "专用1 KiB RAM + 4个ROM常量；非全芯片检测。");
            (void)confirm("测试结束，返回硬件测试列表。");
            diag_release();
            redraw = 1U;
        }
        Delay_ms(10U);
    }
    GTP_IRQ_Disable();
}
