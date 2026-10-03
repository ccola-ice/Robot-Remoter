#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_W 300U
#define SCREEN_H 300U
#define WIDTH_CH_CHAR 32U
#define HEIGHT_CH_CHAR 32U
#define ILI9806G_DispWindow_X_Star 0U
#define ILI9806G_DispWindow_Y_Star 0U
typedef struct { const uint8_t *table; uint16_t Width, Height; } sFONT;
static uint8_t ascii_bitmap[95U * 64U];
static sFONT Font16x32 = { ascii_bitmap,16U,32U };
static uint8_t ucBuffer[128U], chinese_bitmap[128U];
static uint16_t LCD_X_LENGTH = SCREEN_W, LCD_Y_LENGTH = SCREEN_H;
static uint16_t CurrentTextColor = 0xf81fU, CurrentBackColor = 0x07e0U;
static uint16_t actual[SCREEN_W * SCREEN_H], expected[SCREEN_W * SCREEN_H];
static uint16_t window_x, window_y, window_w, window_h;
static unsigned writes, windows, cursor;

static void ILI9806G_OpenWindow(uint16_t x,uint16_t y,uint16_t w,uint16_t h)
{
    assert(w && h && (uint32_t)x+w<=SCREEN_W && (uint32_t)y+h<=SCREEN_H);
    window_x=x; window_y=y; window_w=w; window_h=h; windows++;
}
static void lcd_begin_pixels(void) { cursor=0U; }
static void lcd_write_pixel(uint16_t color)
{
    assert(cursor<(unsigned)window_w*window_h);
    actual[(window_y+cursor/window_w)*SCREEN_W+window_x+cursor%window_w]=color;
    cursor++; writes++;
}
static uint8_t LCD_DrawFontGlyph(uint16_t x,uint16_t y,uint16_t code,uint16_t size,
                                uint8_t bold,uint16_t foreground,uint16_t background)
{
    (void)x; (void)y; (void)code; (void)size;
    (void)bold; (void)foreground; (void)background;
    return 0U;
}
static int GetGBKCode(uint8_t *data,uint16_t code)
{
    assert(code==0xb0a1U);
    memcpy(data,chinese_bitmap,sizeof(chinese_bitmap));
    return 0;
}
#include "lcd_scaled_font_impl.inc"

static void reset(void)
{
    unsigned i;
    writes=windows=cursor=0U;
    for(i=0U;i<SCREEN_W*SCREEN_H;i++)
        actual[i]=expected[i]=(uint16_t)(i*71U+0x1593U);
}

/* Independent legacy reference: expand packed source, then scale into a full
 * byte-per-pixel bitmap before drawing. Buffers exist in this host test only. */
static void reference(uint16_t x,uint16_t y,unsigned iw,unsigned ih,unsigned ow,unsigned oh,
                       const uint8_t *bitmap,uint16_t mode)
{
    uint8_t expanded[48U*48U], scaled[128U*128U];
    unsigned i,j,sx,sy;
    for(i=0U;i<iw*ih;i++) expanded[i]=(bitmap[i/8U]>>(7U-i%8U))&1U;
    for(j=0U;j<oh;j++) {
        sy=(j*((ih*65536U)/oh+1U))/65536U;
        if(sy>=ih) sy=ih-1U;
        for(i=0U;i<ow;i++) {
            sx=(i*((iw*65536U)/ow+1U))/65536U;
            if(sx>=iw) sx=iw-1U;
            scaled[j*ow+i]=expanded[sy*iw+sx];
        }
    }
    for(j=0U;j<oh && (unsigned)y+j<SCREEN_H;j++)
        for(i=0U;i<ow && (unsigned)x+i<SCREEN_W;i++)
            expected[(y+j)*SCREEN_W+x+i]=scaled[j*ow+i]==mode?CurrentBackColor:CurrentTextColor;
}

static void fixture(uint8_t *bitmap,unsigned width,unsigned height,unsigned pattern)
{
    unsigned x,y,bit;
    memset(bitmap,0,(width*height+7U)/8U);
    for(y=0U;y<height;y++) for(x=0U;x<width;x++) {
        bit=y*width+x;
        if(pattern==1U || (pattern==2U && ((x*3U+y*5U)%7U<3U)) ||
           (pattern==3U && (x==0U || y==0U || x+1U==width || y+1U==height)))
            bitmap[bit/8U]|=(uint8_t)(0x80U>>(bit%8U));
    }
}

static unsigned sampled_pixels(void)
{
    static const uint16_t sources[][2]={{3,8},{8,1},{8,16},{15,16},{16,24},{16,32},{24,48},{32,32},{48,48}};
    static const uint16_t targets[][2]={{1,1},{2,2},{8,16},{20,20},{24,32},{32,32},
        {48,48},{64,128},{128,1},{1,128},{127,128},{128,128},{20,200},{200,20},
        {8192,2},{2,8192},{1,16384},{16384,1},{1,257}};
    static const uint16_t origins[][2]={{0,0},{17,13},{SCREEN_W-5U,SCREEN_H-5U},
        {SCREEN_W-1U,SCREEN_H-1U},{SCREEN_W,0},{0,SCREEN_H},{65535,65535}};
    static const uint16_t modes[]={0U,1U,2U,65535U};
    uint8_t guarded[290U];
    unsigned s,t,o,m,p,total=0U;
    for(s=0U;s<sizeof(sources)/sizeof(*sources);s++)
        for(t=0U;t<sizeof(targets)/sizeof(*targets);t++)
            for(o=0U;o<sizeof(origins)/sizeof(*origins);o++)
                for(m=0U;m<sizeof(modes)/sizeof(*modes);m++)
                    for(p=0U;p<4U;p++) {
                        memset(guarded,0xa5,sizeof(guarded));
                        fixture(guarded+1U,sources[s][0],sources[s][1],p);
                        reset();
                        reference(origins[o][0],origins[o][1],sources[s][0],sources[s][1],
                                  targets[t][0],targets[t][1],guarded+1U,modes[m]);
                        lcd_draw_scaled_glyph(origins[o][0],origins[o][1],sources[s][0],sources[s][1],
                                  targets[t][0],targets[t][1],guarded+1U,modes[m]);
                        assert(memcmp(actual,expected,sizeof(actual))==0);
                        assert(guarded[0]==0xa5U && guarded[sizeof(guarded)-1U]==0xa5U);
                        total++;
                    }
    return total;
}

