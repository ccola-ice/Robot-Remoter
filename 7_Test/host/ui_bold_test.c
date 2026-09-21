#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fonts.h"

static uint16_t LCD_X_LENGTH=800U,LCD_Y_LENGTH=480U;
static uint16_t CurrentTextColor=0x1234U,CurrentBackColor=0xabcdU;
static uint16_t frame[480][800];
static unsigned blits;
static void LCD_BlitRGB565(uint16_t x,uint16_t y,uint16_t w,uint16_t h,const uint16_t *pixels)
{
    unsigned row,column;
    assert(w==16U && h==4U && x+w<=LCD_X_LENGTH && y+h<=LCD_Y_LENGTH && pixels);
    for(row=0U;row<h;row++) for(column=0U;column<w;column++) frame[y+row][x+column]=pixels[row*w+column];
    blits++;
}
static uint8_t LCD_DrawFontGlyph(uint16_t x,uint16_t y,uint16_t code,uint16_t size,
    uint8_t bold,uint16_t fg,uint16_t bg)
{ (void)x; (void)y; (void)code; (void)size; (void)bold; (void)fg; (void)bg; return 0U; }

#include "ui_bold_impl.inc"

static uint8_t bit(unsigned character,unsigned x,unsigned y)
{
    const uint8_t *bitmap=&Font16x32.table[(character-32U)*64U];
    return (bitmap[y*2U+x/8U] & (0x80U>>(x&7U))) != 0U;
}
static void check_glyph(unsigned character,unsigned at_x,unsigned at_y)
{
    unsigned x,y; uint8_t ink;
    for(y=0U;y<32U;y++) for(x=0U;x<16U;x++) {
        ink=bit(character,x,y);
        if(x) ink|=bit(character,x-1U,y);
        if(y) ink|=bit(character,x,y-1U);
        assert(frame[at_y+y][at_x+x]==(ink?CurrentTextColor:CurrentBackColor));
    }
}
int main(void)
{
    unsigned character,x,y,before;
    char text[2]={0,0};
    for(character=32U;character<=126U;character++) {
        memset(frame,0x55,sizeof(frame)); blits=0U;
        text[0]=(char)character;
        LCD_DispString_EN_Bold(40U,48U,text);
        assert(blits==8U);
        check_glyph(character,40U,48U);
        assert(frame[47U][40U]==0x5555U && frame[80U][40U]==0x5555U);
        assert(frame[48U][39U]==0x5555U && frame[48U][56U]==0x5555U);
    }
    LCD_DispString_EN_Bold(40U,48U,"LONG");
    LCD_DispString_EN_Bold(40U,48U,"Q   ");
    check_glyph('Q',40U,48U);
    for(y=48U;y<80U;y++) for(x=56U;x<104U;x++) assert(frame[y][x]==CurrentBackColor);
    before=blits;
    LCD_DispString_EN_Bold(784U,448U,"AB");
    assert(blits==before+8U); check_glyph('A',784U,448U);
    before=blits;
    LCD_DispString_EN_Bold(785U,448U,"A");
    LCD_DispString_EN_Bold(784U,449U,"A");
    LCD_DispString_EN_Bold(65535U,0U,"A");
    LCD_DispString_EN_Bold(0U,65535U,"A");
    LCD_DispString_EN_Bold(0U,0U,0);
    assert(blits==before);
    LCD_DispString_EN_Bold(0U,0U,"\n\xff");
    check_glyph('?',0U,0U); check_glyph('?',16U,0U);
    LCD_X_LENGTH=480U; LCD_Y_LENGTH=800U;
    /* Bounds are runtime orientation-aware; avoid drawing outside this test's landscape array. */
    before=blits;
    LCD_DispString_EN_Bold(465U,0U,"A");
    assert(blits==before);
    puts("ASCII bold: all 95 production Font16x32 glyphs match left/up dilation; opaque tails and screen bounds passed");
    return 0;
}
