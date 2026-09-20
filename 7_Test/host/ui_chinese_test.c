#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct { unsigned Width, Height; } TestFont;
static TestFont Font8x16 = {8U,16U}, Font16x32 = {16U,32U}, Font24x48 = {24U,48U};
static TestFont *current_font = &Font8x16;
static uint16_t foreground, background;
static uint16_t frame[480][800];
static unsigned font_reads, blits, strings, fills;
static uint8_t flash_error, failure_mode;
static uint16_t failure_code;
static struct { uint16_t x,y; char text[101]; } rendered[20];

static void LCD_SetFont(TestFont *font) { current_font = font; }
static void LCD_SetTextColor(uint16_t color) { foreground = color; }
static void LCD_SetBackColor(uint16_t color) { background = color; }
static void ILI9806G_Fill(uint16_t x,uint16_t y,uint16_t xe,uint16_t ye,uint16_t color)
{
    unsigned a,b;
    assert(x <= xe && y <= ye && xe <= 800U && ye <= 480U);
    for(b=y;b<ye;b++) for(a=x;a<xe;a++) frame[b][a]=color;
    fills++;
}
static void ILI9806G_DispString_EN(uint16_t x,uint16_t y,char *text)
{
    unsigned i,px,py;
    assert(strings < 20U);
    assert(x + strlen(text)*current_font->Width <= 800U);
    assert(y + current_font->Height <= 480U);
    rendered[strings].x=x; rendered[strings].y=y;
    strcpy(rendered[strings].text,text); strings++;
    for(i=0U;text[i];i++) {
        assert((uint8_t)text[i] >= 32U && (uint8_t)text[i] <= 126U);
        for(py=0U;py<current_font->Height;py++) for(px=0U;px<current_font->Width;px++)
            frame[y+py][x+i*current_font->Width+px]=text[i]==' '?background:foreground;
    }
}
static void LCD_BlitRGB565(uint16_t x,uint16_t y,uint16_t w,uint16_t h,const uint16_t *pixels)
{
    unsigned px,py;
    assert((w == 16U || w == 32U || w == 48U) && h == 4U);
    assert(x+w <= 800U && y+h <= 480U && pixels);
    for(py=0U;py<h;py++) for(px=0U;px<w;px++) frame[y+py][x+px]=pixels[py*w+px];
    blits++;
}
static void ILI9806G_DrawLine(uint16_t a,uint16_t b,uint16_t c,uint16_t d)
{ (void)a; (void)b; (void)c; (void)d; }
static void ILI9806G_DrawRectangle(uint16_t a,uint16_t b,uint16_t c,uint16_t d,uint8_t e)
{ (void)a; (void)b; (void)c; (void)d; (void)e; }
static void ILI9806G_DrawCircle(uint16_t a,uint16_t b,uint16_t c,uint8_t d)
{ (void)a; (void)b; (void)c; (void)d; }
uint8_t FLASH_GetIoError(void) { return flash_error; }

static uint8_t source_pixel(uint16_t code,unsigned x,unsigned y)
{
    if(code == 0xa1a1U) return 0U;
    if(code == 0xb0a1U)
        return (x==1U && y==1U) || (x==2U && y==2U) ||
               (x==31U && y==31U) || (x==16U && y==17U);
    return x==((code & 255U)%30U)+1U && y==((code>>8)%30U)+1U;
}
static int test_get_glyph(uint8_t *bitmap,uint16_t code)
{
    unsigned x,y,i;
    assert((code>>8)>=0xa1U && (code>>8)<=0xf7U && (code&255U)>=0xa1U && (code&255U)<=0xfeU);
    /* The production cache must clear the victim before asking the driver. */
    for(i=0U;i<128U;i++) assert(bitmap[i] == 0U);
    font_reads++;
    if(code == failure_code) {
        if(failure_mode==1U) { memset(bitmap,0xff,128U); return -1; }
        if(failure_mode==2U) { flash_error=1U; return 0; }
        if(failure_mode==3U) return 0;
        if(failure_mode==4U) { memset(bitmap,0xff,128U); return 0; }
    }
    for(y=0U;y<32U;y++) for(x=0U;x<32U;x++)
        if(source_pixel(code,x,y)) bitmap[y*4U+x/8U]|=(uint8_t)(0x80U>>(x&7U));
    return 0;
}
#define GetGBKCode(buffer,code) test_get_glyph(buffer,code)
#include "gui_theme.h"

