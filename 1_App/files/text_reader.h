#ifndef TEXT_READER_H
#define TEXT_READER_H

#include <stdint.h>

#define FILE_TEXT_ROWS 14u
#define FILE_TEXT_COLUMNS 74u

/* 文件输入编码；页面正文统一转换为 ASCII/GB2312，手动选择不会改写原文件。 */
typedef enum {
    FILE_TEXT_ENCODING_UTF8 = 0,
    FILE_TEXT_ENCODING_GBK,
    FILE_TEXT_ENCODING_UTF16_LE,
    FILE_TEXT_ENCODING_UTF16_BE
} file_text_encoding_t;

/* END 表示当前方向已无可翻页内容；CANCELLED 表示后台请求终止本次操作。 */
typedef enum {
    FILE_TEXT_OK = 0,
    FILE_TEXT_END,
    FILE_TEXT_CANCELLED,
    FILE_TEXT_IO_ERROR,
    FILE_TEXT_NOT_OPEN,
    FILE_TEXT_INVALID
} file_text_result_t;

/* 成功提交的阅读页面：行内容为 ASCII/GB2312 字节串，每行均预留结束符。
 * GB2312 双字节字形占两列，page_number 从 1 开始；错误字段可独立于正文更新。 */
typedef struct {
    char lines[FILE_TEXT_ROWS][FILE_TEXT_COLUMNS + 1u];
    uint32_t page_number;
    uint32_t file_size;
    /* 页首、页尾的文件字节位置，不是字符序号；制表符续页状态由模块另行维护。 */
    uint32_t start_offset;
    uint32_t end_offset;
    uint8_t line_count;
    uint8_t has_next;
    uint8_t has_previous;
    file_text_encoding_t encoding;
    /* 页面生成过程的结果及 FatFs 错误码，读取失败时原成功页面仍可显示。 */
    file_text_result_t error;
    uint8_t fatfs_error;
} file_text_page_t;

/* 回调返回非零继续，返回零取消；回调只能运行后台任务，不能重入本模块。 */
typedef uint8_t (*file_text_service_t)(void *context);
/* 注册可选后台回调；设置后跨打开和关闭操作保留，context 原样传回调用者。 */
void file_text_set_service(file_text_service_t service, void *context);

/* 全部操作只读；支持 UTF-8、GBK 和两种 UTF-16 字节序，输出 GB2312 页面。 */
/* 无 BOM 的 UTF-16 按 ASCII 高位零特征识别；无法显示的字符以问号替代。 */
/* 解码后的 NUL 空字符不占显示位置；读取不会删除或改写文件中的零填充。
 * 打开会先关闭旧文件；返回 OK 时首页已就绪，打开或首页读取失败时不保留新文件。 */
file_text_result_t file_text_open(const char *path);
/* 翻页成功才替换当前页；取消或 I/O 错误时保留原正文，调用者可重试。 */
file_text_result_t file_text_next(void);
/* 上一页若已离开书签缓存，会从较早定位点重新分页，耗时可能长于下一页操作。 */
file_text_result_t file_text_previous(void);
/* 手动指定编码后回到第一页；可用于纠正无 BOM 文件的自动判断。
 * 仅首页重读成功时采用新编码并重建书签，失败则保留原编码和页面。 */
file_text_result_t file_text_set_encoding(file_text_encoding_t encoding);
/* 关闭文件并使当前页面失效；允许在未打开文件时调用。 */
void file_text_close(void);
/* 返回模块内部页面，调用者只读；后续阅读操作或关闭可能更新其内容。 */
const file_text_page_t *file_text_get_page(void);

#endif
