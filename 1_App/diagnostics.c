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
#include "bsp_i2c_touch.h"
#include "palette.h"
#include "param.h"
#include <stdio.h>

extern volatile uint16_t ADC1_Value[NUM_OF_ADC1CHANNEL];
extern volatile uint16_t ADC3_Value[NUM_OF_ADC3CHANNEL];
extern volatile param_Config param;

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

void diag_screen(const char *title)
{
    LCD_SetBackColor(BLACK);
    LCD_SetTextColor(BLACK);
    ILI9806G_Clear(0, 0, LCD_X_LENGTH, LCD_Y_LENGTH);
    LCD_SetTextColor(WHITE);
    LCD_SetFont(&Font16x32);
    ILI9806G_DispString_EN(16U, 8U, (char *)title);
    LCD_SetFont(&Font8x16);
}

void diag_line(uint16_t y, const char *text)
{
    char line[97];
    LCD_SetFont(&Font8x16);
    LCD_SetBackColor(BLACK);
    LCD_SetTextColor(WHITE);
    sprintf(line, "%-96.96s", text);
    ILI9806G_DispString_EN(16U, y, line);
}

static uint8_t confirm(const char *message)
{
    int key;
    diag_line(406U, message);
    diag_line(438U, "OK: yes / continue       BACK: no / cancel");
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
        if(!confirm("Uniform color / aligned grid, without gaps or shifted halves?")) return HW_FAIL;
    }
    return HW_PASS;
}

