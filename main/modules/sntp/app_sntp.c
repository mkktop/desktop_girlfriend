/**
 * @file app_sntp.c
 * @brief SNTP 时间同步模块实现
 * @author mkk
 * @date 2026-05-31
 * @note WiFi 获取 IP 后启动 SNTP 同步，使用 esp_sntp API（ESP-IDF 5.x），
 *       同步完成后通过 app_event_set_bits() 通知主线程，
 *       启动 60 秒周期定时器持续刷新时钟显示
 */

#include "app_sntp.h"
#include "app_event.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_timer.h"

#define TAG "app_sntp"

/* 时钟刷新间隔（毫秒） */
#define CLOCK_UPDATE_INTERVAL_MS 60000

/* NTP 服务器 */
#define SNTP_SERVER_PRIMARY   "ntp.aliyun.com"
#define SNTP_SERVER_SECONDARY "ntp.tencent.com"

/* 模块状态 */
static bool s_synced = false;
static bool s_sntp_started = false;
static esp_timer_handle_t s_clock_timer = NULL;

/**
 * @brief 时钟刷新定时器回调
 * @note 运行在 esp_timer 任务中，通过事件位通知主线程更新 UI
 */
static void clock_timer_cb(void *arg)
{
    (void)arg;
    app_event_set_bits(APP_EVENT_TIME_SYNCED);
}

/**
 * @brief SNTP 时间同步通知回调
 * @param tv 同步后的时间
 * @note 运行在 lwIP 线程，不能直接操作 LVGL
 */
static void sntp_sync_time_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synced: %lld", (long long)tv->tv_sec);
    s_synced = true;

    /* 通知主线程更新时钟显示 */
    app_event_set_bits(APP_EVENT_TIME_SYNCED);

    /* 启动周期定时器持续刷新时钟 */
    if (!s_clock_timer) {
        const esp_timer_create_args_t timer_args = {
            .callback = clock_timer_cb,
            .name = "clock_timer",
        };
        esp_timer_create(&timer_args, &s_clock_timer);
    }

    /* 首次同步后启动周期定时器（如果尚未启动） */
    if (!esp_timer_is_active(s_clock_timer)) {
        esp_timer_start_periodic(s_clock_timer, CLOCK_UPDATE_INTERVAL_MS * 1000);
        ESP_LOGI(TAG, "Clock timer started (interval %d ms)", CLOCK_UPDATE_INTERVAL_MS);
    }
}

/**
 * @brief 启动 SNTP 同步
 */
static void start_sntp(void)
{
    if (s_sntp_started) {
        ESP_LOGW(TAG, "SNTP already started");
        return;
    }

    ESP_LOGI(TAG, "Starting SNTP sync...");

    /* 设置时区：中国标准时间 UTC+8 */
    setenv("TZ", "CST-8", 1);
    tzset();

    /* 配置 SNTP */
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, SNTP_SERVER_PRIMARY);
    esp_sntp_setservername(1, SNTP_SERVER_SECONDARY);
    esp_sntp_set_time_sync_notification_cb(sntp_sync_time_cb);

    /* 启动 SNTP */
    esp_sntp_init();
    s_sntp_started = true;

    ESP_LOGI(TAG, "SNTP started, servers: %s, %s", SNTP_SERVER_PRIMARY, SNTP_SERVER_SECONDARY);
}

/**
 * @brief 停止 SNTP 同步
 */
static void stop_sntp(void)
{
    if (!s_sntp_started) {
        return;
    }

    esp_sntp_stop();
    s_sntp_started = false;
    s_synced = false;

    /* 停止时钟定时器 */
    if (s_clock_timer && esp_timer_is_active(s_clock_timer)) {
        esp_timer_stop(s_clock_timer);
    }

    ESP_LOGI(TAG, "SNTP stopped");
}

/**
 * @brief SNTP 事件处理器（由主循环分发调用）
 * @param event_bits 触发的事件位
 * @param user_ctx   用户上下文（未使用）
 */
static void sntp_event_handler(EventBits_t event_bits, void *user_ctx)
{
    if (event_bits & APP_EVENT_WIFI_GOT_IP) {
        start_sntp();
    }
    if (event_bits & APP_EVENT_WIFI_DISCONNECTED) {
        stop_sntp();
    }
}

/**
 * @brief 初始化 SNTP 模块
 */
void app_sntp_init(void)
{
    /* 注册 WiFi 事件处理器 */
    const EventBits_t bits = APP_EVENT_WIFI_GOT_IP | APP_EVENT_WIFI_DISCONNECTED;
    app_event_register_handler(sntp_event_handler, bits, NULL);
    ESP_LOGI(TAG, "SNTP module initialized");
}

/**
 * @brief 查询时间是否已同步
 */
bool app_sntp_is_synced(void)
{
    return s_synced;
}
