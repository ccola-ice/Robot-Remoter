#include "file_image.h"
#include "file_png.h"
#include "ff.h"
#include "tjpgd.h"
#include "bsp_fsmc_lcd.h"
#include <string.h>

#define IMAGE_MAX_SIDE 8192UL
#define IMAGE_MAX_PIXELS (16UL * 1024UL * 1024UL)
#define IMAGE_MAX_BYTES (32UL * 1024UL * 1024UL)
#define IMAGE_LINE_PIXELS 800U
#define IMAGE_IO_BYTES 512U
#define IMAGE_JPEG_POOL 7168U

static void *image_workspace;
static uint32_t image_workspace_capacity;

void file_image_set_workspace(void *buffer, uint32_t capacity)
{
    image_workspace = buffer;
    image_workspace_capacity = buffer ? capacity : 0U;
}

void *file_image_get_workspace(uint32_t *capacity)
{
    if(capacity) *capacity = image_workspace_capacity;
    return image_workspace;
}

/* 文件缓存和解码工作区放在静态区，避免挤占主循环栈。 */
static struct {
    FIL file;
    JDEC jpeg;
    uint32_t pool[IMAGE_JPEG_POOL / 4U];
    uint8_t bytes[IMAGE_IO_BYTES];
    uint16_t pixels[IMAGE_LINE_PIXELS];
    uint8_t (*service)(void *);
    void *context;
    FileImageResult status;
    uint32_t size;
    uint32_t cache_start;
    UINT cache_count;
    uint16_t x, y, width, height;
    uint16_t jpeg_width, jpeg_height;
    uint8_t busy;
} image;

static uint16_t image_le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t image_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t image_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint8_t image_service(void)
{
    if(image.status != FILE_IMAGE_OK) return 0U;
    if(image.service != 0 && image.service(image.context) == 0U) {
        image.status = FILE_IMAGE_CANCELLED;
        return 0U;
    }
    return 1U;
}

static uint8_t image_seek(uint32_t offset)
{
    if(offset > image.size) {
        image.status = FILE_IMAGE_CORRUPT;
        return 0U;
    }
    if(f_lseek(&image.file, (DWORD)offset) != FR_OK || f_tell(&image.file) != offset) {
        image.status = FILE_IMAGE_IO;
        return 0U;
    }
    return 1U;
}

static uint8_t image_read(void *buffer, UINT size)
{
    UINT count = 0U;
    if(!image_service()) return 0U;
    if(f_read(&image.file, buffer, size, &count) != FR_OK) image.status = FILE_IMAGE_IO;
    else if(count != size) image.status = FILE_IMAGE_CORRUPT;
    return image.status == FILE_IMAGE_OK;
}

static uint8_t image_dimensions(uint32_t width, uint32_t height, FileImageInfo *info)
{
    uint32_t dw, dh;
    if(width == 0U || height == 0U) {
        image.status = FILE_IMAGE_CORRUPT;
        return 0U;
    }
    info->width = width;
    info->height = height;
    if(width > IMAGE_MAX_SIDE || height > IMAGE_MAX_SIDE || width * height > IMAGE_MAX_PIXELS) {
        image.status = FILE_IMAGE_TOO_LARGE;
        return 0U;
    }
    dw = width;
    dh = height;
    if(dw > image.width || dh > image.height) {
        if(width * image.height > height * image.width) {
            dw = image.width;
            dh = height * dw / width;
        } else {
            dh = image.height;
            dw = width * dh / height;
        }
    }
    if(dw == 0U) dw = 1U;
    if(dh == 0U) dh = 1U;
    image.x += (uint16_t)((image.width - dw) / 2U);
    image.y += (uint16_t)((image.height - dh) / 2U);
    image.width = info->drawn_width = (uint16_t)dw;
    image.height = info->drawn_height = (uint16_t)dh;
    return 1U;
}

/* 以扇区大小缓存输入，缩小时跳过不需要的像素，避免整行/整帧分配。 */
static uint8_t image_byte(uint32_t offset, uint8_t *value)
{
    uint32_t start;
    UINT count;
    if(offset >= image.size) {
        image.status = FILE_IMAGE_CORRUPT;
        return 0U;
    }
    if(image.cache_count == 0U || offset < image.cache_start ||
       offset - image.cache_start >= image.cache_count) {
        start = offset & ~(uint32_t)(IMAGE_IO_BYTES - 1U);
        count = (UINT)(image.size - start);
        if(image.size - start > IMAGE_IO_BYTES) count = IMAGE_IO_BYTES;
        if(!image_seek(start) || !image_read(image.bytes, count)) return 0U;
        image.cache_start = start;
        image.cache_count = count;
    }
    *value = image.bytes[offset - image.cache_start];
    return 1U;
}

