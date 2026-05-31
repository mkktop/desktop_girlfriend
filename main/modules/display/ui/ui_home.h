/**
 * @file ui_home.h
 * @brief 首页UI头文件
 * @author mkk
 * @date 2026-05-31
 * @note 首页为简洁占位，后续替换为桌面女友主界面
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
 * @brief 首页事件处理
 * @param event_bits 当前触发的事件位
 */
void ui_home_on_event(EventBits_t event_bits);

#endif /* __UI_HOME_H__ */
