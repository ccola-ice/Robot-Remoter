#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ui_menu.h"
#include "file_browser.h"
#include "text_reader.h"
#include "image_viewer.h"
#include "image_gif.h"

#define DIGITAL_CHANNEL_COUNT 8U
#include "file_viewer_state.inc"

static FATFS fs_sdcard;
static FileBrowserState mock_browser;
static FileBrowserEnterResult mock_enter_result;
static file_text_page_t mock_text;
static file_text_result_t mock_text_result;
static FileImageResult mock_image_result;
static FileBrowserService mock_browser_service;
static file_text_service_t mock_text_service;
static void *mock_browser_context, *mock_text_context;
static char mock_path[FILE_BROWSER_PATH_LENGTH];
static char last_opened[FILE_BROWSER_PATH_LENGTH];
static char last_status[160], last_position[64], last_kind[64];
static unsigned text_opens, text_closes, text_nexts, text_previouses, encoding_changes;
static unsigned image_draws, preview_draws, browser_draws, browser_refreshes;
static unsigned background_calls, clock_draws, commits, mounts;
static uint32_t browser_position, browser_total;
static uint8_t browser_retry, text_live, cancel_next_operation, inject_background_back;
static uint8_t transaction, last_text_mode, last_clear, last_lines, cancelled_image_visible;
static uint32_t now_ms;
static unsigned gif_opens, gif_nexts, gif_closes;
static uint8_t gif_opened, gif_ended, gif_end_next, inject_background_ok;

static void menu_handle_page_key(MenuKey key);
static uint8_t menu_file_service(void *context);
static void menu_draw_browser(void);
static void menu_draw_file_viewer(void);
static void menu_dispatch_key(MenuKey key) { menu_handle_page_key(key); }
static void menu_draw_current_page(void)
{
    if(page_changed) { assert(!transaction); gui_prepare_page(); page_changed = 0U; }
    if(current_page == MENU_PAGE_FILE_BROWSER) menu_draw_browser();
    else if(current_page == MENU_PAGE_FILE_VIEWER) menu_draw_file_viewer();
}
static void LCD_BeginUpdate(void) { assert(!transaction); transaction = 1U; }
static void LCD_EndPage(void)
{
    assert(transaction);
    /* 取消图片后必须在同一提交中显示目录，不能先提交半张图片。 */
    assert(!cancelled_image_visible);
    transaction = 0U;
    ++commits;
}
void gui_prepare_page(void) { transaction = 1U; cancelled_image_visible = 0U; }
void gui_clock_overlay(void) { assert(transaction); ++clock_draws; }
static void menu_robot_telemetry_service(void) {}
static uint8_t user_BUTTON_repeat_held(uint8_t key) { (void)key; return 0U; }
static void menu_handle_category_key(MenuKey key) { (void)key; }
static void menu_handle_nrf_key(MenuKey key) { (void)key; assert(0); }
static void menu_handle_calendar_key(MenuKey key) { (void)key; assert(0); }
static void menu_handle_param_key(MenuKey key) { (void)key; assert(0); }
static void menu_handle_system_key(MenuKey key) { (void)key; assert(0); }
static void system_key_beep(void) {}
static int get_tick_count(unsigned long *time) { *time = now_ms; return 0; }
void menu_group_page(uint8_t group) { (void)group; assert(0); }
static void menu_draw_monitor(void) { assert(0); }
static void digital_channel_get_snapshot(uint8_t *raw, uint8_t *stable)
{ (void)raw; (void)stable; assert(0); }
void digital_channel_monitor_page(const uint8_t *raw, const uint8_t *stable)
{ (void)raw; (void)stable; assert(0); }
void imu6050_information(void) { assert(0); }
void system_data_read_and_set(void) { assert(0); }
static void menu_draw_calendar(void) { assert(0); }
void robot_control_page(const GuiRobotTelemetry *data) { (void)data; assert(0); }

