#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"
#include "param.h"

typedef enum { MENU_KEY_NONE, MENU_KEY_LEFT, MENU_KEY_RIGHT, MENU_KEY_OK, MENU_KEY_BACK } MenuKey;
enum { MENU_PAGE_CATEGORY = 1 };
volatile param_Config param;
static param_Config param_edit, param_edit_backup, saved;
static const uint8_t nrf_power_register[4] = {0x09U, 0x0bU, 0x0dU, 0x0fU};
static uint8_t param_selected_item, param_first_visible, param_editing, param_dirty, param_dirty_before_edit;
static uint16_t param_revision;
static char param_status[80];
static uint8_t page_dirty, page_changed, current_page;
static unsigned save_calls, apply_calls;
static uint8_t save_error;

uint8_t write_param(void)
{
    save_calls++;
    if(save_error) return 1U;
    memcpy(&saved, (const void *)&param, sizeof(saved));
    return 0U;
}
static void nrf24l01_apply_settings(uint8_t enable, uint8_t channel, uint8_t power, uint8_t rate)
{
    assert(enable == param.NRF_Mode && channel == param.NRF_Channel);
    assert(power == param.NRF_Power && rate == param.NRF_DataRate);
    apply_calls++;
}
#include "param_menu_impl.inc"

static void assert_text(const char *text, size_t capacity)
{
    size_t i;
    assert(memchr(text, 0, capacity) != NULL);
    for(i = 0U; text[i]; i++) {
        unsigned char a=(unsigned char)text[i], b;
        if(a < 128U) { assert(a >= 32U && a <= 126U); continue; }
        b=(unsigned char)text[++i];
        assert(a >= 0xa1U && a <= 0xf7U && b >= 0xa1U && b <= 0xfeU);
    }
}
static void assert_fixed_font_row(const GuiParamRow *row)
{
    assert_text(row->label, sizeof(row->label));
    assert_text(row->value, sizeof(row->value));
    /* gui_settings_row style 3: one ASCII byte is a 16 px cell;
     * a valid GB2312 pair is a complete 32 px glyph. */
    assert(strlen(row->label) <= 14U);
    assert(strlen(row->value) <= 16U);
}
static void reset_menu(void)
{
    param_load_defaults(&param);
    /* Distinct, valid legacy values must survive ordinary edits and saves. */
    param.keySound=0U; param.onImage=1U; param.clockTime=7U; param.PPM_Out=1U;
    param.chLower[6]=101U; param.chMiddle[7]=2011U;
    save_error=0U; save_calls=apply_calls=0U;
    menu_param_load();
}
static void catalog_and_edit_tests(void)
{
    unsigned item;
    struct { uint32_t before; GuiParamRow row; uint32_t after; } guard;
    param_Config snapshot;
    reset_menu();
    assert(PARAM_ITEM_COUNT == 40U && PARAM_GLOBAL_COUNT == 7U && PARAM_CALIBRATION_CHANNELS == 6U);
    guard.before=0x12345678U; guard.after=0x98765432U;
    for(item=0U; item<PARAM_ITEM_COUNT; item++) {
        memset(&guard.row, 0xa5, sizeof(guard.row));
        menu_param_format_item((uint8_t)item, &guard.row);
        assert(menu_param_supported((uint8_t)item));
        assert(guard.before==0x12345678U && guard.after==0x98765432U);
        assert_fixed_font_row(&guard.row);
        assert(guard.row.label[0] && guard.row.value[0]);
        assert(strstr(guard.row.value, "UNAVAILABLE") == NULL);
        assert(strncmp(guard.row.label, "CH7", 3U) && strncmp(guard.row.label, "CH8", 3U));
    }
    menu_param_format_item(2U, &guard.row); assert(strcmp(guard.row.value,"1000")==0);
    menu_param_format_item(PARAM_ITEM_COUNT, &guard.row);
    assert(!guard.row.label[0] && !guard.row.value[0] && !menu_param_supported(PARAM_ITEM_COUNT));
    menu_handle_param_key(MENU_KEY_OK); assert(!param_editing && !param_dirty);
    for(item=1U; item<PARAM_ACTION_START; item++) {
        param_selected_item=(uint8_t)item;
        snapshot=param_edit;
        menu_handle_param_key(MENU_KEY_OK); assert(param_editing);
        menu_handle_param_key(MENU_KEY_RIGHT);
        if(!param_dirty) menu_handle_param_key(MENU_KEY_LEFT);
        assert(param_dirty);
        assert(memcmp(&snapshot,&param_edit,sizeof(snapshot)) != 0);
        menu_handle_param_key(MENU_KEY_BACK);
        assert(!param_editing && !param_dirty && !memcmp(&snapshot,&param_edit,sizeof(snapshot)));
    }
    param_selected_item=0U; menu_handle_param_key(MENU_KEY_LEFT);
    assert(param_selected_item==39U && param_first_visible==34U);
    menu_handle_param_key(MENU_KEY_RIGHT); assert(param_selected_item==0U && param_first_visible==0U);
    param_selected_item=2U; menu_handle_param_key(MENU_KEY_OK);
    menu_handle_param_key(MENU_KEY_RIGHT); menu_handle_param_key(MENU_KEY_LEFT);
    menu_handle_param_key(MENU_KEY_OK);
    assert(!param_dirty && !param_editing && param_edit.batVoltAdjust==1000U);
    assert_text(param_status,sizeof(param_status));
    puts("parameter catalog: 40 actual fields/actions, Chinese bounded labels, every editable item changes and cancels correctly");
}
static void fixed_font_bounds_tests(void)
{
    unsigned boundary, channel, item, power, rate, enabled;
    GuiParamRow row;
    for(boundary=0U; boundary<2U; boundary++) {
        reset_menu();
        param_edit.warnBatVolt=boundary ? 5.0f : 2.5f;
        param_edit.batVoltAdjust=boundary ? 1500U : 500U;
        param_edit.NRF_Channel=boundary ? 125U : 0U;
        for(channel=0U; channel<chNum; channel++) {
            param_edit.chLower[channel]=boundary ? 4093U : 0U;
            param_edit.chMiddle[channel]=boundary ? 4094U : 1U;
            param_edit.chUpper[channel]=boundary ? 4095U : 2U;
            param_edit.PWMadjustValue[channel]=boundary ? 1000 : -1000;
            param_edit.chReverse[channel]=(uint8_t)boundary;
        }
        assert(param_sanitize(&param_edit)==0U);
        for(power=0U; power<4U; power++) for(rate=0U; rate<3U; rate++)
        for(enabled=0U; enabled<2U; enabled++) {
            param_edit.NRF_Power=nrf_power_register[power];
            param_edit.NRF_DataRate=(uint8_t)rate;
            param_edit.NRF_Mode=(uint8_t)enabled;
            for(item=0U; item<PARAM_ITEM_COUNT; item++) {
                menu_param_format_item((uint8_t)item,&row);
                assert_fixed_font_row(&row);
            }
        }
    }
    puts("fixed 32 px parameter text: all 40 labels/values fit 14/16 cells at legal minima/maxima and every radio mode");
}
static void bounds_tests(void)
{
    unsigned channel, field, direction, step;
    for(channel=0U; channel<PARAM_CALIBRATION_CHANNELS; channel++)
    for(field=0U; field<3U; field++) for(direction=0U; direction<2U; direction++) {
        reset_menu();
        param_selected_item=(uint8_t)(PARAM_CHANNEL_START+channel*5U+field);
        for(step=0U; step<450U; step++) {
            menu_param_adjust(direction ? 1 : -1);
            assert(param_edit.chLower[channel] < param_edit.chMiddle[channel]);
            assert(param_edit.chMiddle[channel] < param_edit.chUpper[channel]);
            assert(param_sanitize(&param_edit)==0U);
        }
        param_dirty=0U; menu_param_adjust(direction ? 1 : -1); assert(!param_dirty);
    }
    reset_menu(); param_selected_item=PARAM_CHANNEL_START+1U;
    param_edit.chLower[0]=2000U; param_edit.chMiddle[0]=2001U; param_edit.chUpper[0]=2002U;
    menu_param_adjust(-1); assert(param_edit.chMiddle[0]==2001U && !param_dirty);
    menu_param_adjust(1); assert(param_edit.chMiddle[0]==2001U && !param_dirty);
    assert(param_sanitize(&param_edit)==0U);
    puts("calibration: 16200 endpoint edits preserve lower < center < upper; Save needs no repair; unchanged endpoint stays clean");
}
static void save_tests(void)
{
    param_Config original, draft;
    reset_menu(); original=param_edit;
    param_selected_item=2U;
    menu_handle_param_key(MENU_KEY_OK); menu_handle_param_key(MENU_KEY_RIGHT); menu_handle_param_key(MENU_KEY_OK);
    assert(!param_editing && param_dirty && param_edit.batVoltAdjust==1010U && param.batVoltAdjust==1000U);
    draft=param_edit; param_selected_item=PARAM_ACTION_START; save_error=1U;
    menu_handle_param_key(MENU_KEY_OK);
    assert(save_calls==1U && !apply_calls && param_dirty);
    assert(!memcmp((const void *)&param,&original,sizeof(original)) && !memcmp(&param_edit,&draft,sizeof(draft)));
    save_error=0U; menu_handle_param_key(MENU_KEY_OK);
    assert(save_calls==2U && apply_calls==1U && !param_dirty);
    assert(!memcmp((const void *)&param,&draft,sizeof(draft)) && !memcmp(&saved,&draft,sizeof(draft)));
    assert(param.keySound==0U && param.onImage==1U && param.clockTime==7U && param.PPM_Out==1U);
    assert(param.chLower[6]==101U && param.chMiddle[7]==2011U);
    param_selected_item=PARAM_ACTION_START+2U; menu_handle_param_key(MENU_KEY_OK);
    assert(param_dirty && param_edit.batVoltAdjust==1000U && param.batVoltAdjust==1010U && save_calls==2U);
    param_selected_item=PARAM_ACTION_START+1U; menu_handle_param_key(MENU_KEY_OK);
    assert(!param_dirty && param_edit.batVoltAdjust==1010U && save_calls==2U);
    param_selected_item=3U; menu_handle_param_key(MENU_KEY_OK); menu_handle_param_key(MENU_KEY_RIGHT);
    assert(param.NRF_Mode==1U && param_edit.NRF_Mode==0U);
    menu_handle_param_key(MENU_KEY_OK); param_selected_item=PARAM_ACTION_START; menu_handle_param_key(MENU_KEY_OK);
    assert(param.NRF_Mode==0U && !param_dirty && apply_calls==2U);
    puts("parameter save: draft isolation, failed-save rollback, successful persistence/application, reload/defaults, legacy bytes preserved");
}
int main(void)
{
    catalog_and_edit_tests(); fixed_font_bounds_tests(); bounds_tests(); save_tests();
    puts("parameter menu regression passed");
    return 0;
}
