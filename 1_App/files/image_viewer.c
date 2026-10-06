/* 静态图片入口按文件签名分派 BMP、JPEG 或 PNG，并统一返回格式、尺寸和错误。
 * BMP/JPEG 共用内部固定工作区，PNG 使用注册的外部工作区；所有显示均可协作取消。 */
#include "image_viewer.h"
#include "image_png.h"
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
#define IMAGE_JPEG_MCU_PIXELS 16U

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
    /* 格式识别、BMP 读取和 JPEG 预检使用 bmp；预检完成后才启用 jpeg。
     * 两种解码过程不会交错，文件对象和回调状态始终独立于工作区。 */
    union {
        struct {
            uint8_t bytes[IMAGE_IO_BYTES];
            uint16_t pixels[IMAGE_LINE_PIXELS];
        } bmp;
        struct {
            JDEC decoder;
            uint32_t pool[IMAGE_JPEG_POOL / 4U];
            /* 预检只允许 8/16 像素宽 MCU；显示只缩小，每次输出最多一行 MCU。 */
            uint16_t pixels[IMAGE_JPEG_MCU_PIXELS];
        } jpeg;
    } workspace;
    /* 回调和错误状态独立于复用的解码区，预检切换为正式解码时继续有效。 */
    uint8_t (*service)(void *);
    void *context;
    FileImageResult status;
    uint32_t size;
    uint32_t cache_start;
    UINT cache_count;
    /* 初始为调用者的目标矩形，尺寸校验后改为实际居中绘制区域。 */
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

/* 读取和像素输出共用取消检查；一旦出错，不再调用后台或继续解码。 */
static uint8_t image_service(void)
{
    if(image.status != FILE_IMAGE_OK) return 0U;
    if(image.service != 0 && image.service(image.context) == 0U) {
        image.status = FILE_IMAGE_CANCELLED;
        return 0U;
    }
    return 1U;
}

/* 有界定位并核对实际位置；格式越界与 FatFs 定位失败分别报告为损坏和 I/O 错误。 */
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

/* 必须读满指定长度才成功，短读视为文件内容不完整，底层读取失败另报 I/O 错误。 */
static uint8_t image_read(void *buffer, UINT size)
{
    UINT count = 0U;
    if(!image_service()) return 0U;
    if(f_read(&image.file, buffer, size, &count) != FR_OK) image.status = FILE_IMAGE_IO;
    else if(count != size) image.status = FILE_IMAGE_CORRUPT;
    return image.status == FILE_IMAGE_OK;
}

/* 用整数交叉乘积比较宽高比，按受限的一边等比缩小，再在目标区域居中。 */
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
        if(!image_seek(start) || !image_read(image.workspace.bmp.bytes, count)) return 0U;
        image.cache_start = start;
        image.cache_count = count;
    }
    *value = image.workspace.bmp.bytes[offset - image.cache_start];
    return 1U;
}

/* 校验 BMP 文件头及像素区边界后，按目标扫描行采样未压缩的 24/32 位像素。
 * 输入是 BGR 排列，转换为 RGB565 后逐行写屏；32 位格式的额外字节不参与显示。 */
