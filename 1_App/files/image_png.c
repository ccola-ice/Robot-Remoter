#include "image_png.h"
#include "ff.h"
#include "bsp_fsmc_lcd.h"
#include <string.h>

/* PNG/W3C、RFC 1950/1951：有界流式解码，无堆分配，也不保存完整原图。
 * 规范：https://www.w3.org/TR/png-3/  https://www.rfc-editor.org/rfc/rfc1951
 * 文件、字典、扫描行和调色板均位于外部 SRAM，避免占用 MCU 内部 RAM。 */
#define PNG_MAX_SIDE 8192UL
#define PNG_MAX_PIXELS (16UL * 1024UL * 1024UL)
#define PNG_MAX_BYTES (32UL * 1024UL * 1024UL)
#define PNG_WINDOW 32768U
#define PNG_LINE_PIXELS 800U
#define PNG_IHDR 0x49484452UL
#define PNG_PLTE 0x504C5445UL
#define PNG_TRNS 0x74524E53UL
#define PNG_IDAT 0x49444154UL
#define PNG_IEND 0x49454E44UL

typedef struct {
    uint16_t count[16];
    uint16_t symbol[288];
} PngHuffman;

typedef struct {
    FIL file;
    FileImageResult status;
    uint8_t (*service)(void *);
    void *context;
    uint8_t *row, *previous;
    uint32_t size, position, remain, chunk_type, crc;
    uint32_t width, height, row_bytes, row_at, pass_width, pass_height, pass_row;
    uint32_t bit_buffer, produced, adler_a, adler_b, expected, window_limit;
    uint16_t input_at, input_count, x, y, draw_width, draw_height;
    uint16_t palette_count, transparent[3];
    uint8_t depth, color, channels, pixel_bytes, interlace, pass;
    uint8_t pass_x, pass_y, step_x, step_y, filter, bit_count, has_transparency;
    uint8_t input[512];
    uint8_t palette[256][4];
    uint8_t lengths[320];
    PngHuffman literal, distance, codes;
    uint16_t pixels[PNG_LINE_PIXELS];
    uint8_t dictionary[PNG_WINDOW];
} Png;

static uint8_t png_busy;

static uint8_t png_fail(Png *p, FileImageResult status)
{
    if(p->status == FILE_IMAGE_OK) p->status = status;
    return 0U;
}

static uint8_t png_service(Png *p)
{
    if(p->status != FILE_IMAGE_OK) return 0U;
    if(p->service && !p->service(p->context)) return png_fail(p, FILE_IMAGE_CANCELLED);
    return 1U;
}

static uint8_t png_raw(Png *p, uint8_t *value)
{
    UINT count = 0U, request;
    if(p->status != FILE_IMAGE_OK) return 0U;
    if(p->position >= p->size) return png_fail(p, FILE_IMAGE_CORRUPT);
    if(p->input_at == p->input_count) {
        if(!png_service(p)) return 0U;
        request = p->size - p->position > sizeof(p->input) ? sizeof(p->input) :
                  (UINT)(p->size - p->position);
        if(f_read(&p->file, p->input, request, &count) != FR_OK)
            return png_fail(p, FILE_IMAGE_IO);
        if(count != request) return png_fail(p, FILE_IMAGE_CORRUPT);
        p->input_at = 0U;
        p->input_count = (uint16_t)count;
    }
    *value = p->input[p->input_at++];
    p->position++;
    return 1U;
}

static uint32_t png_crc(uint32_t crc, uint8_t value)
{
    uint8_t i;
    crc ^= value;
    for(i = 0U; i < 8U; i++) crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320UL : 0U);
    return crc;
}

static uint8_t png_u32(Png *p, uint32_t *value)
{
    uint8_t i, byte;
    *value = 0U;
    for(i = 0U; i < 4U; i++) {
        if(!png_raw(p, &byte)) return 0U;
        *value = (*value << 8) | byte;
    }
    return 1U;
}

