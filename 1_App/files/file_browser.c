/* 文件浏览分为目录扫描、可见窗口和路径导航三层。
 * 列表名称用于显示，实际访问始终使用短文件名；耗时扫描通过回调服务后台任务。 */
#include "file_browser.h"
#include <string.h>

#define FILE_BROWSER_LFN_LENGTH 512U
#define FILE_BROWSER_ALIAS_LENGTH 13U

/* 每深入一级目录保存一个导航记录，只记路径长度，不复制整条父路径。 */
typedef struct {
    uint32_t selected;
    uint32_t first;
    uint16_t path_length;
} FileBrowserParent;

static FileBrowserState browser;
static char aliases[FILE_BROWSER_VISIBLE_ROWS][FILE_BROWSER_ALIAS_LENGTH];
static FileBrowserParent parents[FILE_BROWSER_MAX_DEPTH];
static uint8_t depth;
/* 选中项与窗口起点使用目录内的绝对索引，selected_row 仅表示屏幕中的行号。 */
static uint32_t selected_index;
static uint32_t window_first;
static FileBrowserService service_callback;
static void *service_context;

/* 工作区放在静态存储区，避免长文件名与目录窗口占用主循环栈。 */
static GuiFileEntry scan_entries[FILE_BROWSER_VISIBLE_ROWS];
static char scan_aliases[FILE_BROWSER_VISIBLE_ROWS][FILE_BROWSER_ALIAS_LENGTH];
static char long_name[FILE_BROWSER_LFN_LENGTH];

static uint8_t browser_service(void)
{
    return !service_callback || service_callback(service_context);
}

/* 发布本次操作状态，并递增版本号，供界面识别目录内容或状态的更新。 */
static void browser_status(FileBrowserStatus status, FRESULT result)
{
    browser.status = status;
    browser.result = result;
    ++browser.revision;
}

static void browser_failed(FileBrowserStatus status, FRESULT result)
{
    /* 清除旧卡的目录项，避免读卡失败后仍然打开旧列表中的文件。 */
    browser.visible_count = 0U;
    browser.total_count = 0U;
    browser.selected_row = 0U;
    memset(browser.entries, 0, sizeof(browser.entries));
    memset(aliases, 0, sizeof(aliases));
    browser_status(status, result);
}

/* 检查 FatFs 本地编码中的双字节边界；这里只验证字节范围，不查询字符映射表。 */
static uint8_t dbcs_pair(const unsigned char *text)
{
    return text[0] >= 0x81U && text[0] <= 0xFEU &&
           text[1] >= 0x40U && text[1] <= 0xFEU && text[1] != 0x7FU;
}

/* 将显示名称限制在目标缓冲内，保留完整双字节字符并补字符串结束符。
 * 超长名称优先保留可识别的短扩展名；生成的显示文本不能用作打开路径。 */
static void copy_display_name(char *target, size_t capacity, const char *source)
{
    size_t used = 0U;
    size_t limit = capacity;
    size_t extension_length = 0U;
    const char *extension = NULL;
    const unsigned char *input = (const unsigned char *)source;
    if(strlen(source) >= capacity) {
        const char *candidate = strrchr(source, '.');
        if(candidate && candidate != source) {
            size_t length = strlen(candidate);
            size_t index;
            uint8_t valid = length > 1U && length <= 12U;
            for(index = 1U; valid && index < length; ++index) {
                unsigned char ch = (unsigned char)candidate[index];
                if(!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                     (ch >= '0' && ch <= '9'))) valid = 0U;
            }
            if(valid && length + 3U < capacity) {
                extension = candidate;
                extension_length = length;
                limit -= length + 2U;
            }
        }
    }
    while(*input) {
        size_t length = dbcs_pair(input) ? 2U : 1U;
        if(used + length >= limit) break;
        /* 非法编码显示为问号，避免单独输出多字节字符的首字节。 */
        if(length == 1U && *input >= 0x80U) target[used++] = '?';
        else {
            target[used++] = (char)*input;
            if(length == 2U) target[used++] = (char)input[1];
        }
        input += length;
    }
    /* 保留长名称末尾的短扩展名，便于识别被 SFN 缩写的 JSON、JPEG 等。 */
    if(extension) {
        target[used++] = '.';
        target[used++] = '.';
        memcpy(target + used, extension, extension_length);
        used += extension_length;
    }
    target[used] = '\0';
}

/* 只接受有结束符的短文件名单项，过滤 .、.. 和路径分隔符。
 * 扫描时跳过无效项，使保存的别名可以安全拼接到当前目录后。 */
static uint8_t alias_valid(const char *name)
{
    size_t index = 0U;
    if(!name[0] || !strcmp(name, ".") || !strcmp(name, "..")) return 0U;
    while(index < FILE_BROWSER_ALIAS_LENGTH && name[index]) {
        const unsigned char *ch = (const unsigned char *)&name[index];
        if(index + 1U < FILE_BROWSER_ALIAS_LENGTH && dbcs_pair(ch)) index += 2U;
        else {
            if(*ch >= 0x80U || *ch == '/' || *ch == '\\' || *ch == ':') return 0U;
            ++index;
        }
    }
    return index < FILE_BROWSER_ALIAS_LENGTH;
}

