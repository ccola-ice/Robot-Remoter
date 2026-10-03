#include "image_gif.h"
#include "ff.h"
#include "bsp_fsmc_lcd.h"
#include <string.h>

#define GIF_MAX_SIDE 8192UL
#define GIF_MAX_PIXELS (16UL * 1024UL * 1024UL)
#define GIF_MAX_BYTES (32UL * 1024UL * 1024UL)
#define GIF_CANVAS_WIDTH 320U
#define GIF_CANVAS_HEIGHT 200U
#define GIF_CODES 4096U
#define GIF_IO_BYTES 512U

/* 文件扇区缓存、字典、颜色表和两幅合成画布均使用调用者的外部 SRAM。 */
typedef struct {
    FIL file;
    uint16_t prefix[GIF_CODES];
    uint8_t suffix[GIF_CODES];
    uint8_t stack[GIF_CODES];
    uint16_t global[256];
    uint16_t local[256];
    uint8_t bytes[GIF_IO_BYTES];
} GifWorkspace;

typedef struct {
    uint16_t left, top, width, height;
    uint16_t delay;
    uint8_t disposal, transparent, transparent_index;
} GifFrame;

static struct {
    GifWorkspace *work;
    uint16_t *canvas, *previous, *palette;
    uint8_t (*service)(void *);
    void *context;
    FileImageResult result;
    GifFrame frame, last;
    uint32_t file_size, position, image_start, canvas_pixels;
    uint32_t loops_done, frames_in_pass, delay_ms;
    uint32_t bits, decoded, total_pixels;
    uint16_t x, y, width, height, source_width, source_height;
    uint16_t io_index, io_count, palette_count, global_count;
    uint16_t loop_count, background, screen_background;
    uint16_t pixel_x, pixel_y, output_x, output_y;
    uint8_t bit_count, block_remaining, block_end, interlaced, pass;
    uint8_t background_index, loop_known, opened, finished, has_last;
} gif;

static uint8_t gif_fail(FileImageResult result)
{
    gif.result = result;
    return 0U;
}

static uint8_t gif_service(void)
{
    if(gif.result != FILE_IMAGE_OK) return 0U;
    if(gif.service != 0 && gif.service(gif.context) == 0U)
        return gif_fail(FILE_IMAGE_CANCELLED);
    return 1U;
}

static void gif_close_file(void)
{
    if(gif.opened) {
        if(f_close(&gif.work->file) != FR_OK && gif.result == FILE_IMAGE_OK)
            gif.result = FILE_IMAGE_IO;
        gif.opened = 0U;
    }
}

static uint8_t gif_seek(uint32_t position)
{
    if(position > gif.file_size) return gif_fail(FILE_IMAGE_CORRUPT);
    if(!gif_service()) return 0U;
    if(f_lseek(&gif.work->file, position) != FR_OK ||
       f_tell(&gif.work->file) != position) return gif_fail(FILE_IMAGE_IO);
    gif.position = position;
    gif.io_index = gif.io_count = 0U;
    return 1U;
}

static uint8_t gif_byte(uint8_t *value)
{
    UINT read_count = 0U, requested;
    if(gif.result != FILE_IMAGE_OK) return 0U;
    if(gif.position >= gif.file_size) return gif_fail(FILE_IMAGE_CORRUPT);
    if(gif.io_index == gif.io_count) {
        if(!gif_service()) return 0U;
        requested = (UINT)(gif.file_size - gif.position);
        if(gif.file_size - gif.position > GIF_IO_BYTES) requested = GIF_IO_BYTES;
        if(f_read(&gif.work->file, gif.work->bytes, requested, &read_count) != FR_OK)
            return gif_fail(FILE_IMAGE_IO);
        if(read_count != requested) return gif_fail(FILE_IMAGE_CORRUPT);
        gif.io_index = 0U;
        gif.io_count = (uint16_t)read_count;
    }
    *value = gif.work->bytes[gif.io_index++];
    ++gif.position;
    return 1U;
}

static uint8_t gif_bytes(uint8_t *bytes, uint16_t count)
{
    while(count--) if(!gif_byte(bytes++)) return 0U;
    return 1U;
}

static uint16_t gif_le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint8_t gif_palette(uint16_t *palette, uint16_t count)
{
    uint16_t i;
    uint8_t rgb[3];
    for(i = 0U; i < count; ++i) {
        if(!gif_bytes(rgb, 3U)) return 0U;
        palette[i] = (uint16_t)(((uint16_t)(rgb[0] & 248U) << 8) |
                     ((uint16_t)(rgb[1] & 252U) << 3) | (rgb[2] >> 3));
    }
    return 1U;
}