static uint8_t png_header(Png *p)
{
    uint8_t i, value;
    if(!png_u32(p, &p->remain)) return 0U;
    if(p->size - p->position < 8U || p->remain > p->size - p->position - 8U)
        return png_fail(p, FILE_IMAGE_CORRUPT);
    p->crc = 0xFFFFFFFFUL;
    p->chunk_type = 0U;
    for(i = 0U; i < 4U; i++) {
        if(!png_raw(p, &value)) return 0U;
        if(!((value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z')) ||
           (i == 2U && (value & 32U))) return png_fail(p, FILE_IMAGE_CORRUPT);
        p->crc = png_crc(p->crc, value);
        p->chunk_type = (p->chunk_type << 8) | value;
    }
    return 1U;
}

static uint8_t png_data(Png *p, uint8_t *value)
{
    if(!p->remain) return png_fail(p, FILE_IMAGE_CORRUPT);
    if(!png_raw(p, value)) return 0U;
    p->remain--;
    p->crc = png_crc(p->crc, *value);
    return 1U;
}

static uint8_t png_finish(Png *p)
{
    uint32_t expected;
    if(p->remain || !png_u32(p, &expected)) return png_fail(p, FILE_IMAGE_CORRUPT);
    if(expected != (p->crc ^ 0xFFFFFFFFUL)) return png_fail(p, FILE_IMAGE_CORRUPT);
    return 1U;
}

static uint8_t png_skip(Png *p)
{
    uint8_t value;
    while(p->remain) if(!png_data(p, &value)) return 0U;
    return png_finish(p);
}

static uint8_t png_compressed(Png *p, uint8_t *value)
{
    while(!p->remain) {
        if(!png_finish(p) || !png_header(p)) return 0U;
        if(p->chunk_type != PNG_IDAT) return png_fail(p, FILE_IMAGE_CORRUPT);
    }
    return png_data(p, value);
}

static uint8_t png_bits(Png *p, uint8_t count, uint32_t *value)
{
    uint8_t byte;
    while(p->bit_count < count) {
        if(!png_compressed(p, &byte)) return 0U;
        p->bit_buffer |= (uint32_t)byte << p->bit_count;
        p->bit_count = (uint8_t)(p->bit_count + 8U);
    }
    *value = p->bit_buffer & ((1UL << count) - 1U);
    p->bit_buffer >>= count;
    p->bit_count = (uint8_t)(p->bit_count - count);
    return 1U;
}

/* 规范码表仅存码长计数及按码长排列的符号；拒绝超额和无效的不完整树。 */
static uint8_t png_tree(Png *p, PngHuffman *tree, const uint8_t *lengths,
                        uint16_t count, uint8_t allow_empty, uint8_t allow_single)
{
    uint16_t offset[16], i, used = 0U;
    int32_t left = 1;
    memset(tree->count, 0, sizeof(tree->count));
    for(i = 0U; i < count; i++) {
        if(lengths[i] > 15U) return png_fail(p, FILE_IMAGE_CORRUPT);
        tree->count[lengths[i]]++;
        if(lengths[i]) used++;
    }
    if(!used) return allow_empty ? 1U : png_fail(p, FILE_IMAGE_CORRUPT);
    for(i = 1U; i <= 15U; i++) {
        left = left * 2 - tree->count[i];
        if(left < 0) return png_fail(p, FILE_IMAGE_CORRUPT);
    }
    if(left && !(allow_single && used == 1U && tree->count[1] == 1U))
        return png_fail(p, FILE_IMAGE_CORRUPT);
    offset[1] = 0U;
    for(i = 1U; i < 15U; i++) offset[i + 1U] = offset[i] + tree->count[i];
    for(i = 0U; i < count; i++) if(lengths[i]) tree->symbol[offset[lengths[i]]++] = i;
    return 1U;
}

static uint8_t png_symbol(Png *p, const PngHuffman *tree, uint16_t *symbol)
{
    uint32_t bit, code = 0U, first = 0U, at = 0U;
    uint8_t length;
    for(length = 1U; length <= 15U; length++) {
        if(!png_bits(p, 1U, &bit)) return 0U;
        code |= bit;
        if(code >= first && code - first < tree->count[length]) {
            *symbol = tree->symbol[at + code - first];
            return 1U;
        }
        at += tree->count[length];
        first = (first + tree->count[length]) << 1;
        code <<= 1;
    }
    return png_fail(p, FILE_IMAGE_CORRUPT);
}

static uint8_t png_dynamic(Png *p)
{
    static const uint8_t order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    uint8_t code_lengths[19];
    uint32_t value, literal_count, distance_count, code_count, total, at, repeat;
    uint16_t symbol;
    if(!png_bits(p, 5U, &value)) return 0U;
    literal_count = value + 257U;
    if(literal_count > 286U) return png_fail(p, FILE_IMAGE_CORRUPT);
    if(!png_bits(p, 5U, &value)) return 0U;
    distance_count = value + 1U;
    if(!png_bits(p, 4U, &value)) return 0U;
    code_count = value + 4U;
    memset(code_lengths, 0, sizeof(code_lengths));
    for(at = 0U; at < code_count; at++) {
        if(!png_bits(p, 3U, &value)) return 0U;
        code_lengths[order[at]] = (uint8_t)value;
    }
    if(!png_tree(p, &p->codes, code_lengths, 19U, 0U, 0U)) return 0U;
    total = literal_count + distance_count;
    at = 0U;
    while(at < total) {
        if(!png_symbol(p, &p->codes, &symbol)) return 0U;
        if(symbol < 16U) p->lengths[at++] = (uint8_t)symbol;
        else {
            if(symbol == 16U) {
                if(!at || !png_bits(p, 2U, &value)) return png_fail(p, FILE_IMAGE_CORRUPT);
                repeat = value + 3U;
                value = p->lengths[at - 1U];
            } else {
                if(!png_bits(p, symbol == 17U ? 3U : 7U, &value)) return 0U;
                repeat = value + (symbol == 17U ? 3U : 11U);
                value = 0U;
            }
            if(repeat > total - at) return png_fail(p, FILE_IMAGE_CORRUPT);
            while(repeat--) p->lengths[at++] = (uint8_t)value;
        }
    }
    if(!p->lengths[256]) return png_fail(p, FILE_IMAGE_CORRUPT);
    return png_tree(p, &p->literal, p->lengths, (uint16_t)literal_count, 0U, 1U) &&
           png_tree(p, &p->distance, p->lengths + literal_count, (uint16_t)distance_count, 1U, 1U);
}

static uint8_t png_pass(Png *p)
{
    static const uint8_t start_x[7] = {0,4,0,2,0,1,0};
    static const uint8_t start_y[7] = {0,0,4,0,2,0,1};
    static const uint8_t delta_x[7] = {8,8,4,4,2,2,1};
    static const uint8_t delta_y[7] = {8,8,8,4,4,2,2};
    while(p->pass < (p->interlace ? 7U : 1U)) {
        p->pass_x = p->interlace ? start_x[p->pass] : 0U;
        p->pass_y = p->interlace ? start_y[p->pass] : 0U;
        p->step_x = p->interlace ? delta_x[p->pass] : 1U;
        p->step_y = p->interlace ? delta_y[p->pass] : 1U;
        p->pass_width = p->width <= p->pass_x ? 0U :
                        (p->width - p->pass_x + p->step_x - 1U) / p->step_x;
        p->pass_height = p->height <= p->pass_y ? 0U :
                         (p->height - p->pass_y + p->step_y - 1U) / p->step_y;
        if(p->pass_width && p->pass_height) {
            p->row_bytes = (p->pass_width * p->channels * p->depth + 7U) / 8U;
            p->pass_row = p->row_at = 0U;
            memset(p->previous, 0, p->row_bytes);
            return 1U;
        }
        p->pass++;
    }
    return 0U;
}

static uint8_t png_paeth(uint8_t a, uint8_t b, uint8_t c)
{
    int value = (int)a + b - c;
    int da = value - a, db = value - b, dc = value - c;
    if(da < 0) da = -da;
    if(db < 0) db = -db;
    if(dc < 0) dc = -dc;
    return da <= db && da <= dc ? a : (db <= dc ? b : c);
}

static uint16_t png_sample(const uint8_t *row, uint32_t index, uint8_t depth)
{
    uint32_t bit;
    if(depth == 16U) return (uint16_t)(((uint16_t)row[index * 2U] << 8) | row[index * 2U + 1U]);
    if(depth == 8U) return row[index];
    bit = index * depth;
    return (uint16_t)((row[bit / 8U] >> (8U - depth - bit % 8U)) & ((1U << depth) - 1U));
}

static uint8_t png_pixel(Png *p, uint32_t index, uint16_t *pixel)
{
    uint16_t raw[4];
    uint32_t r, g, b, alpha = 255U, i, max_value;
    for(i = 0U; i < p->channels; i++) raw[i] = png_sample(p->row, index * p->channels + i, p->depth);
    if(p->color == 3U) {
        if(raw[0] >= p->palette_count) return png_fail(p, FILE_IMAGE_CORRUPT);
        r = p->palette[raw[0]][0]; g = p->palette[raw[0]][1]; b = p->palette[raw[0]][2];
        alpha = p->palette[raw[0]][3];
    } else {
        max_value = p->depth == 16U ? 65535U : (1U << p->depth) - 1U;
        r = raw[0] * 255UL / max_value;
        if(p->color == 0U || p->color == 4U) {
            g = b = r;
            if(p->color == 4U) alpha = raw[1] * 255UL / max_value;
            else if(p->has_transparency && raw[0] == p->transparent[0]) alpha = 0U;
        } else {
            g = raw[1] * 255UL / max_value; b = raw[2] * 255UL / max_value;
            if(p->color == 6U) alpha = raw[3] * 255UL / max_value;
            else if(p->has_transparency && raw[0] == p->transparent[0] &&
                    raw[1] == p->transparent[1] && raw[2] == p->transparent[2]) alpha = 0U;
        }
    }
    r = (r * alpha + 255U * (255U - alpha) + 127U) / 255U;
    g = (g * alpha + 255U * (255U - alpha) + 127U) / 255U;
    b = (b * alpha + 255U * (255U - alpha) + 127U) / 255U;
    *pixel = (uint16_t)(((r & 248U) << 8) | ((g & 252U) << 3) | (b >> 3));
    return 1U;
}

static uint8_t png_draw_row(Png *p)
{
    uint32_t at, sy, sx, target_y, left, above, diagonal;
    uint16_t x, run_start = 0U, run_length = 0U;
    uint8_t *swap;
    for(at = 0U; at < p->row_bytes; at++) {
        left = at < p->pixel_bytes ? 0U : p->row[at - p->pixel_bytes];
        above = p->previous[at];
        diagonal = at < p->pixel_bytes ? 0U : p->previous[at - p->pixel_bytes];
        switch(p->filter) {
        case 1U: p->row[at] = (uint8_t)(p->row[at] + left); break;
        case 2U: p->row[at] = (uint8_t)(p->row[at] + above); break;
        case 3U: p->row[at] = (uint8_t)(p->row[at] + (left + above) / 2U); break;
        case 4U: p->row[at] = (uint8_t)(p->row[at] + png_paeth((uint8_t)left, (uint8_t)above, (uint8_t)diagonal)); break;
        default: break;
        }
    }
    /* 即便缩放跳过像素，也校验索引范围，避免接受已损坏的调色板图像。 */
    if(p->color == 3U) for(at = 0U; at < p->pass_width; at++)
        if(png_sample(p->row, at, p->depth) >= p->palette_count) return png_fail(p, FILE_IMAGE_CORRUPT);
    sy = p->pass_y + p->pass_row * p->step_y;
    target_y = (sy * p->draw_height + p->height - 1U) / p->height;
    if(target_y < p->draw_height && target_y * p->height / p->draw_height == sy) {
        for(x = 0U; x < p->draw_width; x++) {
            sx = (uint32_t)x * p->width / p->draw_width;
            if(sx >= p->pass_x && (sx - p->pass_x) % p->step_x == 0U) {
                if(!run_length) run_start = x;
                if(!png_pixel(p, (sx - p->pass_x) / p->step_x, &p->pixels[run_length++])) return 0U;
            } else if(run_length) {
                LCD_BlitRGB565((uint16_t)(p->x + run_start), (uint16_t)(p->y + target_y), run_length, 1U, p->pixels);
                run_length = 0U;
            }
        }
        if(run_length) LCD_BlitRGB565((uint16_t)(p->x + run_start), (uint16_t)(p->y + target_y), run_length, 1U, p->pixels);
    }
    swap = p->previous; p->previous = p->row; p->row = swap;
    p->row_at = 0U;
    if(++p->pass_row == p->pass_height) { p->pass++; (void)png_pass(p); }
    return png_service(p);
}

static uint8_t png_emit(Png *p, uint8_t value)
{
    if(p->produced >= p->expected) return png_fail(p, FILE_IMAGE_CORRUPT);
    p->dictionary[p->produced & (PNG_WINDOW - 1U)] = value;
    p->produced++;
    p->adler_a += value;
    if(p->adler_a >= 65521U) p->adler_a -= 65521U;
    p->adler_b += p->adler_a;
    if(p->adler_b >= 65521U) p->adler_b -= 65521U;
    if(!p->row_at) {
        if(value > 4U) return png_fail(p, FILE_IMAGE_CORRUPT);
        p->filter = value;
    } else p->row[p->row_at - 1U] = value;
    if(++p->row_at == p->row_bytes + 1U && !png_draw_row(p)) return 0U;
    return (p->produced & 1023U) || png_service(p);
}

static uint8_t png_inflate(Png *p)
{
    static const uint16_t length_base[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const uint8_t length_bits[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const uint16_t distance_base[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const uint8_t distance_bits[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    uint32_t final, kind, value, length, distance, complement, checksum = 0U;
    uint16_t symbol, i;
    uint8_t cmf, flg, byte;
    if(!png_compressed(p, &cmf) || !png_compressed(p, &flg)) return 0U;
    if((cmf & 15U) != 8U || (cmf >> 4) > 7U ||
       (((uint16_t)cmf << 8) | flg) % 31U || (flg & 32U)) return png_fail(p, FILE_IMAGE_CORRUPT);
    p->window_limit = 1UL << ((cmf >> 4) + 8U);
    p->adler_a = 1U;
    do {
        if(!png_bits(p, 1U, &final) || !png_bits(p, 2U, &kind)) return 0U;
        if(kind == 0U) {
            p->bit_buffer = p->bit_count = 0U;
            if(!png_bits(p, 16U, &length) || !png_bits(p, 16U, &complement)) return 0U;
            if((length ^ complement) != 65535U) return png_fail(p, FILE_IMAGE_CORRUPT);
            while(length--) {
                if(!png_compressed(p, &byte) || !png_emit(p, byte)) return 0U;
            }
            continue;
        }
        if(kind == 3U) return png_fail(p, FILE_IMAGE_CORRUPT);
        if(kind == 1U) {
            for(i = 0U; i < 288U; i++) p->lengths[i] = i < 144U ? 8U : (i < 256U ? 9U : (i < 280U ? 7U : 8U));
            if(!png_tree(p, &p->literal, p->lengths, 288U, 0U, 0U)) return 0U;
            memset(p->lengths, 5, 32U);
            if(!png_tree(p, &p->distance, p->lengths, 32U, 0U, 0U)) return 0U;
        } else if(!png_dynamic(p)) return 0U;
        for(;;) {
            if(!png_symbol(p, &p->literal, &symbol)) return 0U;
            if(symbol < 256U) { if(!png_emit(p, (uint8_t)symbol)) return 0U; continue; }
            if(symbol == 256U) break;
            if(symbol > 285U) return png_fail(p, FILE_IMAGE_CORRUPT);
            symbol -= 257U;
            if(!png_bits(p, length_bits[symbol], &value)) return 0U;
            length = length_base[symbol] + value;
            if(!png_symbol(p, &p->distance, &symbol)) return 0U;
            if(symbol > 29U || !png_bits(p, distance_bits[symbol], &value)) return png_fail(p, FILE_IMAGE_CORRUPT);
            distance = distance_base[symbol] + value;
            if(distance > p->window_limit || distance > p->produced) return png_fail(p, FILE_IMAGE_CORRUPT);
            while(length--) if(!png_emit(p, p->dictionary[(p->produced - distance) & (PNG_WINDOW - 1U)])) return 0U;
        }
    } while(!final);
    if(p->produced != p->expected) return png_fail(p, FILE_IMAGE_CORRUPT);
    p->bit_buffer = p->bit_count = 0U;
    for(i = 0U; i < 4U; i++) {
        if(!png_compressed(p, &byte)) return 0U;
        checksum = (checksum << 8) | byte;
    }
    if(checksum != ((p->adler_b << 16) | p->adler_a)) return png_fail(p, FILE_IMAGE_CORRUPT);
    if(p->remain || !png_finish(p)) return png_fail(p, FILE_IMAGE_CORRUPT);
    return 1U;
}

static uint32_t png_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3];
}

static uint8_t png_info(Png *p, FileImageInfo *info, uint32_t capacity)
{
    uint8_t header[13], i;
    uint32_t row_capacity, dw, dh;
    if(p->chunk_type != PNG_IHDR || p->remain != 13U) return png_fail(p, FILE_IMAGE_CORRUPT);
    for(i = 0U; i < 13U; i++) if(!png_data(p, header + i)) return 0U;
    if(!png_finish(p)) return 0U;
    p->width = info->width = png_be32(header); p->height = info->height = png_be32(header + 4);
    if(!p->width || !p->height) return png_fail(p, FILE_IMAGE_CORRUPT);
    if(p->width > PNG_MAX_SIDE || p->height > PNG_MAX_SIDE || p->width * p->height > PNG_MAX_PIXELS)
        return png_fail(p, FILE_IMAGE_TOO_LARGE);
    p->depth = header[8]; p->color = header[9]; p->interlace = header[12];
    if(header[10] || header[11] || p->interlace > 1U) return png_fail(p, FILE_IMAGE_UNSUPPORTED);
    if(p->color == 0U || p->color == 3U) {
        if(p->depth != 1U && p->depth != 2U && p->depth != 4U && p->depth != 8U &&
           !(p->color == 0U && p->depth == 16U)) return png_fail(p, FILE_IMAGE_CORRUPT);
        p->channels = 1U;
    } else if(p->color == 2U || p->color == 4U || p->color == 6U) {
        if(p->depth != 8U && p->depth != 16U) return png_fail(p, FILE_IMAGE_CORRUPT);
        p->channels = p->color == 2U ? 3U : (p->color == 4U ? 2U : 4U);
    } else return png_fail(p, FILE_IMAGE_CORRUPT);
    row_capacity = (p->width * p->channels * p->depth + 7U) / 8U;
    if(capacity < sizeof(Png) || row_capacity > (capacity - sizeof(Png)) / 2U)
        return png_fail(p, FILE_IMAGE_NO_MEMORY);
    p->row = (uint8_t *)(p + 1); p->previous = p->row + row_capacity;
    p->pixel_bytes = (uint8_t)((p->channels * p->depth + 7U) / 8U);
    dw = p->width; dh = p->height;
    if(dw > p->draw_width || dh > p->draw_height) {
        if(p->width * p->draw_height > p->height * p->draw_width) {
            dw = p->draw_width; dh = p->height * dw / p->width;
        } else { dh = p->draw_height; dw = p->width * dh / p->height; }
    }
    if(!dw) dw = 1U;
    if(!dh) dh = 1U;
    p->x += (uint16_t)((p->draw_width - dw) / 2U); p->y += (uint16_t)((p->draw_height - dh) / 2U);
    p->draw_width = info->drawn_width = (uint16_t)dw; p->draw_height = info->drawn_height = (uint16_t)dh;
    while(png_pass(p)) { p->expected += (p->row_bytes + 1U) * p->pass_height; p->pass++; }
    p->pass = 0U;
    (void)png_pass(p);
    return 1U;
}

static uint8_t png_palette(Png *p)
{
    uint16_t i;
    uint8_t j;
    if(p->palette_count || p->has_transparency || !p->remain || p->remain % 3U || p->remain > 768U ||
       p->color == 0U || p->color == 4U) return png_fail(p, FILE_IMAGE_CORRUPT);
    p->palette_count = (uint16_t)(p->remain / 3U);
    if(p->color == 3U && p->palette_count > (1U << p->depth)) return png_fail(p, FILE_IMAGE_CORRUPT);
    for(i = 0U; i < p->palette_count; i++) {
        for(j = 0U; j < 3U; j++) if(!png_data(p, &p->palette[i][j])) return 0U;
        p->palette[i][3] = 255U;
    }
    return png_finish(p);
}

static uint8_t png_transparency(Png *p)
{
    uint16_t i, count;
    uint8_t hi, lo;
    if(p->has_transparency) return png_fail(p, FILE_IMAGE_CORRUPT);
    p->has_transparency = 1U;
    if(p->color == 3U) {
        if(!p->palette_count || !p->remain || p->remain > p->palette_count) return png_fail(p, FILE_IMAGE_CORRUPT);
        count = (uint16_t)p->remain;
        for(i = 0U; i < count; i++) if(!png_data(p, &p->palette[i][3])) return 0U;
    } else {
        if((p->color != 0U && p->color != 2U) || p->remain != (p->color == 0U ? 2U : 6U))
            return png_fail(p, FILE_IMAGE_CORRUPT);
        count = p->color == 0U ? 1U : 3U;
        for(i = 0U; i < count; i++) {
            if(!png_data(p, &hi) || !png_data(p, &lo)) return 0U;
            p->transparent[i] = (uint16_t)(((uint16_t)hi << 8) | lo);
            if(p->depth < 16U && p->transparent[i] >= (1U << p->depth)) return png_fail(p, FILE_IMAGE_CORRUPT);
        }
    }
    return png_finish(p);
}

FileImageResult file_png_draw(const char *path, uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h, FileImageInfo *info,
                              uint8_t (*service)(void *), void *context,
                              void *scratch, uint32_t capacity)
{
    static const uint8_t signature[8] = {137,80,78,71,13,10,26,10};
    Png *p = (Png *)scratch;
    FileImageResult result;
    uint8_t i, value, decoded = 0U, after_idat = 0U, ended = 0U;
    if(!path || !info || !w || !h || w > PNG_LINE_PIXELS ||
       (uint32_t)x + w > LCD_X_LENGTH || (uint32_t)y + h > LCD_Y_LENGTH || png_busy)
        return FILE_IMAGE_UNSUPPORTED;
    memset(info, 0, sizeof(*info));
    if(!scratch || ((uintptr_t)scratch % sizeof(void *)) || capacity < sizeof(Png)) return FILE_IMAGE_NO_MEMORY;
    png_busy = 1U;
    memset(p, 0, sizeof(*p));
    p->service = service; p->context = context;
    p->x = x; p->y = y; p->draw_width = w; p->draw_height = h;
    if(!png_service(p)) goto unopened;
    if(f_open(&p->file, path, FA_READ | FA_OPEN_EXISTING) != FR_OK) {
        p->status = FILE_IMAGE_IO; goto unopened;
    }
    p->size = f_size(&p->file);
    if(p->size > PNG_MAX_BYTES) { p->status = FILE_IMAGE_TOO_LARGE; goto close; }
    for(i = 0U; i < 8U; i++) {
        if(!png_raw(p, &value)) goto close;
        if(value != signature[i]) { p->status = FILE_IMAGE_UNSUPPORTED; goto close; }
    }
    info->format = FILE_IMAGE_PNG;
    if(!png_header(p) || !png_info(p, info, capacity)) goto close;
    while(png_header(p)) {
        if(p->chunk_type == PNG_IDAT) {
            if(after_idat || (p->color == 3U && !p->palette_count)) { png_fail(p, FILE_IMAGE_CORRUPT); break; }
            if(decoded) { if(p->remain || !png_finish(p)) { png_fail(p, FILE_IMAGE_CORRUPT); break; } }
            else { if(!png_inflate(p)) break; decoded = 1U; }
        } else {
            if(decoded) after_idat = 1U;
            if(p->chunk_type == PNG_IEND) {
                if(!decoded || p->remain || !png_finish(p)) { png_fail(p, FILE_IMAGE_CORRUPT); break; }
                ended = 1U; break;
            } else if(p->chunk_type == PNG_PLTE) {
                if(decoded || !png_palette(p)) { png_fail(p, FILE_IMAGE_CORRUPT); break; }
            } else if(p->chunk_type == PNG_TRNS) {
                if(decoded || !png_transparency(p)) { png_fail(p, FILE_IMAGE_CORRUPT); break; }
            } else if(p->chunk_type == PNG_IHDR) { png_fail(p, FILE_IMAGE_CORRUPT); break; }
            else if(!(p->chunk_type & 0x20000000UL)) { png_fail(p, FILE_IMAGE_UNSUPPORTED); break; }
            else if(!png_skip(p)) break;
        }
    }
    if(!ended || p->position != p->size) png_fail(p, FILE_IMAGE_CORRUPT);
close:
    if(f_close(&p->file) != FR_OK && p->status == FILE_IMAGE_OK) p->status = FILE_IMAGE_IO;
unopened:
    result = p->status;
    png_busy = 0U;
    return result;
}