static void image_bmp(FileImageInfo *info)
{
    uint32_t dib, width, height, offset, stride, row, position, declared_size;
    uint16_t bpp, x, y;
    uint8_t top_down, blue, green, red;
    info->format = FILE_IMAGE_BMP;
    if(!image_seek(0U) || !image_read(image.workspace.bmp.bytes, 54U)) return;
    dib = image_le32(image.workspace.bmp.bytes + 14U);
    if(dib != 40U && dib != 52U && dib != 56U && dib != 108U && dib != 124U) {
        image.status = FILE_IMAGE_UNSUPPORTED;
        return;
    }
    offset = image_le32(image.workspace.bmp.bytes + 10U);
    declared_size = image_le32(image.workspace.bmp.bytes + 2U);
    width = image_le32(image.workspace.bmp.bytes + 18U);
    height = image_le32(image.workspace.bmp.bytes + 22U);
    top_down = (uint8_t)(height >> 31);
    if(top_down) height = (~height) + 1U;
    bpp = image_le16(image.workspace.bmp.bytes + 28U);
    if(image_le16(image.workspace.bmp.bytes + 26U) != 1U || width >= 0x80000000UL ||
       offset < 14U + dib || offset > image.size ||
       declared_size > image.size || (declared_size != 0U && declared_size < offset)) {
        image.status = FILE_IMAGE_CORRUPT;
        return;
    }
    if((bpp != 24U && bpp != 32U) || image_le32(image.workspace.bmp.bytes + 30U) != 0U) {
        image.status = FILE_IMAGE_UNSUPPORTED;
        return;
    }
    if(!image_dimensions(width, height, info)) return;
    /* BMP 每行按四字节补齐；用除法验证像素区长度，避免行数乘步长溢出。 */
    stride = (width * (bpp / 8U) + 3U) & ~3UL;
    if(height > (image.size - offset) / stride ||
       (declared_size != 0U && height > (declared_size - offset) / stride)) {
        image.status = FILE_IMAGE_CORRUPT;
        return;
    }
    image.cache_count = 0U;
    for(y = 0U; y < image.height; y++) {
        if(!image_service()) return;
        /* 最近邻采样映射到源图行，正高度 BMP 还需反转自底向上的存储顺序。 */
        row = (uint32_t)y * height / image.height;
        if(!top_down) row = height - 1U - row;
        for(x = 0U; x < image.width; x++) {
            position = offset + row * stride + ((uint32_t)x * width / image.width) * (bpp / 8U);
            if(!image_byte(position, &blue) || !image_byte(position + 1U, &green) ||
               !image_byte(position + 2U, &red)) return;
            image.workspace.bmp.pixels[x] = (uint16_t)(((uint16_t)(red & 0xF8U) << 8) |
                                       ((uint16_t)(green & 0xFCU) << 3) | (blue >> 3));
        }
        LCD_BlitRGB565(image.x, (uint16_t)(image.y + y), image.width, 1U, image.workspace.bmp.pixels);
    }
}

