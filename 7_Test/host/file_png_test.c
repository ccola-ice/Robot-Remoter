#include "file_png.h"
#include "ff.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint16_t LCD_X_LENGTH = 800U, LCD_Y_LENGTH = 480U;
static uint16_t frame[800U * 480U];
static uint8_t coverage[800U * 480U];
static uint32_t workspace[280576U / 4U + 4U];
static unsigned calls, cancel_after, reads, fail_read, closes, fail_close, opened;
static uint16_t vx, vy, vw, vh;

FRESULT f_open(FIL *file, const char *path, unsigned mode)
{
    long size;
    (void)mode;
    file->stream = fopen(path, "rb");
    if(!file->stream) return FR_DISK_ERR;
    assert(fseek(file->stream, 0, SEEK_END) == 0);
    size = ftell(file->stream);
    assert(size >= 0);
    file->size = (DWORD)size;
    file->offset = 0U;
    rewind(file->stream);
    opened++;
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

FRESULT f_close(FIL *file)
{
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
    for(row = 0U; row < height; row++) for(column = 0U; column < width; column++) {
        unsigned index = (y + row) * 800U + x + column;
        assert(coverage[index] == 0U);
        coverage[index]++;
        frame[index] = pixels[row * width + column];
    }
}

static uint8_t service(void *context)
{
    assert(context == &calls);
    calls++;
    return !cancel_after || calls < cancel_after;
}

int main(int argc, char **argv)
{
    FileImageInfo info;
    FileImageResult result;
    unsigned expected, index, x, y, dx, dy, capacity;
    FILE *output;
    assert(argc == 12);
    expected = (unsigned)atoi(argv[2]);
    vx = (uint16_t)atoi(argv[3]); vy = (uint16_t)atoi(argv[4]);
    vw = (uint16_t)atoi(argv[5]); vh = (uint16_t)atoi(argv[6]);
    cancel_after = (unsigned)atoi(argv[7]); fail_read = (unsigned)atoi(argv[8]);
    fail_close = (unsigned)atoi(argv[9]); capacity = (unsigned)atoi(argv[10]);
    assert(capacity <= 280576U && capacity % 4U == 0U);
    for(index = 0U; index < 800U * 480U; index++) frame[index] = 0x5A5AU;
    memset(workspace, 0xCC, sizeof(workspace));
    memset(&info, 0, sizeof(info));
    result = file_png_draw(argv[1], vx, vy, vw, vh, &info, service, &calls, workspace + 2U, capacity);
    assert(result <= FILE_IMAGE_NO_MEMORY);
    assert(closes == opened);
    assert(workspace[0] == 0xCCCCCCCCU && workspace[1] == 0xCCCCCCCCU &&
           workspace[capacity / 4U + 2U] == 0xCCCCCCCCU);
    if(expected != 99U && result != expected) {
        fprintf(stderr, "result %u expected %u: %s\n", result, expected, argv[1]);
        return 1;
    }
    if(result == FILE_IMAGE_OK) {
        assert(closes == 1U && calls > 0U && info.format == FILE_IMAGE_PNG);
        dx = vx + (vw - info.drawn_width) / 2U; dy = vy + (vh - info.drawn_height) / 2U;
        for(y = 0U; y < 480U; y++) for(x = 0U; x < 800U; x++) {
            index = y * 800U + x;
            assert(coverage[index] == (x >= dx && x < dx + info.drawn_width &&
                                      y >= dy && y < dy + info.drawn_height));
        }
    }
    output = fopen(argv[11], "wb");
    assert(output && fwrite(frame, sizeof(frame), 1, output) == 1U);
    fclose(output);
    printf("%lu %lu %u %u %u %u %u %u\n", (unsigned long)info.width, (unsigned long)info.height,
           info.drawn_width, info.drawn_height, info.format, calls, closes, reads);
    return 0;
}
