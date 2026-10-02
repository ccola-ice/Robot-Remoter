#include "file_text.h"
#include "ff.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned char data[150000];
static unsigned int length;
static unsigned int available;
static unsigned int read_calls;
static unsigned int seek_calls;
static unsigned int close_calls;
static unsigned int service_calls;
static unsigned int cancel_at;
static int fail_read;
static int fail_seek;
static int fail_open;

FRESULT f_open(FIL *file, const TCHAR *path, BYTE mode)
{
    assert(path != NULL && mode == FA_READ);
    if (fail_open)
        return FR_NO_FILE;
    memset(file, 0, sizeof(*file));
    file->fsize = length;
    return FR_OK;
}

FRESULT f_close(FIL *file)
{
    (void)file;
    ++close_calls;
    return FR_OK;
}

FRESULT f_lseek(FIL *file, DWORD offset)
{
    ++seek_calls;
    if (fail_seek)
        return FR_DISK_ERR;
    assert(offset <= length);
    file->fptr = offset;
    return FR_OK;
}

FRESULT f_read(FIL *file, void *buffer, UINT requested, UINT *count)
{
    unsigned int remaining = file->fptr < available ? available - file->fptr : 0;
    ++read_calls;
    assert(requested <= 512u);
    *count = 0;
    if (fail_read)
        return FR_DISK_ERR;
    *count = requested < remaining ? requested : remaining;
    memcpy(buffer, data + file->fptr, *count);
    file->fptr += *count;
    return FR_OK;
}

static uint8_t service(void *context)
{
    assert(context == &service_calls);
    ++service_calls;
    return cancel_at == 0u || service_calls < cancel_at;
}

static void reset_data(const void *bytes, unsigned int size)
{
    file_text_close();
    if (size != 0u)
        memmove(data, bytes, size);
    length = size;
    available = size;
    read_calls = seek_calls = close_calls = service_calls = cancel_at = 0;
    fail_read = fail_seek = fail_open = 0;
    file_text_set_service(service, &service_calls);
}

static void assert_line(unsigned int row, const char *expected)
{
    const file_text_page_t *page = file_text_get_page();
    if (strcmp(page->lines[row], expected) != 0) {
        fprintf(stderr, "line %u mismatch: %s / %s\n", row, page->lines[row], expected);
        assert(0);
    }
}

static void assert_unchanged(const file_text_page_t *before)
{
    const file_text_page_t *page = file_text_get_page();
    assert(memcmp(page->lines, before->lines, sizeof(page->lines)) == 0);
    assert(page->page_number == before->page_number);
    assert(page->start_offset == before->start_offset);
    assert(page->end_offset == before->end_offset);
    assert(page->encoding == before->encoding);
    assert(page->has_next == before->has_next);
}

static void test_empty_and_errors(void)
{
    reset_data(NULL, 0);
    assert(file_text_open("empty") == FILE_TEXT_OK);
    assert(file_text_get_page()->line_count == 0);
    assert(file_text_get_page()->page_number == 1);
    assert(file_text_next() == FILE_TEXT_END);
    assert(file_text_previous() == FILE_TEXT_END);
    file_text_close();
    assert(close_calls == 1);
    assert(file_text_next() == FILE_TEXT_NOT_OPEN);
    assert(file_text_open(NULL) == FILE_TEXT_INVALID);
    fail_open = 1;
    assert(file_text_open("missing") == FILE_TEXT_IO_ERROR);
    assert(file_text_get_page()->fatfs_error == FR_NO_FILE);
    assert(close_calls == 1);
    reset_data("hello", 5);
    fail_read = 1;
    assert(file_text_open("io-failure") == FILE_TEXT_IO_ERROR);
    assert(close_calls == 1);
    fail_read = 0;
    cancel_at = service_calls + 1u;
    assert(file_text_open("cancel") == FILE_TEXT_CANCELLED);
}

