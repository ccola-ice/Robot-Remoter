#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct { const uint8_t *table; uint16_t Width,Height; } sFONT;
static const uint8_t legacy_table[95U*144U]={0U};
static sFONT Font8x16={legacy_table,8U,16U};
static sFONT Font16x32={legacy_table,16U,32U};
static sFONT Font24x48={legacy_table,24U,48U};
static sFONT *LCD_Currentfonts=&Font16x32;
static uint16_t LCD_X_LENGTH=800U,LCD_Y_LENGTH=480U;
static uint16_t CurrentTextColor=0xf81fU,CurrentBackColor=0x07e0U;
#define WIDTH_CH_CHAR 32U
#define HEIGHT_CH_CHAR 32U
#define ILI9806G_DispWindow_X_Star 0U
#define ILI9806G_DispWindow_Y_Star 0U
static uint8_t lcd_page_active;
static uint8_t ucBuffer[128],zoomBuff[48U*48U];
static uint16_t frame[480][800];
static uint8_t fixture[48U*12U],native_enabled=1U;
static unsigned blits,lookups,windows,legacy_pixels,flash_reads,zooms,legacy_chars;
static uint16_t last_code,last_size;
static uint8_t last_bold;
static struct { uint16_t x,y,w,h; } band_log[96];

/* Alternating four coverage levels; poison unused bits to catch row-stride errors. */
static uint8_t fixture_level(unsigned code,unsigned x,unsigned y,unsigned bold)
{ return code==' ' ? 0U : (uint8_t)((x+y+bold)%4U); }
static const uint8_t *UI_FontBitmap(uint16_t code,uint16_t size,uint8_t bold)
{
    unsigned width=code<128U?size/2U:size,stride=(width+3U)/4U,x,y,shift;
    lookups++; last_code=code; last_size=size; last_bold=bold;
    if(!native_enabled || (code!='A' && code!='B' && code!=' ' && code!=0xb0a1U)) return NULL;
    assert(size==16U || size==20U || size==32U || size==48U);
    memset(fixture,255,sizeof(fixture));
    for(y=0U;y<size;y++) for(x=0U;x<width;x++) {
        shift=6U-2U*(x%4U);
        fixture[y*stride+x/4U]&=(uint8_t)~(3U<<shift);
        fixture[y*stride+x/4U]|=(uint8_t)(fixture_level(code,x,y,bold)<<shift);
    }
    return fixture;
}
static void LCD_BlitRGB565(uint16_t x,uint16_t y,uint16_t width,uint16_t height,const uint16_t *pixels)
{
    unsigned row,column;
    assert(blits<96U && height==4U && width<=48U && pixels);
    assert((uint32_t)x+width<=LCD_X_LENGTH && (uint32_t)y+height<=LCD_Y_LENGTH);
    assert((uint32_t)x+width<=800U && (uint32_t)y+height<=480U);
    band_log[blits].x=x; band_log[blits].y=y;
    band_log[blits].w=width; band_log[blits].h=height; blits++;
    for(row=0U;row<height;row++) for(column=0U;column<width;column++)
        frame[y+row][x+column]=pixels[row*width+column];
}
static uint8_t lcd_page_draw_ascii(uint16_t x,uint16_t y,uint8_t *glyph)
{ (void)x; (void)y; (void)glyph; return 0U; }
static void ILI9806G_OpenWindow(uint16_t x,uint16_t y,uint16_t width,uint16_t height)
{ (void)x; (void)y; (void)width; (void)height; windows++; }
static void lcd_begin_pixels(void) {}
static void lcd_write_pixel(uint16_t color) { (void)color; legacy_pixels++; }
static int GetGBKCode(uint8_t *data,uint16_t code)
{ (void)code; memset(data,0,128U); flash_reads++; return 0; }
static void ILI9806G_zoomChar(uint16_t a,uint16_t b,uint16_t c,uint16_t d,uint8_t *input,uint8_t *output,uint8_t mode)
{ (void)a; (void)b; (void)c; (void)d; (void)input; (void)output; (void)mode; zooms++; }
static void ILI9806G_DrawChar_Ex(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint8_t *bitmap,uint16_t mode)
{ (void)x; (void)y; (void)w; (void)h; (void)bitmap; (void)mode; legacy_chars++; }
#include "lcd_native_font_impl.inc"

