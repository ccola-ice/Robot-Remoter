#include "image_gif.h"
#include "ff.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint16_t LCD_X_LENGTH = 800U, LCD_Y_LENGTH = 480U;
static uint16_t pixels[320U * 200U];
static union { void *align; unsigned char bytes[280576U + 32U]; } memory;
static unsigned calls, cancel_after, reads, fail_read, seeks, fail_seek;
static unsigned opened, closes, fail_close, blits;
static uint16_t drawn_w, drawn_h, view_w, view_h;

FRESULT f_open(FIL *file, const char *path, unsigned mode)
{
    long size;
    (void)mode;
    file->stream = fopen(path, "rb");
    if(!file->stream) return FR_DISK_ERR;
    ++opened;
    assert(fseek(file->stream, 0, SEEK_END) == 0);
    size = ftell(file->stream);
    assert(size >= 0);
    file->size = (DWORD)size;
    file->offset = 0U;
    rewind(file->stream);
    return FR_OK;
}

FRESULT f_read(FIL *file, void *buffer, UINT bytes, UINT *count)
{
    if(++reads == fail_read) return FR_DISK_ERR;
    *count = (UINT)fread(buffer, 1, bytes, file->stream);
    file->offset += *count;
    return ferror(file->stream) ? FR_DISK_ERR : FR_OK;
}

FRESULT f_lseek(FIL *file, DWORD offset)
{
    if(++seeks == fail_seek) return FR_DISK_ERR;
    if(fseek(file->stream, (long)offset, SEEK_SET)) return FR_DISK_ERR;
    file->offset = offset;
    return FR_OK;
}

FRESULT f_close(FIL *file)
{
    ++closes;
    assert(fclose(file->stream) == 0);
    return fail_close ? FR_DISK_ERR : FR_OK;
}

void LCD_BlitRGB565(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                    const uint16_t *source)
{
    assert(width <= 320U && height <= 200U && width && height);
    assert(x == 24U + (view_w - width) / 2U);
    assert(y == 136U + (view_h - height) / 2U);
    drawn_w = width;
    drawn_h = height;
    memcpy(pixels, source, width * height * sizeof(uint16_t));
    ++blits;
}

static uint8_t service(void *context)
{
    assert(context == &calls);
    return ++calls != cancel_after;
}

int main(int argc, char **argv)
{
    FileImageInfo info;
    FileImageResult result;
    unsigned expected, capacity, max_frames, i, previous_blits = 0U;
    FILE *output;
    assert(argc == 13);
    expected = (unsigned)atoi(argv[2]);
    capacity = (unsigned)atoi(argv[3]);
    cancel_after = (unsigned)atoi(argv[4]);
    fail_read = (unsigned)atoi(argv[5]);
    fail_seek = (unsigned)atoi(argv[6]);
    fail_close = (unsigned)atoi(argv[7]);
    max_frames = (unsigned)atoi(argv[8]);
    view_w = (uint16_t)atoi(argv[9]);
    view_h = (uint16_t)atoi(argv[10]);
    output = fopen(argv[11], "wb");
    assert(output && capacity <= 280576U);
    memset(&memory, 0xA5, sizeof(memory));
    memset(&info, 0, sizeof(info));
    result = file_gif_open(argv[1], 24U, 136U, view_w, view_h, 0xFFFFU, &info,
                           atoi(argv[12]) ? 0 : memory.bytes + 16U,
                           capacity, service, &calls);
    for(i = 0U; i < max_frames; ++i) {
        if(blits != previous_blits) {
            assert(blits == previous_blits + 1U && result == FILE_IMAGE_OK);
            assert(drawn_w == info.drawn_width && drawn_h == info.drawn_height);
            assert(fwrite(pixels, drawn_w * drawn_h * 2U, 1U, output) == 1U);
            printf("frame %lu\n", (unsigned long)file_gif_delay_ms());
            previous_blits = blits;
        }
        if(result != FILE_IMAGE_OK || file_gif_finished() || i + 1U == max_frames) break;
        assert(file_gif_is_open());
        result = file_gif_next();
    }
    assert(fclose(output) == 0);
    if(result != FILE_IMAGE_OK) assert(!file_gif_is_open() && file_gif_finished());
    if(expected != 99U && result != expected) {
        fprintf(stderr, "GIF result %u expected %u: %s\n", result, expected, argv[1]);
        return 1;
    }
    printf("result %u %lu %lu %u %u %u %u %u %u %u %u\n", result,
           (unsigned long)info.width, (unsigned long)info.height,
           info.drawn_width, info.drawn_height, info.format,
           blits, file_gif_finished(), calls, reads, seeks);
    file_gif_close();
    file_gif_close();
    assert(opened == closes);
    assert(!file_gif_is_open() && file_gif_finished());
    for(i = 0U; i < 16U; ++i) assert(memory.bytes[i] == 0xA5U);
    for(i = capacity + 16U; i < sizeof(memory.bytes); ++i) assert(memory.bytes[i] == 0xA5U);
    return 0;
}