static void test_encoding(void)
{
    static const unsigned char utf8[] = {
        0xef, 0xbb, 0xbf, 'A', 0xe4, 0xb8, 0xad, 0xe6, 0x96, 0x87, '\n',
        0xf0, 0x9f, 0x98, 0x80, 'B', 0, 'C', '\n'
    };
    static const unsigned char gbk[] = {'A', 0xd6, 0xd0, 0xce, 0xc4, '\n', 0x81, 0x40};
    static const unsigned char le[] = {
        0xff, 0xfe, 'A', 0, 0x2d, 0x4e, 0x87, 0x65, 13, 0, 10, 0,
        0x3d, 0xd8, 0, 0xde, 'B', 0, 0, 0xd8, 'C', 0, 0, 0xdc, 'D'
    };
    static const unsigned char be[] = {0xfe, 0xff, 0, 'A', 0x4e, 0x2d, 0x65, 0x87};
    static const unsigned char truncated_utf8[] = {0xef, 0xbb, 0xbf, 0xe4, 0xb8};
    static const unsigned char invalid_utf8[] = {
        0xef, 0xbb, 0xbf, 0xc0, 0xaf, 0xed, 0xa0, 0x80,
        0xf4, 0x90, 0x80, 0x80, 0xe2, 'A', 0xf5, 0x80
    };
    reset_data(utf8, sizeof(utf8));
    assert(file_text_open("utf8") == FILE_TEXT_OK);
    assert(file_text_get_page()->encoding == FILE_TEXT_ENCODING_UTF8);
    assert_line(0, "A\xd6\xd0\xce\xc4");
    assert_line(1, "?BC");
    reset_data(utf8 + 3, sizeof(utf8) - 3);
    assert(file_text_open("utf8-no-bom") == FILE_TEXT_OK);
    assert(file_text_get_page()->encoding == FILE_TEXT_ENCODING_UTF8);
    assert_line(0, "A\xd6\xd0\xce\xc4");
    reset_data(gbk, sizeof(gbk));
    assert(file_text_open("gbk") == FILE_TEXT_OK);
    assert(file_text_get_page()->encoding == FILE_TEXT_ENCODING_GBK);
    assert_line(0, "A\xd6\xd0\xce\xc4");
    assert_line(1, "?");
    assert(file_text_set_encoding(FILE_TEXT_ENCODING_UTF8) == FILE_TEXT_OK);
    assert_line(0, "A????");
    assert(file_text_set_encoding(FILE_TEXT_ENCODING_GBK) == FILE_TEXT_OK);
    assert_line(0, "A\xd6\xd0\xce\xc4");
    reset_data(le, sizeof(le));
    assert(file_text_open("utf16le") == FILE_TEXT_OK);
    assert(file_text_get_page()->encoding == FILE_TEXT_ENCODING_UTF16_LE);
    assert_line(0, "A\xd6\xd0\xce\xc4");
    assert_line(1, "?B?C??");
    reset_data(be, sizeof(be));
    assert(file_text_open("utf16be") == FILE_TEXT_OK);
    assert(file_text_get_page()->encoding == FILE_TEXT_ENCODING_UTF16_BE);
    assert_line(0, "A\xd6\xd0\xce\xc4");
    reset_data(truncated_utf8, sizeof(truncated_utf8));
    assert(file_text_open("truncated-codepoint") == FILE_TEXT_OK);
    assert_line(0, "??");
    reset_data(invalid_utf8, sizeof(invalid_utf8));
    assert(file_text_open("invalid-codepoints") == FILE_TEXT_OK);
    assert_line(0, "?????A??");
    reset_data(NULL, 0);
    memset(data, 'a', 8191);
    memcpy(data + 8191, utf8 + 4, 3);
    length = available = 8194;
    assert(file_text_open("probe-boundary") == FILE_TEXT_OK);
    assert(file_text_get_page()->encoding == FILE_TEXT_ENCODING_UTF8);
    reset_data(NULL, 0);
    memset(data, 'a', 511);
    memcpy(data + 511, utf8 + 4, 3);
    length = available = 514;
    assert(file_text_open("utf8-read-boundary") == FILE_TEXT_OK);
    assert(file_text_get_page()->encoding == FILE_TEXT_ENCODING_UTF8);
    assert(strlen(file_text_get_page()->lines[6]) == 69);
    assert(memcmp(file_text_get_page()->lines[6] + 67, "\xd6\xd0", 2) == 0);
}