static void reset(void)
{
    memset(frame,0x5a,sizeof(frame));
    blits=lookups=windows=legacy_pixels=flash_reads=zooms=legacy_chars=0U;
    native_enabled=1U; LCD_X_LENGTH=800U; LCD_Y_LENGTH=480U;
    CurrentTextColor=0xf81fU; CurrentBackColor=0x07e0U;
}
static void check_glyph(unsigned x,unsigned y,unsigned code,unsigned size,unsigned bold,unsigned inverse)
{
    static const uint16_t colors[4]={0x07e0U,0x554aU,0xaab5U,0xf81fU};
    unsigned row,column,width=code<128U?size/2U:size,level;
    for(row=0U;row<size;row++) for(column=0U;column<width;column++) {
        level=fixture_level(code,column,row,bold);
        assert(frame[y+row][x+column]==colors[inverse?3U-level:level]);
    }
}
static void renderer_sizes_and_color(void)
{
    static const unsigned sizes[]={16U,20U,32U,48U};
    static const unsigned codes[]={'A',0xb0a1U};
    unsigned s,c,bold,width,i,x,y;
    for(s=0U;s<4U;s++) for(c=0U;c<2U;c++) for(bold=0U;bold<2U;bold++) {
        reset(); width=codes[c]<128U?sizes[s]/2U:sizes[s];
        assert(LCD_DrawFontGlyph(40U,48U,codes[c],sizes[s],bold,CurrentTextColor,CurrentBackColor)==1U);
        assert(lookups==1U && last_code==codes[c] && last_size==sizes[s] && last_bold==bold);
        assert(blits==sizes[s]/4U);
        for(i=0U;i<blits;i++) assert(band_log[i].x==40U && band_log[i].y==48U+4U*i && band_log[i].w==width);
        check_glyph(40U,48U,codes[c],sizes[s],bold,0U);
        for(y=0U;y<480U;y++) for(x=0U;x<800U;x++)
            if(x<40U || x>=40U+width || y<48U || y>=48U+sizes[s]) assert(frame[y][x]==0x5a5aU);
    }
    reset(); assert(LCD_DrawFontGlyph(0U,0U,'A',16U,0U,0xffffU,0U));
    assert(frame[0][0]==0U && frame[0][1]==0x52aaU && frame[0][2]==0xad55U && frame[0][3]==0xffffU);
    assert(LCD_DrawFontGlyph(0U,0U,' ',16U,0U,0xffffU,0U));
    for(y=0U;y<16U;y++) for(x=0U;x<8U;x++) assert(frame[y][x]==0U);
}
static void boundaries_and_fallback(void)
{
    static const unsigned sizes[]={16U,20U,32U,48U};
    unsigned i,code,width,size;
    for(i=0U;i<4U;i++) for(code=0U;code<2U;code++) {
        reset(); size=sizes[i]; width=code?size:size/2U;
        assert(LCD_DrawFontGlyph(800U-width,480U-size,code?0xb0a1U:'A',size,0U,1U,2U));
        assert(blits==size/4U); blits=0U;
        assert(LCD_DrawFontGlyph(801U-width,480U-size,code?0xb0a1U:'A',size,0U,1U,2U));
        assert(LCD_DrawFontGlyph(800U-width,481U-size,code?0xb0a1U:'A',size,0U,1U,2U));
        assert(LCD_DrawFontGlyph(65535U,65535U,code?0xb0a1U:'A',size,0U,1U,2U));
        assert(!blits);
    }
    reset(); assert(!LCD_DrawFontGlyph(0U,0U,'A',24U,0U,1U,2U) && !lookups);
    assert(!LCD_DrawFontGlyph(0U,0U,0xb0a2U,32U,0U,1U,2U) && !blits);
    assert(!LCD_DrawFontGlyph(0U,0U,0U,16U,0U,1U,2U) && !blits);
    LCD_X_LENGTH=480U; LCD_Y_LENGTH=800U;
    assert(LCD_DrawFontGlyph(473U,0U,'A',16U,0U,1U,2U) && !blits);
}
static void entry_points(void)
{
    unsigned i;
    sFONT special={legacy_table,15U,32U};
    sFONT *fonts[]={&Font8x16,&Font16x32,&Font24x48};
    uint8_t mixed[]={'A',0xb0U,0xa1U,'B',0U};
    for(i=0U;i<3U;i++) {
        reset(); LCD_Currentfonts=fonts[i]; ILI9806G_DispChar_EN(40U,48U,'A');
        assert(last_size==fonts[i]->Height && !last_bold && !windows && !legacy_pixels);
        check_glyph(40U,48U,'A',fonts[i]->Height,0U,0U);
    }
    reset(); LCD_Currentfonts=&special; ILI9806G_DispChar_EN(40U,48U,'A');
    assert(!lookups && windows==1U && legacy_pixels);
    reset(); LCD_Currentfonts=&Font16x32; ILI9806G_DispChar_EN(65535U,0U,'A');
    assert(lookups==1U && !blits && !windows && !legacy_pixels);
    reset(); ILI9806G_DispChar_EN(40U,48U,(char)0xffU);
    assert(last_code=='?' && windows==1U && legacy_pixels==512U);
    reset(); ILI9806G_DispChar_EN(40U,48U,'\n');
    assert(last_code=='?' && windows==1U && legacy_pixels==512U);
    reset(); ILI9806G_DispChar_CH(40U,48U,0xb0a1U);
    assert(last_size==32U && !flash_reads && !windows); check_glyph(40U,48U,0xb0a1U,32U,0U,0U);
    reset(); ILI9806G_DispChar_CH(769U,449U,0xb0a1U);
    assert(lookups==1U && !blits && !windows && !flash_reads);
    reset(); ILI9806G_DispChar_CH(40U,48U,0xb0a2U);
    assert(!blits && flash_reads==1U && windows==1U && legacy_pixels==1024U);
    reset(); LCD_DispString_EN_Bold(40U,48U,"AB");
    assert(lookups==2U && last_bold==1U && blits==16U);
    check_glyph(40U,48U,'A',32U,1U,0U); check_glyph(56U,48U,'B',32U,1U,0U);
    reset(); ILI9806G_DispString_EN_CH(40U,48U,(char *)mixed);
    assert(lookups==3U && !flash_reads && !windows);
    check_glyph(40U,48U,'A',32U,0U,0U); check_glyph(56U,48U,0xb0a1U,32U,0U,0U); check_glyph(88U,48U,'B',32U,0U,0U);
    reset(); ILI9806G_DispStringLine_EN_CH(48U,(char *)mixed);
    assert(lookups==3U && !flash_reads && !windows);
    check_glyph(0U,48U,'A',32U,0U,0U); check_glyph(16U,48U,0xb0a1U,32U,0U,0U); check_glyph(48U,48U,'B',32U,0U,0U);
    reset(); ILI9806G_DispString_EN_CH_YDir(40U,48U,(char *)mixed);
    assert(lookups==3U && !flash_reads && !windows);
    check_glyph(40U,48U,'A',32U,0U,0U); check_glyph(40U,80U,0xb0a1U,32U,0U,0U); check_glyph(40U,112U,'B',32U,0U,0U);
    reset(); ILI9806G_DisplayStringEx(40U,48U,20U,20U,mixed,0U);
    assert(lookups==3U && blits==15U && !zooms && !flash_reads);
    check_glyph(40U,48U,'A',20U,0U,0U); check_glyph(50U,48U,0xb0a1U,20U,0U,0U); check_glyph(70U,48U,'B',20U,0U,0U);
    reset(); ILI9806G_DisplayStringEx(40U,48U,20U,20U,mixed,1U);
    check_glyph(40U,48U,'A',20U,0U,1U); check_glyph(50U,48U,0xb0a1U,20U,0U,1U);
    reset(); ILI9806G_DisplayStringEx(40U,48U,24U,20U,mixed,0U);
    assert(!lookups && zooms==3U && legacy_chars==3U && flash_reads==1U);
    reset(); ILI9806G_DisplayStringEx(40U,48U,20U,20U,mixed,2U);
    assert(!lookups && zooms==3U && legacy_chars==3U);
    reset(); native_enabled=0U; ILI9806G_DisplayStringEx(40U,48U,20U,20U,mixed,0U);
    assert(lookups==3U && !blits && zooms==3U && legacy_chars==3U && flash_reads==1U);
    reset(); ILI9806G_DisplayStringEx(790U,460U,20U,20U,(uint8_t *)"AB",0U);
    /* Preserve the legacy initial full-cell wrap and bottom-to-top wrap. */
    check_glyph(0U,0U,'A',20U,0U,0U); check_glyph(10U,0U,'B',20U,0U,0U);
    reset(); ILI9806G_DisplayStringEx_YDir(40U,48U,20U,20U,mixed,1U);
    assert(lookups==3U && !zooms && !flash_reads);
    check_glyph(40U,48U,'A',20U,0U,1U); check_glyph(40U,68U,0xb0a1U,20U,0U,1U); check_glyph(40U,88U,'B',20U,0U,1U);
}
int main(void)
{
    renderer_sizes_and_color(); boundaries_and_fallback(); entry_points();
    puts("Native LCD font: 16/20/32/48px 2bpp stride, four RGB565 levels, opaque cells, bounds, bold and legacy entry/fallback/inversion/wrap passed");
    return 0;
}
