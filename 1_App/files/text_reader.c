/* 流式阅读链路：字节缓存 -> 编码识别与码点解码 -> LCD 字符转换 -> 固定行列分页。
 * 当前页与待提交页分离，翻页只保存定位书签，不把全文或所有页面常驻内存。 */
#include "text_reader.h"
#include "ff.h"
#include <string.h>

#define TEXT_READ_BYTES 512u
#define TEXT_PROBE_BYTES 8192u
#define TEXT_HISTORY 64u
#define TEXT_EOF 0xffffffffu

/* 分页位置除文件偏移外还保存未输出的制表符空格，避免跨页时丢失缩进。 */
typedef struct {
    uint32_t offset;
    uint8_t spaces;
} text_position_t;

/* 环形书签槽同时记录页号，避免取模覆盖后把旧槽误认成目标页的位置。 */
typedef struct {
    uint32_t number;
    text_position_t position;
} text_bookmark_t;

/* 固定内存：文件对象、两个页面、读取缓冲和最近 64 页定位，不缓存全文。 */
static FIL text_file;
static file_text_page_t text_page;
static file_text_page_t text_pending;
static uint8_t text_buffer[TEXT_READ_BYTES];
static text_bookmark_t text_history[TEXT_HISTORY];
/* 缓存范围描述文件中的实际字节；逻辑游标可以在该范围内回退而无需重新读卡。 */
static uint32_t text_buffer_offset;
static uint32_t text_buffer_length;
static uint32_t text_size;
static uint32_t text_cursor;
static uint32_t text_bom_bytes;
static text_position_t text_next_position;
static file_text_encoding_t text_encoding;
static file_text_result_t text_status;
static uint8_t text_disk_error;
static uint8_t text_opened;
static uint8_t text_service_ticks;
static file_text_service_t text_service;
static void *text_service_context;

/* 编译期守住 8 KiB 预算，额外预留 128 字节给标量状态和对齐。 */
typedef char text_memory_budget[(sizeof(text_file) + sizeof(text_page) +
    sizeof(text_pending) + sizeof(text_buffer) + sizeof(text_history) + 128u <= 8192u) ? 1 : -1];

/* 已有错误时立即停下；否则调用后台服务，并将主动退出转换为取消状态。 */
static uint8_t text_poll(void)
{
    if (text_status != FILE_TEXT_OK)
        return 0;
    if (text_service != 0 && !text_service(text_service_context)) {
        text_status = FILE_TEXT_CANCELLED;
        return 0;
    }
    return 1;
}

/* 每次独立阅读操作重新建立错误状态，使取消或上次翻页失败后仍可再次尝试。 */
static void text_begin(void)
{
    text_status = FILE_TEXT_OK;
    text_disk_error = FR_OK;
    text_service_ticks = 0;
    (void)text_poll();
}

/* 只更新当前页的错误信息，保留上一次成功提交的正文、页号及翻页位置。 */
static file_text_result_t text_failure(void)
{
    text_page.error = text_status;
    text_page.fatfs_error = text_disk_error;
    return text_status;
}

/* 返回逻辑游标处的下一字节，缓存未命中时按需定位并补读。
 * TEXT_EOF 同时表示正常文件尾和读取失败，调用方需结合 text_status 区分。 */
static uint32_t text_byte(void)
{
    UINT count;
    FRESULT result;
    if (text_status != FILE_TEXT_OK || text_cursor >= text_size)
        return TEXT_EOF;
    if (text_cursor < text_buffer_offset ||
        text_cursor - text_buffer_offset >= text_buffer_length) {
        if (!text_poll())
            return TEXT_EOF;
        if ((uint32_t)f_tell(&text_file) != text_cursor) {
            result = f_lseek(&text_file, text_cursor);
            if (result != FR_OK)
                goto failed;
        }
        text_buffer_offset = text_cursor;
        text_buffer_length = 0;
        result = f_read(&text_file, text_buffer, TEXT_READ_BYTES, &count);
        if (result != FR_OK)
            goto failed;
        /* 打开后文件被截短、介质失效而返回提前结束时，不把残页当作成功。 */
        if (count == 0u) {
            result = FR_INT_ERR;
            goto failed;
        }
        text_buffer_length = count;
    }
    return text_buffer[text_cursor++ - text_buffer_offset];
failed:
    text_status = FILE_TEXT_IO_ERROR;
    text_disk_error = (uint8_t)result;
    return TEXT_EOF;
}