/* 复现用户的 FatFs 测试文件：52 字节 GBK 正文后跟 460 个 NUL。 */
static void test_nul_padding(void)
{
    static const unsigned char sentence[] =
        "STM32F407 \xbd\xf1\xcc\xec\xca\xc7\xb8\xf6\xba\xc3\xc8\xd5\xd7\xd3\xa3\xac"
        "SD\xbf\xa8\xd0\xc2\xbd\xa8\xce\xc4\xbc\xfe\xcf\xb5\xcd\xb3\xb2\xe2\xca\xd4\xce\xc4\xbc\xfe\r\n";
    static const unsigned char embedded[] = {'A', 0, 0, 'B', '\r', 0, '\n', 'C'};
    char expected[sizeof(sentence)];
    unsigned int encoding, index, row;
    WCHAR value;
    file_text_page_t saved;
    assert(sizeof(sentence) - 1u == 52u);
    memcpy(expected, sentence, sizeof(sentence));
    expected[sizeof(sentence) - 3u] = 0;
    for (encoding = FILE_TEXT_ENCODING_UTF8; encoding <= FILE_TEXT_ENCODING_UTF16_BE; ++encoding) {
        reset_data(NULL, 0);
        if (encoding == FILE_TEXT_ENCODING_UTF16_LE) {
            data[length++] = 0xffu; data[length++] = 0xfeu;
        } else if (encoding == FILE_TEXT_ENCODING_UTF16_BE) {
            data[length++] = 0xfeu; data[length++] = 0xffu;
        }
        for (index = 0; index < sizeof(sentence) - 1u; ++index) {
            value = sentence[index];
            if (encoding == FILE_TEXT_ENCODING_GBK) {
                data[length++] = (unsigned char)value;
                continue;
            }
            if (value >= 0x80u) {
                value = ff_convert((WCHAR)((value << 8) | sentence[index + 1u]), 1);
                ++index;
            }
            if (encoding == FILE_TEXT_ENCODING_UTF16_LE) {
                data[length++] = (unsigned char)value;
                data[length++] = (unsigned char)(value >> 8);
            } else if (encoding == FILE_TEXT_ENCODING_UTF16_BE) {
                data[length++] = (unsigned char)(value >> 8);
                data[length++] = (unsigned char)value;
            } else if (value < 0x80u) data[length++] = (unsigned char)value;
            else {
                assert(value >= 0x800u);
                data[length++] = (unsigned char)(0xe0u | (value >> 12));
                data[length++] = (unsigned char)(0x80u | ((value >> 6) & 0x3fu));
                data[length++] = (unsigned char)(0x80u | (value & 0x3fu));
            }
        }
        memset(data + length, 0, 460u);
        length += 460u;
        if (encoding == FILE_TEXT_ENCODING_GBK) assert(length == 512u);
        if (encoding == FILE_TEXT_ENCODING_UTF8) assert(length == 531u);
        available = length;
        assert(file_text_open("nul-padded-fatfs-test") == FILE_TEXT_OK);
        assert(file_text_get_page()->encoding == (file_text_encoding_t)encoding);
        assert_line(0, expected);
        assert(file_text_get_page()->line_count == 1u);
        assert(file_text_get_page()->end_offset == length);
        assert(file_text_get_page()->file_size == length);
        assert(!file_text_get_page()->has_next && file_text_next() == FILE_TEXT_END);
        /* 改用错误编码后仍可切回，旧文件的 NUL 尾部保持可读兼容。 */
        assert(file_text_set_encoding(FILE_TEXT_ENCODING_UTF16_BE) == FILE_TEXT_OK);
        assert(file_text_set_encoding((file_text_encoding_t)encoding) == FILE_TEXT_OK);
        assert_line(0, expected);
        if (encoding >= FILE_TEXT_ENCODING_UTF16_LE) {
            reset_data(data + 2u, length - 2u);
            assert(file_text_open("nul-padded-utf16-no-bom") == FILE_TEXT_OK);
            assert(file_text_get_page()->encoding == (file_text_encoding_t)encoding);
            assert_line(0, expected);
        }
    }
    reset_data(embedded, sizeof(embedded));
    assert(file_text_open("embedded-nul") == FILE_TEXT_OK);
    assert_line(0, "AB");
    assert_line(1, "C");

    reset_data(NULL, 0);
    memset(data, 0, 14000u);
    length = available = 14000u;
    assert(file_text_open("only-nul") == FILE_TEXT_OK);
    assert(file_text_get_page()->line_count == 0u && !file_text_get_page()->has_next);
    assert(file_text_get_page()->end_offset == length);
    for (row = 0; row < FILE_TEXT_ROWS; ++row) {
        data[row * 2u] = 'A'; data[row * 2u + 1u] = '\n';
    }
    assert(file_text_open("full-page-with-nul-tail") == FILE_TEXT_OK);
    assert(file_text_get_page()->line_count == FILE_TEXT_ROWS);
    assert(file_text_get_page()->end_offset == length && !file_text_get_page()->has_next);

    /* 跨缓冲区跳过 NUL 时保留后续正文；取消不能破坏当前已显示页面。 */
    memcpy(data + length - 4u, "TAIL", 4u);
    assert(file_text_open("nul-gap-with-more-text") == FILE_TEXT_OK);
    assert(file_text_get_page()->has_next);
    saved = *file_text_get_page();
    cancel_at = service_calls + 3u;
    assert(file_text_next() == FILE_TEXT_CANCELLED);
    assert_unchanged(&saved);
    cancel_at = 0;
    assert(file_text_next() == FILE_TEXT_OK);
    assert_line(0, "TAIL");
    assert(!file_text_get_page()->has_next);
    assert(file_text_previous() == FILE_TEXT_OK);
    for (row = 0; row < FILE_TEXT_ROWS; ++row) assert_line(row, "A");
    assert(file_text_next() == FILE_TEXT_OK);
    assert_line(0, "TAIL");
}