static uint8_t gif_extension_blocks(void)
{
    uint8_t count, ignored;
    uint16_t i;
    for(;;) {
        if(!gif_byte(&count)) return 0U;
        if(count == 0U) return 1U;
        for(i = 0U; i < count; ++i) if(!gif_byte(&ignored)) return 0U;
    }
}

static void gif_control_reset(void)
{
    memset(&gif.frame, 0, sizeof(gif.frame));
    gif.frame.delay = 10U;
}

static uint8_t gif_extension(void)
{
    uint8_t label, size, data[11], end;
    if(!gif_byte(&label)) return 0U;
    if(label == 0xF9U) {
        if(!gif_byte(&size)) return 0U;
        if(size != 4U) return gif_fail(FILE_IMAGE_CORRUPT);
        if(!gif_bytes(data, 4U) || !gif_byte(&end)) return 0U;
        if(end != 0U || (data[0] & 0xE0U)) return gif_fail(FILE_IMAGE_CORRUPT);
        gif.frame.disposal = (data[0] >> 2) & 7U;
        if(gif.frame.disposal > 3U) return gif_fail(FILE_IMAGE_UNSUPPORTED);
        gif.frame.delay = gif_le16(data + 1);
        gif.frame.transparent = data[0] & 1U;
        gif.frame.transparent_index = data[3];
        return 1U;
    }
    if(label == 0x01U) return gif_fail(FILE_IMAGE_UNSUPPORTED);
    if(label == 0xFFU) {
        if(!gif_byte(&size)) return 0U;
        if(size != 11U) return gif_fail(FILE_IMAGE_CORRUPT);
        if(!gif_bytes(data, 11U)) return 0U;
        if(memcmp(data, "NETSCAPE2.0", 11U) == 0 ||
           memcmp(data, "ANIMEXTS1.0", 11U) == 0) {
            if(!gif_byte(&size)) return 0U;
            if(size != 3U) return gif_fail(FILE_IMAGE_CORRUPT);
            if(!gif_bytes(data, 3U)) return 0U;
            if(data[0] != 1U) return gif_fail(FILE_IMAGE_CORRUPT);
            if(!gif.loop_known) {
                gif.loop_known = 1U;
                gif.loop_count = gif_le16(data + 1);
            }
        }
    }
    return gif_extension_blocks();
}

/* GIF 的数据块仅负责封装，LZW 位流可跨越任意数据块边界。 */
static uint8_t gif_data_byte(uint8_t *value)
{
    if(gif.block_end) return gif_fail(FILE_IMAGE_CORRUPT);
    if(gif.block_remaining == 0U) {
        if(!gif_byte(&gif.block_remaining)) return 0U;
        if(gif.block_remaining == 0U) {
            gif.block_end = 1U;
            return gif_fail(FILE_IMAGE_CORRUPT);
        }
    }
    if(!gif_byte(value)) return 0U;
    --gif.block_remaining;
    return 1U;
}

static uint8_t gif_code(uint8_t width, uint16_t *code)
{
    uint8_t byte;
    while(gif.bit_count < width) {
        if(!gif_data_byte(&byte)) return 0U;
        gif.bits |= (uint32_t)byte << gif.bit_count;
        gif.bit_count += 8U;
    }
    *code = (uint16_t)(gif.bits & ((1UL << width) - 1U));
    gif.bits >>= width;
    gif.bit_count -= width;
    return 1U;
}

/* 向上取整可找出以指定源坐标作为采样点的第一个目标像素。 */
static uint16_t gif_scale_ceil(uint16_t source, uint16_t target, uint16_t full)
{
    return (uint16_t)(((uint32_t)source * target + full - 1U) / full);
}

static void gif_row_start(void)
{
    uint16_t source_y = gif.frame.top + gif.pixel_y;
    gif.output_y = gif_scale_ceil(source_y, gif.height, gif.source_height);
    if(gif.output_y >= gif.height ||
       (uint32_t)gif.output_y * gif.source_height / gif.height != source_y)
        gif.output_y = gif.height;
    gif.output_x = gif_scale_ceil(gif.frame.left, gif.width, gif.source_width);
}