/* 旧版 TJpgD 假定段头完整。预检限制段长、表项和颜色格式，防止损坏文件越界。 */
static uint8_t image_jpeg_tables(uint8_t marker, UINT length)
{
    UINT at = 0U, i, n, slots, count;
    uint8_t kind;
    while(at < length) {
        kind = image.workspace.bmp.bytes[at++];
        if(marker == 0xDBU) {
            if((kind & 0xF0U) != 0U) {
                image.status = FILE_IMAGE_UNSUPPORTED;
                return 0U;
            }
            if(kind > 3U || length - at < 64U) goto invalid;
            for(i = 0U; i < 64U; i++) if(image.workspace.bmp.bytes[at + i] == 0U) break;
            if(i != 64U) goto invalid;
            at += 64U;
        } else {
            if((kind & 0xEEU) != 0U || length - at < 16U) goto invalid;
            n = 0U;
            slots = 1U;
            for(i = 0U; i < 16U; i++) {
                count = image.workspace.bmp.bytes[at++];
                slots *= 2U;
                if(count > slots) break;
                slots -= count;
                n += count;
            }
            if(i != 16U || n == 0U || n > 256U || n > length - at) goto invalid;
            for(i = 0U; i < n; i++) {
                count = image.workspace.bmp.bytes[at + i];
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

/* 在交给 TJpgD 前遍历 JPEG 段，验证其支持的基线三分量布局和采样方式。
 * 同时取得原图尺寸、限制 MCU 大小并检查扫描数据尾部；不兼容和损坏分别记录状态。 */
static uint8_t image_jpeg_validate(FileImageInfo *info)
{
    uint32_t offset = 2U;
    UINT length, i;
    uint16_t marker;
    uint8_t seen_frame = 0U, in_marker = 0U, value;
    while(offset < image.size) {
        if(!image_seek(offset) || !image_read(image.workspace.bmp.bytes, 4U)) return 0U;
        marker = image_be16(image.workspace.bmp.bytes);
        length = image_be16(image.workspace.bmp.bytes + 2U);
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
            if(!image_read(image.workspace.bmp.bytes, length)) return 0U;
            if(marker == 0xFFC0U) {
                if(length < 6U) break;
                if(image.workspace.bmp.bytes[0] != 8U || image.workspace.bmp.bytes[5] != 3U) {
                    image.status = FILE_IMAGE_UNSUPPORTED;
                    return 0U;
                }
                if(length != 15U || seen_frame) break;
                for(i = 0U; i < 3U; i++) {
                    value = image.workspace.bmp.bytes[7U + 3U * i];
                    if(image.workspace.bmp.bytes[6U + 3U * i] != i + 1U ||
                       image.workspace.bmp.bytes[8U + 3U * i] > 3U ||
                       (i == 0U ? (value != 0x11U && value != 0x21U && value != 0x22U) : value != 0x11U)) {
                        image.status = FILE_IMAGE_UNSUPPORTED;
                        return 0U;
                    }
                }
                if(!image_dimensions(image_be16(image.workspace.bmp.bytes + 3U),
                                     image_be16(image.workspace.bmp.bytes + 1U), info)) return 0U;
                seen_frame = 1U;
            } else if(marker == 0xFFC4U || marker == 0xFFDBU) {
                if(length == 0U) break;
                if(!image_jpeg_tables((uint8_t)marker, length)) return 0U;
            } else if(marker == 0xFFDDU) {
                if(length != 2U) break;
            } else {
                if(length != 10U || !seen_frame) break;
                if(image.workspace.bmp.bytes[0] != 3U || image.workspace.bmp.bytes[7] != 0U ||
                   image.workspace.bmp.bytes[8] != 63U || image.workspace.bmp.bytes[9] != 0U) {
                    image.status = FILE_IMAGE_UNSUPPORTED;
                    return 0U;
                }
                for(i = 0U; i < 3U; i++) {
                    if(image.workspace.bmp.bytes[1U + 2U * i] != i + 1U ||
                       image.workspace.bmp.bytes[2U + 2U * i] != (i ? 0x11U : 0U)) break;
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

/* TJpgD 的空目标缓冲表示跳过数据，此时只移动文件位置，不额外读取。 */
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

/* TJpgD 每次提供一个已解码的 MCU 矩形，回调将其中命中目标采样点的像素逐行输出。
 * 返回非零继续解码；取消或越界时返回零，并通过 image.status 保留具体原因。 */
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
            image.workspace.jpeg.pixels[x - x0] = source[sy * source_width + sx];
        }
        LCD_BlitRGB565((uint16_t)(image.x + x0), (uint16_t)(image.y + y),
                      (uint16_t)(x1 - x0), 1U, image.workspace.jpeg.pixels);
    }
    return 1U;
}

/* 完成预检后重置文件位置并启动 TJpgD；结束时将库错误映射为应用层图片结果。
 * 回调已记录的 I/O 或取消状态优先保留，避免被解码器的通用错误覆盖。 */
static void image_jpeg(FileImageInfo *info)
{
    JRESULT result;
    uint8_t scale = 0U;
    info->format = FILE_IMAGE_JPEG;
    if(!image_jpeg_validate(info) || !image_seek(0U)) return;
    memset(&image.workspace.jpeg.decoder, 0, sizeof(image.workspace.jpeg.decoder));
    result = jd_prepare(&image.workspace.jpeg.decoder, image_jpeg_input, image.workspace.jpeg.pool,
                        sizeof(image.workspace.jpeg.pool), 0);
    if(result == JDR_OK && image.status == FILE_IMAGE_OK) {
        /* 先使用解码器的 1/2、1/4 或 1/8 缩放减小计算量，再由输出回调精确采样。 */
        while(scale < 3U && (info->width >> (scale + 1U)) >= image.width &&
              (info->height >> (scale + 1U)) >= image.height) scale++;
        image.jpeg_width = (uint16_t)(info->width >> scale);
        image.jpeg_height = (uint16_t)(info->height >> scale);
        result = jd_decomp(&image.workspace.jpeg.decoder, image_jpeg_output, scale);
    }
    if(image.status != FILE_IMAGE_OK || result == JDR_OK) return;
    if(result == JDR_FMT2 || result == JDR_FMT3 || result == JDR_MEM1 || result == JDR_MEM2)
        image.status = FILE_IMAGE_UNSUPPORTED;
    else image.status = FILE_IMAGE_CORRUPT;
}

/* 管理一次静态图片显示的完整生命周期：校验区域、打开、识别、解码和关闭。
 * busy 防止后台回调重入；各退出路径释放该标志，PNG 分派前关闭识别阶段的文件对象。 */
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
    else if(image_read(image.workspace.bmp.bytes, 2U)) {
        if(image.workspace.bmp.bytes[0] == 'B' && image.workspace.bmp.bytes[1] == 'M') image_bmp(info);
        else if(image.workspace.bmp.bytes[0] == 0xFFU && image.workspace.bmp.bytes[1] == 0xD8U) image_jpeg(info);
        else if(image.workspace.bmp.bytes[0] == 0x89U && image.workspace.bmp.bytes[1] == 'P') {
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
