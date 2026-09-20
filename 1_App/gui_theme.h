#ifndef GUI_THEME_H
#define GUI_THEME_H

/* Small code-drawn UI primitives. Include after the LCD/font declarations.
 * No reference-project assets, heap allocation or extra framebuffer. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define UI_RGB(r,g,b) ((uint16_t)((((r) & 248U) << 8) | (((g) & 252U) << 3) | ((b) >> 3)))
#define UI_BG      UI_RGB(240U,244U,248U)
#define UI_SURFACE UI_RGB(255U,255U,255U)
#define UI_INK     UI_RGB(28U,44U,64U)
#define UI_MUTED   UI_RGB(100U,117U,137U)
#define UI_LINE    UI_RGB(215U,225U,234U)
#define UI_ACCENT  UI_RGB(34U,132U,230U)
#define UI_TINT    UI_RGB(226U,240U,254U)
#define UI_GREEN   UI_RGB(19U,150U,116U)
#define UI_RED     UI_RGB(205U,61U,78U)
#define UI_AMBER   UI_RGB(188U,118U,18U)
#define UI_TRACK   UI_RGB(224U,232U,240U)

enum { UI_ICON_CONTROL, UI_ICON_CHANNEL, UI_ICON_SWITCH, UI_ICON_SETTINGS,
       UI_ICON_RADIO, UI_ICON_CHIP, UI_ICON_IMU, UI_ICON_GPS, UI_ICON_FOLDER,
       UI_ICON_TOOLS, UI_ICON_MEMORY };

static __inline void ui_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if(!w || !h) return;
    ILI9806G_Fill(x, y, x + w, y + h, color);
}

static __inline void ui_text(uint16_t x, uint16_t y, uint8_t columns,
    const char *text, uint16_t fg, uint16_t bg, uint8_t large)
{
    char field[101];
    uint16_t width = large == 2U ? 24U : large == 1U ? 16U : 8U;
    uint16_t height = width * 2U;
    uint8_t count, i;
    if(x >= 800U || y + height > 480U) return;
    count = (uint8_t)((800U - x) / width);
    if(columns > count) columns = count;
    if(columns > 100U) columns = 100U;
    if(!columns) return;
    memset(field, ' ', columns);
    if(text) for(i = 0U; i < columns && text[i]; i++)
        field[i] = (text[i] >= 32 && text[i] <= 126) ? text[i] : ' ';
    field[columns] = '\0';
    LCD_SetFont(large == 2U ? &Font24x48 : large == 1U ? &Font16x32 : &Font8x16);
    LCD_SetBackColor(bg); LCD_SetTextColor(fg);
    ILI9806G_DispString_EN(x, y, field);
}

static __inline void ui_round_rect(uint16_t x, uint16_t y, uint16_t w,
    uint16_t h, uint16_t radius, uint16_t color)
{
    uint16_t row, inset;
    int32_t dy, edge;
    if(!w || !h) return;
    if(radius > w / 2U) radius = w / 2U;
    if(radius > h / 2U) radius = h / 2U;
    if(!radius) { ui_fill(x,y,w,h,color); return; }
    ui_fill(x, y + radius, w, h - radius * 2U, color);
    /* Scan lines avoid overlapping circle blits at rounded corners. */
    for(row = 0U; row < radius; row++) {
        dy = (int32_t)radius - row - 1;
        inset = 0U;
        do {
            edge = (int32_t)radius - inset - 1;
            if(edge * edge + dy * dy <= (int32_t)radius * radius) break;
            inset++;
        } while(inset < radius);
        ui_fill(x + inset, y + row, w - 2U * inset, 1U, color);
        ui_fill(x + inset, y + h - row - 1U, w - 2U * inset, 1U, color);
    }
}

static __inline void ui_shell(const char *title, const char *subtitle, const char *section)
{
    ui_fill(0U,0U,800U,480U,UI_BG);
    ui_fill(0U,0U,800U,32U,UI_INK);
    ui_text(20U,8U,20U,"ROBOT REMOTE",UI_SURFACE,UI_INK,0U);
    ui_text(208U,8U,32U,section,UI_SURFACE,UI_INK,0U);
    ui_text(24U,48U,27U,title,UI_INK,UI_BG,1U);
    ui_text(464U,60U,39U,subtitle,UI_MUTED,UI_BG,0U);
}

static __inline void ui_footer(const char *left, const char *right)
{
    ui_fill(0U,448U,800U,32U,UI_SURFACE);
    ui_fill(0U,448U,800U,1U,UI_LINE);
    ui_text(20U,456U,59U,left,UI_MUTED,UI_SURFACE,0U);
    ui_text(524U,456U,32U,right,UI_ACCENT,UI_SURFACE,0U);
}