static uint8_t gif_pixel(uint8_t value)
{
    static const uint8_t starts[4] = {0U, 4U, 2U, 1U};
    static const uint8_t steps[4] = {8U, 8U, 4U, 2U};
    uint16_t source_x;
    if(gif.decoded >= gif.total_pixels || value >= gif.palette_count)
        return gif_fail(FILE_IMAGE_CORRUPT);
    if((gif.decoded & 255U) == 0U && !gif_service()) return 0U;
    source_x = gif.frame.left + gif.pixel_x;
    if(gif.output_y < gif.height && gif.output_x < gif.width &&
       (uint32_t)gif.output_x * gif.source_width / gif.width == source_x) {
        if(!gif.frame.transparent || value != gif.frame.transparent_index)
            gif.canvas[(uint32_t)gif.output_y * gif.width + gif.output_x] = gif.palette[value];
        ++gif.output_x;
    }
    ++gif.decoded;
    if(++gif.pixel_x == gif.frame.width) {
        gif.pixel_x = 0U;
        if(gif.interlaced) {
            gif.pixel_y += steps[gif.pass];
            while(gif.pixel_y >= gif.frame.height && gif.pass < 3U)
                gif.pixel_y = starts[++gif.pass];
        } else ++gif.pixel_y;
        if(gif.decoded < gif.total_pixels) gif_row_start();
    }
    return 1U;
}

static uint8_t gif_lzw(void)
{
    uint8_t minimum, width, first = 0U, ignored;
    uint16_t clear, end, next, old = 0xFFFFU, code, input, count;
    uint32_t commands = 0U;
    if(!gif_byte(&minimum)) return 0U;
    if(minimum < 2U || minimum > 8U) return gif_fail(FILE_IMAGE_CORRUPT);
    clear = (uint16_t)(1U << minimum);
    end = clear + 1U;
    next = end + 1U;
    width = minimum + 1U;
    gif.bits = gif.bit_count = gif.block_remaining = gif.block_end = 0U;
    gif.decoded = gif.pixel_x = gif.pixel_y = gif.pass = 0U;
    gif.total_pixels = (uint32_t)gif.frame.width * gif.frame.height;
    gif_row_start();
    if(!gif_code(width, &code)) return 0U;
    if(code != clear) return gif_fail(FILE_IMAGE_CORRUPT);
    for(;;) {
        if((++commands & 255U) == 0U && !gif_service()) return 0U;
        if(!gif_code(width, &code)) return 0U;
        if(code == clear) {
            next = end + 1U;
            width = minimum + 1U;
            old = 0xFFFFU;
            continue;
        }
        if(code == end) {
            if(gif.decoded != gif.total_pixels) return gif_fail(FILE_IMAGE_CORRUPT);
            while(gif.block_remaining) {
                if(!gif_byte(&ignored)) return 0U;
                --gif.block_remaining;
            }
            return gif_extension_blocks();
        }
        if(old == 0xFFFFU) {
            if(code >= clear || !gif_pixel((uint8_t)code)) return gif_fail(
                gif.result == FILE_IMAGE_OK ? FILE_IMAGE_CORRUPT : gif.result);
            old = code;
            first = (uint8_t)code;
            continue;
        }
        input = code;
        count = 0U;
        if(code == next) {
            if(next >= GIF_CODES) return gif_fail(FILE_IMAGE_CORRUPT);
            gif.work->stack[count++] = first;
            code = old;
        } else if(code > next) return gif_fail(FILE_IMAGE_CORRUPT);
        while(code >= clear) {
            if(code <= end || code >= next || count >= GIF_CODES)
                return gif_fail(FILE_IMAGE_CORRUPT);
            gif.work->stack[count++] = gif.work->suffix[code];
            /* 每条新字典项只引用更早的项，因此坏数据不能构成引用环。 */
            if(gif.work->prefix[code] >= code) return gif_fail(FILE_IMAGE_CORRUPT);
            code = gif.work->prefix[code];
        }
        first = (uint8_t)code;
        if(count >= GIF_CODES) return gif_fail(FILE_IMAGE_CORRUPT);
        gif.work->stack[count++] = first;
        while(count) if(!gif_pixel(gif.work->stack[--count])) return 0U;
        if(next < GIF_CODES) {
            gif.work->prefix[next] = old;
            gif.work->suffix[next] = first;
            ++next;
            if(next == (1U << width) && width < 12U) ++width;
        }
        old = input;
    }
}