FRESULT f_mount(FATFS *fs, const TCHAR *path, BYTE option)
{
    assert(fs == &fs_sdcard && !strcmp(path, "0:") && option == 0U);
    ++mounts;
    return FR_OK;
}

void file_browser_init(FileBrowserService callback, void *context)
{ mock_browser_service = callback; mock_browser_context = context; }
const FileBrowserState *file_browser_state(void) { return &mock_browser; }
void file_browser_move(int direction)
{
    if(direction > 0 && mock_browser.selected_row < 5U) ++mock_browser.selected_row;
    else if(direction < 0 && mock_browser.selected_row) --mock_browser.selected_row;
}
FileBrowserEnterResult file_browser_enter(char *path, size_t capacity)
{
    assert(capacity > strlen(mock_path));
    if(!mock_browser.visible_count) return FILE_BROWSER_ENTER_EMPTY;
    if(!mock_browser_service(mock_browser_context)) return FILE_BROWSER_ENTER_ERROR;
    if(mock_enter_result == FILE_BROWSER_ENTER_FILE) strcpy(path, mock_path);
    return mock_enter_result;
}
uint8_t file_browser_back(void) { return mock_browser.virtual_root; }
void file_browser_refresh(void)
{
    ++browser_refreshes;
    assert(mock_browser_service(mock_browser_context));
    mock_browser.status = FILE_BROWSER_OK;
    mock_browser.visible_count = 1U;
    mock_browser.total_count = 1U;
    mock_browser.first_index = mock_browser.selected_row = 0U;
}

void file_text_set_service(file_text_service_t callback, void *context)
{ mock_text_service = callback; mock_text_context = context; }
static uint8_t text_service(void)
{
    if(cancel_next_operation) {
        cancel_next_operation = 0U;
        menu_post_key(MENU_KEY_BACK);
    }
    return mock_text_service(mock_text_context);
}
file_text_result_t file_text_open(const char *path)
{
    ++text_opens;
    strcpy(last_opened, path);
    if(!text_service()) return FILE_TEXT_CANCELLED;
    text_live = mock_text_result == FILE_TEXT_OK;
    mock_text.page_number = 1U;
    return mock_text_result;
}
file_text_result_t file_text_next(void)
{
    ++text_nexts;
    if(!text_service()) return FILE_TEXT_CANCELLED;
    if(mock_text_result == FILE_TEXT_OK) {
        ++mock_text.page_number;
        mock_text.has_previous = 1U;
    }
    return mock_text_result;
}
file_text_result_t file_text_previous(void)
{
    ++text_previouses;
    if(!text_service()) return FILE_TEXT_CANCELLED;
    if(mock_text.page_number > 1U) --mock_text.page_number;
    return mock_text_result;
}
file_text_result_t file_text_set_encoding(file_text_encoding_t encoding)
{
    ++encoding_changes;
    mock_text.encoding = encoding;
    mock_text.page_number = 1U;
    return mock_text_result;
}
void file_text_close(void) { ++text_closes; text_live = 0U; }
const file_text_page_t *file_text_get_page(void) { return &mock_text; }

