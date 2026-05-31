/**
 * @file ui_home.h
 * @brief 首页UI头文件（聊天表情界面）
 * @author mkk
 * @date 2026-05-31
 * @note 首页布局：居中 GIF 动态表情 + 底部对话字幕，
 *       表情使用 gifdec 解码器播放 64x64 Noto Emoji GIF，
 *       后续可替换为自定义表情
 */

#ifndef __UI_HOME_H__
#define __UI_HOME_H__

#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

/**
 * @brief 创建首页UI
 * @param parent 页面容器，组件创建在 parent 下
 * @note 必须在 lvgl_port_lock() 保护下调用
 */
void ui_home_create(lv_obj_t *parent);

/**
 * @brief 销毁首页资源
 * @note 在页面切换前调用，释放 GIF 播放器等资源
 */
void ui_home_destroy(void);

/**
 * @brief 首页事件处理
 * @param event_bits 当前触发的事件位
 */
void ui_home_on_event(EventBits_t event_bits);

/**
 * @brief 设置表情
 * @param emotion 表情名称，支持 21 种：
 *        neutral, happy, laughing, funny, sad, angry, crying,
 *        loving, embarrassed, surprised, shocked, thinking,
 *        winking, cool, relaxed, delicious, kissy, confident,
 *        sleepy, silly, confused
 * @note 必须在 lvgl_port_lock() 保护下调用
 */
void ui_home_set_emotion(const char *emotion);

/**
 * @brief 设置底部对话字幕
 * @param text 字幕文字（传空字符串或 NULL 清除）
 * @note 必须在 lvgl_port_lock() 保护下调用
 */
void ui_home_set_subtitle(const char *text);

#endif /* __UI_HOME_H__ */
