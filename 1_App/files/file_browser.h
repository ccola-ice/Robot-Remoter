#ifndef FILE_BROWSER_H
#define FILE_BROWSER_H

#include <stddef.h>
#include <stdint.h>
#include "ff.h"
#include "ui_pages.h"

#define FILE_BROWSER_VISIBLE_ROWS 6U
#define FILE_BROWSER_PATH_LENGTH 512U
#define FILE_BROWSER_MAX_DEPTH 40U

/* 长目录扫描中的协作服务入口；允许为空，调用期间不能重入文件浏览接口。 */
typedef uint8_t (*FileBrowserService)(void *context);

/* 浏览状态与 FatFs 原始错误分开保存，便于区分空目录、路径上限和主动取消。 */
typedef enum {
    FILE_BROWSER_OK,
    FILE_BROWSER_EMPTY,
    FILE_BROWSER_IO_ERROR,
    FILE_BROWSER_PATH_LIMIT,
    FILE_BROWSER_CANCELLED
} FileBrowserStatus;

/* 进入操作的结果独立于浏览状态，供上层决定切目录、打开文件或显示错误。 */
typedef enum {
    FILE_BROWSER_ENTER_DIRECTORY,
    FILE_BROWSER_ENTER_FILE,
    FILE_BROWSER_ENTER_ERROR,
    FILE_BROWSER_ENTER_EMPTY
} FileBrowserEnterResult;

/* 只读界面快照：entries 仅含当前窗口，total_count 表示整个目录的有效项数。
 * 模块内部持有此对象，调用者不应修改或跨操作假定其中内容不变。 */
typedef struct {
    char path[FILE_BROWSER_PATH_LENGTH];
    GuiFileEntry entries[FILE_BROWSER_VISIBLE_ROWS];
    uint32_t total_count;
    uint32_t first_index;
    /* 内容或状态更新的版本号；同一窗口内只移动选中行时可能保持不变。 */
    uint16_t revision;
    uint8_t visible_count;
    /* 非空窗口中选中项在 entries 中的下标；对应的目录绝对索引为 first_index + selected_row。 */
    uint8_t selected_row;
    uint8_t virtual_root;
    FileBrowserStatus status;
    FRESULT result;
} FileBrowserState;

/* 初始化单个浏览会话并回到只含 SD 入口的虚拟根。
 * 回调服务输入与控制通信；返回 0 中止本次文件系统操作。 */
void file_browser_init(FileBrowserService service, void *context);
/* 返回模块持有的状态地址，后续刷新、进入或移动操作会更新该对象。 */
const FileBrowserState *file_browser_state(void);
/* direction 的正负分别表示下一项/上一项，零不移动；有效列表在首尾循环。 */
void file_browser_move(int direction);
/* 文件路径使用独立短文件名，绝不使用可能截断的显示名称。
 * 仅返回 FILE_BROWSER_ENTER_FILE 时 path 含可打开路径，capacity 包括结束符空间；
 * 进入目录使用内部路径，无需调用者提供输出缓冲。 */
FileBrowserEnterResult file_browser_enter(char *path, size_t capacity);
/* 已在虚拟根时返回 1，调用方可退出文件浏览页面。 */
uint8_t file_browser_back(void);
/* 完整重扫当前目录并修正已越界的选择；介质失败或取消时会清空旧目录项。 */
void file_browser_refresh(void);

#endif