void *file_image_get_workspace(uint32_t *capacity)
{
    static uint32_t workspace[2];
    *capacity = sizeof(workspace); return workspace;
}
FileImageResult file_gif_open(const char *path, uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h, uint16_t background,
                              FileImageInfo *info, void *scratch, uint32_t capacity,
                              uint8_t (*service)(void *), void *context)
{
    ++gif_opens;
    assert(transaction && !strcmp(path, mock_path) && scratch && capacity == 8U);
    assert(x == 24U && y == 136U && w == 752U && h == 280U && background == 0xffffU);
    if(cancel_next_operation) { cancel_next_operation = 0U; menu_post_key(MENU_KEY_BACK); }
    if(!service(context)) return FILE_IMAGE_CANCELLED;
    gif_opened = mock_image_result == FILE_IMAGE_OK; gif_ended = 0U;
    info->width = 640U; info->height = 400U;
    info->drawn_width = 320U; info->drawn_height = 200U;
    return mock_image_result;
}
FileImageResult file_gif_next(void)
{
    assert(transaction && gif_opened); ++gif_nexts;
    if(cancel_next_operation) {
        cancel_next_operation = 0U; menu_post_key(MENU_KEY_BACK);
        assert(!menu_file_service(NULL)); gif_opened = 0U;
        return FILE_IMAGE_CANCELLED;
    }
    if(mock_image_result != FILE_IMAGE_OK) gif_opened = 0U;
    if(gif_end_next) { gif_end_next = 0U; gif_ended = 1U; gif_opened = 0U; }
    return mock_image_result;
}
void file_gif_close(void) { if(gif_opened) ++gif_closes; gif_opened = 0U; }
uint32_t file_gif_delay_ms(void) { return 100U; }
uint8_t file_gif_finished(void) { return gif_ended; }
uint8_t file_gif_is_open(void) { return gif_opened; }

FileImageResult file_image_draw(const char *path, uint16_t x, uint16_t y,
                               uint16_t width, uint16_t height, FileImageInfo *info,
                               uint8_t (*service)(void *), void *context)
{
    ++image_draws;
    assert(transaction && !strcmp(path, mock_path));
    assert(x == 24U && y == 136U && width == 752U && height == 280U);
    if(cancel_next_operation) { cancel_next_operation = 0U; menu_post_key(MENU_KEY_BACK); }
    if(!service(context)) { cancelled_image_visible = 1U; return FILE_IMAGE_CANCELLED; }
    info->width = 1200U; info->height = 600U;
    info->drawn_width = 560U; info->drawn_height = 280U;
    return mock_image_result;
}

void file_browser_page(const char *path, const GuiFileEntry *entries, uint8_t count,
                       uint8_t selected, uint8_t first, uint16_t revision, const char *status)
{
    assert(transaction && path && entries && selected <= 5U && count <= 6U && first == 0U);
    (void)revision;
    ++browser_draws;
    strcpy(last_status, status);
}
void file_browser_position(uint32_t selected, uint32_t total, uint8_t retry)
{ browser_position = selected; browser_total = total; browser_retry = retry; }
void file_preview_page(const char *name, const char *kind, const char *lines,
                       uint16_t stride, uint8_t count, const char *status,
                       const char *position, uint8_t clear, uint8_t text_mode)
{
    assert(transaction && name && kind && status && position);
    if(count) assert(lines && stride);
    ++preview_draws;
    strcpy(last_status, status); strcpy(last_position, position); strcpy(last_kind, kind);
    last_text_mode = text_mode; last_clear = clear; last_lines = count;
}

#include "file_viewer_functions.inc"

static void background(void)
{
    ++background_calls;
    if(inject_background_back) { inject_background_back = 0U; menu_post_key(MENU_KEY_BACK); }
    if(inject_background_ok && current_page == MENU_PAGE_FILE_VIEWER) {
        inject_background_ok = 0U; menu_post_key(MENU_KEY_OK);
    }
}

