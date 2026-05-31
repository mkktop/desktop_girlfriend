/**
 * @file app_gif.h
 * @brief GIF 播放器封装——基于 gifdec 解码器 + LVGL lv_img + lv_timer
 * @author mkk
 * @date 2026-05-31
 * @note 替代 LVGL 内置 lv_gif 控件（帧处理有 bug），
 *       使用 gifdec 纯 C 解码器正确处理帧动画，
 *       渲染到 ARGB8888 canvas，通过 lv_timer 驱动帧切换
 */

#ifndef __APP_GIF_H__
#define __APP_GIF_H__

#include "lvgl.h"

/**
 * @brief GIF 播放器实例（不透明指针）
 */
typedef struct app_gif_player app_gif_player_t;

/**
 * @brief 创建 GIF 播放器
 * @param parent 父容器，内部创建 lv_img 控件
 * @return 播放器指针，失败返回 NULL
 * @note 必须在 lvgl_port_lock() 保护下调用
 */
app_gif_player_t *app_gif_create(lv_obj_t *parent);

/**
 * @brief 设置 GIF 数据源并开始播放
 * @param player   播放器指针
 * @param data     GIF 文件原始数据（需保持有效直到下次 set_src 或 destroy）
 * @param data_size 数据字节数
 * @note 支持重复调用切换表情，自动清理旧资源
 */
void app_gif_set_src(app_gif_player_t *player, const uint8_t *data, int data_size);

/**
 * @brief 销毁 GIF 播放器
 * @param player 播放器指针
 * @note 停止定时器、释放 gifdec 资源和播放器结构体，
 *       lv_img 控件由页面容器的 lv_obj_clean() 自动销毁
 */
void app_gif_destroy(app_gif_player_t *player);

/**
 * @brief 获取 lv_img 控件（用于对齐、设置大小等）
 * @param player 播放器指针
 * @return lv_obj_t* lv_img 控件，失败返回 NULL
 */
lv_obj_t *app_gif_get_obj(app_gif_player_t *player);

#endif /* __APP_GIF_H__ */