static void test_wrapping(void)
{
    unsigned int i;
    char expected[FILE_TEXT_COLUMNS + 1];
    reset_data(NULL, 0);
    memset(data, 'a', FILE_TEXT_COLUMNS);
    memcpy(data + FILE_TEXT_COLUMNS, "\r\nnext\rthird\n\tend", 17);
    length = available = FILE_TEXT_COLUMNS + 17;
    assert(file_text_open("wrap-newline") == FILE_TEXT_OK);
    memset(expected, 'a', FILE_TEXT_COLUMNS);
    expected[FILE_TEXT_COLUMNS] = 0;
    assert_line(0, expected);
    assert_line(1, "next");
    assert_line(2, "third");
    assert_line(3, "    end");
    reset_data(NULL, 0);
    memset(data, 'a', FILE_TEXT_COLUMNS - 1);
    memcpy(data + FILE_TEXT_COLUMNS - 1, "\xe4\xb8\xadZ", 4);
    length = available = FILE_TEXT_COLUMNS + 3;
    assert(file_text_open("wrap-chinese") == FILE_TEXT_OK);
    expected[FILE_TEXT_COLUMNS - 1] = 0;
    assert_line(0, expected);
    assert_line(1, "\xd6\xd0Z");
    reset_data(NULL, 0);
    for (i = 0; i < FILE_TEXT_ROWS - 1; ++i)
        data[i] = '\n';
    memset(data + i, 'x', FILE_TEXT_COLUMNS - 1);
    i += FILE_TEXT_COLUMNS - 1;
    memcpy(data + i, "\tY", 2);
    length = available = i + 2;
    assert(file_text_open("tab-page-boundary") == FILE_TEXT_OK);
    assert(file_text_get_page()->has_next);
    assert(file_text_next() == FILE_TEXT_OK);
    assert_line(0, "  Y");
    assert(file_text_previous() == FILE_TEXT_OK);
    assert(file_text_next() == FILE_TEXT_OK);
    assert_line(0, "  Y");
    reset_data(NULL, 0);
    memset(data, 'q', 511);
    memcpy(data + 511, "\r\nTAIL", 6);
    length = available = 517;
    assert(file_text_open("crlf-read-boundary") == FILE_TEXT_OK);
    assert_line(7, "TAIL");
}

