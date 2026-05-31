/**
 * @file ui_wifi_config.h
 * @brief WiFi配网引导页面头文件
 * @author mkk
 * @date 2026-05-31
 * @note 配网模式下在屏幕上显示AP热点名称、密码和浏览器访问地址
 */

#ifndef __UI_WIFI_CONFIG_H__
#define __UI_WIFI_CONFIG_H__

/**
 * @brief 创建WiFi配网引导页面
 * @note 必须在 lvgl_port_lock() 保护下调用，
 *       从 app_wifi 获取 AP SSID 和密码动态生成引导文本
 */
void ui_wifi_config_create(void);

/**
 * @brief 销毁WiFi配网引导页面
 * @note 必须在 lvgl_port_lock() 保护下调用
 */
void ui_wifi_config_destroy(void);

#endif /* __UI_WIFI_CONFIG_H__ */
