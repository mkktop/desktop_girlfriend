/**
 * @file app_sntp.h
 * @brief SNTP 时间同步模块头文件
 * @author mkk
 * @date 2026-05-31
 * @note 基于 ESP-IDF lwIP SNTP 实现，WiFi 获取 IP 后自动启动同步，
 *       使用公共 NTP 服务器（阿里云、腾讯云），
 *       同步完成后通过事件系统通知主线程更新 UI
 */

#ifndef __APP_SNTP_H__
#define __APP_SNTP_H__

#include <stdbool.h>

/**
 * @brief 初始化 SNTP 模块
 * @note 注册 WiFi 事件处理器，实际同步在获取 IP 后自动启动，
 *       必须在事件系统初始化之后调用
 */
void app_sntp_init(void);

/**
 * @brief 查询时间是否已同步
 * @return true 已同步，false 未同步
 */
bool app_sntp_is_synced(void);

#endif /* __APP_SNTP_H__ */
