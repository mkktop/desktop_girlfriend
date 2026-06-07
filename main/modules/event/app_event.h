/**
 * @file app_event.h
 * @brief 事件系统头文件
 * @author mkk
 * @date 2026-05-30
 * @note 基于 FreeRTOS EventGroup 的事件驱动架构，
 *       支持观察者模式注册和延迟回调（Schedule）机制，
 *       所有事件处理在主线程上下文执行，保证线程安全
 */

#ifndef __APP_EVENT_H__
#define __APP_EVENT_H__

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

/* ============================================================
 * 事件位分配（FreeRTOS EventGroup 共 24 位可用）
 *
 * Bit 0-5:   WiFi 事件
 * Bit 6-7:   WiFi 操作状态
 * Bit 8-9:   配网事件
 * Bit 10-11: 网络/应用事件
 * Bit 12-15: 音频事件
 * Bit 16-19: AI 对话事件（未来）
 * Bit 20-22: 系统事件（未来）
 * Bit 23:    SCHEDULE_PENDING（内部使用）
 * ============================================================ */

/* WiFi 事件 */
#define APP_EVENT_WIFI_CONNECTED       (1U << 0)  /* WiFi 已连接到AP */
#define APP_EVENT_WIFI_DISCONNECTED    (1U << 1)  /* WiFi 连接断开 */
#define APP_EVENT_WIFI_GOT_IP         (1U << 2)  /* 获取到IP地址 */
#define APP_EVENT_WIFI_AP_START       (1U << 3)  /* AP模式启动 */
#define APP_EVENT_WIFI_AP_STOP        (1U << 4)  /* AP模式停止 */
#define APP_EVENT_WIFI_STA_START      (1U << 5)  /* Station模式启动 */
#define APP_EVENT_WIFI_SCANNING       (1U << 6)  /* 正在扫描WiFi */
#define APP_EVENT_WIFI_CONNECTING     (1U << 7)  /* 正在连接WiFi */
#define APP_EVENT_WIFI_CONFIG_ENTER   (1U << 8)  /* 进入配网模式 */
#define APP_EVENT_WIFI_CONFIG_EXIT    (1U << 9)  /* 退出配网模式 */

/* 网络/应用事件 */
#define APP_EVENT_TIME_SYNCED         (1U << 10) /* SNTP 时间同步完成 */

/* 音频事件 */
#define APP_EVENT_AUDIO_OUTPUT_IDLE   (1U << 12) /* 播放完成，输出空闲 */
#define APP_EVENT_AUDIO_INPUT_READY   (1U << 13) /* 录音数据就绪 */

/* 内部：延迟回调队列就绪 */
#define APP_EVENT_SCHEDULE_PENDING (1U << 23)

/* 等待所有事件位（不含 SCHEDULE_PENDING） */
#define APP_EVENT_ALL_BITS  (0x00FFFFFF)

/**
 * @brief 事件观察者回调函数类型
 * @param event_bits 触发的事件位掩码
 * @param user_ctx   注册时传入的用户上下文
 */
typedef void (*app_event_handler_t)(EventBits_t event_bits, void *user_ctx);

/**
 * @brief 延迟回调函数类型（Schedule 机制）
 * @param arg 用户传入的参数
 */
typedef void (*app_schedule_fn_t)(void *arg);

/**
 * @brief 初始化事件系统
 * @note 创建 EventGroup、mutex 和 Schedule 队列，
 *       必须在其他 app_event 函数之前调用
 */
void app_event_init(void);

/**
 * @brief 阻塞等待事件位
 * @param bits_to_wait 等待的事件位掩码
 * @param clear_on_exit 是否在返回时清除匹配的位
 * @param timeout_ms 超时时间（ms），portMAX_DELAY 永久等待
 * @return 返回时设置的事件位
 */
EventBits_t app_event_wait(EventBits_t bits_to_wait, bool clear_on_exit, uint32_t timeout_ms);

/**
 * @brief 设置事件位（线程安全）
 * @param bits 要设置的事件位掩码
 * @note 可从任意任务或中断调用，唤醒阻塞在 app_event_wait 的主循环
 */
void app_event_set_bits(EventBits_t bits);

/**
 * @brief 注册事件观察者
 * @param handler   回调函数
 * @param event_bits 关心的事件位掩码（仅这些事件触发时调用）
 * @param user_ctx  传递给回调的用户上下文
 * @return 0成功，-1失败（已满或参数无效）
 */
int app_event_register_handler(app_event_handler_t handler, EventBits_t event_bits, void *user_ctx);

/**
 * @brief 注销事件观察者
 * @param handler 之前注册的回调函数
 */
void app_event_unregister_handler(app_event_handler_t handler);

/**
 * @brief 向匹配的观察者分发事件
 * @param bits 当前触发的事件位
 * @note 由主循环调用，在主线程上下文中执行所有匹配的回调
 */
void app_event_dispatch_to_handlers(EventBits_t bits);

/**
 * @brief 提交延迟回调到主线程执行
 * @param fn  回调函数
 * @param arg 传递给回调的参数
 * @return 0成功，-1队列已满
 * @note 可从任意任务调用，回调在主循环中执行
 */
int app_event_schedule(app_schedule_fn_t fn, void *arg);

/**
 * @brief 执行所有排队的延迟回调
 * @note 由主循环在收到 SCHEDULE_PENDING 后调用
 */
void app_event_schedule_drain(void);

#endif /* __APP_EVENT_H__ */
