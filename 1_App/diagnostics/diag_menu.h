#ifndef DIAG_MENU_H
#define DIAG_MENU_H
#include <stdint.h>

/* 页面使用逻辑颜色编号，绘制入口统一转换为 LCD 的 RGB565 颜色值。 */
#define DIAG_UI_BLACK  0U
#define DIAG_UI_WHITE  1U
#define DIAG_UI_BLUE   2U
#define DIAG_UI_GREY   3U
#define DIAG_UI_RED    4U
#define DIAG_UI_GREEN  5U
#define DIAG_UI_YELLOW 6U

/* 前台阻塞式硬件诊断页面；进入前由调用方停止按键音，退出后由调用方恢复常规输入状态。 */
void diagnostics_menu(void);
/* 前台阻塞式浏览/编辑 EEPROM 页面；实际写入需要用户完成两位编辑并再次确认。 */
void eeprom_menu(void);
/* 开始页面合成并绘制标题，清空旧行缓存；首次 key/release 调用时提交完整画面。 */
void diag_screen(const char *title);
/* 在固定左边距绘制 32 像素文字，以 y 为缓存键；相同文本跳过重绘，缩短时清理尾部。 */
void diag_line(uint16_t y, const char *text);
/* 紧凑的 8×16 ASCII 文本行，仅用于 EEPROM 十六进制表格。 */
void diag_ascii_line(uint16_t y, const char *text);
/* 以下绘制接口均使用屏幕像素坐标和 DIAG_UI_* 颜色；fill 填充区域，frame 只画边框。 */
void diag_ui_fill(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                  uint8_t color);
void diag_ui_frame(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                   uint8_t color);
/* text 使用现有中文字库及方形字高 size，ascii 使用固定 8×16 英文字体。 */
void diag_ui_text(uint16_t x, uint16_t y, uint16_t size,
                  uint8_t foreground, uint8_t background, const char *text);
void diag_ui_ascii(uint16_t x, uint16_t y, uint8_t foreground,
                   uint8_t background, const char *text);
/* 每 10 ms 轮询并消抖，返回 MenuKey 或 -1；上下长按 500 ms 后连发，
 * 间隔 120 ms，持续 1500 ms 后加速至 60 ms；OK/BACK 不连发。 */
int diag_key(void);
/* 提交待显示页面，并阻塞等待四键连续 3 次采样均松开，再重置消抖和长按状态。 */
void diag_release(void);
#endif