static void setup(const char *path, const char *name)
{
    memset(&mock_browser, 0, sizeof(mock_browser));
    strcpy(mock_browser.path, "0:/DOCS");
    mock_browser.total_count = 130U;
    mock_browser.first_index = 90U;
    mock_browser.visible_count = 6U;
    mock_browser.selected_row = 5U;
    strcpy(mock_browser.entries[5].name, name);
    strcpy(mock_path, path);
    memset(&mock_text, 0, sizeof(mock_text));
    strcpy(mock_text.lines[0], "Document content");
    mock_text.line_count = 1U;
    mock_text.encoding = FILE_TEXT_ENCODING_UTF8;
    mock_text.file_size = 10000U;
    mock_text.end_offset = 1000U;
    mock_text.has_next = 1U;
    mock_text.fatfs_error = FR_DISK_ERR;
    mock_enter_result = FILE_BROWSER_ENTER_FILE;
    mock_text_result = FILE_TEXT_OK;
    mock_image_result = FILE_IMAGE_OK;
    text_opens = text_closes = text_nexts = text_previouses = encoding_changes = 0U;
    image_draws = preview_draws = browser_draws = browser_refreshes = 0U;
    now_ms = gif_opens = gif_nexts = gif_closes = 0U;
    gif_opened = gif_ended = gif_paused = gif_step_pending = gif_toggle_pending = 0U;
    gif_end_next = inject_background_ok = 0U;
    clock_draws = commits = background_calls = mounts = 0U;
    transaction = text_live = cancel_next_operation = inject_background_back = cancelled_image_visible = 0U;
    event_read_index = event_write_index = repeat_pending = 0U;
    refresh_tick_count = clock_refresh_tick_count = clock_refresh_due = refresh_due = 0U;
    page_dirty = page_changed = 1U;
    current_page = MENU_PAGE_FILE_BROWSER;
    menu_set_background_service(background);
    menu_browser_load_drives();
    assert(mounts == 1U);
    menu_process();
    assert(browser_position == 96U && browser_total == 130U && !browser_retry);
    assert(last_status[0] == '\0');
}

static void press(MenuKey key) { menu_post_key(key); menu_process(); }

static void test_text_navigation_encoding_and_return(void)
{
    unsigned closes;
    setup("0:/DOCS/README~1.TXT", "Readable document.txt");
    press(MENU_KEY_OK);
    assert(current_page == MENU_PAGE_FILE_VIEWER && file_view_kind == FILE_VIEW_TEXT);
    assert(text_live && text_opens == 1U && !strcmp(last_opened, mock_path));
    assert(last_text_mode && strstr(last_kind, "UTF-8") && last_lines == 1U);
    press(MENU_KEY_RIGHT); assert(text_nexts == 1U && mock_text.page_number == 2U);
    press(MENU_KEY_LEFT); assert(text_previouses == 1U && mock_text.page_number == 1U);
    press(MENU_KEY_OK); assert(encoding_changes == 1U && mock_text.encoding == FILE_TEXT_ENCODING_GBK);
    press(MENU_KEY_OK); assert(encoding_changes == 2U && mock_text.encoding == FILE_TEXT_ENCODING_UTF16_LE);
    assert(strstr(last_kind, "UTF-16 LE"));
    press(MENU_KEY_OK); assert(encoding_changes == 3U && mock_text.encoding == FILE_TEXT_ENCODING_UTF16_BE);
    assert(strstr(last_kind, "UTF-16 BE"));
    press(MENU_KEY_OK); assert(encoding_changes == 4U && mock_text.encoding == FILE_TEXT_ENCODING_UTF8);
    closes = text_closes;
    press(MENU_KEY_BACK);
    assert(current_page == MENU_PAGE_FILE_BROWSER && !text_live && text_closes == closes + 1U);
    assert(mock_browser.first_index == 90U && mock_browser.selected_row == 5U);
    assert(browser_position == 96U && browser_refreshes == 0U);
}

static void test_image_stable_refresh_and_retry(void)
{
    unsigned i, previews;
    setup("0:/PHOTO~1.JPG", "A photograph.JPG");
    press(MENU_KEY_OK);
    assert(file_view_kind == FILE_VIEW_IMAGE && image_draws == 1U && !last_clear);
    assert(!last_text_mode && strstr(last_status, "1200 x 600"));
    previews = preview_draws;
    for(i = 0U; i < 100U; ++i) { menu_tick_10ms(); menu_process(); }
    assert(image_draws == 1U && preview_draws == previews && clock_draws >= 5U);
    press(MENU_KEY_RIGHT); press(MENU_KEY_LEFT);
    assert(image_draws == 1U && preview_draws == previews);
    mock_image_result = FILE_IMAGE_IO;
    press(MENU_KEY_OK);
    assert(image_draws == 2U && last_clear && strstr(last_status, "OK"));
    mock_image_result = FILE_IMAGE_OK;
    press(MENU_KEY_OK);
    assert(image_draws == 3U && !last_clear);
    press(MENU_KEY_BACK);
    assert(current_page == MENU_PAGE_FILE_BROWSER && !file_image_pending);
}

