/* Exercises the production retained-framebuffer update path, with the same
 * physical panel mock and real drawing functions as lcd_page_test.c. */
static void update_fixture(void)
{
    static const uint16_t pixels[] = { RED, GREEN, BLUE, BLACK, BLUE2, YELLOW, GREY, RED };
    ILI9806G_Fill(17U,19U,93U,52U,BLUE);
    LCD_SetFont(&Font16x32); LCD_SetBackColor(BLUE); LCD_SetTextColor(WHITE);
    ILI9806G_DispString_EN(24U,20U,"UPDATE");
    ILI9806G_DrawCircle(150U,90U,15U,0U);
    LCD_BlitRGB565(798U,479U,4U,2U,pixels);
}

static void test_partial_transactions(void)
{
    unsigned i, used;
    LCD_PageBuffer_Enable(0U);
    LCD_BeginPage(WHITE); update_fixture();
    memcpy(expected,panel,sizeof(panel));
    LCD_PageBuffer_Enable(1U);
    LCD_BeginPage(WHITE); LCD_EndPage();
    memcpy(old_frame,panel,sizeof(panel));
    bus_writes = pixel_writes = memory_commands = 0U;
    LCD_BeginUpdate(); assert(lcd_page_active == 2U);
    update_fixture();
    assert(bus_writes == 0U && memcmp(panel,old_frame,sizeof(panel)) == 0);
    LCD_EndPage();
    assert(memcmp(panel,expected,sizeof(panel)) == 0);
    assert(memory_commands >= 2U && memory_commands <= 12U);
    assert(pixel_writes < 15000U);
    used = pixel_writes;
    bus_writes = 0U;
    LCD_BeginUpdate(); LCD_EndPage(); assert(bus_writes == 0U);
    /* Legacy direct drawing invalidates the image; it must never be undone by
       a later partial transaction that replays stale SRAM. */
    ILI9806G_DrawPoint(400U,200U,RED);
    ILI9806G_DrawPoint(401U,200U,GREEN);
    assert(lcd_page_committed == 0U);
    LCD_BeginUpdate(); assert(lcd_page_active == 0U);
    ILI9806G_DrawPoint(402U,200U,BLUE); LCD_EndPage();
    assert(panel[200U*800U+400U] == RED && panel[200U*800U+401U] == GREEN);
    assert(panel[200U*800U+402U] == BLUE);
    LCD_BeginPage(WHITE); LCD_EndPage();
    LCD_BeginUpdate(); assert(lcd_page_active == 2U);
    ILI9806G_DrawPoint(400U,200U,RED); LCD_EndPage();
    assert(panel[200U*800U+400U] == RED);
    LCD_PageBuffer_Enable(0U);
    LCD_BeginUpdate(); assert(lcd_page_active == 0U);
    ILI9806G_DrawPoint(401U,200U,GREEN); LCD_EndPage();
    assert(panel[200U*800U+401U] == GREEN);
    LCD_PageBuffer_Enable(1U);
    LCD_X_LENGTH = 480U; LCD_Y_LENGTH = 800U;
    LCD_BeginPage(WHITE); LCD_EndPage();
    bus_writes = pixel_writes = memory_commands = 0U;
    LCD_BeginUpdate(); ILI9806G_DrawPoint(479U,799U,RED);
    assert(bus_writes == 0U); LCD_EndPage();
    assert(memory_commands == 1U && pixel_writes == 256U);
    assert(panel[800U*480U-1U] == RED);
    LCD_X_LENGTH = 800U; LCD_Y_LENGTH = 480U;
    LCD_BeginPage(WHITE); LCD_EndPage();
    LCD_BeginUpdate(); ILI9806G_DrawPoint(40U,40U,RED);
    ILI9806G_GramScan(9U); /* Rejected input must not cancel the transaction. */
    assert(lcd_page_active == 2U && lcd_page_committed != 0U);
    ILI9806G_GramScan(7U); /* Same dimensions, different physical mapping. */
    assert(LCD_SCAN_MODE == 7U && LCD_X_LENGTH == 800U && LCD_Y_LENGTH == 480U);
    assert(lcd_page_active == 0U && lcd_page_committed == 0U && lcd_page_window_valid == 0U);
    bus_writes = 0U; LCD_EndPage(); LCD_BeginUpdate(); assert(bus_writes == 0U && lcd_page_active == 0U);
    ILI9806G_GramScan(5U); LCD_BeginPage(WHITE); LCD_EndPage();
    LCD_BeginUpdate(); ILI9806G_DrawPoint(40U,40U,RED);
    LCD_InvalidatePage(); /* Legacy raw-GRAM writers cannot commit stale SRAM. */
    bus_writes = 0U; LCD_EndPage(); assert(bus_writes == 0U && lcd_page_committed == 0U);
    for(i = 800U*480U; i < sizeof(sram)/sizeof(sram[0]); i++) assert(sram[i] == 0x1234U);
    printf("LCD partial transaction: fixture %u pixels; portrait edge 256; no interim panel writes; direct fallback coherent.\n",used);
}