static void test_utf16_without_bom_and_manual_encoding(void)
{
    static const unsigned char le[] = {
        'A', 0, ' ', 0, 0x2d, 0x4e, 0x87, 0x65, 13, 0, 10, 0, 'B', 0
    };
    static const unsigned char be[] = {
        0, 'A', 0, ' ', 0x4e, 0x2d, 0x65, 0x87, 0, 13, 0, 10, 0, 'B'
    };
    static const unsigned char le_chinese[] = {0x2d, 0x4e, 0x87, 0x65};
    static const unsigned char be_chinese[] = {0x4e, 0x2d, 0x65, 0x87};
    static const unsigned char utf8_bom[] = {0xef, 0xbb, 0xbf, 0xe4, 0xb8, 0xad};
    static const unsigned char le_bom[] = {0xff, 0xfe, 0x2d, 0x4e, 0x87, 0x65};
    static const unsigned char ascii[] = "ordinary ASCII text\n";
    unsigned int encoding;
    unsigned int index;
    reset_data(le, sizeof(le));
    assert(file_text_open("utf16le-no-bom") == FILE_TEXT_OK);
    assert(file_text_get_page()->encoding == FILE_TEXT_ENCODING_UTF16_LE);
    assert(file_text_get_page()->start_offset == 0);
    assert_line(0, "A \xd6\xd0\xce\xc4");
    assert_line(1, "B");
    reset_data(be, sizeof(be));
    assert(file_text_open("utf16be-no-bom") == FILE_TEXT_OK);
    assert(file_text_get_page()->encoding == FILE_TEXT_ENCODING_UTF16_BE);
    assert_line(0, "A \xd6\xd0\xce\xc4");
    assert_line(1, "B");

    /* 没有 ASCII 高位零时不盲猜；用户仍可手动指定 UTF-16 字节序。 */
    reset_data(le_chinese, sizeof(le_chinese));
    assert(file_text_open("utf16le-chinese-no-bom") == FILE_TEXT_OK);
    assert(file_text_set_encoding(FILE_TEXT_ENCODING_UTF16_LE) == FILE_TEXT_OK);
    assert_line(0, "\xd6\xd0\xce\xc4");
    reset_data(be_chinese, sizeof(be_chinese));
    assert(file_text_open("utf16be-chinese-no-bom") == FILE_TEXT_OK);
    assert(file_text_set_encoding(FILE_TEXT_ENCODING_UTF16_BE) == FILE_TEXT_OK);
    assert_line(0, "\xd6\xd0\xce\xc4");
    reset_data(ascii, sizeof(ascii) - 1u);
    assert(file_text_open("plain-ascii") == FILE_TEXT_OK);
    assert(file_text_get_page()->encoding == FILE_TEXT_ENCODING_UTF8);
    assert_line(0, "ordinary ASCII text");

    /* 来回切换编码不应把文件 BOM 加入正文，也不能吞掉正文前几个字节。 */
    reset_data(utf8_bom, sizeof(utf8_bom));
    assert(file_text_open("utf8-bom-cycle") == FILE_TEXT_OK);
    for (encoding = 0; encoding <= FILE_TEXT_ENCODING_UTF16_BE; ++encoding)
        assert(file_text_set_encoding((file_text_encoding_t)encoding) == FILE_TEXT_OK);
    assert(file_text_set_encoding(FILE_TEXT_ENCODING_UTF8) == FILE_TEXT_OK);
    assert(file_text_get_page()->start_offset == 3);
    assert_line(0, "\xd6\xd0");
    reset_data(le_bom, sizeof(le_bom));
    assert(file_text_open("utf16-bom-cycle") == FILE_TEXT_OK);
    for (encoding = 0; encoding <= FILE_TEXT_ENCODING_UTF16_BE; ++encoding)
        assert(file_text_set_encoding((file_text_encoding_t)encoding) == FILE_TEXT_OK);
    assert(file_text_set_encoding(FILE_TEXT_ENCODING_UTF16_LE) == FILE_TEXT_OK);
    assert(file_text_get_page()->start_offset == 2);
    assert_line(0, "\xd6\xd0\xce\xc4");

    reset_data(NULL, 0);
    for (index = 0; index < 1200; ++index) {
        data[index * 2u] = 'A';
        data[index * 2u + 1u] = 0;
    }
    length = available = 2400;
    assert(file_text_open("utf16-pagination") == FILE_TEXT_OK);
    assert(file_text_get_page()->encoding == FILE_TEXT_ENCODING_UTF16_LE);
    assert(file_text_get_page()->end_offset == FILE_TEXT_COLUMNS * FILE_TEXT_ROWS * 2u);
    assert(file_text_next() == FILE_TEXT_OK);
    assert(file_text_get_page()->page_number == 2);
    assert(file_text_get_page()->start_offset == FILE_TEXT_COLUMNS * FILE_TEXT_ROWS * 2u);
    assert(file_text_previous() == FILE_TEXT_OK);
    assert(file_text_get_page()->start_offset == 0);
    assert(file_text_get_page()->encoding == FILE_TEXT_ENCODING_UTF16_LE);
}

