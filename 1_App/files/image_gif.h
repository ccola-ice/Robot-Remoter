#ifndef IMAGE_GIF_H
#define IMAGE_GIF_H

#include <stdint.h>
#include "image_viewer.h"

/* 工作区在播放期间必须保持有效，且不能同时借给 PNG 或其它模块。
 * 先完整解码再提交 RGB565 画布；调用者负责 LCD 页缓冲的开始与提交。
 * 当前画布最大 320×200，较大源图等比例缩小，保留完整动画合成语义。
 * service 返回 0 时取消；任何错误都会关闭文件，保留屏幕上的上一帧。
 * open 会关闭旧动画并立即解出首帧，path、info 与 scratch 均不可为空。
 * capacity 按字节计，内部处理起始地址对齐；background 是透明区域的屏幕背景色。 */
FileImageResult file_gif_open(const char *path, uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h, uint16_t background,
                              FileImageInfo *info, void *scratch,
                              uint32_t capacity, uint8_t (*service)(void *),
                              void *context);
/* 推进到下一帧，不负责等待帧间延时；正常结束仍返回 OK，需同时检查 finished。 */
FileImageResult file_gif_next(void);
/* 关闭文件并解除工作区引用后，调用者才可将该内存交给其他模块；可重复调用。 */
void file_gif_close(void);
/* 返回最近提交帧应停留的毫秒数，已将文件中的百分之一秒转换并限制最短为 20 ms。 */
uint32_t file_gif_delay_ms(void);
/* 播完、主动关闭或出错时返回非零；用于区分 next 的成功返回是否仍表示播放中。 */
uint8_t file_gif_finished(void);
/* 表示当前仍持有文件，可用于切换页面或复用外部 SRAM 前的资源清理。 */
uint8_t file_gif_is_open(void);

#endif
