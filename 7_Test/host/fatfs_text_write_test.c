/* Exercise production SD/Flash tests against memory-only FatFs operations. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef unsigned UINT;
typedef unsigned char BYTE;
typedef struct { unsigned dummy; } FATFS;
typedef struct { unsigned slot, offset; } FIL;
typedef enum { FR_OK, FR_DISK_ERR, FR_NO_FILESYSTEM } FRESULT;
#define FA_READ 1U
#define FA_WRITE 2U
#define FA_CREATE_ALWAYS 4U
#define FA_OPEN_EXISTING 0U

static struct {
    char name[128];
    unsigned char data[512];
    UINT size;
} files[4];
static unsigned file_count, write_count, mount_count, close_count, fault;
static char output[16384];
static size_t output_length;
FATFS fs_flash, fs_sdcard;

static int test_printf(const char *format, ...)
{
    int count;
    va_list args;
    va_start(args, format);
    count = vsnprintf(output + output_length, sizeof(output) - output_length, format, args);
    va_end(args);
    assert(count >= 0 && (size_t)count < sizeof(output) - output_length);
    output_length += (size_t)count;
    return count;
}

static FRESULT f_mount(FATFS *fs, const char *path, BYTE option)
{
    assert(fs != NULL && option == 1U);
    assert(strcmp(path, "0:") == 0 || strcmp(path, "1:") == 0);
    ++mount_count;
    return FR_OK;
}

static FRESULT f_mkfs(const char *path, BYTE option, UINT unit)
{
    (void)path; (void)option; (void)unit;
    assert(!"the normal write tests must never format a volume");
    return FR_DISK_ERR;
}

static FRESULT f_open(FIL *file, const char *path, BYTE flags)
{
    unsigned slot;
    for(slot = 0; slot < file_count; ++slot)
        if(strcmp(files[slot].name, path) == 0) break;
    if(slot == file_count) {
        assert(flags & FA_CREATE_ALWAYS);
        assert(file_count < 4U && strlen(path) < sizeof(files[slot].name));
        strcpy(files[slot].name, path);
        ++file_count;
    }
    if(flags & FA_CREATE_ALWAYS) files[slot].size = 0;
    file->slot = slot;
    file->offset = 0;
    return FR_OK;
}

static FRESULT f_write(FIL *file, const void *buffer, UINT size, UINT *written)
{
    UINT accepted = size;
    int target = size >= 9U && memcmp(buffer, "STM32F407 ", 9U) == 0;
    assert(file->slot < file_count);
    assert(size <= sizeof(files[0].data) - file->offset);
    ++write_count;
    if(target && fault == 1U) accepted = size - 1U;
    if(target && fault == 2U) accepted = 0U;
    memcpy(files[file->slot].data + file->offset, buffer, accepted);
    file->offset += accepted;
    files[file->slot].size = file->offset;
    *written = accepted;
    return target && fault == 2U ? FR_DISK_ERR : FR_OK;
}

static FRESULT f_read(FIL *file, void *buffer, UINT size, UINT *read)
{
    UINT available = files[file->slot].size - file->offset;
    if(size > available) size = available;
    memcpy(buffer, files[file->slot].data + file->offset, size);
    file->offset += size;
    *read = size;
    return FR_OK;
}

static FRESULT f_close(FIL *file)
{
    (void)file;
    ++close_count;
    return FR_OK;
}

static void Delay_us(unsigned delay) { (void)delay; }
#define printf test_printf
#include "production_fatfs_text.inc"
#undef printf

static const unsigned char expected[] = {
    0x53,0x54,0x4d,0x33,0x32,0x46,0x34,0x30,0x37,0x20,
    0xbd,0xf1,0xcc,0xec,0xca,0xc7,0xb8,0xf6,0xba,0xc3,0xc8,0xd5,
    0xd7,0xd3,0xa3,0xac,0x53,0x44,0xbf,0xa8,0xd0,0xc2,0xbd,0xa8,
    0xce,0xc4,0xbc,0xfe,0xcf,0xb5,0xcd,0xb3,0xb2,0xe2,0xca,0xd4,
    0xce,0xc4,0xbc,0xfe,0x0d,0x0a
};
/* GBK bytes for the short-write diagnostic prefix. */
static const char short_message[] = "\xce\xc4\xbc\xfe\xd0\xb4\xc8\xeb\xb2\xbb\xcd\xea\xd5\xfb";

static void run_case(unsigned flash, unsigned failure)
{
    unsigned target = flash ? 2U : 0U;
    UINT length = failure == 2U ? 0U : (UINT)sizeof(expected) - (failure == 1U);
    memset(files, 0, sizeof(files));
    memset(output, 0, sizeof(output));
    memset(fatfs_read_buff, 0, sizeof(fatfs_read_buff));
    file_count = write_count = mount_count = close_count = 0;
    output_length = 0;
    fault = failure;
    if(flash) fatfs_flash_test();
    else fatfs_sdcard_test();
    assert(mount_count == 1U);
    assert(write_count == (flash ? 3U : 1U));
    assert(close_count == (flash ? 5U : 2U));
    assert(file_count == (flash ? 3U : 1U));
    assert(files[target].size == length);
    assert(memcmp(files[target].data, expected, length) == 0);
    assert(memchr(files[target].data, '\0', length) == NULL);
    assert((strstr(output, short_message) != NULL) == (failure == 1U));
}

int main(void)
{
    unsigned flash, failure;
    assert(sizeof(expected) == 52U);
    for(flash = 0; flash < 2U; ++flash)
        for(failure = 0; failure < 3U; ++failure)
            run_case(flash, failure);
    puts("FatFs text writes passed: SD/Flash, exact 52-byte GBK payload, short writes and I/O errors.");
    return 0;
}
