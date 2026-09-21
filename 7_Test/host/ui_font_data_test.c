#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ui_font_data.inc"
#include "ui_font_expected.inc"

static uint8_t expected_code(uint16_t code)
{
    unsigned i;
    if(code >= 32U && code <= 126U) return 1U;
    for(i=0U;i<sizeof(expected_codes)/sizeof(expected_codes[0]);i++)
        if(expected_codes[i] == code) return 1U;
    return 0U;
}

static unsigned check_glyph(uint16_t code, unsigned size, unsigned bold)
{
    const uint8_t *bitmap = UI_FontBitmap(code,(uint16_t)size,(uint8_t)bold);
    unsigned width = code < 128U ? size/2U : size;
    unsigned stride = (width+3U)/4U, x, y, ink = 0U, levels = 0U;
    assert(bitmap);
    assert(bitmap == UI_FontBitmap(code,(uint16_t)size,(uint8_t)bold));
    for(y=0U;y<size;y++) {
        for(x=0U;x<width;x++) {
            unsigned value = (bitmap[y*stride+x/4U] >> (6U-2U*(x%4U))) & 3U;
            levels |= 1U << value;
            ink += value != 0U;
        }
        if(width%4U)
            assert((bitmap[y*stride+stride-1U] & ((1U << (2U*(4U-width%4U)))-1U)) == 0U);
    }
    if(code == 32U || code == 0xa1a1U) assert(!ink);
    else assert(ink);
    return levels;
}

int main(void)
{
    static const unsigned sizes[] = {16U,20U,32U,48U};
    unsigned i,j,code,size,levels,changed=0U;
    assert(sizeof(expected_codes)/sizeof(expected_codes[0]) > 300U);
    assert(UI_FontBitmap(0xb2ceU,32U,0U)); /* actual label character: parameter */
    assert(UI_FontBitmap(0xceb4U,16U,0U)); /* service label: not yet tested */
    for(i=0U;i<sizeof(sizes)/sizeof(sizes[0]);i++) {
        size=sizes[i]; levels=0U;
        for(code=32U;code<=126U;code++) levels |= check_glyph((uint16_t)code,size,0U);
        for(j=0U;j<sizeof(expected_codes)/sizeof(expected_codes[0]);j++) {
            if(j) assert(expected_codes[j-1U] < expected_codes[j]);
            levels |= check_glyph(expected_codes[j],size,0U);
        }
        assert(levels == 15U); /* four coverage levels exist at every native size */
        for(code=0U;code<=0xffffU;code++) {
            const uint8_t *regular=UI_FontBitmap((uint16_t)code,(uint16_t)size,0U);
            assert((regular != NULL) == expected_code((uint16_t)code));
            if(size != 32U) assert(regular == UI_FontBitmap((uint16_t)code,(uint16_t)size,255U));
        }
    }
    levels=0U;
    for(code=32U;code<=126U;code++) {
        const uint8_t *regular=UI_FontBitmap((uint16_t)code,32U,0U);
        const uint8_t *bold=UI_FontBitmap((uint16_t)code,32U,1U);
        levels |= check_glyph((uint16_t)code,32U,1U);
        assert(bold == UI_FontBitmap((uint16_t)code,32U,255U));
        changed += memcmp(regular,bold,128U) != 0;
    }
    for(j=0U;j<sizeof(expected_codes)/sizeof(expected_codes[0]);j++) {
        code=expected_codes[j];
        levels |= check_glyph((uint16_t)code,32U,1U);
        changed += memcmp(UI_FontBitmap((uint16_t)code,32U,0U),UI_FontBitmap((uint16_t)code,32U,1U),256U) != 0;
    }
    assert(levels==15U && changed>300U);
    for(size=0U;size<64U;size++) {
        if(size==16U || size==20U || size==32U || size==48U) continue;
        assert(!UI_FontBitmap('A',(uint16_t)size,0U));
        assert(!UI_FontBitmap(0xb2ceU,(uint16_t)size,1U));
    }
    assert(!UI_FontBitmap('A',65535U,1U));
    puts("Native font assets: all source glyphs, exhaustive 16-bit lookup, native sizes, four coverage levels, bold faces and zero row padding passed");
    return 0;
}