/* 严格校验 UTF-8，拒绝超长形式、代理项和越界码点；错误首字节单独替换。 */
static uint32_t text_utf8(uint8_t *valid)
{
    uint32_t start = text_cursor;
    uint32_t first = text_byte();
    uint32_t value;
    uint32_t next;
    uint32_t minimum;
    uint8_t remaining;
    *valid = 1;
    if (first == TEXT_EOF || first < 0x80u)
        return first;
    if (first >= 0xc2u && first <= 0xdfu) {
        value = first & 0x1fu;
        remaining = 1;
        minimum = 0x80u;
    } else if (first >= 0xe0u && first <= 0xefu) {
        value = first & 0x0fu;
        remaining = 2;
        minimum = 0x800u;
    } else if (first >= 0xf0u && first <= 0xf4u) {
        value = first & 7u;
        remaining = 3;
        minimum = 0x10000u;
    } else {
        *valid = 0;
        return '?';
    }
    while (remaining-- != 0u) {
        next = text_byte();
        if (next == TEXT_EOF || (next & 0xc0u) != 0x80u) {
            text_cursor = start + 1u;
            *valid = 0;
            return '?';
        }
        value = (value << 6) | (next & 0x3fu);
    }
    if (value < minimum || value > 0x10ffffu ||
        (value >= 0xd800u && value <= 0xdfffu)) {
        *valid = 0;
        return '?';
    }
    return value;
}

/* 按已选字节序读取一个 UTF-16 码元；奇数字节尾部以问号表示残缺码元。 */
static uint32_t text_utf16_unit(void)
{
    uint32_t first = text_byte();
    uint32_t second;
    if (first == TEXT_EOF)
        return TEXT_EOF;
    second = text_byte();
    if (second == TEXT_EOF)
        return '?';
    return text_encoding == FILE_TEXT_ENCODING_UTF16_LE ?
        first | (second << 8) : (first << 8) | second;
}

/* 将当前编码统一解码为 Unicode 码点，GBK 借用 FatFs 码表，UTF-16 合并代理对。
 * 非法续字节或无效代理对不会吞掉后续可独立解码的字符。 */
static uint32_t text_codepoint(void)
{
    uint32_t value;
    uint32_t second;
    uint32_t after_first;
    uint8_t valid;
    /* 命中读取缓存时同样定期服务后台，长时间解码不会阻塞输入和控制通信。 */
    if (++text_service_ticks == 128u) {
        text_service_ticks = 0;
        if (!text_poll())
            return TEXT_EOF;
    }
    if (text_encoding == FILE_TEXT_ENCODING_UTF8)
        return text_utf8(&valid);
    if (text_encoding == FILE_TEXT_ENCODING_GBK) {
        value = text_byte();
        if (value == TEXT_EOF || value < 0x80u)
            return value;
        if (value < 0x81u || value > 0xfeu)
            return '?';
        after_first = text_cursor;
        second = text_byte();
        if (second < 0x40u || second > 0xfeu || second == 0x7fu) {
            text_cursor = after_first;
            return '?';
        }
        value = ff_convert((WCHAR)((value << 8) | second), 1);
        return value != 0u ? value : '?';
    }
    value = text_utf16_unit();
    if (value >= 0xd800u && value <= 0xdbffu) {
        after_first = text_cursor;
        second = text_utf16_unit();
        if (second >= 0xdc00u && second <= 0xdfffu)
            return 0x10000u + ((value - 0xd800u) << 10) + second - 0xdc00u;
        text_cursor = after_first;
        return '?';
    }
    if (value >= 0xdc00u && value <= 0xdfffu)
        return '?';
    return value;
}

/* NUL 是不可见的空字符，旧测试文件尾部含大量零填充。
 * 必须在解码后忽略 U+0000，不能丢弃 UTF-16 字符内部的零字节。 */
static uint32_t text_nonzero_codepoint(void)
{
    uint32_t value;
    do {
        value = text_codepoint();
    } while (value == 0u);
    return value;
}