static void test_focus_transactions(void)
{
    unsigned group, selected, next, before, i, pixels_max = 0U, app_max = 0U, param_max = 0U;
    uint8_t start, count;
    checking_ui_text = 1U;
    LCD_PageBuffer_Enable(1U);
    for(group = 0U; group < MENU_GROUP_COUNT; group++) {
        next = (group+1U)%MENU_GROUP_COUNT;
        gui_prepare_page(); menu_group_page((uint8_t)next); LCD_EndPage();
        memcpy(expected,panel,sizeof(panel));
        gui_prepare_page(); menu_group_page((uint8_t)group); LCD_EndPage();
        memcpy(old_frame,panel,sizeof(panel)); bus_writes = pixel_writes = 0U;
        LCD_BeginUpdate(); menu_group_page((uint8_t)next);
        assert(bus_writes == 0U && memcmp(panel,old_frame,sizeof(panel)) == 0);
        LCD_EndPage();
        assert(memcmp(panel,expected,sizeof(panel)) == 0 && pixel_writes < 30000U);
        if(pixel_writes > pixels_max) pixels_max = pixel_writes;
        start = menu_group_first((uint8_t)group); count = menu_group_count((uint8_t)group);
        for(i = 0U; i < count; i++) {
            selected = start+i; next = start+(i+1U)%count;
            gui_prepare_page(); main_menu((uint8_t)next); LCD_EndPage(); memcpy(expected,panel,sizeof(panel));
            gui_prepare_page(); main_menu((uint8_t)selected); LCD_EndPage(); memcpy(old_frame,panel,sizeof(panel));
            bus_writes = pixel_writes = 0U;
            LCD_BeginUpdate(); main_menu((uint8_t)next);
            assert(bus_writes == 0U && memcmp(panel,old_frame,sizeof(panel)) == 0);
            LCD_EndPage();
            assert(memcmp(panel,expected,sizeof(panel)) == 0 && pixel_writes < 100000U);
            if(pixel_writes > app_max) app_max = pixel_writes;
        }
    }
    for(before = 0U; before < 6U; before++) {
        next = (before+1U)%6U;
        gui_prepare_page(); parameter_settings_page(preview_rows,6U,(uint8_t)next,0U,40U,0U,0U,1U,""); LCD_EndPage();
        memcpy(expected,panel,sizeof(panel));
        gui_prepare_page(); parameter_settings_page(preview_rows,6U,(uint8_t)before,0U,40U,0U,0U,1U,""); LCD_EndPage();
        memcpy(old_frame,panel,sizeof(panel)); bus_writes = pixel_writes = 0U;
        LCD_BeginUpdate(); parameter_settings_page(preview_rows,6U,(uint8_t)next,0U,40U,0U,0U,1U,"");
        assert(bus_writes == 0U && memcmp(panel,old_frame,sizeof(panel)) == 0);
        LCD_EndPage();
        assert(memcmp(panel,expected,sizeof(panel)) == 0 && pixel_writes < 100000U);
        if(pixel_writes > param_max) param_max = pixel_writes;
    }
    checking_ui_text = 0U;
    printf("LCD focus transactions: category max %u, app max %u, parameter max %u pixels (<384000); all equal full redraws.\n",pixels_max,app_max,param_max);
}