static void image_bmp(FileImageInfo *info)
{
    uint32_t dib, width, height, offset, stride, row, position, declared_size;
    uint16_t bpp, x, y;
    uint8_t top_down, blue, green, red;
    info->format = FILE_IMAGE_BMP;
    if(!image_seek(0U) || !image_read(image.bytes, 54U)) return;
    dib = image_le32(image.bytes + 14U);
    if(dib != 40U && dib != 52U && dib != 56U && dib != 108U && dib != 124U) {
        image.status = FILE_IMAGE_UNSUPPORTED;
        return;
    }
    offset = image_le32(image.bytes + 10U);
    declared_size = image_le32(image.bytes + 2U);
    width = image_le32(image.bytes + 18U);
    height = image_le32(image.bytes + 22U);
    top_down = (uint8_t)(height >> 31);
    if(top_down) height = (~height) + 1U;
    bpp = image_le16(image.bytes + 28U);
    if(image_le16(image.bytes + 26U) != 1U || width >= 0x80000000UL ||
       offset < 14U + dib || offset > image.size ||
       declared_size > image.size || (declared_size != 0U && declared_size < offset)) {
        image.status = FILE_IMAGE_CORRUPT;
        return;
    }
    if((bpp != 24U && bpp != 32U) || image_le32(image.bytes + 30U) != 0U) {
        image.status = FILE_IMAGE_UNSUPPORTED;
        return;
    }
    if(!image_dimensions(width, height, info)) return;
    stride = (width * (bpp / 8U) + 3U) & ~3UL;
    if(height > (image.size - offset) / stride ||
       (declared_size != 0U && height > (declared_size - offset) / stride)) {
        image.status = FILE_IMAGE_CORRUPT;
        return;
    }
    image.cache_count = 0U;
    for(y = 0U; y < image.height; y++) {
        if(!image_service()) return;
        row = (uint32_t)y * height / image.height;
        if(!top_down) row = height - 1U - row;
        for(x = 0U; x < image.width; x++) {
            position = offset + row * stride + ((uint32_t)x * width / image.width) * (bpp / 8U);
            if(!image_byte(position, &blue) || !image_byte(position + 1U, &green) ||
               !image_byte(position + 2U, &red)) return;
            image.pixels[x] = (uint16_t)(((uint16_t)(red & 0xF8U) << 8) |
                                       ((uint16_t)(green & 0xFCU) << 3) | (blue >> 3));
        }
        LCD_BlitRGB565(image.x, (uint16_t)(image.y + y), image.width, 1U, image.pixels);
    }
}

/* 旧版 TJpgD 假定段头完整。预检限制段长、表项和颜色格式，防止损坏文件越界。 */
static uint8_t image_jpeg_tables(uint8_t marker, UINT length)
{
    UINT at = 0U, i, n, slots, count;
    uint8_t kind;
    while(at < length) {
        kind = image.bytes[at++];
        if(marker == 0xDBU) {
            if((kind & 0xF0U) != 0U) {
                image.status = FILE_IMAGE_UNSUPPORTED;
                return 0U;
            }
            if(kind > 3U || length - at < 64U) goto invalid;
            for(i = 0U; i < 64U; i++) if(image.bytes[at + i] == 0U) break;
            if(i != 64U) goto invalid;
            at += 64U;
        } else {
            if((kind & 0xEEU) != 0U || length - at < 16U) goto invalid;
            n = 0U;
            slots = 1U;
            for(i = 0U; i < 16U; i++) {
                count = image.bytes[at++];
                slots *= 2U;
                if(count > slots) break;
                slots -= count;
                n += count;
            }
            if(i != 16U || n == 0U || n > 256U || n > length - at) goto invalid;
            for(i = 0U; i < n; i++) {
                count = image.bytes[at + i];
                if((kind & 0x10U) == 0U) {
                    if(count > 11U) break;
                } else if((count & 15U) > 10U || ((count & 15U) == 0U && count != 0U && count != 0xF0U)) break;
            }
            if(i != n) goto invalid;
            at += n;
        }
    }
    if(at == length) return 1U;
invalid:
    image.status = FILE_IMAGE_CORRUPT;
    return 0U;
}

