#ifndef IMAGE_PNG_H
#define IMAGE_PNG_H

#include "image_viewer.h"

/* 使用调用方提供的外部 SRAM；支持 PNG 全部标准色型、位深及 Adam7 隔行。
 * 透明像素合成到白底，图片仅缩小；工作区须至少 4 字节对齐，不可重入。
 * path、info、scratch 必须有效，capacity 以字节计并需容纳字典及两条原图扫描行；
 * 因而即使显示区域很小，较宽的源图仍可能返回 FILE_IMAGE_NO_MEMORY。
 * service 可为空，返回零可取消；工作区只在本次调用期间借用，返回时文件已关闭。
 * 解码按行更新 LCD，调用方负责页缓冲及出错后的显示处理。 */
FileImageResult file_png_draw(const char *path, uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h, FileImageInfo *info,
                              uint8_t (*service)(void *), void *context,
                              void *scratch, uint32_t capacity);

#endif
