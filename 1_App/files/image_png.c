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

/* 规范 Huffman 码表：按码长统计数量，并将符号按码长及原符号顺序紧凑排列。 */
typedef struct {
    uint16_t count[16];
    uint16_t symbol[288];
} PngHuffman;

/* 一次 PNG 解码的完整上下文，处理链为文件字节、PNG 块、DEFLATE、扫描行和像素。
 * 结构体后紧接两条原图行缓冲，共用调用方工作区；函数返回后不保留文件或指针引用。 */
typedef struct {
    FIL file;
    FileImageResult status;
    uint8_t (*service)(void *);
    void *context;
    uint8_t *row, *previous;
    /* 文件位置与当前块剩余长度分别计数，CRC 仅覆盖块类型和该块的数据。 */
    uint32_t size, position, remain, chunk_type, crc;
    uint32_t width, height, row_bytes, row_at, pass_width, pass_height, pass_row;
    /* 压缩流状态跨 IDAT 块延续；expected 限制展开总量，window_limit 限制回溯距离。 */
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

/* 保留最先发生的错误，后续清理或格式检查不能掩盖 I/O 失败与取消原因。 */
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

/* 最底层文件读取，只负责扇区缓存、边界及 I/O 状态，不修改块级 CRC 和剩余长度。 */
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

/* 读取块长度及四字节类型，预留尾部 CRC 空间并验证类型字符。
 * 成功后 remain 指向数据长度，crc 已包含类型字段，数据尚未消费。 */
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

/* 消费当前块的一字节有效载荷，同时推进剩余长度和 CRC；不能跨块读取。 */
static uint8_t png_data(Png *p, uint8_t *value)
{
    if(!p->remain) return png_fail(p, FILE_IMAGE_CORRUPT);
    if(!png_raw(p, value)) return 0U;
    p->remain--;
    p->crc = png_crc(p->crc, *value);
    return 1U;
}

/* 数据必须已恰好消费完，再读取并核对块尾 CRC；返回成功后才可读取下一块。 */
static uint8_t png_finish(Png *p)
{
    uint32_t expected;
    if(p->remain || !png_u32(p, &expected)) return png_fail(p, FILE_IMAGE_CORRUPT);
    if(expected != (p->crc ^ 0xFFFFFFFFUL)) return png_fail(p, FILE_IMAGE_CORRUPT);
    return 1U;
}

/* 即使忽略辅助块内容，也完整读取并校验 CRC，避免悄悄接受损坏的文件。 */
static uint8_t png_skip(Png *p)
{
    uint8_t value;
    while(p->remain) if(!png_data(p, &value)) return 0U;
    return png_finish(p);
}

/* 连续 IDAT 块拼成同一压缩流；跨块时校验各自 CRC，但保留 DEFLATE 位缓存。 */
static uint8_t png_compressed(Png *p, uint8_t *value)
{
    while(!p->remain) {
        if(!png_finish(p) || !png_header(p)) return 0U;
        if(p->chunk_type != PNG_IDAT) return png_fail(p, FILE_IMAGE_CORRUPT);
    }
    return png_data(p, value);
}

/* DEFLATE 按低位优先装入位缓存；只消费请求位数，其余位留给后续字段或码字。 */
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

/* 逐位扩展码字，利用各码长的首码与数量定位符号，不建立额外的指针树。
 * 超过最长 15 位仍无法匹配时，判定压缩流损坏。 */
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

/* 动态码表先解出码长表，再展开重复码长，最终构造字面量/长度与距离两棵树。 */
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

/* Adam7 将图像分成七个稀疏子图；跳过空子图，并清零每轮首行的上行预测数据。 */
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

/* Paeth 从左、上、左上三个已恢复字节中选择最接近 a+b-c 的预测值。 */
static uint8_t png_paeth(uint8_t a, uint8_t b, uint8_t c)
{
    int value = (int)a + b - c;
    int da = value - a, db = value - b, dc = value - c;
    if(da < 0) da = -da;
    if(db < 0) db = -db;
    if(dc < 0) dc = -dc;
    return da <= db && da <= dc ? a : (db <= dc ? b : c);
}

/* 按样本索引提取原始通道值：低位深样本从字节高位起打包，16 位样本使用大端序。 */
static uint16_t png_sample(const uint8_t *row, uint32_t index, uint8_t depth)
{
    uint32_t bit;
    if(depth == 16U) return (uint16_t)(((uint16_t)row[index * 2U] << 8) | row[index * 2U + 1U]);
    if(depth == 8U) return row[index];
    bit = index * depth;
    return (uint16_t)((row[bit / 8U] >> (8U - depth - bit % 8U)) & ((1U << depth) - 1U));
}

/* 将当前行的一个像素从灰度、索引色或 RGB(A) 统一转换为 RGB565。
 * 透明色比较使用未缩放的原始样本值，避免量化后错误地命中透明颜色。 */
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
    /* LCD 输出不带透明通道，统一合成到白底后再量化为 RGB565。 */
    r = (r * alpha + 255U * (255U - alpha) + 127U) / 255U;
    g = (g * alpha + 255U * (255U - alpha) + 127U) / 255U;
    b = (b * alpha + 255U * (255U - alpha) + 127U) / 255U;
    *pixel = (uint16_t)(((r & 248U) << 8) | ((g & 252U) << 3) | (b >> 3));
    return 1U;
}

