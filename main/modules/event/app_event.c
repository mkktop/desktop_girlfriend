/**
 * @file app_event.c
 * @brief 应用事件系统实现
 * @author mkk
 * @date 2026-05-30
 * @note 轻量级事件回调系统，支持多个订阅者，
 *       同步分发事件（在发送者的线程上下文中执行回调）
 */

#include "app_event.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "app_event";

/* 最大回调数量 */
#define APP_EVENT_MAX_CALLBACKS 8

/* 回调函数指针数组 */
static app_event_cb_t s_callbacks[APP_EVENT_MAX_CALLBACKS];

/* 事件名称表（用于日志） */
static const char *s_event_names[] = {
    [APP_EVENT_WIFI_CONNECTED]    = "WIFI_CONNECTED",
    [APP_EVENT_WIFI_DISCONNECTED] = "WIFI_DISCONNECTED",
    [APP_EVENT_WIFI_GOT_IP]       = "WIFI_GOT_IP",
    [APP_EVENT_AP_START]          = "AP_START",
    [APP_EVENT_AP_STOP]           = "AP_STOP",
    [APP_EVENT_STA_START]         = "STA_START",
};

void app_event_init(void)
{
    memset(s_callbacks, 0, sizeof(s_callbacks));
    ESP_LOGI(TAG, "Event system initialized");
}

int app_event_register(app_event_cb_t cb)
{
    if (!cb) {
        ESP_LOGE(TAG, "Invalid callback");
        return -1;
    }

    for (int i = 0; i < APP_EVENT_MAX_CALLBACKS; i++) {
        if (s_callbacks[i] == NULL) {
            s_callbacks[i] = cb;
            ESP_LOGI(TAG, "Callback registered at slot %d", i);
            return 0;
        }
    }

    ESP_LOGE(TAG, "No free callback slots");
    return -1;
}

void app_event_unregister(app_event_cb_t cb)
{
    if (!cb) {
        return;
    }

    for (int i = 0; i < APP_EVENT_MAX_CALLBACKS; i++) {
        if (s_callbacks[i] == cb) {
            s_callbacks[i] = NULL;
            ESP_LOGI(TAG, "Callback unregistered from slot %d", i);
            return;
        }
    }
}

void app_event_send(app_event_t event, void *data)
{
    const char *name = (event < sizeof(s_event_names) / sizeof(s_event_names[0]))
                       ? s_event_names[event] : "UNKNOWN";
    ESP_LOGI(TAG, "Event: %s", name);

    for (int i = 0; i < APP_EVENT_MAX_CALLBACKS; i++) {
        if (s_callbacks[i]) {
            s_callbacks[i](event, data);
        }
    }
}
