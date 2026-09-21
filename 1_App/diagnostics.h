#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H
#include <stdint.h>

#define DIAG_UI_BLACK  0U
#define DIAG_UI_WHITE  1U
#define DIAG_UI_BLUE   2U
#define DIAG_UI_GREY   3U
#define DIAG_UI_RED    4U
#define DIAG_UI_GREEN  5U
#define DIAG_UI_YELLOW 6U

/* 前台服务页面；退出后恢复正常应用任务。 */
void diagnostics_menu(void);
void eeprom_menu(void);
void diag_screen(const char *title);
void diag_line(uint16_t y, const char *text);
/* 紧凑的 8×16 ASCII 文本行，仅用于 EEPROM 十六进制表格。 */
void diag_ascii_line(uint16_t y, const char *text);
void diag_ui_fill(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                  uint8_t color);
void diag_ui_frame(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                   uint8_t color);
void diag_ui_text(uint16_t x, uint16_t y, uint16_t size,
                  uint8_t foreground, uint8_t background, const char *text);
void diag_ui_ascii(uint16_t x, uint16_t y, uint8_t foreground,
                   uint8_t background, const char *text);
int diag_key(void); /* 以 10 ms 周期轮询并消抖；有按下事件时返回 MenuKey，否则返回 -1。 */
void diag_release(void);
#endif