static void public_paths(void)
{
    static const uint16_t sizes[][2]={{2,1},{20,20},{24,32},{32,32},{128,128},{20,200}};
    uint8_t mixed[]={'A',0xb0U,0xa1U,'B',0U};
    uint8_t broken[]={0xb0U,0U}, invalid[]={0x7fU,0U};
    unsigned i,mode,x,y,w,h;
    fixture(ascii_bitmap+('A'-32U)*64U,16U,32U,2U);
    fixture(ascii_bitmap+('B'-32U)*64U,16U,32U,3U);
    fixture(ascii_bitmap+('?' -32U)*64U,16U,32U,1U);
    fixture(chinese_bitmap,32U,32U,3U);
    for(i=0U;i<sizeof(sizes)/sizeof(*sizes);i++) for(mode=0U;mode<3U;mode++) {
        w=sizes[i][0]; h=sizes[i][1];
        reset();
        reference(0U,0U,16U,32U,w/2U,h,ascii_bitmap+('A'-32U)*64U,mode);
        reference(h>SCREEN_H?0U:w/2U,0U,32U,32U,w,h,chinese_bitmap,mode);
        x=w+w/2U; y=0U;
        if(x+w>SCREEN_W) { x=0U; y+=h; }
        if(y+h>SCREEN_H) { x=0U; y=0U; }
        reference(x,y,16U,32U,w/2U,h,ascii_bitmap+('B'-32U)*64U,mode);
        ILI9806G_DisplayStringEx(0U,0U,w,h,mixed,mode);
        assert(memcmp(actual,expected,sizeof(actual))==0);
    }
    reset();
    reference(3U,7U,16U,24U,10U,20U,ascii_bitmap+('A'-32U)*64U,1U);
    reference(3U,27U,32U,32U,20U,20U,chinese_bitmap,1U);
    reference(3U,47U,16U,24U,10U,20U,ascii_bitmap+('B'-32U)*64U,1U);
    ILI9806G_DisplayStringEx_YDir(3U,7U,20U,20U,mixed,1U);
    assert(memcmp(actual,expected,sizeof(actual))==0);
    reset();
    ILI9806G_DisplayStringEx(0U,0U,20U,20U,broken,0U);
    ILI9806G_DisplayStringEx_YDir(0U,0U,20U,20U,broken,0U);
    assert(!writes && !windows);
    reference(0U,0U,16U,32U,10U,20U,ascii_bitmap+('?' -32U)*64U,0U);
    ILI9806G_DisplayStringEx(0U,0U,20U,20U,invalid,0U);
    assert(memcmp(actual,expected,sizeof(actual))==0);
}

static void invalid_sizes(void)
{
    static const uint16_t dimensions[][4]={{0,32,20,20},{32,0,20,20},{49,32,20,20},
        {48,49,20,20},{65535,65535,20,20},{32,32,0,20},{32,32,20,0},
        {32,32,129,128},{32,32,128,129},{32,32,65535,65535}};
    static const uint16_t invalid[][2]={{0,20},{20,0},{129,128},{128,129},{65535,65535}};
    unsigned i;
    reset();
    for(i=0U;i<sizeof(dimensions)/sizeof(*dimensions);i++)
        lcd_draw_scaled_glyph(0U,0U,dimensions[i][0],dimensions[i][1],
                              dimensions[i][2],dimensions[i][3],chinese_bitmap,0U);
    lcd_draw_scaled_glyph(0U,0U,32U,32U,20U,20U,NULL,0U);
    for(i=0U;i<sizeof(invalid)/sizeof(*invalid);i++) {
        ILI9806G_DisplayStringEx(0U,0U,invalid[i][0],invalid[i][1],(uint8_t *)"A",0U);
        ILI9806G_DisplayStringEx_YDir(0U,0U,invalid[i][0],invalid[i][1],(uint8_t *)"A",0U);
    }
    ILI9806G_DisplayStringEx(0U,0U,20U,20U,NULL,0U);
    ILI9806G_DisplayStringEx_YDir(0U,0U,20U,20U,NULL,0U);
    assert(!writes && !windows && memcmp(actual,expected,sizeof(actual))==0);
}

int main(void)
{
    unsigned cases=sampled_pixels();
    public_paths(); invalid_sizes();
    printf("LCD cached-row glyphs: %u legacy pixel comparisons, packed/byte-aligned sources, ASCII/Chinese/YDir, inversion, clipping, 128px and invalid bounds passed\n",cases);
    return 0;
}
