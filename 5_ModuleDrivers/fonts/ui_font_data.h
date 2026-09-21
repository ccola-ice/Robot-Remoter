#ifndef UI_FONT_DATA_H
#define UI_FONT_DATA_H
#include <stdint.h>

/* Remoter UI native cell bitmaps. The returned const storage is permanent.
 * code: printable ASCII (32..126), or a two-byte GB2312 code (high byte first).
 * size: cell height 16, 20, 32 or 48; Chinese width=size, ASCII width=size/2.
 * bold: selects the separate bold face at 32 px; other sizes use regular.
 * Rows are independent, MSB-first 2-bit coverage values (0..3), with stride
 * (width+3)/4 bytes. Unused low bits in the last byte of a row are zero.
 * Unsupported size/code or a Chinese glyph outside the built-in set: NULL.
 * Include ui_font_data.inc in fonts.c exactly once to provide the definition.
 */
const uint8_t *UI_FontBitmap(uint16_t code, uint16_t size, uint8_t bold);
#endif
