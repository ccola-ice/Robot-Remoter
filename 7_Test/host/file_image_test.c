#include "file_image.h"
#include "ff.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint16_t LCD_X_LENGTH = 800U, LCD_Y_LENGTH = 480U;
static uint16_t frame[800U * 480U];
static uint8_t coverage[800U * 480U];
static unsigned calls, cancel_after, reads, fail_read, fail_seek, closes, fail_close, opens;
static uint16_t vx, vy, vw, vh;
static uint64_t workspace[280576U / 8U + 2U];

FRESULT f_open(FIL *file, const char *path, unsigned mode)
{
    long size;
    (void)mode;
    file->stream = fopen(path, "rb");
    if(!file->stream) return FR_DISK_ERR;
    fseek(file->stream, 0, SEEK_END);
    size = ftell(file->stream);
    assert(size >= 0);
    file->size = (DWORD)size;
    file->offset = 0U;
    rewind(file->stream);
    opens++;
    return FR_OK;
}

FRESULT f_read(FIL *file, void *buffer, UINT bytes, UINT *count)
{
    reads++;
    if(fail_read && reads == fail_read) return FR_DISK_ERR;
    *count = (UINT)fread(buffer, 1, bytes, file->stream);
    file->offset += *count;
    return ferror(file->stream) ? FR_DISK_ERR : FR_OK;
}

FRESULT f_lseek(FIL *file, DWORD offset)
{
    if(fail_seek) return FR_DISK_ERR;
    if(fseek(file->stream, (long)offset, SEEK_SET)) return FR_DISK_ERR;
    file->offset = offset;
    return FR_OK;
}

FRESULT f_close(FIL *file)
{
    assert(closes < opens);
    closes++;
    assert(fclose(file->stream) == 0);
    return fail_close ? FR_DISK_ERR : FR_OK;
}

void LCD_BlitRGB565(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                    const uint16_t *pixels)
{
    uint16_t row, column;
    assert(x >= vx && y >= vy && width && height);
    assert((uint32_t)x + width <= (uint32_t)vx + vw);
    assert((uint32_t)y + height <= (uint32_t)vy + vh);
    for(row = 0U; row < height; row++) {
        for(column = 0U; column < width; column++) {
            unsigned index = (y + row) * 800U + x + column;
            assert(coverage[index] == 0U);
            coverage[index]++;
            frame[index] = pixels[row * width + column];
        }
    }
}

static uint8_t service(void *context)
{
    assert(context == &calls);
    calls++;
    return !cancel_after || calls < cancel_after;
}

static void verify_coverage(const FileImageInfo *info)
{
    unsigned x, y, index;
    unsigned dx = vx + (vw - info->drawn_width) / 2U;
    unsigned dy = vy + (vh - info->drawn_height) / 2U;
    for(y = 0U; y < 480U; y++) for(x = 0U; x < 800U; x++) {
        index = y * 800U + x;
        assert(coverage[index] == (x >= dx && x < dx + info->drawn_width &&
                                  y >= dy && y < dy + info->drawn_height));
    }
}

/* 在同一进程内切换 PNG → BMP → JPEG，验证解码锁和文件资源已经释放。 */
static void verify_following_images(const char *bmp, const char *jpeg)
{
    FileImageInfo info;
    const char *paths[2];
    unsigned index, before;
    paths[0] = bmp; paths[1] = jpeg;
    cancel_after = fail_read = fail_seek = fail_close = 0U;
    for(index = 0U; index < 2U; index++) {
        memset(coverage, 0, sizeof(coverage));
        memset(frame, 0x5A, sizeof(frame));
        before = closes;
        assert(file_image_draw(paths[index], vx, vy, vw, vh, &info, service, &calls) == FILE_IMAGE_OK);
        assert(closes == before + 1U && opens == closes);
        assert(info.format == (index ? FILE_IMAGE_JPEG : FILE_IMAGE_BMP));
        verify_coverage(&info);
    }
}

int main(int argc, char **argv)
{
    FileImageInfo info;
    FileImageResult result;
    unsigned expected, index, capacity = 0U;
    FILE *output;
    assert(argc == 12 || argc == 13 || argc == 15);
    expected = (unsigned)atoi(argv[2]);
    vx = (uint16_t)atoi(argv[3]);
    vy = (uint16_t)atoi(argv[4]);
    vw = (uint16_t)atoi(argv[5]);
    vh = (uint16_t)atoi(argv[6]);
    cancel_after = (unsigned)atoi(argv[7]);
    fail_read = (unsigned)atoi(argv[8]);
    fail_seek = (unsigned)atoi(argv[9]);
    fail_close = (unsigned)atoi(argv[10]);
    memset(workspace, 0xCC, sizeof(workspace));
    if(argc >= 13) {
        capacity = (unsigned)atoi(argv[12]);
        assert(capacity <= 280576U);
        file_image_set_workspace(capacity ? workspace + 1U : NULL, capacity);
        assert(file_image_get_workspace(NULL) == (capacity ? workspace + 1U : NULL));
        {
            uint32_t registered;
            (void)file_image_get_workspace(&registered);
            assert(registered == capacity);
        }
    }
    for(index = 0U; index < 800U * 480U; index++) frame[index] = 0x5A5AU;
    memset(&info, 0, sizeof(info));
    result = file_image_draw(argv[1], vx, vy, vw, vh, &info, service, &calls);
    assert(result <= FILE_IMAGE_NO_MEMORY && closes == opens);
    assert(workspace[0] == UINT64_C(0xCCCCCCCCCCCCCCCC) &&
           workspace[sizeof(workspace) / sizeof(workspace[0]) - 1U] == UINT64_C(0xCCCCCCCCCCCCCCCC));
    if(expected != 99U && result != expected) {
        fprintf(stderr, "result %u expected %u: %s\n", result, expected, argv[1]);
        return 1;
    }
    if(result == FILE_IMAGE_OK) {
        assert(closes == (info.format == FILE_IMAGE_PNG ? 2U : 1U) && calls > 0U);
        verify_coverage(&info);
    }
    output = fopen(argv[11], "wb");
    assert(output);
    assert(fwrite(frame, sizeof(frame), 1, output) == 1U);
    fclose(output);
    printf("%lu %lu %u %u %u %u %u %u\n", (unsigned long)info.width, (unsigned long)info.height,
           info.drawn_width, info.drawn_height, info.format, calls, closes, reads);
    if(argc == 15) verify_following_images(argv[13], argv[14]);
    file_image_set_workspace(NULL, 280576U);
    {
        uint32_t registered = 1U;
        assert(file_image_get_workspace(&registered) == NULL && registered == 0U);
    }
    return 0;
}
