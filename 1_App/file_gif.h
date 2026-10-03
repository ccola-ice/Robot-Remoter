#ifndef FILE_GIF_H
#define FILE_GIF_H

#include <stdint.h>
#include "file_image.h"

/* 工作区在播放期间必须保持有效，且不能同时借给 PNG 或其它模块。
 * 先完整解码再提交 RGB565 画布；调用者负责 LCD 页缓冲的开始与提交。
 * 当前画布最大 320×200，较大源图等比例缩小，保留完整动画合成语义。
 * service 返回 0 时取消；任何错误都会关闭文件，保留屏幕上的上一帧。 */
FileImageResult file_gif_open(const char *path, uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h, uint16_t background,
                              FileImageInfo *info, void *scratch,
                              uint32_t capacity, uint8_t (*service)(void *),
                              void *context);
FileImageResult file_gif_next(void);
void file_gif_close(void);
uint32_t file_gif_delay_ms(void);
uint8_t file_gif_finished(void);
uint8_t file_gif_is_open(void);

#endif
