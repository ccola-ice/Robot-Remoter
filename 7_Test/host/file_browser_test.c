#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "file_browser.h"

static unsigned root_count, child_count, opened, closed, reads, service_calls;
static int card_present, read_error_at, cancel_at, deep_mode, close_error;
static char last_open[FILE_BROWSER_PATH_LENGTH];
static const char *long_extension;

static uint8_t service(void *context)
{
    assert(context == &root_count);
    ++service_calls;
    return cancel_at < 0 || service_calls < (unsigned)cancel_at;
}

FRESULT f_opendir(DIR *directory, const TCHAR *path)
{
    if(!card_present) return FR_NOT_READY;
    if(!deep_mode && strcmp(path, "0:") && strcmp(path, "0:/DIR00007")) return FR_NO_PATH;
    strcpy(last_open, path);
    memset(directory, 0, sizeof(*directory));
    directory->sclust = strcmp(path, "0:") != 0;
    ++opened;
    return FR_OK;
}

FRESULT f_readdir(DIR *directory, FILINFO *info)
{
    unsigned index = directory->index++;
    unsigned count = deep_mode ? 1U : directory->sclust ? child_count : root_count;
    ++reads;
    if(!card_present) return FR_NOT_READY;
    if(read_error_at >= 0 && index == (unsigned)read_error_at) return FR_DISK_ERR;
    if(index >= count) {
        info->fname[0] = 0;
        return FR_OK;
    }
    assert(info->lfsize >= 511U);
    info->lfname[0] = 0;
    info->fattrib = 0;
    info->fsize = 1000U + index;
    info->fdate = 0x5678U;
    info->ftime = 0x1234U;
    if(deep_mode) {
        strcpy(info->fname, deep_mode == 2 ? "SUB" : "12345678.ABC");
        info->fattrib = AM_DIR;
    } else if(directory->sclust) {
        sprintf(info->fname, "C%05u.TXT", index);
    } else if(index < 2U) {
        unsigned i;
        sprintf(info->fname, "LONGNA~%u.TXT", index + 1U);
        /* 两个超过显示长度的文件名拥有不同别名，必须打开正确对象。 */
        for(i = 0U; i < 510U; i += 2U) {
            info->lfname[i] = (char)0xD6U;
            info->lfname[i + 1U] = (char)0xD0U;
        }
        info->lfname[508U] = (char)(0xD6U + index);
        info->lfname[510U] = 0;
        if(long_extension) {
            size_t length = strlen(long_extension);
            size_t prefix = (510U - length) & ~(size_t)1U;
            strcpy(info->lfname + prefix, long_extension);
            strcpy(info->fname, !strcmp(long_extension, ".json") ? "LONGNA~1.JSO" : "LONGNA~1.JPE");
        }
    } else if(index == 7U) {
        strcpy(info->fname, "DIR00007");
        strcpy(info->lfname, "Readable child directory");
        info->fattrib = AM_DIR;
    } else {
        sprintf(info->fname, "F%05u.TXT", index);
    }
    return FR_OK;
}

FRESULT f_closedir(DIR *directory)
{
    (void)directory;
    ++closed;
    return close_error ? FR_DISK_ERR : FR_OK;
}

static void reset(void)
{
    root_count = 130U;
    child_count = 3U;
    opened = closed = reads = service_calls = 0U;
    card_present = 1;
    read_error_at = cancel_at = -1;
    close_error = deep_mode = 0;
    long_extension = NULL;
    last_open[0] = 0;
    file_browser_init(service, &root_count);
}

static void enter_sd(void);