static void test_errors_retry_cancel_and_unsupported(void)
{
    unsigned closes;
    setup("0:/BROKEN.TXT", "Broken.txt");
    mock_text_result = FILE_TEXT_IO_ERROR;
    press(MENU_KEY_OK);
    assert(file_text_result == FILE_TEXT_IO_ERROR && strstr(last_status, "OK"));
    mock_text_result = FILE_TEXT_OK;
    press(MENU_KEY_OK);
    assert(text_opens == 2U && file_text_result == FILE_TEXT_OK && !encoding_changes);
    closes = text_closes;
    cancel_next_operation = 1U;
    press(MENU_KEY_RIGHT);
    assert(current_page == MENU_PAGE_FILE_BROWSER && text_closes == closes + 1U && !text_live);
    inject_background_back = 1U;
    press(MENU_KEY_OK);
    assert(current_page == MENU_PAGE_FILE_BROWSER && !text_live);

    setup("0:/IMAGE.BMP", "Image.bmp");
    cancel_next_operation = 1U;
    press(MENU_KEY_OK);
    assert(current_page == MENU_PAGE_FILE_BROWSER && !page_dirty && !page_changed && !file_image_pending);
    menu_process();
    assert(browser_draws >= 2U && !transaction);

    setup("0:/MANUAL.PDF", "Manual.pdf");
    press(MENU_KEY_OK);
    assert(file_view_kind == FILE_VIEW_UNSUPPORTED && last_lines >= 6U);
    assert(!text_opens && !image_draws && !last_text_mode);
    press(MENU_KEY_OK); assert(!text_opens && !image_draws);
    closes = text_closes;
    press(MENU_KEY_BACK);
    assert(current_page == MENU_PAGE_FILE_BROWSER && text_closes == closes + 1U);

    setup("0:/DATA~1.JSO", "Data.json");
    press(MENU_KEY_OK);
    assert(file_view_kind == FILE_VIEW_TEXT && !strcmp(last_opened, "0:/DATA~1.JSO"));
}

static void test_empty_and_error_directory_retry(void)
{
    setup("0:/README.TXT", "Readme.txt");
    mock_browser.visible_count = 0U;
    mock_browser.total_count = 0U;
    mock_browser.status = FILE_BROWSER_IO_ERROR;
    mock_browser.result = FR_NOT_READY;
    page_dirty = 1U;
    menu_process();
    assert(browser_retry && strstr(last_status, "OK"));
    press(MENU_KEY_OK);
    assert(browser_refreshes == 1U && mock_browser.visible_count == 1U);
    assert(current_page == MENU_PAGE_FILE_BROWSER && !text_opens);
    mock_browser.visible_count = mock_browser.total_count = 0U;
    mock_browser.status = FILE_BROWSER_EMPTY;
    press(MENU_KEY_OK);
    assert(browser_refreshes == 2U);
    mock_browser.visible_count = mock_browser.total_count = 0U;
    mock_browser.status = FILE_BROWSER_CANCELLED;
    press(MENU_KEY_OK);
    assert(browser_refreshes == 3U);
}

static void test_long_display_names_keep_document_and_image_type(void)
{
    unsigned index;
    char name[GUI_FILE_NAME_LENGTH];
    /* 与目录模块的 CP936 长名称截断结果一致，SFN 仍为唯一打开路径。 */
    for(index = 0U; index < 120U; index += 2U) {
        name[index] = (char)0xD6U;
        name[index + 1U] = (char)0xD0U;
    }
    name[120U] = name[121U] = '.';
    strcpy(name + 122U, ".json");
    setup("0:/LONGNA~1.JSO", name);
    press(MENU_KEY_OK);
    assert(file_view_kind == FILE_VIEW_TEXT && text_opens == 1U && !image_draws);
    assert(!strcmp(last_opened, "0:/LONGNA~1.JSO") && !strcmp(file_view_name, name));
    press(MENU_KEY_BACK);
    strcpy(name + 122U, ".jpeg");
    setup("0:/LONGNA~1.JPE", name);
    press(MENU_KEY_OK);
    assert(file_view_kind == FILE_VIEW_IMAGE && image_draws == 1U && !text_opens);
    assert(!strcmp(file_view_name, name));
    press(MENU_KEY_BACK);
}