/* 将 CR、CRLF 统一为换行；预读到普通字符时回退游标，留给下次处理。 */
static uint32_t text_character(void)
{
    uint32_t value = text_nonzero_codepoint();
    uint32_t after_cr;
    if (value == '\r') {
        after_cr = text_cursor;
        if (text_nonzero_codepoint() != '\n')
            text_cursor = after_cr;
        return '\n';
    }
    return value;
}

/* LCD 的双字节字形入口仅接受 GB2312；GBK 扩展字符不能原样传给它。 */
static uint16_t text_display_code(uint32_t value)
{
    uint16_t code;
    if (value >= 0x20u && value <= 0x7eu)
        return (uint16_t)value;
    if (value > 0xffffu)
        return '?';
    code = ff_convert((WCHAR)value, 0);
    if ((code >> 8) >= 0xa1u && (code >> 8) <= 0xf7u &&
        (code & 0xffu) >= 0xa1u && (code & 0xffu) <= 0xfeu)
        return code;
    return '?';
}

/* 无 BOM 的 UTF-16 只能按特征判断；没有高位零证据时保留 UTF-8/GBK 判断。 */
static uint8_t text_detect_utf16(uint32_t limit)
{
    uint32_t first;
    uint32_t second;
    uint32_t little_ascii = 0;
    uint32_t big_ascii = 0;
    uint32_t little_invalid = 0;
    uint32_t big_invalid = 0;
    uint32_t pairs = 0;
    uint32_t value;
    if ((text_size & 1u) != 0u)
        return 0;
    text_cursor = 0;
    while (text_cursor + 1u < limit && text_status == FILE_TEXT_OK) {
        first = text_byte();
        second = text_byte();
        if (first == TEXT_EOF || second == TEXT_EOF)
            break;
        ++pairs;
        if (second == 0u && ((first >= 0x20u && first <= 0x7eu) ||
            first == '\t' || first == '\r' || first == '\n'))
            ++little_ascii;
        if (first == 0u && ((second >= 0x20u && second <= 0x7eu) ||
            second == '\t' || second == '\r' || second == '\n'))
            ++big_ascii;
        value = first | (second << 8);
        if ((value != 0u && value < 0x20u && value != '\t' && value != '\r' && value != '\n') ||
            value == 0xfffeu || value == 0xffffu)
            ++little_invalid;
        value = (first << 8) | second;
        if ((value != 0u && value < 0x20u && value != '\t' && value != '\r' && value != '\n') ||
            value == 0xfffeu || value == 0xffffu)
            ++big_invalid;
        if ((pairs & 63u) == 0u && !text_poll())
            break;
    }
    /* 至少两个同向 ASCII 字符，且没有相反字节序或控制码的矛盾证据。 */
    if (text_status == FILE_TEXT_OK && little_ascii >= 2u &&
        big_ascii == 0u && little_invalid == 0u) {
        text_encoding = FILE_TEXT_ENCODING_UTF16_LE;
        return 1;
    }
    if (text_status == FILE_TEXT_OK && big_ascii >= 2u &&
        little_ascii == 0u && big_invalid == 0u) {
        text_encoding = FILE_TEXT_ENCODING_UTF16_BE;
        return 1;
    }
    return 0;
}

/* 优先识别 BOM；无 BOM 时在有界样本中依次检查 UTF-16 特征和 UTF-8 合法性，
 * 不满足 UTF-8 的样本回退为 GBK。检测只选择编码，实际分页从 BOM 后重新开始。 */
static file_text_result_t text_detect(void)
{
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t limit;
    uint8_t valid;
    text_cursor = 0;
    a = text_byte();
    b = text_byte();
    c = text_byte();
    text_bom_bytes = 0;
    text_encoding = FILE_TEXT_ENCODING_UTF8;
    if (a == 0xefu && b == 0xbbu && c == 0xbfu) {
        text_bom_bytes = 3;
        return text_status;
    }
    if ((a == 0xffu && b == 0xfeu) || (a == 0xfeu && b == 0xffu)) {
        text_bom_bytes = 2;
        text_encoding = a == 0xffu ? FILE_TEXT_ENCODING_UTF16_LE : FILE_TEXT_ENCODING_UTF16_BE;
        return text_status;
    }
    limit = text_size < TEXT_PROBE_BYTES ? text_size : TEXT_PROBE_BYTES;
    if (text_detect_utf16(limit))
        return text_status;
    text_cursor = 0;
    /* 从文件起点校验完整码点；跨过采样末尾的字符仍读完，避免边界误判。 */
    while (text_cursor < limit && text_status == FILE_TEXT_OK) {
        (void)text_utf8(&valid);
        if (!valid) {
            text_encoding = FILE_TEXT_ENCODING_GBK;
            break;
        }
        if ((text_cursor & 127u) == 0u && !text_poll())
            break;
    }
    return text_status;
}