static void gif_fill(uint16_t color)
{
    uint32_t i;
    for(i = 0U; i < gif.canvas_pixels; ++i) gif.canvas[i] = color;
}

static void gif_dispose(void)
{
    uint16_t x, y, left, top, right, bottom, color;
    if(gif.last.disposal == 3U) {
        memcpy(gif.canvas, gif.previous, gif.canvas_pixels * sizeof(uint16_t));
    } else if(gif.last.disposal == 2U) {
        left = gif_scale_ceil(gif.last.left, gif.width, gif.source_width);
        right = gif_scale_ceil(gif.last.left + gif.last.width, gif.width, gif.source_width);
        top = gif_scale_ceil(gif.last.top, gif.height, gif.source_height);
        bottom = gif_scale_ceil(gif.last.top + gif.last.height, gif.height, gif.source_height);
        color = gif.last.transparent ? gif.screen_background : gif.background;
        for(y = top; y < bottom; ++y)
            for(x = left; x < right; ++x) gif.canvas[(uint32_t)y * gif.width + x] = color;
    }
}

static uint8_t gif_image(void)
{
    uint8_t data[9];
    if(!gif_bytes(data, 9U)) return 0U;
    gif.frame.left = gif_le16(data);
    gif.frame.top = gif_le16(data + 2);
    gif.frame.width = gif_le16(data + 4);
    gif.frame.height = gif_le16(data + 6);
    if(gif.frame.width == 0U || gif.frame.height == 0U ||
       (uint32_t)gif.frame.left + gif.frame.width > gif.source_width ||
       (uint32_t)gif.frame.top + gif.frame.height > gif.source_height || (data[8] & 0x18U))
        return gif_fail(FILE_IMAGE_CORRUPT);
    gif.interlaced = (data[8] & 0x40U) != 0U;
    if(data[8] & 0x80U) {
        gif.palette_count = (uint16_t)(2U << (data[8] & 7U));
        gif.palette = gif.work->local;
        if(!gif_palette(gif.palette, gif.palette_count)) return 0U;
    } else {
        gif.palette_count = gif.global_count;
        gif.palette = gif.work->global;
    }
    if(gif.palette_count == 0U) return gif_fail(FILE_IMAGE_UNSUPPORTED);
    if(gif.frame.transparent && gif.frame.transparent_index >= gif.palette_count)
        return gif_fail(FILE_IMAGE_CORRUPT);
    if(!gif.has_last) gif_fill(gif.frame.transparent ? gif.screen_background : gif.background);
    else gif_dispose();
    if(gif.frame.disposal == 3U)
        memcpy(gif.previous, gif.canvas, gif.canvas_pixels * sizeof(uint16_t));
    if(!gif_lzw() || !gif_service()) return 0U;
    LCD_BlitRGB565(gif.x, gif.y, gif.width, gif.height, gif.canvas);
    gif.last = gif.frame;
    gif.has_last = 1U;
    ++gif.frames_in_pass;
    gif.delay_ms = (uint32_t)gif.frame.delay * 10U;
    if(gif.delay_ms < 20U) gif.delay_ms = 20U;
    gif_control_reset();
    return 1U;
}

FileImageResult file_gif_next(void)
{
    uint8_t marker;
    if(!gif.opened) return gif.result;
    while(gif.result == FILE_IMAGE_OK) {
        if(!gif_byte(&marker)) break;
        if(marker == 0x2CU) {
            if(gif_image()) return FILE_IMAGE_OK;
            break;
        }
        if(marker == 0x21U) {
            if(!gif_extension()) break;
            continue;
        }
        if(marker != 0x3BU || gif.frames_in_pass == 0U) {
            gif_fail(FILE_IMAGE_CORRUPT);
            break;
        }
        if(!gif.loop_known || (gif.loop_count && gif.loops_done >= gif.loop_count)) break;
        ++gif.loops_done;
        gif.frames_in_pass = 0U;
        gif.has_last = 0U;
        gif_control_reset();
        if(!gif_seek(gif.image_start)) break;
    }
    gif_close_file();
    gif.finished = 1U;
    return gif.result;
}