static void reset_drawing(void)
{
    memset(frame,0x55,sizeof(frame));
    memset(rendered,0,sizeof(rendered));
    strings=blits=fills=0U;
}
static void check_area(uint16_t x,uint16_t y,unsigned size,uint16_t code)
{
    unsigned a,b,sx,sy; uint8_t ink;
    for(b=0U;b<size;b++) for(a=0U;a<size;a++) {
        sx=a*32U/size; sy=b*32U/size;
        ink=source_pixel(code,sx,sy);
        if(size==16U) ink|=source_pixel(code,sx+1U,sy)|source_pixel(code,sx,sy+1U)|source_pixel(code,sx+1U,sy+1U);
        assert(frame[y+b][x+a]==(ink?0x1234U:0xabcdU));
    }
}
static void test_ascii_and_mixed_width(void)
{
    unsigned reads=font_reads;
    reset_drawing();
    ui_text(10U,10U,7U,"A\nB",0x1234U,0xabcdU,0U);
    assert(strings==1U && !blits && !fills && font_reads==reads);
    assert(strcmp(rendered[0].text,"A B    ")==0);
    reset_drawing();
    ui_text(10U,10U,6U,"A\xb0\xa1" "B",0x1234U,0xabcdU,0U);
    assert(strings==2U && blits==4U && !fills && font_reads==reads+1U);
    assert(rendered[0].x==10U && strcmp(rendered[0].text,"A")==0);
    assert(rendered[1].x==34U && strcmp(rendered[1].text,"B  ")==0);
    check_area(18U,10U,16U,0xb0a1U);
    assert(frame[9U][10U]==0x5555U && frame[10U][58U]==0x5555U);
    /* The odd source stroke at (1,1) must survive OR downsampling. */
    assert(frame[10U][18U]==0x1234U);
    reads=font_reads;
    reset_drawing();
    ui_text(0U,0U,2U,"A\xb0\xa1" "B",1U,2U,0U);
    assert(strings==1U && strcmp(rendered[0].text,"A ")==0 && !blits && font_reads==reads);
    reset_drawing();
    ui_text(0U,0U,3U,"A\xb0\xa1" "B",0x1234U,0xabcdU,0U);
    assert(strings==1U && strcmp(rendered[0].text,"A")==0 && blits==4U && font_reads==reads);
}
static void test_font_sizes_and_padding(void)
{
    unsigned size,large,reads=font_reads;
    for(large=0U;large<3U;large++) {
        size=large==0U?16U:large==1U?32U:48U;
        reset_drawing();
        ui_text(80U,64U,4U,"\xb0\xa1",0x1234U,0xabcdU,(uint8_t)large);
        check_area(80U,64U,size,0xb0a1U);
        assert(blits==size/4U && strings==1U && !fills);
        assert(rendered[0].x==80U+size && strcmp(rendered[0].text,"  ")==0);
        assert(frame[64U][80U+size*2U-1U]==0xabcdU);
    }
    assert(font_reads==reads); /* The 32x32 bitmap is shared across all sizes. */
    reset_drawing();
    ui_text(784U,464U,20U,"\xb0\xa1",0x1234U,0xabcdU,0U);
    assert(blits==4U && !strings);
    reset_drawing();
    ui_text(792U,464U,20U,"\xb0\xa1",1U,2U,0U);
    assert(!blits && strings==1U && strcmp(rendered[0].text," ")==0);
    reset_drawing();
    ui_text(0U,465U,2U,"\xb0\xa1",1U,2U,0U);
    ui_text(0U,449U,2U,"\xb0\xa1",1U,2U,1U);
    ui_text(0U,433U,2U,"\xb0\xa1",1U,2U,2U);
    assert(!strings && !blits);
}
static void test_gb2312_boundaries(void)
{
    unsigned reads=font_reads;
    reset_drawing();
    ui_text(0U,0U,4U,"\xa1\xa1\xf7\xfe",0x1234U,0xabcdU,0U);
    assert(font_reads==reads+2U && blits==8U);
    check_area(0U,0U,16U,0xa1a1U); check_area(16U,0U,16U,0xf7feU);
    reads=font_reads;
    reset_drawing();
    ui_text(0U,0U,11U,"\xa0\xa1\xf8\xa1\xa1\xa0\x81\x40" "Q\xa1",1U,2U,0U);
    assert(font_reads==reads && !blits && strings==1U);
    assert(strcmp(rendered[0].text,"? ? ? ? Q? ")==0);
    assert(ui_chinese_bitmap(0xa0a1U)==0 && ui_chinese_bitmap(0xf8a1U)==0 &&
           ui_chinese_bitmap(0xa1a0U)==0 && ui_chinese_bitmap(0xa1ffU)==0);
    assert(font_reads==reads);
}
static void test_cache_and_failed_glyphs(void)
{
    unsigned i,reads; uint8_t mode;
    for(i=0U;i<70U;i++) assert(ui_chinese_bitmap((uint16_t)(0xb1a1U+i)) != 0);
    reads=font_reads;
    assert(ui_chinese_bitmap(0xb1e6U) != 0 && font_reads==reads);
    assert(ui_chinese_bitmap(0xb1a1U) != 0 && font_reads==reads+1U);
    for(mode=1U;mode<=4U;mode++) {
        failure_code=(uint16_t)(0xc0a1U+mode); failure_mode=mode; flash_error=0U;
        reset_drawing();
        ui_chinese_glyph(10U,10U,16U,failure_code,0x1234U,0xabcdU);
        assert(blits==4U && !fills);
        /* Failure is fully opaque and visibly different from an old glyph. */
        assert(frame[10U][10U]==0xabcdU && frame[12U][12U]==0x1234U);
        reads=font_reads;
        flash_error=0U; failure_mode=0U;
        assert(ui_chinese_bitmap(failure_code)!=0 && font_reads==reads+1U);
    }
    reads=font_reads;
    flash_error=1U;
    assert(ui_chinese_bitmap(0xc5a1U)==0 && font_reads==reads);
    /* A previously cached valid glyph remains available during a Flash fault. */
    assert(ui_chinese_bitmap(failure_code)!=0 && font_reads==reads);
    flash_error=0U;
}
int main(void)
{
    test_ascii_and_mixed_width();
    test_font_sizes_and_padding();
    test_gb2312_boundaries();
    test_cache_and_failed_glyphs();
    puts("Chinese UI: mixed-width clipping, GB2312 boundaries, 16/32/48 px pixels, cache eviction and failed reads passed");
    return 0;
}
