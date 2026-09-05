#ifndef TOUCH_TEST_LCD_H
#define TOUCH_TEST_LCD_H
#include <stdint.h>
extern uint16_t LCD_X_LENGTH, LCD_Y_LENGTH;
extern uint8_t LCD_SCAN_MODE;
extern const int Font16x32;
void LCD_SetFont(const void *font);
void LCD_SetColors(uint16_t fg, uint16_t bg);
void LCD_SetBackColor(uint16_t color);
void LCD_SetTextColor(uint16_t color);
void ILI9806G_GramScan(uint8_t mode);
void ILI9806G_Clear(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void ILI9806G_DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t fill);
void ILI9806G_DrawLine(uint16_t x, uint16_t y, uint16_t xx, uint16_t yy);
void ILI9806G_DrawCircle(uint16_t x, uint16_t y, uint16_t r, uint8_t fill);
void ILI9806G_SetPointPixel(uint16_t x, uint16_t y);
void ILI9806G_DispString_EN(uint16_t x, uint16_t y, const char *str);
void ILI9806G_DispString_EN_CH(uint16_t x, uint16_t y, const char *str);
#endif
