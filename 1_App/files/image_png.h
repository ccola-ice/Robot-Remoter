#ifndef IMAGE_PNG_H
#define IMAGE_PNG_H

#include "image_viewer.h"

/* 使用调用方提供的外部 SRAM；支持 PNG 全部标准色型、位深及 Adam7 隔行。
 * 透明像素合成到白底，图片仅缩小；工作区须至少 4 字节对齐，不可重入。 */
FileImageResult file_png_draw(const char *path, uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h, FileImageInfo *info,
                              uint8_t (*service)(void *), void *context,
                              void *scratch, uint32_t capacity);

#endif
