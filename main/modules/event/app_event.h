/**
 * @file app_event.h
 * @brief 应用事件系统头文件
 * @author mkk
 * @date 2026-05-30
 * @note 提供轻量级的事件回调注册和分发机制，
 *       用于模块间解耦通信
 */

#ifndef __APP_EVENT_H__
#define __APP_EVENT_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 应用事件类型枚举
 */
typedef enum {
    APP_EVENT_WIFI_CONNECTED,       /* WiFi 已连接到AP */
    APP_EVENT_WIFI_DISCONNECTED,    /* WiFi 连接断开 */
    APP_EVENT_WIFI_GOT_IP,          /* 获取到IP地址 */
    APP_EVENT_AP_START,             /* AP模式启动 */
    APP_EVENT_AP_STOP,              /* AP模式停止 */
    APP_EVENT_STA_START,            /* Station模式启动 */
} app_event_t;

/**
 * @brief 事件回调函数类型
 * @param event 事件类型
 * @param data 事件相关数据（由发送者提供，可能为NULL）
 */
typedef void (*app_event_cb_t)(app_event_t event, void *data);

/**
 * @brief 初始化事件系统
 */
void app_event_init(void);

/**
 * @brief 注册事件回调
 * @param cb 回调函数指针
 * @return 0成功，-1失败（已满或参数无效）
 */
int app_event_register(app_event_cb_t cb);

/**
 * @brief 注销事件回调
 * @param cb 回调函数指针
 */
void app_event_unregister(app_event_cb_t cb);

/**
 * @brief 发送事件（同步调用所有已注册的回调）
 * @param event 事件类型
 * @param data 事件相关数据（可为NULL）
 */
void app_event_send(app_event_t event, void *data);

#endif /* __APP_EVENT_H__ */