/* 虚拟根只展示 SD 入口；进入后才以 FatFs 的 0: 作为实际文件系统根目录。 */
static void browser_virtual_root(void)
{
    memset(browser.entries, 0, sizeof(browser.entries));
    memset(aliases, 0, sizeof(aliases));
    strcpy(browser.path, "/");
    strcpy(browser.entries[0].name, "SD");
    browser.entries[0].is_directory = 1U;
    browser.visible_count = 1U;
    browser.selected_row = 0U;
    browser.total_count = 1U;
    browser.first_index = 0U;
    browser.virtual_root = 1U;
    depth = 0U;
    selected_index = 0U;
    window_first = 0U;
    browser_status(FILE_BROWSER_OK, FR_OK);
}

/* 完整刷新统计目录数量；滚动时只读取目标窗口，保持内存占用固定。 */
static void browser_scan(uint8_t count_all)
{
    DIR directory;
    FILINFO info;
    FRESULT result, close_result;
    uint32_t count = 0U;
    uint8_t visible = 0U;
    uint8_t reached_end = 0U;
    uint8_t cancelled = 0U;
    uint8_t retried = 0U;

scan_again:
    count = 0U;
    visible = reached_end = cancelled = 0U;
    if(!browser_service()) {
        browser_failed(FILE_BROWSER_CANCELLED, FR_OK);
        return;
    }
    result = f_opendir(&directory, browser.path);
    if(result != FR_OK) {
        browser_failed(FILE_BROWSER_IO_ERROR, result);
        return;
    }
    memset(scan_entries, 0, sizeof(scan_entries));
    memset(scan_aliases, 0, sizeof(scan_aliases));
    memset(&info, 0, sizeof(info));
    info.lfname = long_name;
    info.lfsize = sizeof(long_name);
    for(;;) {
        if(!browser_service()) {
            cancelled = 1U;
            break;
        }
        long_name[0] = '\0';
        result = f_readdir(&directory, &info);
        if(result != FR_OK) break;
        if(!info.fname[0]) {
            reached_end = 1U;
            break;
        }
        if((info.fattrib & 0x08U) || !alias_valid(info.fname)) continue;
        if(count >= window_first && visible < FILE_BROWSER_VISIBLE_ROWS) {
            GuiFileEntry *entry = &scan_entries[visible];
            copy_display_name(entry->name, sizeof(entry->name),
                              long_name[0] ? long_name : info.fname);
            entry->size = info.fsize;
            entry->date = info.fdate;
            entry->time = info.ftime;
            entry->is_directory = (info.fattrib & AM_DIR) != 0U;
            strcpy(scan_aliases[visible], info.fname);
            ++visible;
        }
        ++count;
        if(!count_all && visible == FILE_BROWSER_VISIBLE_ROWS) break;
    }
    close_result = f_closedir(&directory);
    if(cancelled) {
        browser_failed(FILE_BROWSER_CANCELLED, FR_OK);
        return;
    }
    if(result == FR_OK) result = close_result;
    if(result != FR_OK) {
        browser_failed(FILE_BROWSER_IO_ERROR, result);
        return;
    }
    if(reached_end) browser.total_count = count;
    if(count && reached_end && selected_index >= count) {
        selected_index = count - 1U;
        window_first = selected_index >= FILE_BROWSER_VISIBLE_ROWS ?
                       selected_index - FILE_BROWSER_VISIBLE_ROWS + 1U : 0U;
        /* 目录刷新后变短时，再读取包含最后一项的窗口。 */
        if(retried) {
            browser_failed(FILE_BROWSER_IO_ERROR, FR_INT_ERR);
            return;
        }
        retried = 1U;
        count_all = 0U;
        goto scan_again;
    }
    if(!count) selected_index = window_first = 0U;
    /* 扫描并关闭目录成功后统一提交窗口，避免界面读取到只更新了一半的列表。 */
    memcpy(browser.entries, scan_entries, sizeof(browser.entries));
    memcpy(aliases, scan_aliases, sizeof(aliases));
    browser.visible_count = visible;
    browser.first_index = window_first;
    browser.selected_row = visible ? (uint8_t)(selected_index - window_first) : 0U;
    browser_status(visible ? FILE_BROWSER_OK : FILE_BROWSER_EMPTY, FR_OK);
}

/* 注册协作回调并重置到虚拟根；本模块只保存单个浏览会话。 */
void file_browser_init(FileBrowserService service, void *context)
{
    memset(&browser, 0, sizeof(browser));
    service_callback = service;
    service_context = context;
    browser_virtual_root();
}

const FileBrowserState *file_browser_state(void)
{
    return &browser;
}