static void assert_long_page(unsigned int page_number)
{
    unsigned int row;
    unsigned int col;
    unsigned int offset = (page_number - 1) * FILE_TEXT_COLUMNS * FILE_TEXT_ROWS;
    const file_text_page_t *page = file_text_get_page();
    assert(page->page_number == page_number);
    assert(page->start_offset == offset);
    for (row = 0; row < page->line_count; ++row) {
        unsigned int expected = length - offset;
        if (expected > FILE_TEXT_COLUMNS)
            expected = FILE_TEXT_COLUMNS;
        assert(strlen(page->lines[row]) == expected);
        for (col = 0; col < expected; ++col)
            assert(page->lines[row][col] == (char)data[offset++]);
    }
    assert(page->end_offset == offset);
}

static void test_large_forward_backward_and_cancel(void)
{
    unsigned int i;
    unsigned int last;
    file_text_page_t saved;
    reset_data(NULL, 0);
    length = available = 100000;
    for (i = 0; i < length; ++i)
        data[i] = (unsigned char)('a' + i % 26);
    assert(file_text_open("large") == FILE_TEXT_OK);
    for (i = 1;; ++i) {
        assert_long_page(i);
        if (!file_text_get_page()->has_next)
            break;
        assert(file_text_next() == FILE_TEXT_OK);
    }
    last = i;
    assert(last > 64);
    for (i = last; i > 1; --i) {
        assert_long_page(i);
        assert(file_text_previous() == FILE_TEXT_OK);
    }
    assert_long_page(1);
    assert(seek_calls > 0);
    assert(service_calls > 0);
    assert(file_text_next() == FILE_TEXT_OK);
    saved = *file_text_get_page();
    cancel_at = service_calls + 3;
    assert(file_text_next() == FILE_TEXT_CANCELLED);
    assert_unchanged(&saved);
    cancel_at = 0;
    assert(file_text_next() == FILE_TEXT_OK);
    assert_long_page(3);
    saved = *file_text_get_page();
    cancel_at = service_calls + 2;
    assert(file_text_set_encoding(FILE_TEXT_ENCODING_GBK) == FILE_TEXT_CANCELLED);
    assert_unchanged(&saved);
    cancel_at = 0;
    assert(file_text_previous() == FILE_TEXT_OK);
    assert_long_page(2);
    assert(file_text_open("large-reopen") == FILE_TEXT_OK);
    while (file_text_get_page()->page_number < 80)
        assert(file_text_next() == FILE_TEXT_OK);
    while (file_text_get_page()->page_number > 18)
        assert(file_text_previous() == FILE_TEXT_OK);
    saved = *file_text_get_page();
    cancel_at = service_calls + 20;
    assert(file_text_previous() == FILE_TEXT_CANCELLED);
    assert_unchanged(&saved);
    cancel_at = 0;
    assert(file_text_previous() == FILE_TEXT_OK);
    assert_long_page(17);
}

static void test_fault_retry(void)
{
    file_text_page_t saved;
    reset_data(NULL, 0);
    memset(data, 'x', 4000);
    length = available = 4000;
    assert(file_text_open("faults") == FILE_TEXT_OK);
    saved = *file_text_get_page();
    fail_read = 1;
    assert(file_text_next() == FILE_TEXT_IO_ERROR);
    assert(file_text_get_page()->fatfs_error == FR_DISK_ERR);
    assert_unchanged(&saved);
    fail_read = 0;
    assert(file_text_next() == FILE_TEXT_OK);
    assert(file_text_get_page()->page_number == 2);
    saved = *file_text_get_page();
    fail_seek = 1;
    assert(file_text_previous() == FILE_TEXT_IO_ERROR);
    assert_unchanged(&saved);
    fail_seek = 0;
    assert(file_text_previous() == FILE_TEXT_OK);
    available = 1600;
    assert(file_text_next() == FILE_TEXT_IO_ERROR);
    assert(file_text_get_page()->fatfs_error == FR_INT_ERR);
    available = length;
    assert(file_text_next() == FILE_TEXT_OK);
    assert(file_text_get_page()->page_number == 2);
}