/* Symmetric meter: only the changed span is touched during live updates. */
static __inline void ui_bipolar_bar(uint16_t x, uint16_t y, uint16_t w,
    uint16_t h, int16_t value, int16_t *previous, uint8_t first)
{
    int16_t pixel, old;
    uint16_t middle = w / 2U;
    if(w < 2U || h == 0U) return;
    if(value > 1000) value = 1000;
    if(value < -1000) value = -1000;
    pixel = (int16_t)((int32_t)value * ((int32_t)middle - 1L) / 1000L);
    old = first ? 0 : *previous;
    if(first) ui_fill(x,y,w,h,UI_TRACK);
    if(first || pixel != old) {
        if(old >= 0 && pixel >= 0) {
            if(pixel > old) ui_fill(x+middle+old,y,(uint16_t)(pixel-old),h,UI_ACCENT);
            else if(pixel < old) ui_fill(x+middle+pixel,y,(uint16_t)(old-pixel),h,UI_TRACK);
        } else if(old <= 0 && pixel <= 0) {
            if(pixel < old) ui_fill((uint16_t)(x+middle+pixel),y,(uint16_t)(old-pixel),h,UI_GREEN);
            else if(pixel > old) ui_fill((uint16_t)(x+middle+old),y,(uint16_t)(pixel-old),h,UI_TRACK);
        } else {
            if(old > 0) ui_fill(x+middle,y,(uint16_t)old,h,UI_TRACK);
            else ui_fill((uint16_t)(x+middle+old),y,(uint16_t)-old,h,UI_TRACK);
            if(pixel > 0) ui_fill(x+middle,y,(uint16_t)pixel,h,UI_ACCENT);
            else ui_fill((uint16_t)(x+middle+pixel),y,(uint16_t)-pixel,h,UI_GREEN);
        }
        ui_fill(x + middle,y,1U,h,UI_MUTED);
        *previous = pixel;
    }
}

/* Original line icons use a fixed 48 px drawing cell, scaled by size / 48. */
static __inline void ui_icon(uint16_t x, uint16_t y, uint16_t size,
    uint8_t id, uint16_t fg, uint16_t bg)
{
    uint16_t s = size < 48U ? 1U : size / 48U;
    uint16_t cx = x + 24U * s, cy = y + 24U * s;
    uint8_t i;
    LCD_SetTextColor(fg);
#define UI_L(a,b,c,d) ILI9806G_DrawLine(x+(a)*s,y+(b)*s,x+(c)*s,y+(d)*s)
#define UI_R(a,b,c,d) ILI9806G_DrawRectangle(x+(a)*s,y+(b)*s,(c)*s,(d)*s,0U)
    switch(id) {
    case UI_ICON_CONTROL:
        UI_R(5U,13U,38U,25U); UI_L(13U,20U,13U,32U); UI_L(7U,26U,19U,26U);
        ILI9806G_DrawCircle(x+32U*s,y+22U*s,3U*s,0U);
        ILI9806G_DrawCircle(x+37U*s,y+30U*s,3U*s,0U); break;
    case UI_ICON_CHANNEL: case UI_ICON_SETTINGS:
        for(i=0U;i<3U;i++) {
            UI_L(8U+16U*i,7U,8U+16U*i,41U);
            ui_fill(x+(4U+16U*i)*s,y+(i==1U?28U:14U)*s,9U*s,7U*s,fg);
        } break;
    case UI_ICON_SWITCH:
        ui_round_rect(x+4U*s,y+9U*s,40U*s,13U*s,6U*s,fg);
        LCD_SetTextColor(bg);
        ILI9806G_DrawCircle(x+35U*s,y+15U*s,4U*s,1U);
        LCD_SetTextColor(fg);
        UI_R(4U,28U,40U,13U); ILI9806G_DrawCircle(x+12U*s,y+34U*s,4U*s,1U); break;
    case UI_ICON_RADIO:
        UI_L(24U,20U,24U,42U); UI_L(17U,42U,31U,42U);
        ILI9806G_DrawCircle(cx,y+16U*s,4U*s,1U);
        UI_L(13U,8U,8U,16U); UI_L(8U,16U,13U,24U);
        UI_L(35U,8U,40U,16U); UI_L(40U,16U,35U,24U);
        UI_L(6U,3U,0U,16U); UI_L(0U,16U,6U,29U);
        UI_L(42U,3U,48U,16U); UI_L(48U,16U,42U,29U); break;
    case UI_ICON_CHIP: case UI_ICON_MEMORY:
        UI_R(12U,12U,24U,24U); UI_R(17U,17U,14U,14U);
        for(i=0U;i<4U;i++) { UI_L(15U+6U*i,5U,15U+6U*i,11U); UI_L(15U+6U*i,37U,15U+6U*i,43U);
            UI_L(5U,15U+6U*i,11U,15U+6U*i); UI_L(37U,15U+6U*i,43U,15U+6U*i); } break;
    case UI_ICON_IMU:
        ILI9806G_DrawCircle(cx,cy,19U*s,0U); UI_L(5U,24U,43U,24U);
        UI_L(24U,5U,24U,43U); UI_L(10U,34U,38U,14U); break;
    case UI_ICON_GPS:
        ILI9806G_DrawCircle(cx,y+18U*s,12U*s,0U);
        ILI9806G_DrawCircle(cx,y+18U*s,4U*s,0U);
        UI_L(14U,25U,24U,43U); UI_L(34U,25U,24U,43U); break;
    case UI_ICON_FOLDER:
        UI_L(4U,13U,19U,13U); UI_L(19U,13U,25U,19U); UI_L(25U,19U,43U,19U);
        UI_L(4U,13U,4U,39U); UI_L(4U,39U,43U,39U); UI_L(43U,19U,43U,39U); UI_L(4U,23U,43U,23U); break;
    default:
        UI_L(9U,40U,35U,14U); UI_L(14U,43U,39U,18U);
        UI_L(9U,40U,14U,43U); UI_L(35U,14U,31U,6U);
        UI_L(31U,6U,39U,10U); UI_L(39U,10U,43U,18U); UI_L(43U,18U,39U,18U); break;
    }
#undef UI_L
#undef UI_R
}
#endif
