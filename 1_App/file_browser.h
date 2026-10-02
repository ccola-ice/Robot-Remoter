#ifndef FILE_BROWSER_H
#define FILE_BROWSER_H

#include <stddef.h>
#include <stdint.h>
#include "ff.h"
#include "gui.h"

#define FILE_BROWSER_VISIBLE_ROWS 6U
#define FILE_BROWSER_PATH_LENGTH 512U
#define FILE_BROWSER_MAX_DEPTH 40U

typedef uint8_t (*FileBrowserService)(void *context);

typedef enum {
    FILE_BROWSER_OK,
    FILE_BROWSER_EMPTY,
    FILE_BROWSER_IO_ERROR,
    FILE_BROWSER_PATH_LIMIT,
    FILE_BROWSER_CANCELLED
} FileBrowserStatus;

typedef enum {
    FILE_BROWSER_ENTER_DIRECTORY,
    FILE_BROWSER_ENTER_FILE,
    FILE_BROWSER_ENTER_ERROR,
    FILE_BROWSER_ENTER_EMPTY
} FileBrowserEnterResult;

typedef struct {
    char path[FILE_BROWSER_PATH_LENGTH];
    GuiFileEntry entries[FILE_BROWSER_VISIBLE_ROWS];
    uint32_t total_count;
    uint32_t first_index;
    uint16_t revision;
    uint8_t visible_count;
    uint8_t selected_row;
    uint8_t virtual_root;
    FileBrowserStatus status;
    FRESULT result;
} FileBrowserState;

/* 回调服务输入与控制通信；返回 0 中止本次文件系统操作。 */
void file_browser_init(FileBrowserService service, void *context);
const FileBrowserState *file_browser_state(void);
void file_browser_move(int direction);
/* 文件路径使用独立短文件名，绝不使用可能截断的显示名称。 */
FileBrowserEnterResult file_browser_enter(char *path, size_t capacity);
/* 已在虚拟根时返回 1，调用方可退出文件浏览页面。 */
uint8_t file_browser_back(void);
void file_browser_refresh(void);

#endif
