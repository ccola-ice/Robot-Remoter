#ifndef FILE_IMAGE_H
#define FILE_IMAGE_H

#include <stdint.h>

typedef enum {
    FILE_IMAGE_OK = 0,
    FILE_IMAGE_UNSUPPORTED,
    FILE_IMAGE_CORRUPT,
    FILE_IMAGE_IO,
    FILE_IMAGE_TOO_LARGE,
    FILE_IMAGE_CANCELLED
} FileImageResult;

enum { FILE_IMAGE_BMP = 1, FILE_IMAGE_JPEG = 2 };

typedef struct {
    uint32_t width;
    uint32_t height;
    uint16_t drawn_width;
    uint16_t drawn_height;
    uint8_t format;
} FileImageInfo;

/* 在给定区域内居中显示图片，仅缩小、不放大。内部使用固定缓冲，不可重入。
 * 支持未压缩的 24/32 位 BMP 和 baseline YCbCr JPEG；其余格式返回不支持。
 * service 可为空；返回 0 可取消，返回 1 继续。回调应维护遥控后台任务。
 * 调用方负责清除背景，以及管理 LCD 页缓冲的开始和提交。 */
FileImageResult file_image_draw(const char *path, uint16_t x, uint16_t y,
                               uint16_t w, uint16_t h, FileImageInfo *info,
                               uint8_t (*service)(void *), void *context);

#endif