static void test_long_name_extension_preserved(void)
{
    static const char * const extensions[] = {".json", ".jpeg"};
    unsigned test, index;
    char path[FILE_BROWSER_PATH_LENGTH];
    for(test = 0U; test < 2U; ++test) {
        const char *name;
        reset();
        long_extension = extensions[test];
        enter_sd();
        name = file_browser_state()->entries[0].name;
        assert(strlen(name) == 127U);
        assert(!strcmp(name + 122U, extensions[test]));
        assert(name[120U] == '.' && name[121U] == '.');
        for(index = 0U; index < 120U; index += 2U) {
            assert((unsigned char)name[index] == 0xD6U);
            assert((unsigned char)name[index + 1U] == 0xD0U);
        }
        assert(file_browser_enter(path, sizeof(path)) == FILE_BROWSER_ENTER_FILE);
        assert(!strcmp(path, test == 0U ? "0:/LONGNA~1.JSO" : "0:/LONGNA~1.JPE"));
    }
}

static void enter_sd(void)
{
    assert(file_browser_state()->virtual_root);
    assert(file_browser_state()->visible_count == 1U);
    assert(file_browser_enter(NULL, 0U) == FILE_BROWSER_ENTER_DIRECTORY);
    assert(!file_browser_state()->virtual_root);
    assert(!strcmp(last_open, "0:"));
    assert(opened == closed);
}

static uint32_t selected(void)
{
    const FileBrowserState *state = file_browser_state();
    return state->first_index + state->selected_row;
}

static void test_large_directory_and_aliases(void)
{
    const FileBrowserState *state;
    char path[FILE_BROWSER_PATH_LENGTH];
    unsigned index, previous_reads;
    uint16_t previous_revision;
    reset();
    enter_sd();
    state = file_browser_state();
    assert(state->total_count == 130U && state->visible_count == 6U);
    assert(strlen(state->entries[0].name) == 126U);
    for(index = 0U; index < 126U; index += 2U) {
        assert((unsigned char)state->entries[0].name[index] == 0xD6U);
        assert((unsigned char)state->entries[0].name[index + 1U] == 0xD0U);
    }
    assert(file_browser_enter(path, sizeof(path)) == FILE_BROWSER_ENTER_FILE);
    assert(!strcmp(path, "0:/LONGNA~1.TXT"));
    assert(file_browser_enter(path, 4U) == FILE_BROWSER_ENTER_ERROR);
    assert(state->status == FILE_BROWSER_PATH_LIMIT && path[0] == 0);
    file_browser_move(1);
    assert(file_browser_enter(path, sizeof(path)) == FILE_BROWSER_ENTER_FILE);
    assert(!strcmp(path, "0:/LONGNA~2.TXT"));
    file_browser_move(-1);
    previous_reads = reads;
    previous_revision = state->revision;
    for(index = 1U; index < 6U; ++index) file_browser_move(1);
    assert(reads == previous_reads);
    assert(state->revision == previous_revision);
    for(index = 6U; index < 130U; ++index) {
        file_browser_move(1);
        assert(selected() == index);
        assert(state->visible_count <= 6U && state->selected_row < state->visible_count);
        assert(opened == closed);
    }
    assert(!strcmp(state->entries[state->selected_row].name, "F00129.TXT"));
    assert(file_browser_enter(path, sizeof(path)) == FILE_BROWSER_ENTER_FILE);
    assert(!strcmp(path, "0:/F00129.TXT"));
    file_browser_move(1);
    assert(selected() == 0U && state->first_index == 0U);
    file_browser_move(-1);
    assert(selected() == 129U && state->first_index == 124U);
    for(index = 129U; index > 0U; --index) {
        file_browser_move(-1);
        assert(selected() == index - 1U);
    }
    file_browser_move(-1);
    assert(selected() == 129U);
}

