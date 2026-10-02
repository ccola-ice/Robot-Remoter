#ifndef FILE_TEXT_H
#define FILE_TEXT_H

#include <stdint.h>

#define FILE_TEXT_ROWS 14u
#define FILE_TEXT_COLUMNS 74u

typedef enum {
    FILE_TEXT_ENCODING_UTF8 = 0,
    FILE_TEXT_ENCODING_GBK,
    FILE_TEXT_ENCODING_UTF16_LE,
    FILE_TEXT_ENCODING_UTF16_BE
} file_text_encoding_t;

typedef enum {
    FILE_TEXT_OK = 0,
    FILE_TEXT_END,
    FILE_TEXT_CANCELLED,
    FILE_TEXT_IO_ERROR,
    FILE_TEXT_NOT_OPEN,
    FILE_TEXT_INVALID
} file_text_result_t;

typedef struct {
    char lines[FILE_TEXT_ROWS][FILE_TEXT_COLUMNS + 1u];
    uint32_t page_number;
    uint32_t file_size;
    uint32_t start_offset;
    uint32_t end_offset;
    uint8_t line_count;
    uint8_t has_next;
    uint8_t has_previous;
    file_text_encoding_t encoding;
    file_text_result_t error;
    uint8_t fatfs_error;
} file_text_page_t;

/* 回调返回非零继续，返回零取消；回调只能运行后台任务，不能重入本模块。 */
typedef uint8_t (*file_text_service_t)(void *context);
void file_text_set_service(file_text_service_t service, void *context);

/* 全部操作只读；支持 UTF-8、GBK 和两种 UTF-16 字节序，输出 GB2312 页面。 */
/* 无 BOM 的 UTF-16 按 ASCII 高位零特征识别；无法显示的字符以问号替代。 */
/* 解码后的 NUL 空字符不占显示位置；读取不会删除或改写文件中的零填充。 */
file_text_result_t file_text_open(const char *path);
file_text_result_t file_text_next(void);
file_text_result_t file_text_previous(void);
/* 手动指定编码后回到第一页；可用于纠正无 BOM 文件的自动判断。 */
file_text_result_t file_text_set_encoding(file_text_encoding_t encoding);
void file_text_close(void);
const file_text_page_t *file_text_get_page(void);

#endif
