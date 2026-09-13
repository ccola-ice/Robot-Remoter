#include "diagnostics.h"
#include "hardware_tests.h"
#include "menu.h"
#include "bsp_i2c_eeprom.h"
#include "bsp_Systick.h"
#include <stdio.h>
#include <string.h>

static uint8_t eeprom_ui_valid;
static uint8_t eeprom_ui_address;
static uint8_t eeprom_ui_editing;
static uint8_t eeprom_ui_digit;
static uint8_t eeprom_ui_value;
static uint8_t eeprom_ui_read_ok;
static char eeprom_ui_status[96];

static uint16_t eeprom_text_width(const char *text, uint16_t size)
{
    const uint8_t *scan = (const uint8_t *)text;
    uint16_t width = 0U;
    while(*scan != 0U) {
        if(*scan > 0x80U && scan[1] != 0U) { width += size; scan += 2; }
        else { width += size / 2U; scan++; }
    }
    return width;
}

static void eeprom_draw_shell(void)
{
    uint8_t col;
    char text[96];

    diag_screen("EEPROM / AT24C08");
    diag_ui_fill(4U, 72U, 792U, 32U, DIAG_UI_GREY);
    diag_ui_text(20U, 72U, 32U, DIAG_UI_BLACK, DIAG_UI_GREY,
                 "0x00-0xFE 可编辑 | 0xFF 自检保留 | 禁止重叠区");

    diag_ui_fill(4U, 112U, 792U, 224U, DIAG_UI_WHITE);
    diag_ui_frame(4U, 112U, 792U, 224U, DIAG_UI_BLUE);
    diag_ui_fill(8U, 116U, 784U, 40U, DIAG_UI_GREY);
    diag_ui_text(20U, 120U, 32U, DIAG_UI_BLUE, DIAG_UI_GREY, "ADDR");
    for(col = 0U; col < 8U; col++) {
        sprintf(text, "+%u", col);
        diag_ui_text(124U + (uint16_t)col * 84U, 120U, 32U,
                     DIAG_UI_BLUE, DIAG_UI_GREY, text);
    }
    diag_ui_fill(4U, 344U, 792U, 40U, DIAG_UI_GREY);

    /* The table and status panels replace the old page directly.  Erase only
       their narrow gutters so entering EEPROM never needs a blank frame. */
    diag_ui_fill(0U, 64U, 800U, 8U, DIAG_UI_WHITE);
    diag_ui_fill(0U, 72U, 4U, 408U, DIAG_UI_WHITE);
    diag_ui_fill(796U, 72U, 4U, 408U, DIAG_UI_WHITE);
    diag_ui_fill(4U, 104U, 792U, 8U, DIAG_UI_WHITE);
    diag_ui_fill(4U, 336U, 792U, 8U, DIAG_UI_WHITE);
    diag_ui_fill(4U, 384U, 792U, 4U, DIAG_UI_WHITE);
    diag_ui_fill(4U, 420U, 792U, 16U, DIAG_UI_WHITE);
    diag_ui_fill(4U, 468U, 792U, 12U, DIAG_UI_WHITE);
}

static void eeprom_draw_cell(uint8_t cell_address, uint8_t selected_address,
                             uint8_t editing, uint8_t value)
{
    uint8_t byte, error;
    uint8_t selected = cell_address == selected_address;
    uint8_t selection_color = editing ? DIAG_UI_RED : DIAG_UI_BLUE;
    uint16_t x = 112U + (uint16_t)(cell_address & 7U) * 84U;
    uint16_t y = 164U + (uint16_t)((cell_address & 0x1fU) >> 3) * 42U;
    char text[4];

    error = EEPROM_Random_Read(cell_address, &byte);
    if(selected && editing) { byte = value; error = 0U; }
    diag_ui_fill(x, y - 2U, 56U, 36U,
                 selected ? selection_color : DIAG_UI_WHITE);
    if(selected) diag_ui_frame(x, y - 2U, 56U, 36U, selection_color);
    if(error) sprintf(text, "--");
    else sprintf(text, "%02X", byte);
    diag_ui_text(x + 12U, y, 32U,
                 selected ? DIAG_UI_WHITE : DIAG_UI_BLACK,
                 selected ? selection_color : DIAG_UI_WHITE, text);
}

static void eeprom_draw_table(uint8_t address, uint8_t editing, uint8_t value)
{
    uint8_t row, col;
    uint8_t page = address & 0xe0U;
    uint8_t base;
    uint16_t y;
    char text[4];

    for(row = 0U; row < 4U; row++) {
        base = page + row * 8U;
        y = 164U + (uint16_t)row * 42U;
        sprintf(text, "%02X", base);
        diag_ui_text(24U, y, 32U, DIAG_UI_BLUE, DIAG_UI_WHITE, text);
        for(col = 0U; col < 8U; col++) {
            eeprom_draw_cell(base + col, address, editing, value);
        }
    }
    diag_ui_frame(4U, 112U, 792U, 224U, DIAG_UI_BLUE);
}

static void eeprom_draw_selected(uint8_t address, uint8_t editing,
                                 uint8_t digit, uint8_t read_ok, uint8_t value)
{
    uint8_t selection_color = editing ? DIAG_UI_RED : DIAG_UI_BLUE;
    uint16_t width;
    char text[64];

    if(!read_ok) sprintf(text, "0x%02X = 读取错误", address);
    else sprintf(text, "0x%02X = 0x%02X  %s", address, value,
                 !editing ? "浏览" : digit == 0U ? "修改高4位" :
                 digit == 1U ? "修改低4位" : "确认写入");
    diag_ui_text(20U, 348U, 32U, selection_color, DIAG_UI_GREY, text);
    width = eeprom_text_width(text, 32U);
    if(width < 768U)
        diag_ui_fill(20U + width, 348U, 768U - width, 32U, DIAG_UI_GREY);
}