static void test_circular_windows(void)
{
    static const unsigned counts[] = {0U, 1U, 3U, 6U, 7U, 13U, 130U};
    unsigned test, previous_reads;
    for(test = 0U; test < sizeof(counts) / sizeof(counts[0]); ++test) {
        const FileBrowserState *state;
        uint32_t last, last_first;
        reset();
        root_count = counts[test];
        enter_sd();
        state = file_browser_state();
        previous_reads = reads;
        last = root_count ? root_count - 1U : 0U;
        last_first = root_count > FILE_BROWSER_VISIBLE_ROWS ?
                     root_count - FILE_BROWSER_VISIBLE_ROWS : 0U;
        file_browser_move(-1);
        assert(selected() == last && state->first_index == last_first);
        if(root_count) assert(state->entries[state->selected_row].size == 1000U + last);
        assert(state->visible_count == (root_count < FILE_BROWSER_VISIBLE_ROWS ?
                                       root_count : FILE_BROWSER_VISIBLE_ROWS));
        file_browser_move(1);
        assert(selected() == 0U && state->first_index == 0U);
        if(root_count) assert(state->entries[0].size == 1000U);
        if(root_count <= FILE_BROWSER_VISIBLE_ROWS) assert(reads == previous_reads);
        assert(opened == closed);
    }
}

static void test_wrap_read_failure_and_directory_shrink(void)
{
    const FileBrowserState *state;
    reset();
    root_count = 13U;
    enter_sd();
    state = file_browser_state();
    read_error_at = 9;
    file_browser_move(-1);
    assert(state->status == FILE_BROWSER_IO_ERROR && state->visible_count == 0U);
    assert(opened == closed);
    read_error_at = -1;
    file_browser_refresh();
    assert(state->status == FILE_BROWSER_OK && selected() == 12U);
    file_browser_move(1);
    assert(selected() == 0U && state->first_index == 0U);

    cancel_at = (int)service_calls + 3;
    file_browser_move(-1);
    assert(state->status == FILE_BROWSER_CANCELLED && state->visible_count == 0U);
    assert(opened == closed);
    cancel_at = -1;
    file_browser_refresh();
    assert(state->status == FILE_BROWSER_OK && selected() == 12U);
    file_browser_move(1);
    root_count = 3U;
    file_browser_move(-1);
    assert(state->status == FILE_BROWSER_OK && state->total_count == 3U);
    assert(selected() == 2U && state->first_index == 0U);
    file_browser_move(1);
    assert(selected() == 0U && opened == closed);
}

static void test_navigation_restore_and_empty(void)
{
    uint32_t saved_first;
    unsigned index;
    char path[FILE_BROWSER_PATH_LENGTH];
    reset();
    enter_sd();
    for(index = 0U; index < 7U; ++index) file_browser_move(1);
    saved_first = file_browser_state()->first_index;
    assert(file_browser_enter(NULL, 0U) == FILE_BROWSER_ENTER_DIRECTORY);
    assert(!strcmp(last_open, "0:/DIR00007"));
    assert(file_browser_state()->total_count == 3U);
    assert(file_browser_enter(path, sizeof(path)) == FILE_BROWSER_ENTER_FILE);
    assert(!strcmp(path, "0:/DIR00007/C00000.TXT"));
    assert(!file_browser_back());
    assert(selected() == 7U && file_browser_state()->first_index == saved_first);
    child_count = 0U;
    assert(file_browser_enter(NULL, 0U) == FILE_BROWSER_ENTER_DIRECTORY);
    assert(file_browser_state()->status == FILE_BROWSER_EMPTY);
    assert(file_browser_state()->visible_count == 0U);
    assert(file_browser_enter(path, sizeof(path)) == FILE_BROWSER_ENTER_EMPTY);
    child_count = 1U;
    file_browser_refresh();
    assert(file_browser_state()->status == FILE_BROWSER_OK);
    assert(file_browser_state()->total_count == 1U);
    assert(!file_browser_back());
    assert(selected() == 7U);
    assert(!file_browser_back());
    assert(file_browser_state()->virtual_root);
    assert(file_browser_back());
    assert(opened == closed);
}