static void test_png_and_gif_playback(void)
{
    unsigned nexts;
    setup("0:/ALPHA.PNG", "Alpha.png");
    press(MENU_KEY_OK);
    assert(file_view_kind == FILE_VIEW_IMAGE && image_draws == 1U);
    press(MENU_KEY_BACK);

    setup("0:/ANIM.GIF", "Animation.gif");
    press(MENU_KEY_OK);
    assert(file_view_kind == FILE_VIEW_GIF && gif_opens == 1U && gif_opened);
    assert(last_text_mode == 2U && !image_draws && !text_opens);
    now_ms = 99U; menu_process(); assert(gif_nexts == 0U);
    now_ms = 100U; menu_process(); assert(gif_nexts == 1U);
    press(MENU_KEY_OK); assert(gif_paused);
    now_ms = 5000U; clock_refresh_due = 1U; menu_process();
    assert(gif_nexts == 1U && !transaction);
    press(MENU_KEY_OK); assert(!gif_paused && gif_nexts == 1U);
    now_ms = 5100U; menu_process(); assert(gif_nexts == 2U);
    now_ms = 9000U; menu_process(); assert(gif_nexts == 3U);
    menu_process(); assert(gif_nexts == 3U); /* No accumulated frame burst. */
    gif_end_next = 1U;
    now_ms = 12000U; menu_process(); assert(gif_nexts == 4U && !gif_opened);
    assert(strstr(last_status, "OK"));
    press(MENU_KEY_OK); assert(gif_opens == 2U && !gif_ended);
    now_ms += 100U; cancel_next_operation = 1U; menu_process();
    assert(current_page == MENU_PAGE_FILE_BROWSER && !gif_opened && !transaction);

    setup("0:/ANIM.GIF", "Animation.gif");
    now_ms = UINT32_MAX - 50U;
    press(MENU_KEY_OK);
    now_ms = 48U; menu_process(); assert(gif_nexts == 0U);
    now_ms = 49U; menu_process(); assert(gif_nexts == 1U);
    now_ms = 149U; mock_image_result = FILE_IMAGE_IO; menu_process();
    assert(!gif_opened && strstr(last_status, "OK"));
    mock_image_result = FILE_IMAGE_OK;
    press(MENU_KEY_OK); assert(gif_opened && gif_opens == 2U);
    nexts = gif_nexts;
    now_ms += 100U; press(MENU_KEY_BACK);
    assert(!gif_opened && gif_nexts == nexts && current_page == MENU_PAGE_FILE_BROWSER);

    setup("0:/ANIM.GIF", "Animation.gif");
    inject_background_ok = 1U;
    press(MENU_KEY_OK);
    assert(gif_toggle_pending && !gif_paused);
    menu_process(); assert(gif_paused && !gif_toggle_pending);
    now_ms = 5000U; menu_process(); assert(gif_nexts == 0U);
    press(MENU_KEY_BACK); assert(!gif_opened);
}

int main(void)
{
    test_text_navigation_encoding_and_return();
    test_image_stable_refresh_and_retry();
    test_errors_retry_cancel_and_unsupported();
    test_empty_and_error_directory_retry();
    test_long_display_names_keep_document_and_image_type();
    test_png_and_gif_playback();
    puts("file viewer menu tests: PASS (text, PNG/GIF, animation pause/replay/timing, clock, retry, cancel, close)");
    return 0;
}