static uint8_t image_jpeg_validate(FileImageInfo *info)
{
    uint32_t offset = 2U;
    UINT length, i;
    uint16_t marker;
    uint8_t seen_frame = 0U, in_marker = 0U, value;
    while(offset < image.size) {
        if(!image_seek(offset) || !image_read(image.bytes, 4U)) return 0U;
        marker = image_be16(image.bytes);
        length = image_be16(image.bytes + 2U);
        if((marker >> 8) != 0xFFU || length < 2U ||
           image.size - offset < (uint32_t)length + 2U) break;
        offset += (uint32_t)length + 2U;
        length -= 2U;
        if(marker == 0xFFC0U || marker == 0xFFC4U || marker == 0xFFDBU ||
           marker == 0xFFDAU || marker == 0xFFDDU) {
            if(length > IMAGE_IO_BYTES) {
                image.status = FILE_IMAGE_UNSUPPORTED;
                return 0U;
            }
            if(!image_read(image.bytes, length)) return 0U;
            if(marker == 0xFFC0U) {
                if(length < 6U) break;
                if(image.bytes[0] != 8U || image.bytes[5] != 3U) {
                    image.status = FILE_IMAGE_UNSUPPORTED;
                    return 0U;
                }
                if(length != 15U || seen_frame) break;
                for(i = 0U; i < 3U; i++) {
                    value = image.bytes[7U + 3U * i];
                    if(image.bytes[6U + 3U * i] != i + 1U ||
                       image.bytes[8U + 3U * i] > 3U ||
                       (i == 0U ? (value != 0x11U && value != 0x21U && value != 0x22U) : value != 0x11U)) {
                        image.status = FILE_IMAGE_UNSUPPORTED;
                        return 0U;
                    }
                }
                if(!image_dimensions(image_be16(image.bytes + 3U), image_be16(image.bytes + 1U), info)) return 0U;
                seen_frame = 1U;
            } else if(marker == 0xFFC4U || marker == 0xFFDBU) {
                if(length == 0U) break;
                if(!image_jpeg_tables((uint8_t)marker, length)) return 0U;
            } else if(marker == 0xFFDDU) {
                if(length != 2U) break;
            } else {
                if(length != 10U || !seen_frame) break;
                if(image.bytes[0] != 3U || image.bytes[7] != 0U || image.bytes[8] != 63U || image.bytes[9] != 0U) {
                    image.status = FILE_IMAGE_UNSUPPORTED;
                    return 0U;
                }
                for(i = 0U; i < 3U; i++) {
                    if(image.bytes[1U + 2U * i] != i + 1U || image.bytes[2U + 2U * i] != (i ? 0x11U : 0U)) break;
                }
                if(i != 3U) {
                    image.status = FILE_IMAGE_UNSUPPORTED;
                    return 0U;
                }
                /* 检查熵编码数据的终止标志，避免把缺失尾部的文件显示为成功。 */
                image.cache_count = 0U;
                while(offset < image.size) {
                    if(!image_byte(offset++, &value)) return 0U;
                    if(in_marker && value == 0xD9U) return 1U;
                    if(in_marker && value != 0U && value != 0xFFU && (value < 0xD0U || value > 0xD7U)) break;
                    in_marker = value == 0xFFU;
                }
                break;
            }
        } else if(marker < 0xFFE0U || marker > 0xFFFEU) {
            image.status = FILE_IMAGE_UNSUPPORTED;
            return 0U;
        }
    }
    image.status = FILE_IMAGE_CORRUPT;
    return 0U;
}

static UINT image_jpeg_input(JDEC *decoder, BYTE *buffer, UINT size)
{
    UINT count = 0U;
    uint32_t offset;
    (void)decoder;
    if(!image_service()) return 0U;
    if(buffer != 0) {
        if(f_read(&image.file, buffer, size, &count) != FR_OK) image.status = FILE_IMAGE_IO;
        return image.status == FILE_IMAGE_OK ? count : 0U;
    }
    offset = (uint32_t)f_tell(&image.file);
    if(size > image.size - offset) {
        image.status = FILE_IMAGE_CORRUPT;
        return 0U;
    }
    return image_seek(offset + size) ? size : 0U;
}

