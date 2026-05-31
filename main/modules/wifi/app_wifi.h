/**
 * @file app_wifi.h
 * @brief WiFi配网模块头文件
 * @author mkk
 * @date 2026-05-30
 * @note 提供WiFi AP+STA模式配网功能，内置HTTP服务器
 */

#ifndef __APP_WIFI_H__
#define __APP_WIFI_H__

#include "esp_err.h"
#include <stdint.h>

/* WiFi连接超时时间(ms) */
#define WIFI_CONNECT_TIMEOUT 30000

/**
 * @brief 初始化WiFi模块
 * @return ESP_OK成功，其他值失败
 * @note 初始化网络接口、事件循环、WiFi驱动，注册事件处理函数
 */
esp_err_t app_wifi_init(void);

/**
 * @brief 启动WiFi和配网服务器
 * @return ESP_OK成功，其他值失败
 * @note 以AP+STA模式启动WiFi，并启动HTTP配网服务器
 */
esp_err_t app_wifi_start(void);

/**
 * @brief 断开WiFi连接
 * @return ESP_OK成功，其他值失败
 */
esp_err_t app_wifi_disconnect(void);

/**
 * @brief 获取WiFi连接状态
 * @return 1已连接，0未连接
 */
uint8_t app_wifi_get_status(void);

/**
 * @brief 获取当前连接的IP地址
 * @return IP地址字符串，未连接时返回 "0.0.0.0"
 */
const char *app_wifi_get_ip(void);

/**
 * @brief 获取当前连接的WiFi名称
 * @return SSID字符串，未连接时返回空字符串
 */
const char *app_wifi_get_ssid(void);

/**
 * @brief 获取STA模式的MAC地址
 * @param mac 存储MAC地址的缓冲区（至少6字节）
 * @return ESP_OK成功，其他值失败
 */
esp_err_t app_wifi_get_mac(uint8_t *mac);

/**
 * @brief 关闭AP模式，保留STA模式
 * @return ESP_OK成功，其他值失败
 */
esp_err_t app_wifi_stop_ap(void);

/**
 * @brief 获取配网AP热点名称
 * @return AP SSID字符串，未进入配网模式时返回空字符串
 */
const char *app_wifi_get_ap_ssid(void);

/**
 * @brief 获取配网AP热点密码
 * @return AP密码字符串
 */
const char *app_wifi_get_ap_password(void);

#endif /* __APP_WIFI_H__ */