static HwResult keys_test(void)
{
    uint8_t seen[10] = {0}, previous[4] = {0}, stable[4] = {0};
    uint16_t tick, back = 0U;
    uint8_t i, value, complete;
    char text[96];
    diag_line(64U, "Press AND release each menu key; move every DCH switch/button through both levels.");
    diag_line(92U, "Hold BACK for 1 second to cancel. Timeout: 60 seconds.");
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
            sprintf(text, "L:%u R:%u OK:%u BACK:%u | DCH1:%u 2:%u 3:%u 4:%u 5:%u 6:%u",
                    seen[0], seen[1], seen[2], seen[3], seen[4], seen[5], seen[6], seen[7], seen[8], seen[9]);
            diag_line(160U, text);
            diag_line(190U, "0=unseen, 1/2=one level seen, 3=both stable levels seen");
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
    diag_line(64U, "Move all 9 analog controls to BOTH ends. Required raw range: <=410 and >=3685.");
    diag_line(90U, "BACK cancels. Timeout 90 seconds. This checks travel, not calibration accuracy.");
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
                sprintf(text, "Control %u: current %4u  min %4u  max %4u  %s", i + 1U,
                        value, minimum[i], maximum[i],
                        seen[i] == 3U ? "PASS" : "MOVE");
                diag_line(128U + i * 26U, text);
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
    diag_line(64U, "Measure battery voltage with a meter, then set that reference below.");
    diag_line(92U, "LEFT/RIGHT: -/+10 mV   OK: compare   BACK: cancel. Does not change calibration.");
    for(;;) {
        sprintf(text, "Meter reference: %lu mV (allowed 2500..4500 mV)", (unsigned long)reference);
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
    sprintf(text, "ADC result %lu mV, meter %lu mV; allowed error +/-150 mV",
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
    if(!confirm("Is LED1 on, with LED2 off?")) goto cleanup;
    LED1_OFF; LED2_ON;
    if(!confirm("Is LED2 on, with LED1 off?")) goto cleanup;
    LED1_OFF; LED2_OFF;
    buzzer_tone();
    if(!confirm("Did the buzzer sound, and are both LEDs now off?")) goto cleanup;
    result = HW_PASS;
cleanup:
    GPIO_WriteBit(LED1_GPIO_PORT, LED1_PIN, led1 ? Bit_SET : Bit_RESET);
    GPIO_WriteBit(LED2_GPIO_PORT, LED2_PIN, led2 ? Bit_SET : Bit_RESET);
    return result;
}

static HwResult touch_test(void)
{
    uint16_t x, y;
    int key;
    if(!GTP_CalibrationIsReady()) return HW_BLOCKED;
    if(!confirm("Draw both diagonals and a vertical line across the old gap; inspect tracking.")) return HW_CANCELLED;
    Palette_Init(LCD_SCAN_MODE);
    LCD_SetTextColor(RED);
    for(x = 220U; x <= 760U; x += 270U)
        for(y = 40U; y <= 440U; y += 200U) ILI9806G_DrawCircle(x, y, 12U, 0U);
    GTP_IRQ_Enable();
    for(;;) {
        GTP_Service();
        key = diag_key();
        if(key == MENU_KEY_OK || key == MENU_KEY_BACK) break;
        Delay_ms(1U);
    }
    GTP_IRQ_Disable();
    diag_screen("TOUCH QUALITY / OPERATOR CHECK");
    if(key == MENU_KEY_BACK) return HW_CANCELLED;
    return confirm("All 9 targets align and strokes stay continuous, with no jumps or double lines?") ? HW_PASS : HW_FAIL;
}

#define TEST_COUNT 13U
void diagnostics_menu(void)
{
    static const char * const names[TEST_COUNT] = {"LCD colors / grid", "Keys / digital switches",
        "Analog control travel", "Battery meter comparison", "LEDs / buzzer", "UART4 cable loopback",
        "NRF diagnostic TX + ACK", "NRF diagnostic RX", "Flash dedicated-sector R/W",
        "EEPROM reserved-byte R/W", "SD temporary-file R/W", "Touch quality (operator)", "MCU memory sample"};
    static uint8_t states[TEST_COUNT]; /* 0=untested; else HwResult+1, this power cycle. */
    uint8_t selected = 0U, i, redraw = 1U;
    int key;
    HwResult result;
    char line[96];
    GTP_IRQ_Disable();
    diag_release();
    for(;;) {
        if(redraw) {
            diag_screen("HARDWARE TESTS");
            for(i = 0; i < TEST_COUNT; i++) {
                sprintf(line, "%c %2u. %-32s %s", i == selected ? '>' : ' ', i + 1U,
                        names[i], states[i] ? hardware_result_name((HwResult)(states[i] - 1U)) : "NOT TESTED");
                diag_line(60U + 25U * i, line);
            }
            diag_line(410U, "LEFT/RIGHT select | OK run | BACK return. Results remain until power-off.");
            diag_line(440U, "Storage tests do not run on every boot. Boot results remain a separate snapshot.");
            redraw = 0U;
        }
        key = diag_key();
        if(key == MENU_KEY_BACK) break;
        if(key == MENU_KEY_LEFT) { selected = selected ? selected - 1U : TEST_COUNT - 1U; redraw = 1U; }
        if(key == MENU_KEY_RIGHT) { selected = (selected + 1U) % TEST_COUNT; redraw = 1U; }
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
                    if(confirm("Disconnect UART4 peripheral. Jumper PA0 TX to PA1 RX. Cable ready?"))
                        result = hardware_uart_loopback_test();
                    break;
                case 6: case 7:
                    if(confirm(selected == 6U ? "Start peer RX first. Diagnostic channel 40, 1 Mbps, address D7 43 44 47 31. Ready?" :
                               "Start this RX, then peer TX within 15s. Channel 40, 1 Mbps. Ready?"))
                        result = hardware_radio_test(selected == 7U);
                    break;
                case 8:
                    if(confirm("Write/verify/erase dedicated blank sector 0x5FE000. Keep power connected. Start?"))
                        result = hardware_flash_write_test();
                    break;
                case 9:
                    if(confirm("Write/verify/restore reserved byte 0xFF at I2C 0x50. Keep power connected. Start?"))
                        result = hardware_eeprom_write_test();
                    break;
                case 10:
                    if(confirm("Create, verify and delete a NEW 4 KiB temporary file on SD. Start?"))
                        result = hardware_sd_write_test();
                    break;
                case 11: result = touch_test(); break;
                case 12: result = hardware_memory_test(); break;
                default: break;
            }
            /* Cancellation never hides a recorded failure from an earlier attempt. */
            if(result != HW_CANCELLED) states[selected] = (uint8_t)result + 1U;
            printf("[DIAG] %s: %s\r\n", names[selected], hardware_result_name(result));
            diag_screen(names[selected]);
            diag_line(100U, hardware_result_name(result));
            if(result == HW_BLOCKED) diag_line(160U, "Prerequisite missing or reserved storage occupied. No pass recorded.");
            if(selected == 12U) diag_line(160U, "Owned 1 KiB RAM patterns and four ROM constants; not full-chip integrity.");
            (void)confirm("Test finished. Return to hardware test list.");
            diag_release();
            redraw = 1U;
        }
        Delay_ms(10U);
    }
    GTP_IRQ_Disable();
}
