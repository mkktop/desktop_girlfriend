/**
 * @file gifdec.h
 * @brief 纯 C GIF89a 解码器
 * @note 基于 xiaozhi-esp32 项目的 gifdec 库，支持：
 *       - 全局/局部调色板
 *       - LZW 解压（两种实现：缓存表/动态表）
 *       - 交错图像
 *       - 图形控制扩展（透明、帧处理方式、延迟）
 *       - NETSCAPE 循环计数
 *       输出格式：ARGB8888（LV_COLOR_FORMAT_ARGB8888）
 */

#ifndef GIFDEC_H
#define GIFDEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdint.h>

typedef struct _gd_Palette {
    int size;
    uint8_t colors[0x100 * 3];
} gd_Palette;

typedef struct _gd_GCE {
    uint16_t delay;
    uint8_t tindex;
    uint8_t disposal;
    int input;
    int transparency;
} gd_GCE;

typedef struct _gd_GIF {
    lv_fs_file_t fd;
    const char * data;
    uint8_t is_file;
    uint32_t f_rw_p;
    int32_t anim_start;
    uint16_t width, height;
    uint16_t depth;
    int32_t loop_count;
    gd_GCE gce;
    gd_Palette * palette;
    gd_Palette lct, gct;
    void (*plain_text)(
        struct _gd_GIF * gif, uint16_t tx, uint16_t ty,
        uint16_t tw, uint16_t th, uint8_t cw, uint8_t ch,
        uint8_t fg, uint8_t bg
    );
    void (*comment)(struct _gd_GIF * gif);
    void (*application)(struct _gd_GIF * gif, char id[8], char auth[3]);
    uint16_t fx, fy, fw, fh;
    uint8_t bgindex;
    uint8_t * canvas, * frame;
#if LV_GIF_CACHE_DECODE_DATA
    uint8_t *lzw_cache;
#endif
} gd_GIF;

/**
 * @brief 从文件系统打开 GIF（通过 LVGL fs 抽象）
 * @param fname 文件路径
 * @return gd_GIF 指针，失败返回 NULL
 */
gd_GIF * gd_open_gif_file(const char * fname);

/**
 * @brief 从内存数据打开 GIF
 * @param data GIF 文件原始数据指针（需保持有效直到 gd_close_gif）
 * @return gd_GIF 指针，失败返回 NULL
 */
gd_GIF * gd_open_gif_data(const void * data);

/**
 * @brief 渲染当前帧到指定缓冲区
 * @param gif GIF 上下文
 * @param buffer ARGB8888 输出缓冲区（width * height * 4 字节）
 */
void gd_render_frame(gd_GIF * gif, uint8_t * buffer);

/**
 * @brief 推进到下一帧
 * @return 1=成功获取帧，0=动画结束（GIF trailer），-1=错误
 */
int gd_get_frame(gd_GIF * gif);

/**
 * @brief 重绕到动画起始位置
 */
void gd_rewind(gd_GIF * gif);

/**
 * @brief 关闭 GIF 并释放所有资源
 */
void gd_close_gif(gd_GIF * gif);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GIFDEC_H */