/* 按显示列数分页：中文占两列、制表符补齐到四列边界；
 * 行尾放不下的完整字符回退到原字节位置，下一行重新解码。 */
static file_text_result_t text_render(text_position_t start, uint32_t page_number,
                                     text_position_t *end)
{
    uint32_t value;
    uint32_t before;
    uint16_t code;
    uint8_t row;
    uint8_t columns;
    uint8_t width;
    uint8_t spaces = start.spaces;
    text_cursor = start.offset;
    memset(&text_pending, 0, sizeof(text_pending));
    text_pending.page_number = page_number;
    text_pending.file_size = text_size;
    text_pending.encoding = text_encoding;
    text_pending.start_offset = start.offset;
    text_pending.has_previous = page_number > 1u;
    for (row = 0; row < FILE_TEXT_ROWS; ++row) {
        columns = 0;
        for (;;) {
            before = text_cursor;
            if (spaces != 0u) {
                if (columns == FILE_TEXT_COLUMNS)
                    break;
                --spaces;
                value = ' ';
            } else {
                value = text_character();
                if (text_status != FILE_TEXT_OK)
                    return text_status;
                if (value == TEXT_EOF) {
                    if (columns != 0u)
                        text_pending.line_count = row + 1u;
                    goto finished;
                }
                if (value == '\n')
                    break;
                if (columns == FILE_TEXT_COLUMNS) {
                    text_cursor = before;
                    break;
                }
                if (value == '\t') {
                    spaces = (uint8_t)(4u - (columns & 3u));
                    continue;
                }
            }
            code = text_display_code(value);
            width = code > 0xffu ? 2u : 1u;
            if (columns + width > FILE_TEXT_COLUMNS) {
                text_cursor = before;
                break;
            }
            if (width == 2u)
                text_pending.lines[row][columns++] = (char)(code >> 8);
            text_pending.lines[row][columns++] = (char)code;
        }
        text_pending.line_count = row + 1u;
    }
finished:
    /* 页面刚好填满时也检查零填充尾部，避免再生成一页空白。
     * 遇到正文便恢复位置；扫描仍经过后台回调，可随时取消。 */
    if (spaces == 0u && text_cursor < text_size) {
        before = text_cursor;
        value = text_nonzero_codepoint();
        if (text_status != FILE_TEXT_OK)
            return text_status;
        if (value != TEXT_EOF)
            text_cursor = before;
    }
    end->offset = text_cursor;
    end->spaces = spaces;
    text_pending.end_offset = text_cursor;
    text_pending.has_next = text_cursor < text_size || spaces != 0u;
    return text_status;
}

/* 页号取模覆盖旧书签，保持定位内存固定；缓存外的旧页由向前重扫恢复。 */
static void text_remember(uint32_t number, text_position_t position)
{
    text_history[number % TEXT_HISTORY].number = number;
    text_history[number % TEXT_HISTORY].position = position;
}

/* 仅在整页渲染成功后替换当前页及定位信息，取消或读盘失败不会提交残页。 */
static void text_commit(text_position_t start, text_position_t end)
{
    text_page = text_pending;
    text_next_position = end;
    text_remember(text_page.page_number, start);
    if (text_page.has_next)
        text_remember(text_page.page_number + 1u, end);
}

void file_text_set_service(file_text_service_t service, void *context)
{
    text_service = service;
    text_service_context = context;
}

/* 释放当前文件并清空页面、缓存和书签；重复关闭也可用于重置阅读会话。 */
void file_text_close(void)
{
    if (text_opened)
        (void)f_close(&text_file);
    text_opened = 0;
    text_buffer_length = 0;
    memset(&text_page, 0, sizeof(text_page));
    memset(text_history, 0, sizeof(text_history));
    text_page.error = FILE_TEXT_NOT_OPEN;
}

/* 先结束旧会话，再打开文件、探测编码并渲染首页。
 * 只有首页完整生成后才建立可翻页状态；任一步失败都会关闭新文件。 */