static void eeprom_draw_page(uint8_t address, uint8_t editing, uint8_t digit,
                             uint8_t *read_ok, uint8_t *value,
                             const char *status)
{
    uint8_t page_changed;
    uint8_t cell_changed;

    if(!editing) *read_ok = EEPROM_Random_Read(address, value) == 0U;
    page_changed = !eeprom_ui_valid ||
                   ((address & 0xe0U) != (eeprom_ui_address & 0xe0U));
    cell_changed = !eeprom_ui_valid || address != eeprom_ui_address ||
                   editing != eeprom_ui_editing || *value != eeprom_ui_value;

    if(!eeprom_ui_valid) eeprom_draw_shell();
    if(page_changed) {
        eeprom_draw_table(address, editing, *value);
    } else if(cell_changed) {
        if(address != eeprom_ui_address)
            eeprom_draw_cell(eeprom_ui_address, address, 0U, 0U);
        eeprom_draw_cell(address, address, editing, *value);
    }

    if(cell_changed || digit != eeprom_ui_digit || *read_ok != eeprom_ui_read_ok)
        eeprom_draw_selected(address, editing, digit, *read_ok, *value);

    if(!eeprom_ui_valid || editing != eeprom_ui_editing ||
       strcmp(status, eeprom_ui_status) != 0) {
        diag_ui_text(20U, 388U, 32U,
                     editing ? DIAG_UI_RED : DIAG_UI_BLACK,
                     DIAG_UI_WHITE, status);
        if(eeprom_text_width(status, 32U) < 768U)
            diag_ui_fill(20U + eeprom_text_width(status, 32U), 388U,
                         768U - eeprom_text_width(status, 32U), 32U,
                         DIAG_UI_WHITE);
    }
    if(!eeprom_ui_valid || editing != eeprom_ui_editing) {
        const char *controls = editing ?
            "LEFT/RIGHT：修改  OK：下一步  BACK：放弃" :
            "LEFT/RIGHT：选择  OK：编辑  BACK：返回";
        uint16_t width = eeprom_text_width(controls, 32U);
        diag_ui_text(12U, 436U, 32U, DIAG_UI_BLUE, DIAG_UI_WHITE, controls);
        if(width < 776U)
            diag_ui_fill(12U + width, 436U, 776U - width, 32U, DIAG_UI_WHITE);
    }

    eeprom_ui_valid = 1U;
    eeprom_ui_address = address;
    eeprom_ui_editing = editing;
    eeprom_ui_digit = digit;
    eeprom_ui_value = *value;
    eeprom_ui_read_ok = *read_ok;
    strncpy(eeprom_ui_status, status, sizeof(eeprom_ui_status) - 1U);
    eeprom_ui_status[sizeof(eeprom_ui_status) - 1U] = '\0';
}

void eeprom_menu(void)
{
    uint8_t address = 0U, value = 0U, editing = 0U, digit = 0U, read_ok = 0U;
    uint8_t redraw = 1U, byte, error;
    int key;
    char status[96] = "选择字节，按 OK 编辑";
    eeprom_ui_valid = 0U;
    eeprom_draw_page(address, editing, digit, &read_ok, &value, status);
    diag_release();
    redraw = 0U;
    for(;;) {
        if(redraw) {
            eeprom_draw_page(address, editing, digit, &read_ok, &value, status);
            redraw = 0U;
        }
        key = diag_key();
        if(key == MENU_KEY_BACK) {
            if(!editing) break;
            editing = 0U;
            sprintf(status, "已放弃，EEPROM 未改变");
            redraw = 1U;
        }
        if(key == MENU_KEY_LEFT || key == MENU_KEY_RIGHT) {
            if(!editing) address += key == MENU_KEY_RIGHT ? 1U : 255U;
            else if(digit < 2U) {
                uint8_t shift = digit == 0U ? 4U : 0U;
                uint8_t nibble = ((value >> shift) + (key == MENU_KEY_RIGHT ? 1U : 15U)) & 0x0fU;
                value = (value & ~(0x0fU << shift)) | (nibble << shift);
            }
            redraw = 1U;
        }
        if(key == MENU_KEY_OK) {
            if(!editing) {
                if(address == HW_EEPROM_TEST_BYTE) sprintf(status, "0xFF 为开机自检保留，只读");
                else if(!read_ok) sprintf(status, "读取失败，请检查 EEPROM");
                else { editing = 1U; digit = 0U; sprintf(status, "修改暂存，确认后写入"); }
            } else if(digit < 2U) {
                digit++;
                if(digit == 2U) sprintf(status, "写 0x%02X 到 0x%02X？OK 确认", value, address);
            } else {
                error = EEPROM_Byte_Write(address, value);
                if(!error) error = EEPROM_Random_Read(address, &byte);
                if(!error && byte != value) error = 9U;
                sprintf(status, error ? "写入失败，代码%u" : "写入并回读通过，代码%u", error);
                printf("[EEPROM] address=%02X requested=%02X result=%u\r\n", address, value, error);
                editing = 0U;
            }
            redraw = 1U;
        }
        Delay_ms(10U);
    }
}
