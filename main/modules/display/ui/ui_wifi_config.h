/**
 * @file ui_wifi_config.h
 * @brief WiFi配网引导页面头文件
 * @author mkk
 * @date 2026-05-31
 * @note 配网模式下在屏幕上显示AP热点名称、密码和浏览器访问地址
 */

#ifndef __UI_WIFI_CONFIG_H__
#define __UI_WIFI_CONFIG_H__

#include "lvgl.h"

/**
 * @brief 创建WiFi配网引导页面
 * @param parent 页面容器，组件创建在 parent 下
 * @note 必须在 lvgl_port_lock() 保护下调用，
 *       从 app_wifi 获取 AP SSID 和密码动态生成引导文本
 */
void ui_wifi_config_create(lv_obj_t *parent);

#endif /* __UI_WIFI_CONFIG_H__ */