file_text_result_t file_text_open(const char *path)
{
    FRESULT result;
    text_position_t start;
    text_position_t end;
    file_text_close();
    text_begin();
    if (text_status != FILE_TEXT_OK)
        return text_failure();
    if (path == 0 || path[0] == 0) {
        text_status = FILE_TEXT_INVALID;
        return text_failure();
    }
    result = f_open(&text_file, path, FA_READ);
    if (result != FR_OK) {
        text_status = FILE_TEXT_IO_ERROR;
        text_disk_error = (uint8_t)result;
        return text_failure();
    }
    text_opened = 1;
    text_size = (uint32_t)f_size(&text_file);
    if (text_detect() == FILE_TEXT_OK) {
        start.offset = text_bom_bytes;
        start.spaces = 0;
        if (text_render(start, 1, &end) == FILE_TEXT_OK) {
            text_commit(start, end);
            return FILE_TEXT_OK;
        }
    }
    (void)f_close(&text_file);
    text_opened = 0;
    return text_failure();
}

/* 从上次提交的页尾续读，包括尚未展开完的制表符空格。
 * 无下一页时返回 FILE_TEXT_END；失败时仍可基于原页面再次发起翻页。 */
file_text_result_t file_text_next(void)
{
    text_position_t start = text_next_position;
    text_position_t end;
    if (!text_opened)
        return FILE_TEXT_NOT_OPEN;
    if (!text_page.has_next)
        return FILE_TEXT_END;
    text_begin();
    if (text_status == FILE_TEXT_OK &&
        text_render(start, text_page.page_number + 1u, &end) == FILE_TEXT_OK) {
        text_commit(start, end);
        return FILE_TEXT_OK;
    }
    return text_failure();
}

/* 查找不晚于目标页的最近书签，再逐页推进到上一页。
 * 被环形缓存淘汰的页从可用书签或正文起点重建，因此回翻不依赖整页缓存。 */
file_text_result_t file_text_previous(void)
{
    text_position_t start;
    text_position_t end;
    uint32_t number = 1;
    uint32_t target;
    uint8_t index;
    if (!text_opened)
        return FILE_TEXT_NOT_OPEN;
    if (!text_page.has_previous)
        return FILE_TEXT_END;
    target = text_page.page_number - 1u;
    start.offset = text_bom_bytes;
    start.spaces = 0;
    /* 超过缓存的旧页从最近可用位置重扫；每次读取均可让出后台并取消。 */
    for (index = 0; index < TEXT_HISTORY; ++index) {
        if (text_history[index].number > number && text_history[index].number <= target) {
            number = text_history[index].number;
            start = text_history[index].position;
        }
    }
    text_begin();
    while (text_status == FILE_TEXT_OK) {
        if (text_render(start, number, &end) != FILE_TEXT_OK)
            break;
        if (number == target) {
            text_commit(start, end);
            return FILE_TEXT_OK;
        }
        if (!text_pending.has_next) {
            text_status = FILE_TEXT_IO_ERROR;
            text_disk_error = FR_INT_ERR;
            break;
        }
        ++number;
        start = end;
        text_remember(number, start);
        if (!text_poll())
            break;
    }
    return text_failure();
}

/* 使用指定编码重新试读首页；失败则恢复原编码，成功才替换页面和书签。 */
file_text_result_t file_text_set_encoding(file_text_encoding_t encoding)
{
    file_text_encoding_t previous = text_encoding;
    text_position_t start;
    text_position_t end;
    if (!text_opened)
        return FILE_TEXT_NOT_OPEN;
    if ((unsigned int)encoding > FILE_TEXT_ENCODING_UTF16_BE)
        return FILE_TEXT_INVALID;
    start.offset = text_bom_bytes;
    start.spaces = 0;
    /* 编码会改变字符边界和分页位置，先试读首页，成功后再清除旧编码的书签。 */
    text_encoding = encoding;
    text_begin();
    if (text_status == FILE_TEXT_OK && text_render(start, 1, &end) == FILE_TEXT_OK) {
        memset(text_history, 0, sizeof(text_history));
        text_commit(start, end);
        return FILE_TEXT_OK;
    }
    text_encoding = previous;
    return text_failure();
}

const file_text_page_t *file_text_get_page(void)
{
    return &text_page;
}