/* 逐字穿过真实解码和分页入口，覆盖 GB2312 区域内全部有效 FatFs 映射。 */
static void test_gb2312_full_repertoire(void)
{
    static unsigned char expected[(0xf7u - 0xa1u + 1u) * 94u * 2u];
    unsigned int high;
    unsigned int low;
    unsigned int encoding;
    unsigned int count = 0;
    unsigned int chinese = 0;
    unsigned int output;
    unsigned int row;
    unsigned int row_length;
    unsigned int page_count;
    WCHAR code;
    WCHAR unicode;
    const file_text_page_t *page;
    for (high = 0xa1u; high <= 0xf7u; ++high) {
        for (low = 0xa1u; low <= 0xfeu; ++low) {
            code = (WCHAR)((high << 8) | low);
            unicode = ff_convert(code, 1);
            if (unicode == 0u)
                continue;
            assert(ff_convert(unicode, 0) == code);
            expected[count * 2u] = (unsigned char)high;
            expected[count * 2u + 1u] = (unsigned char)low;
            ++count;
            if (high >= 0xb0u)
                ++chinese;
        }
    }
    assert(count >= 7445u);
    assert(chinese == 6763u);
    for (encoding = FILE_TEXT_ENCODING_UTF8; encoding <= FILE_TEXT_ENCODING_GBK; ++encoding) {
        reset_data(NULL, 0);
        for (output = 0; output < count * 2u; output += 2u) {
            code = (WCHAR)((expected[output] << 8) | expected[output + 1u]);
            if (encoding == FILE_TEXT_ENCODING_GBK) {
                data[length++] = expected[output];
                data[length++] = expected[output + 1u];
            } else {
                unicode = ff_convert(code, 1);
                if (unicode < 0x800u) {
                    data[length++] = (unsigned char)(0xc0u | (unicode >> 6));
                    data[length++] = (unsigned char)(0x80u | (unicode & 0x3fu));
                } else {
                    data[length++] = (unsigned char)(0xe0u | (unicode >> 12));
                    data[length++] = (unsigned char)(0x80u | ((unicode >> 6) & 0x3fu));
                    data[length++] = (unsigned char)(0x80u | (unicode & 0x3fu));
                }
            }
        }
        available = length;
        assert(file_text_open("gb2312-full-repertoire") == FILE_TEXT_OK);
        assert(file_text_get_page()->encoding == (file_text_encoding_t)encoding);
        output = 0;
        page_count = 0;
        for (;;) {
            page = file_text_get_page();
            ++page_count;
            assert(page->page_number == page_count);
            for (row = 0; row < page->line_count; ++row) {
                row_length = (unsigned int)strlen(page->lines[row]);
                assert(row_length != 0u && (row_length & 1u) == 0u);
                assert(output + row_length <= count * 2u);
                assert(memcmp(page->lines[row], expected + output, row_length) == 0);
                assert(strchr(page->lines[row], '?') == NULL);
                output += row_length;
            }
            if (!page->has_next)
                break;
            assert(file_text_next() == FILE_TEXT_OK);
        }
        assert(output == count * 2u);
        assert(page_count > 1u);
    }
    printf("file_text: all %u GB2312-region mappings (%u Chinese, %u symbols) "
           "match in UTF-8 and GBK pagination\n", count, chinese, count - chinese);
}

int main(void)
{
    test_empty_and_errors();
    test_encoding();
    test_nul_padding();
    test_utf16_without_bom_and_manual_encoding();
    test_wrapping();
    test_large_forward_backward_and_cancel();
    test_fault_retry();
    test_gb2312_full_repertoire();
    file_text_close();
    puts("file_text: encoding, wrapping, large pagination, cancellation and I/O tests passed");
    return 0;
}