/* 重扫当前目录并重新统计总数；若删除文件导致选择越界，扫描过程会修正窗口。 */
void file_browser_refresh(void)
{
    if(browser.virtual_root) browser_virtual_root();
    else browser_scan(1U);
}

/* 根据方向符号移动一项，首尾循环；只有选中项离开可见窗口时才重读目录。 */
void file_browser_move(int direction)
{
    if(browser.virtual_root || !browser.visible_count || !direction) return;
    if(direction < 0) {
        selected_index = selected_index ? selected_index - 1U : browser.total_count - 1U;
    } else {
        selected_index = selected_index + 1U < browser.total_count ? selected_index + 1U : 0U;
    }
    /* 首尾循环后仍按选中项调整窗口，短目录无需重新读取。 */
    if(selected_index < window_first) {
        window_first = selected_index;
        browser_scan(0U);
    } else if(selected_index >= window_first + browser.visible_count) {
        window_first = selected_index - FILE_BROWSER_VISIBLE_ROWS + 1U;
        browser_scan(0U);
    } else {
        browser.selected_row = (uint8_t)(selected_index - window_first);
        /* 同一窗口内只改变选中行，让界面仅重绘旧选中行与新选中行。 */
        if(browser.status != FILE_BROWSER_OK || browser.result != FR_OK)
            browser_status(FILE_BROWSER_OK, FR_OK);
    }
}

/* 文件项通过 path 输出完整短文件名路径，目录项则推进内部导航并扫描新目录。
 * 拼接前检查缓冲容量及导航深度；失败原因保存在 browser 的状态和 FatFs 结果中。 */
FileBrowserEnterResult file_browser_enter(char *path, size_t capacity)
{
    const GuiFileEntry *entry;
    const char *alias;
    size_t path_length, alias_length;
    if(path && capacity) path[0] = '\0';
    if(browser.virtual_root) {
        strcpy(browser.path, "0:");
        browser.virtual_root = 0U;
        selected_index = window_first = 0U;
        browser_scan(1U);
        return browser.status == FILE_BROWSER_IO_ERROR || browser.status == FILE_BROWSER_CANCELLED ?
               FILE_BROWSER_ENTER_ERROR : FILE_BROWSER_ENTER_DIRECTORY;
    }
    if(!browser.visible_count) return FILE_BROWSER_ENTER_EMPTY;
    if(!browser_service()) {
        browser_status(FILE_BROWSER_CANCELLED, FR_OK);
        return FILE_BROWSER_ENTER_ERROR;
    }
    entry = &browser.entries[browser.selected_row];
    alias = aliases[browser.selected_row];
    path_length = strlen(browser.path);
    alias_length = strlen(alias);
    if(path_length + alias_length + 2U > sizeof(browser.path)) {
        browser_status(FILE_BROWSER_PATH_LIMIT, FR_OK);
        return FILE_BROWSER_ENTER_ERROR;
    }
    if(!entry->is_directory) {
        if(!path || path_length + alias_length + 2U > capacity) {
            browser_status(FILE_BROWSER_PATH_LIMIT, FR_OK);
            return FILE_BROWSER_ENTER_ERROR;
        }
        memcpy(path, browser.path, path_length);
        path[path_length] = '/';
        strcpy(path + path_length + 1U, alias);
        browser_status(FILE_BROWSER_OK, FR_OK);
        return FILE_BROWSER_ENTER_FILE;
    }
    if(depth >= FILE_BROWSER_MAX_DEPTH) {
        browser_status(FILE_BROWSER_PATH_LIMIT, FR_OK);
        return FILE_BROWSER_ENTER_ERROR;
    }
    /* 入栈保存父目录的路径截断点与浏览位置，返回时可恢复原选中项和滚动窗口。 */
    parents[depth].selected = selected_index;
    parents[depth].first = window_first;
    parents[depth].path_length = (uint16_t)path_length;
    ++depth;
    /* 文件访问与路径栏共用完整的短文件名路径，目录列表仍保留长文件名。 */
    browser.path[path_length] = '/';
    strcpy(browser.path + path_length + 1U, alias);
    selected_index = window_first = 0U;
    browser_scan(1U);
    return browser.status == FILE_BROWSER_IO_ERROR || browser.status == FILE_BROWSER_CANCELLED ?
           FILE_BROWSER_ENTER_ERROR : FILE_BROWSER_ENTER_DIRECTORY;
}

/* 从子目录弹出导航记录并重新扫描父目录，从 SD 根返回虚拟根。
 * 仅在调用前已经处于虚拟根时返回 1，表示浏览页面可以退出。 */
uint8_t file_browser_back(void)
{
    if(browser.virtual_root) return 1U;
    if(!depth) browser_virtual_root();
    else {
        --depth;
        browser.path[parents[depth].path_length] = '\0';
        selected_index = parents[depth].selected;
        window_first = parents[depth].first;
        browser_scan(1U);
    }
    return 0U;
}
