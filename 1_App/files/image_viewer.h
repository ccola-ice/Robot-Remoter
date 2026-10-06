#ifndef IMAGE_VIEWER_H
#define IMAGE_VIEWER_H

#include <stdint.h>

/* 各图片解码器共用结果码：不支持的格式、损坏的数据、读盘错误和容量不足分别报告。 */
typedef enum {
    FILE_IMAGE_OK = 0,
    FILE_IMAGE_UNSUPPORTED,
    FILE_IMAGE_CORRUPT,
    FILE_IMAGE_IO,
    FILE_IMAGE_TOO_LARGE,
    FILE_IMAGE_CANCELLED,
    FILE_IMAGE_NO_MEMORY
} FileImageResult;

enum { FILE_IMAGE_BMP = 1, FILE_IMAGE_JPEG = 2, FILE_IMAGE_PNG = 3, FILE_IMAGE_GIF = 4 };

/* 原图尺寸与实际绘制尺寸分开报告；format 使用 FILE_IMAGE_* 格式常量。
 * 解码失败时字段可能只填充一部分，应先判断操作结果再用于后续显示。 */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint16_t drawn_width;
    uint16_t drawn_height;
    uint8_t format;
} FileImageInfo;

/* 仅在外部 SRAM 自检通过后注册；工作区与 LCD 帧缓冲严格分离。
 * PNG 与 GIF 串行共用此区域，切换文件时必须先关闭旧 GIF。
 * capacity 以字节计；传入空指针可注销，本接口不会分配或释放该内存。 */
void file_image_set_workspace(void *buffer, uint32_t capacity);
/* 获取已注册的借用地址；capacity 可为空，非空时同时返回可用字节数。 */
void *file_image_get_workspace(uint32_t *capacity);

/* 在给定区域内居中显示图片，仅缩小、不放大。内部使用固定缓冲，不可重入。
 * 支持未压缩的 24/32 位 BMP、baseline YCbCr JPEG 和 PNG。
 * service 可为空；返回 0 可取消，返回 1 继续。回调应维护遥控后台任务。
 * 调用方负责清除背景，以及管理 LCD 页缓冲的开始和提交。
 * x/y/w/h 指定 LCD 内的有效目标区域，info 可为空；图片按文件头而非扩展名识别。
 * 取消或损坏可能发生在部分像素已绘制之后，上层应据返回值决定是否提交页面。 */
FileImageResult file_image_draw(const char *path, uint16_t x, uint16_t y,
                               uint16_t w, uint16_t h, FileImageInfo *info,
                               uint8_t (*service)(void *), void *context);

#endif