/* 完成逆滤波后，将当前子图中命中缩放采样的像素输出到 LCD。
 * Adam7 子图的目标像素可能不连续，因此只将相邻像素合并为一次行块传输。 */
static uint8_t png_draw_row(Png *p)
{
    uint32_t at, sy, sx, target_y, left, above, diagonal;
    uint16_t x, run_start = 0U, run_length = 0U;
    uint8_t *swap;
    /* 逐字节逆滤波依赖已恢复的左侧像素和上一行，不能在缩放前跳过未显示的数据。 */
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
    /* 交换两行缓冲保存预测所需的上一行，无需复制整行或保留完整图像。 */
    swap = p->previous; p->previous = p->row; p->row = swap;
    p->row_at = 0U;
    if(++p->pass_row == p->pass_height) { p->pass++; (void)png_pass(p); }
    return png_service(p);
}

/* 接收一个 DEFLATE 输出字节，更新滑动字典和校验，再送入行过滤/显示阶段。
 * 每行第一个字节是滤波类型；超过预期展开长度时立即停止，避免写出行缓冲。 */
static uint8_t png_emit(Png *p, uint8_t value)
{
    if(p->produced >= p->expected) return png_fail(p, FILE_IMAGE_CORRUPT);
    /* 字典和 Adler 校验保留原始解压字节；扫描行缓冲随后才执行逆滤波。 */
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

/* 解析 zlib 封装，依次展开未压缩、固定 Huffman 或动态 Huffman 数据块。
 * 最后核对输出总量、Adler 校验和当前 IDAT 尾部，任一不符都不能算解码成功。 */
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
            /* 逐字节回拷允许源与目标重叠，新输出的字节可继续作为同一段的来源。 */
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

/* IHDR 决定色型、通道数和位深；据此划分两条原图行缓冲并检查工作区容量。
 * 随后计算等比显示区域及各隔行子图的展开字节总量，为流式解码建立边界。 */
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
    /* 每行另含一个滤波字节，预计算总输出量以拒绝截断或超出图像大小的压缩流。 */
    while(png_pass(p)) { p->expected += (p->row_bytes + 1U) * p->pass_height; p->pass++; }
    p->pass = 0U;
    (void)png_pass(p);
    return 1U;
}

/* 读取 PLTE 并默认设为不透明；索引色条目数必须适配位深，且不能在透明表之后重建。 */
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

/* tRNS 对索引色提供逐项 alpha，对灰度/RGB 提供一个完全透明的原始颜色键。
 * 自带 alpha 的色型无需且不允许再附加该块，长度和样本范围也必须匹配色型。 */
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

/* 验证签名后按 PNG 块顺序解码，强制 IHDR 在前、IDAT 连续、IEND 正确收尾。
 * 未识别的关键块不能忽略；所有已打开文件的退出路径统一关闭，并释放重入保护。 */
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
