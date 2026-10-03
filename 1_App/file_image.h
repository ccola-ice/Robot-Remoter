#ifndef FILE_IMAGE_H
#define FILE_IMAGE_H

#include <stdint.h>

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

typedef struct {
    uint32_t width;
    uint32_t height;
    uint16_t drawn_width;
    uint16_t drawn_height;
    uint8_t format;
} FileImageInfo;

/* 仅在外部 SRAM 自检通过后注册；工作区与 LCD 帧缓冲严格分离。
 * PNG 与 GIF 串行共用此区域，切换文件时必须先关闭旧 GIF。 */
void file_image_set_workspace(void *buffer, uint32_t capacity);
void *file_image_get_workspace(uint32_t *capacity);

/* 在给定区域内居中显示图片，仅缩小、不放大。内部使用固定缓冲，不可重入。
 * 支持未压缩的 24/32 位 BMP、baseline YCbCr JPEG 和 PNG。
 * service 可为空；返回 0 可取消，返回 1 继续。回调应维护遥控后台任务。
 * 调用方负责清除背景，以及管理 LCD 页缓冲的开始和提交。 */
FileImageResult file_image_draw(const char *path, uint16_t x, uint16_t y,
                               uint16_t w, uint16_t h, FileImageInfo *info,
                               uint8_t (*service)(void *), void *context);

#endif