static UINT image_jpeg_output(JDEC *decoder, void *bitmap, JRECT *rect)
{
    const uint16_t *source = (const uint16_t *)bitmap;
    uint32_t x0, x1, y0, y1, sx, sy;
    uint16_t x, y, source_width;
    (void)decoder;
    if(!image_service()) return 0U;
    /* 以输出坐标反算采样位置，MCU 之间不留缝，末列末行也不会越界。 */
    x0 = ((uint32_t)rect->left * image.width + image.jpeg_width - 1U) / image.jpeg_width;
    x1 = ((uint32_t)(rect->right + 1U) * image.width + image.jpeg_width - 1U) / image.jpeg_width;
    y0 = ((uint32_t)rect->top * image.height + image.jpeg_height - 1U) / image.jpeg_height;
    y1 = ((uint32_t)(rect->bottom + 1U) * image.height + image.jpeg_height - 1U) / image.jpeg_height;
    source_width = (uint16_t)(rect->right - rect->left + 1U);
    if(x1 > image.width || y1 > image.height) {
        image.status = FILE_IMAGE_CORRUPT;
        return 0U;
    }
    if(x0 == x1 || y0 == y1) return 1U;
    for(y = (uint16_t)y0; y < y1; y++) {
        sy = (uint32_t)y * image.jpeg_height / image.height - rect->top;
        for(x = (uint16_t)x0; x < x1; x++) {
            sx = (uint32_t)x * image.jpeg_width / image.width - rect->left;
            image.pixels[x - x0] = source[sy * source_width + sx];
        }
        LCD_BlitRGB565((uint16_t)(image.x + x0), (uint16_t)(image.y + y),
                      (uint16_t)(x1 - x0), 1U, image.pixels);
    }
    return 1U;
}

static void image_jpeg(FileImageInfo *info)
{
    JRESULT result;
    uint8_t scale = 0U;
    info->format = FILE_IMAGE_JPEG;
    if(!image_jpeg_validate(info) || !image_seek(0U)) return;
    memset(&image.jpeg, 0, sizeof(image.jpeg));
    result = jd_prepare(&image.jpeg, image_jpeg_input, image.pool, sizeof(image.pool), 0);
    if(result == JDR_OK && image.status == FILE_IMAGE_OK) {
        while(scale < 3U && (info->width >> (scale + 1U)) >= image.width &&
              (info->height >> (scale + 1U)) >= image.height) scale++;
        image.jpeg_width = (uint16_t)(info->width >> scale);
        image.jpeg_height = (uint16_t)(info->height >> scale);
        result = jd_decomp(&image.jpeg, image_jpeg_output, scale);
    }
    if(image.status != FILE_IMAGE_OK || result == JDR_OK) return;
    if(result == JDR_FMT2 || result == JDR_FMT3 || result == JDR_MEM1 || result == JDR_MEM2)
        image.status = FILE_IMAGE_UNSUPPORTED;
    else image.status = FILE_IMAGE_CORRUPT;
}

FileImageResult file_image_draw(const char *path, uint16_t x, uint16_t y,
                               uint16_t w, uint16_t h, FileImageInfo *info,
                               uint8_t (*service)(void *), void *context)
{
    FileImageInfo local_info;
    FileImageResult result;
    if(image.busy || path == 0 || w == 0U || h == 0U || w > IMAGE_LINE_PIXELS ||
       x >= LCD_X_LENGTH || y >= LCD_Y_LENGTH || w > LCD_X_LENGTH - x || h > LCD_Y_LENGTH - y)
        return FILE_IMAGE_UNSUPPORTED;
    if(info == 0) info = &local_info;
    memset(info, 0, sizeof(*info));
    image.busy = 1U;
    image.service = service;
    image.context = context;
    image.status = FILE_IMAGE_OK;
    image.cache_count = 0U;
    image.x = x;
    image.y = y;
    image.width = w;
    image.height = h;
    if(f_open(&image.file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
        image.busy = 0U;
        return FILE_IMAGE_IO;
    }
    image.size = (uint32_t)f_size(&image.file);
    if(image.size > IMAGE_MAX_BYTES) image.status = FILE_IMAGE_TOO_LARGE;
    else if(image_read(image.bytes, 2U)) {
        if(image.bytes[0] == 'B' && image.bytes[1] == 'M') image_bmp(info);
        else if(image.bytes[0] == 0xFFU && image.bytes[1] == 0xD8U) image_jpeg(info);
        else if(image.bytes[0] == 0x89U && image.bytes[1] == 'P') {
            /* PNG 有独立的流式文件对象，交接前关闭当前对象。 */
            if(f_close(&image.file) != FR_OK) {
                image.busy = 0U;
                return FILE_IMAGE_IO;
            }
            result = file_png_draw(path, x, y, w, h, info, service, context,
                                  image_workspace, image_workspace_capacity);
            image.busy = 0U;
            return result;
        } else image.status = FILE_IMAGE_UNSUPPORTED;
    }
    if(f_close(&image.file) != FR_OK && image.status == FILE_IMAGE_OK) image.status = FILE_IMAGE_IO;
    result = image.status;
    image.busy = 0U;
    return result;
}