static void test_shrink_and_failures(void)
{
    unsigned index;
    reset();
    enter_sd();
    for(index = 0U; index < 129U; ++index) file_browser_move(1);
    root_count = 3U;
    file_browser_refresh();
    assert(file_browser_state()->total_count == 3U && selected() == 2U);
    assert(file_browser_state()->visible_count == 3U);
    card_present = 0;
    file_browser_refresh();
    assert(file_browser_state()->status == FILE_BROWSER_IO_ERROR);
    assert(file_browser_state()->result == FR_NOT_READY);
    assert(file_browser_state()->visible_count == 0U);
    card_present = 1;
    file_browser_refresh();
    assert(file_browser_state()->status == FILE_BROWSER_OK && selected() == 2U);
    read_error_at = 1;
    file_browser_refresh();
    assert(file_browser_state()->result == FR_DISK_ERR);
    assert(file_browser_state()->visible_count == 0U && opened == closed);
    read_error_at = -1;
    close_error = 1;
    file_browser_refresh();
    assert(file_browser_state()->result == FR_DISK_ERR && opened == closed);
    close_error = 0;
    cancel_at = (int)service_calls + 3;
    file_browser_refresh();
    assert(file_browser_state()->status == FILE_BROWSER_CANCELLED && opened == closed);
    assert(file_browser_state()->visible_count == 0U);
    cancel_at = -1;
    file_browser_refresh();
    assert(file_browser_state()->status == FILE_BROWSER_OK && selected() == 2U);
    root_count = 0U;
    file_browser_refresh();
    assert(file_browser_state()->status == FILE_BROWSER_EMPTY);
    assert(file_browser_state()->total_count == 0U && selected() == 0U);
}

static void test_path_boundary_and_cancel_before_open(void)
{
    unsigned levels, saved_opened;
    reset();
    deep_mode = 1;
    enter_sd();
    for(levels = 0U; levels < 39U; ++levels)
        assert(file_browser_enter(NULL, 0U) == FILE_BROWSER_ENTER_DIRECTORY);
    assert(strlen(file_browser_state()->path) == 509U);
    saved_opened = opened;
    assert(file_browser_enter(NULL, 0U) == FILE_BROWSER_ENTER_ERROR);
    assert(file_browser_state()->status == FILE_BROWSER_PATH_LIMIT);
    assert(opened == saved_opened);
    for(levels = 0U; levels < 39U; ++levels) assert(!file_browser_back());
    assert(!strcmp(file_browser_state()->path, "0:"));
    cancel_at = (int)service_calls + 1;
    saved_opened = opened;
    file_browser_refresh();
    assert(file_browser_state()->status == FILE_BROWSER_CANCELLED);
    assert(opened == saved_opened && opened == closed);
    cancel_at = -1;
    file_browser_refresh();
    assert(file_browser_state()->status == FILE_BROWSER_OK);

    reset();
    deep_mode = 2;
    enter_sd();
    for(levels = 0U; levels < FILE_BROWSER_MAX_DEPTH; ++levels)
        assert(file_browser_enter(NULL, 0U) == FILE_BROWSER_ENTER_DIRECTORY);
    saved_opened = opened;
    assert(file_browser_enter(NULL, 0U) == FILE_BROWSER_ENTER_ERROR);
    assert(file_browser_state()->status == FILE_BROWSER_PATH_LIMIT);
    assert(opened == saved_opened && opened == closed);
    for(levels = 0U; levels < FILE_BROWSER_MAX_DEPTH; ++levels) assert(!file_browser_back());
    assert(!strcmp(file_browser_state()->path, "0:"));
}

int main(void)
{
    test_large_directory_and_aliases();
    test_circular_windows();
    test_wrap_read_failure_and_directory_shrink();
    test_navigation_restore_and_empty();
    test_shrink_and_failures();
    test_path_boundary_and_cancel_before_open();
    test_long_name_extension_preserved();
    puts("file browser tests: PASS (circular windows, large directories, CP936/SFN, navigation, media errors, cancellation, path bounds)");
    return 0;
}