FileImageResult file_gif_open(const char *path, uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h, uint16_t background,
                              FileImageInfo *info, void *scratch,
                              uint32_t capacity, uint8_t (*service)(void *),
                              void *context)
{
    uint8_t header[13];
    uint32_t dw, dh, needed, padding;
    uintptr_t aligned;
    file_gif_close();
    memset(&gif, 0, sizeof(gif));
    gif.finished = 1U;
    if(info != 0) memset(info, 0, sizeof(*info));
    if(path == 0 || info == 0 || w == 0U || h == 0U ||
       (uint32_t)x + w > LCD_X_LENGTH || (uint32_t)y + h > LCD_Y_LENGTH)
        return gif.result = FILE_IMAGE_CORRUPT;
    if(scratch == 0) return gif.result = FILE_IMAGE_NO_MEMORY;
    aligned = ((uintptr_t)scratch + sizeof(void *) - 1U) & ~(uintptr_t)(sizeof(void *) - 1U);
    padding = (uint32_t)(aligned - (uintptr_t)scratch);
    if(capacity < padding || capacity - padding < sizeof(GifWorkspace))
        return gif.result = FILE_IMAGE_NO_MEMORY;
    gif.work = (GifWorkspace *)aligned;
    gif.service = service;
    gif.context = context;
    gif.screen_background = background;
    gif.background = background;
    if(!gif_service()) return gif.result;
    if(f_open(&gif.work->file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK)
        return gif.result = FILE_IMAGE_IO;
    gif.opened = 1U;
    gif.finished = 0U;
    gif.file_size = f_size(&gif.work->file);
    if(gif.file_size > GIF_MAX_BYTES) gif_fail(FILE_IMAGE_TOO_LARGE);
    else if(gif_bytes(header, 13U)) {
        if(memcmp(header, "GIF87a", 6U) && memcmp(header, "GIF89a", 6U))
            gif_fail(FILE_IMAGE_UNSUPPORTED);
        else {
            info->format = FILE_IMAGE_GIF;
            info->width = gif.source_width = gif_le16(header + 6);
            info->height = gif.source_height = gif_le16(header + 8);
            if(info->width == 0U || info->height == 0U) gif_fail(FILE_IMAGE_CORRUPT);
            else if(info->width > GIF_MAX_SIDE || info->height > GIF_MAX_SIDE ||
                    info->width * info->height > GIF_MAX_PIXELS) gif_fail(FILE_IMAGE_TOO_LARGE);
            else {
                dw = w < GIF_CANVAS_WIDTH ? w : GIF_CANVAS_WIDTH;
                dh = h < GIF_CANVAS_HEIGHT ? h : GIF_CANVAS_HEIGHT;
                if(dw > info->width) dw = info->width;
                if(dh > info->height) dh = info->height;
                if(info->width * dh > info->height * dw) dh = info->height * dw / info->width;
                else dw = info->width * dh / info->height;
                if(dw == 0U) dw = 1U;
                if(dh == 0U) dh = 1U;
                gif.canvas_pixels = dw * dh;
                needed = sizeof(GifWorkspace) + gif.canvas_pixels * 4U;
                if(capacity - padding < needed) gif_fail(FILE_IMAGE_NO_MEMORY);
                else {
                    gif.canvas = (uint16_t *)(gif.work + 1);
                    gif.previous = gif.canvas + gif.canvas_pixels;
                    gif.width = info->drawn_width = (uint16_t)dw;
                    gif.height = info->drawn_height = (uint16_t)dh;
                    gif.x = x + (uint16_t)((w - dw) / 2U);
                    gif.y = y + (uint16_t)((h - dh) / 2U);
                    gif.background_index = header[11];
                    if(header[10] & 0x80U) {
                        gif.global_count = (uint16_t)(2U << (header[10] & 7U));
                        if(gif.background_index >= gif.global_count) gif_fail(FILE_IMAGE_CORRUPT);
                        else if(gif_palette(gif.work->global, gif.global_count))
                            gif.background = gif.work->global[gif.background_index];
                    }
                    gif.image_start = gif.position;
                    gif_control_reset();
                    if(gif.result == FILE_IMAGE_OK) return file_gif_next();
                }
            }
        }
    }
    gif_close_file();
    gif.finished = 1U;
    return gif.result;
}

void file_gif_close(void)
{
    gif_close_file();
    gif.finished = 1U;
    gif.work = 0;
    gif.canvas = gif.previous = 0;
}

uint32_t file_gif_delay_ms(void)
{
    return gif.delay_ms;
}

uint8_t file_gif_finished(void)
{
    return gif.finished;
}

uint8_t file_gif_is_open(void)
{
    return gif.opened;
}
